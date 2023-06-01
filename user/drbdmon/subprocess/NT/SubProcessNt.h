#ifndef SUBPROCESSNT_H
#define SUBPROCESSNT_H

#include <cstddef>
#include <new>
#include <memory>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <VList.h>
#include <subprocess/CmdLine.h>
#include <subprocess/SubProcess.h>

extern "C"
{
    #define NOMINMAX
    #include <windows.h>
    #include <processthreadsapi.h>
    #include <synchapi.h>
}

class SubProcessNt : public SubProcess
{
  public:
    static const DWORD  OUT_BUFFER_SIZE;
    static const DWORD  IN_BUFFER_SIZE;

    static const size_t BUFFER_CAP[];
    static const size_t BUFFER_CAP_SIZE;

    SubProcessNt();
    virtual ~SubProcessNt() noexcept;

    // @throws SubProcess::Exception
    virtual uint64_t get_pid() const noexcept override;
    virtual void execute(const CmdLine& cmd) override;
    virtual void terminate(const bool force) override;

  private:
    mutable std::mutex  proc_lock;
    bool                enable_spawn    {true};

    static std::atomic<uint64_t>    instance_id;

    HANDLE events_pipe      {INVALID_HANDLE_VALUE};
    HANDLE errors_pipe      {INVALID_HANDLE_VALUE};
    HANDLE events_writer    {INVALID_HANDLE_VALUE};
    HANDLE errors_writer    {INVALID_HANDLE_VALUE};
    HANDLE proc_handle      {INVALID_HANDLE_VALUE};
    HANDLE io_port          {INVALID_HANDLE_VALUE};

    DWORD proc_id           {0};

    ULONG_PTR events_key    {1};
    ULONG_PTR errors_key    {2};

    size_t  out_buffer_cap_idx  = 0;
    size_t  err_buffer_cap_idx  = 0;

    HANDLE create_pipe(const std::string& pipe_name);
    HANDLE create_pipe_writer(const std::string& pipe_name);
    HANDLE spawn_process(const CmdLine& cmd, HANDLE events_writer, HANDLE errors_writer);
    void read_subproc_output();
    void submit_read_op(
        HANDLE*         op_handle_ptr,
        OVERLAPPED*     op_io_state,
        char* const     op_read_buffer
    );
    void read_completion_handler(
        HANDLE*         op_handle_ptr,
        char*           op_read_buffer,
        OVERLAPPED*     op_io_state,
        std::string*    op_data,
        size_t&         dst_buffer_cap_idx,
        DWORD           bytes_read,
        const size_t    max_length
    );
    void await_subproc_exit();

    void append_escaped(const std::string& src_text, std::string& dst_text);
    std::unique_ptr<char[]> copy_to_buffer(const std::string& text);

    void safe_close_handle(HANDLE* const handle_ptr) noexcept;
};

#endif /* SUBPROCESSNT_H */
