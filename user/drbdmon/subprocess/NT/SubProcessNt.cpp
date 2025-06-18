#include <subprocess/NT/SubProcessNt.h>

std::atomic<uint64_t>   SubProcessNt::instance_id(0);

const DWORD     SubProcessNt::OUT_BUFFER_SIZE   = 0;
const DWORD     SubProcessNt::IN_BUFFER_SIZE    = 8192;

// Buffer capacity increment steps. Increment steps must be larger than the buffer size for efficient operation.
// 16 kiB, 64 kiB
const size_t    SubProcessNt::BUFFER_CAP[]      = {1UL << 14, 1UL << 16};
const size_t    SubProcessNt::BUFFER_CAP_SIZE   = sizeof (SubProcessNt::BUFFER_CAP) / sizeof (size_t);

SubProcessNt::SubProcessNt():
    SubProcess::SubProcess()
{
}

SubProcessNt::~SubProcessNt() noexcept
{
    safe_close_handle(&events_pipe);
    safe_close_handle(&errors_pipe);
    safe_close_handle(&events_writer);
    safe_close_handle(&errors_writer);
    safe_close_handle(&proc_handle);
    safe_close_handle(&io_port);
}

uint64_t SubProcessNt::get_pid() const noexcept
{
    proc_lock.lock();
    const uint64_t generic_pid = static_cast<uint64_t> (proc_id);
    proc_lock.unlock();
    return generic_pid;
}

void SubProcessNt::execute(const CmdLine& cmd)
{
    const uint64_t pipes_id = instance_id.fetch_add(1);
    const DWORD this_proc_id = GetCurrentProcessId();

    {
        std::string id_suffix;
        id_suffix.reserve(40);
        id_suffix.append(std::to_string(static_cast<unsigned long> (this_proc_id)));
        id_suffix.append(".");
        id_suffix.append(std::to_string(static_cast<unsigned long long> (pipes_id)));

        std::string events_pipe_name;
        events_pipe_name.reserve(80);
        events_pipe_name.append("\\\\.\\pipe\\DRBDmon-subproc-out.");
        events_pipe_name.append(id_suffix);

        std::string errors_pipe_name;
        errors_pipe_name.reserve(80);
        errors_pipe_name.append("\\\\.\\pipe\\DRBDmon-subproc-err.");
        errors_pipe_name.append(id_suffix);

        events_pipe = create_pipe(events_pipe_name);
        errors_pipe = create_pipe(errors_pipe_name);

        events_writer = create_pipe_writer(events_pipe_name);
        errors_writer = create_pipe_writer(errors_pipe_name);
    }

    io_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (io_port == INVALID_HANDLE_VALUE)
    {
        throw SubProcess::Exception("I/O Error: Completion port initialization failed");
    }

    CreateIoCompletionPort(events_pipe, io_port, events_key, 0);
    CreateIoCompletionPort(errors_pipe, io_port, errors_key, 0);

    HANDLE spawned_proc_handle = spawn_process(cmd, events_writer, errors_writer);
    safe_close_handle(&events_writer);
    safe_close_handle(&errors_writer);

    if (spawned_proc_handle != INVALID_HANDLE_VALUE)
    {
        read_subproc_output();
        safe_close_handle(&io_port);
        await_subproc_exit();
    }
    else
    {
        safe_close_handle(&io_port);
    }
}

void SubProcessNt::terminate(const bool force)
{
    const int local_exit_status = exit_status.load();
    proc_lock.lock();
    const HANDLE local_proc_handle = proc_handle;
    enable_spawn = false;
    proc_lock.unlock();

    if (local_proc_handle != INVALID_HANDLE_VALUE && local_exit_status == SubProcess::EXIT_STATUS_NONE)
    {
        // Termination is always forced on NT, as there is no way to gracefully shut down
        // external command line or background processes
        TerminateProcess(local_proc_handle, 1);
    }
}

HANDLE SubProcessNt::create_pipe(const std::string& pipe_name)
{
    HANDLE pipe_handle = CreateNamedPipeA(
        pipe_name.c_str(),
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, // instances
        OUT_BUFFER_SIZE, // out buffer size
        IN_BUFFER_SIZE, // in buffer size
        0, // timeout (not used)
        nullptr
    );
    if (pipe_handle == INVALID_HANDLE_VALUE)
    {
        throw SubProcess::Exception("Pipe creation failed");
    }
    return pipe_handle;
}

