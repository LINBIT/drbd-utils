#include <platform/NT/SystemApiImpl.h>
#include <subprocess/NT/SubProcessNt.h>
#include <terminal/NT/TerminalControlImpl.h>
#include <memory>
#include <stdexcept>

extern "C"
{
    #define NOMINMAX
    #include <windows.h>
}

const char* const   NtApi::CONFIG_FILE_NAME     = "drbdmon.ces";

NtApi::NtApi()
{
}

NtApi::~NtApi() noexcept
{
}

std::unique_ptr<SubProcess> NtApi::create_subprocess_handler()
{
    return std::unique_ptr<SubProcess>(dynamic_cast<SubProcess*> (new SubProcessNt()));
}

std::unique_ptr<TerminalControl> NtApi::create_terminal_control()
{
    return std::unique_ptr<TerminalControl>(dynamic_cast<TerminalControl*> (new TerminalControlImpl()));
}

std::string NtApi::get_config_file_path()
{
    std::string path;
    DWORD buffer_size = 0;
    HANDLE proc_token = GetCurrentProcessToken();
    BOOL rc = GetUserProfileDirectoryA(proc_token, nullptr, &buffer_size);
    if (buffer_size > 0)
    {
       std::unique_ptr<char[]> buffer_mgr(new char[buffer_size * sizeof (TCHAR)]);
       char* const buffer = buffer_mgr.get();
       rc = GetUserProfileDirectoryA(proc_token, buffer, buffer_size);
       if (rc != 0)
       {
           path.append(buffer);
           path.append(1, '\\');
           path.append(CONFIG_FILE_NAME);
       }
    }
    return path;
}

bool NtApi::is_file_accessible(const char* const file_path)
{
    return PathFileExistsA(file_path) != 0;
}

namespace system_api
{
    constexpr size_t MAX_COMPUTER_NAME_LENGTH = 256;

    std::unique_ptr<SystemApi> create_system_api()
    {
        return std::unique_ptr<SystemApi>(dynamic_cast<SystemApi*> (new NtApi()));
    }

    bool init_security(MessageLog& log)
    {
        // No-op on NT, there is no mechanism like Unix setuid for a process to gain privileges when
        // it is started by an unprivileged user
        return true;
    }

    void init_node_name(std::unique_ptr<std::string>& node_name_mgr)
    {
        try
        {
            std::unique_ptr<char[]> name_buffer(new char[MAX_COMPUTER_NAME_LENGTH]);
            DWORD length = static_cast<DWORD> (MAX_COMPUTER_NAME_LENGTH);
            BOOL rc = GetComputerNameExA(ComputerNamePhysicalNetBIOS, name_buffer.get(), &length);
            if (rc != FALSE)
            {
                if (length >= 1 && length < MAX_COMPUTER_NAME_LENGTH)
                {
                    node_name_mgr = std::unique_ptr<std::string>(new std::string());
                    node_name_mgr->append(name_buffer.get(), length);
                }
            }
        }
        catch (std::bad_alloc&)
        {
            node_name_mgr = nullptr;
        }
    }
}
