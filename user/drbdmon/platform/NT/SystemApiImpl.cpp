#include <platform/NT/SystemApiImpl.h>
#include <platform/IoException.h>
#include <subprocess/NT/SubProcessNt.h>
#include <terminal/NT/TerminalControlImpl.h>
#include <memory>
#include <stdexcept>

extern "C"
{
    #define NOMINMAX
    #include <windows.h>
    #include <userenv.h>
    #include <shlobj.h>
    #include <shlwapi.h>
    #include <objbase.h>
}

const char* const   NtApi::CONFIG_FILE_NAME     = "drbdmon.ces";
const char* const   NtApi::CONFIG_FILE_DIR      = "DRBDmon";

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

std::string NtApi::get_config_file_dir()
{
    std::string path;
    bool have_path = false;

    PWSTR app_data_path = nullptr;
    HRESULT dir_rc = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &app_data_path);
    if (dir_rc == S_OK && app_data_path != nullptr)
    {
        const int conv_out_size = WideCharToMultiByte(
            CP_UTF8,                // Convert to UTF-8
            WC_NO_BEST_FIT_CHARS,   // No similar characters, for path security
            app_data_path,          // UTF-16 input
            -1,                     // input length; input is null terminated
            nullptr,                // UTF-8 output, not used
            0,                      // output length, 0 to determine required buffer size
            nullptr,                // default character for invalid input
            nullptr                 // address of flag indicating default character use, not used
        );
        if (conv_out_size > 0)
        {
            const size_t out_buffer_size = static_cast<size_t> (conv_out_size);
            std::unique_ptr<char[]> app_data_path_utf8_mgr = std::unique_ptr<char[]>(new char[out_buffer_size]);
            char* const app_data_path_utf8 = app_data_path_utf8_mgr.get();
            const int conv_length = WideCharToMultiByte(
                CP_UTF8,
                WC_NO_BEST_FIT_CHARS,
                app_data_path,
                -1,
                app_data_path_utf8,
                conv_out_size,
                nullptr,
                nullptr
            );
            if (conv_length == conv_out_size)
            {
                path.append(app_data_path_utf8);
                path.append(1, '\\');
                path.append(CONFIG_FILE_DIR);
                have_path = true;
            }
        }
    }

    if (app_data_path != nullptr)
    {
        CoTaskMemFree(app_data_path);
    }

    if (!have_path)
    {
        throw IoException("Cannot determine the %LOCALAPPDATA% path");
    }

    return path;
}

std::string NtApi::get_config_file_path()
{
    std::string path = get_config_file_dir();

    path.append(1, '\\');
    path.append(CONFIG_FILE_NAME);

    return path;
}

void NtApi::prepare_save_config_file()
{
    std::string path = get_config_file_dir();

    const char* const api_path = path.c_str();
    const bool path_exists = is_file_accessible(api_path);
    if (!path_exists)
    {
        const bool path_created = CreateDirectoryA(api_path, nullptr);
        if (!path_created)
        {
            throw IoException("Cannot create configuration directory");
        }
    }
}

std::string NtApi::file_name_for_path(const std::string path) const
{
    std::string file_name;
    const size_t split_idx = path.rfind('\\');
    if (split_idx == std::string::npos)
    {
        file_name = path;
    }
    else
    {
        file_name = path.substr(split_idx + 1);
    }
    return file_name;
}

bool NtApi::is_file_accessible(const char* const file_path) const
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

    void init_node_name(std::string& node_name)
    {
        node_name.clear();
        try
        {
            std::unique_ptr<char[]> name_buffer(new char[MAX_COMPUTER_NAME_LENGTH]);
            DWORD length = static_cast<DWORD> (MAX_COMPUTER_NAME_LENGTH);
            BOOL rc = GetComputerNameExA(ComputerNamePhysicalNetBIOS, name_buffer.get(), &length);
            if (rc != FALSE)
            {
                if (length >= 1 && length < MAX_COMPUTER_NAME_LENGTH)
                {
                    node_name->append(name_buffer.get(), length);
                }
            }
        }
        catch (std::bad_alloc&)
        {
            // no-op
        }
    }
}