HANDLE SubProcessNt::create_pipe_writer(const std::string& pipe_name)
{
    std::unique_ptr<SECURITY_ATTRIBUTES> writer_sec_attr(new SECURITY_ATTRIBUTES);
    ZeroMemory(writer_sec_attr.get(), sizeof (*writer_sec_attr));
    writer_sec_attr->nLength = sizeof (*writer_sec_attr);
    writer_sec_attr->bInheritHandle = TRUE;
    HANDLE writer_handle = CreateFileA(
        pipe_name.c_str(),
        GENERIC_WRITE,
        0, // No shared access
        writer_sec_attr.get(),
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        nullptr
    );
    if (writer_handle == INVALID_HANDLE_VALUE)
    {
        throw SubProcess::Exception("Attaching a writer to a pipe failed");
    }
    return writer_handle;
}

HANDLE SubProcessNt::spawn_process(const CmdLine& cmd, HANDLE events_writer, HANDLE errors_writer)
{
    std::unique_ptr<PROCESS_INFORMATION> cmd_proc_info_mgr(new PROCESS_INFORMATION);
    PROCESS_INFORMATION* const cmd_proc_info = cmd_proc_info_mgr.get();
    ZeroMemory(cmd_proc_info, sizeof (*cmd_proc_info));

    std::unique_ptr<STARTUPINFO> cmd_startup_info_mgr(new STARTUPINFO);
    STARTUPINFO* const cmd_startup_info = cmd_startup_info_mgr.get();
    ZeroMemory(cmd_startup_info, sizeof (*cmd_startup_info));

    cmd_startup_info->cb = sizeof (*cmd_startup_info);
    cmd_startup_info->hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    cmd_startup_info->hStdOutput = events_writer;
    cmd_startup_info->hStdError = errors_writer;
    cmd_startup_info->dwFlags = STARTF_USESTDHANDLES;

    std::string exec_path;
    CmdLine::StringList::ValuesIterator arg_iter = cmd.get_argument_iterator();
    if (arg_iter.has_next())
    {
        const std::string* const arg_value = arg_iter.next();
        exec_path.append(*arg_value);
    }

    std::string command_line;
    command_line.reserve(1024);
    append_escaped(exec_path, command_line);
    while (arg_iter.has_next())
    {
        const std::string* const arg_value = arg_iter.next();
        command_line.append(" ");
        append_escaped(*arg_value, command_line);
    }

    const size_t command_line_buffer_length = command_line.length() + 1;
    std::unique_ptr<char[]> command_line_buffer = copy_to_buffer(command_line);

    BOOL rc = 0;
    proc_lock.lock();
    if (enable_spawn)
    {
        rc = CreateProcessA(
            exec_path.c_str(),
            command_line_buffer.get(),
            nullptr, // process SECURITY_ATTRIBUTES: default
            nullptr, // thread SECURITY_ATTRIBUTES: default
            TRUE, // inherit handles
            0, // creation flags
            nullptr, // environment: inherited
            nullptr, // default directory: inherited
            cmd_startup_info,
            cmd_proc_info
        );
    }
    if (rc == 0)
    {
        proc_lock.unlock();
        throw SubProcess::Exception("Execution of the external program failed");
    }
    else
    {
        safe_close_handle(&(cmd_proc_info->hThread));
        proc_handle = cmd_proc_info->hProcess;
        proc_id = cmd_proc_info->dwProcessId;
    }
    proc_lock.unlock();

    return proc_handle;
}

