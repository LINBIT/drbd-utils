#ifndef SUBPROCESS_H
#define SUBPROCESS_H

#include <default_types.h>
#include <new>
#include <memory>
#include <atomic>
#include <stdexcept>
#include <subprocess/CmdLine.h>

class SubProcess
{
  public:
    static const size_t     SUBPROC_OUT_MAX_SIZE;
    static const size_t     SUBPROC_ERR_MAX_SIZE;

    static const size_t     READ_BUFFER_SIZE;

    static const int        EXIT_STATUS_NONE;
    static const int        EXIT_STATUS_FAILED;

    SubProcess();
    virtual ~SubProcess() noexcept;

    virtual int get_exit_status() const;
    virtual const std::string& get_stdout_output() const noexcept;
    virtual const std::string& get_stderr_output() const noexcept;

    // @throws SubProcess::Exception
    virtual uint64_t get_pid() const noexcept = 0;
    virtual void execute(const CmdLine& cmd) = 0;
    virtual void terminate(const bool force) = 0;

    class Exception : public std::exception
    {
      public:
        // @throws std::bad_alloc
        Exception();
        // @throws std::bad_alloc
        Exception(const char* const error_msg);
        // @throws std::bad_alloc
        Exception(const std::string& error_msg);
        virtual ~Exception() noexcept;
        virtual const char* what() const noexcept override;
      private:
        std::string saved_error_msg;
    };

  protected:
    std::atomic<int> exit_status;

    std::string subproc_out;
    std::string subproc_err;
};

#endif /* SUBPROCESS_H */
