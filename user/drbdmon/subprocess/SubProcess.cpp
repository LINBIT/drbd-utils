#include <subprocess/SubProcess.h>

const size_t    SubProcess::SUBPROC_OUT_MAX_SIZE    = 1UL << 18; // 256 kiB
const size_t    SubProcess::SUBPROC_ERR_MAX_SIZE    = 1UL << 18; // 256 kiB

const size_t    SubProcess::READ_BUFFER_SIZE        = 1UL << 13; // 8 kiB

const int       SubProcess::EXIT_STATUS_NONE        = -1;
const int       SubProcess::EXIT_STATUS_FAILED      = -2;

SubProcess::SubProcess():
    exit_status(EXIT_STATUS_NONE)
{
}

SubProcess::~SubProcess() noexcept
{
}

int SubProcess::get_exit_status() const
{
    return exit_status.load();
}

const std::string& SubProcess::get_stdout_output() const noexcept
{
    return subproc_out;
}

const std::string& SubProcess::get_stderr_output() const noexcept
{
    return subproc_err;
}

// @throws std::bad_alloc
SubProcess::Exception::Exception()
{
    saved_error_msg = "SubProcess::Exception";
}

// @throws std::bad_alloc
SubProcess::Exception::Exception(const char* const error_msg)
{
    if (error_msg != nullptr)
    {
        saved_error_msg = error_msg;
    }
}

// @throws std::bad_alloc
SubProcess::Exception::Exception(const std::string& error_msg)
{

    saved_error_msg = error_msg;
}

SubProcess::Exception::~Exception() noexcept
{
}

const char* SubProcess::Exception::what() const noexcept
{
    return saved_error_msg.c_str();
}