void SubProcessNt::read_subproc_output()
{
    std::unique_ptr<char[]> events_read_buffer(new char[READ_BUFFER_SIZE]);
    std::unique_ptr<char[]> errors_read_buffer(new char[READ_BUFFER_SIZE]);

    char* const events_read_buffer_ptr = events_read_buffer.get();
    char* const errors_read_buffer_ptr = errors_read_buffer.get();

    std::unique_ptr<OVERLAPPED> events_io_state(new OVERLAPPED);
    std::unique_ptr<OVERLAPPED> errors_io_state(new OVERLAPPED);

    OVERLAPPED* const events_io_state_ptr = events_io_state.get();
    ZeroMemory(events_io_state_ptr, sizeof (*events_io_state_ptr));
    OVERLAPPED* const errors_io_state_ptr = errors_io_state.get();
    ZeroMemory(errors_io_state_ptr, sizeof (*errors_io_state_ptr));

    submit_read_op(&events_pipe, events_io_state_ptr, events_read_buffer_ptr);
    submit_read_op(&errors_pipe, errors_io_state_ptr, errors_read_buffer_ptr);

    while (events_pipe != INVALID_HANDLE_VALUE || errors_pipe != INVALID_HANDLE_VALUE)
    {
        DWORD bytes_read = 0;
        ULONG_PTR op_key = 0;
        OVERLAPPED* op_io_state = nullptr;
        BOOL port_rc = GetQueuedCompletionStatus(
            io_port,
            &bytes_read,
            &op_key,
            &op_io_state,
            INFINITE
        );
        if (port_rc != 0)
        {
            // I/O operation result dequeued
            if (op_key == events_key)
            {
                read_completion_handler(
                    &events_pipe,
                    events_read_buffer_ptr,
                    op_io_state,
                    &subproc_out,
                    out_buffer_cap_idx,
                    bytes_read,
                    SUBPROC_OUT_MAX_SIZE
                );
            }
            else
            if (op_key == errors_key)
            {
                read_completion_handler(
                    &errors_pipe,
                    errors_read_buffer_ptr,
                    op_io_state,
                    &subproc_err,
                    err_buffer_cap_idx,
                    bytes_read,
                    SUBPROC_ERR_MAX_SIZE
                );
            }
        }
        else
        {
            if (op_io_state != nullptr)
            {
                // Failed I/O operation result dequeued
                if (op_key == events_key)
                {
                    safe_close_handle(&events_pipe);
                }
                else
                if (op_key == errors_key)
                {
                    safe_close_handle(&errors_pipe);
                }
            }
            else
            {
                // I/O error on the completion port, nothing dequeued
                safe_close_handle(&events_pipe);
                safe_close_handle(&errors_pipe);
            }
        }
    }
}

void SubProcessNt::submit_read_op(
    HANDLE*         op_handle_ptr,
    OVERLAPPED*     op_io_state,
    char* const     op_read_buffer
)
{
    ReadFile(
        *op_handle_ptr,
        op_read_buffer,
        READ_BUFFER_SIZE,
        nullptr,
        op_io_state
    );
    const DWORD error_id = GetLastError();
    if (error_id != ERROR_IO_PENDING)
    {
        safe_close_handle(op_handle_ptr);
    }
}

void SubProcessNt::read_completion_handler(
    HANDLE*         op_handle_ptr,
    char*           op_read_buffer,
    OVERLAPPED*     op_io_state,
    std::string*    op_data,
    size_t&         dst_buffer_cap_idx,
    DWORD           bytes_read,
    const size_t    max_length
)
{
    const size_t current_length = op_data->length();
    if (current_length < max_length)
    {
        const size_t copy_length = std::min(
            static_cast<size_t> (max_length - current_length),
            static_cast<size_t> (bytes_read)
        );
        const size_t result_length = current_length + copy_length;
        if (result_length > op_data->capacity())
        {
            if (dst_buffer_cap_idx < BUFFER_CAP_SIZE)
            {
                op_data->reserve(std::min(BUFFER_CAP[dst_buffer_cap_idx], max_length));
                ++dst_buffer_cap_idx;
            }
            else
            {
                op_data->reserve(max_length);
            }
        }
        op_data->append(op_read_buffer, copy_length);
    }
    submit_read_op(op_handle_ptr, op_io_state, op_read_buffer);
}

void SubProcessNt::await_subproc_exit()
{
    DWORD wait_rc = WaitForSingleObject(
        proc_handle,
        INFINITE
    );
    if (wait_rc == WAIT_OBJECT_0)
    {
        DWORD proc_exit_code = 255;
        BOOL rc = GetExitCodeProcess(proc_handle, &proc_exit_code);
        if (rc != 0)
        {
            exit_status.store(static_cast<int> (proc_exit_code));
        }
        else
        {
            exit_status.store(EXIT_STATUS_FAILED);
        }
    }
    safe_close_handle(&proc_handle);
}

void SubProcessNt::append_escaped(const std::string& src_text, std::string& dst_text)
{
    // FIXME: Implement escaping
    dst_text.append(src_text);
}

std::unique_ptr<char[]> SubProcessNt::copy_to_buffer(const std::string& text)
{
    const size_t text_length = text.length();
    std::unique_ptr<char[]> buffer(new char[text_length + 1]);
    if (text_length >= 1)
    {
        const char* const source_buffer = text.c_str();
        size_t idx = 0;
        while (idx < text_length)
        {
            buffer[idx] = source_buffer[idx];
            ++idx;
        }
        buffer[idx] = '\0';
    }
    return buffer;
}

void SubProcessNt::safe_close_handle(HANDLE* handle_ptr) noexcept
{
    if (*handle_ptr != INVALID_HANDLE_VALUE && *handle_ptr != nullptr)
    {
        CloseHandle(*handle_ptr);
        *handle_ptr = INVALID_HANDLE_VALUE;
    }
}
