#ifndef SUBPROCESSLX_H
#define SUBPROCESSLX_H

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
    #include <unistd.h>
}

class SubProcessLx : public SubProcess
{
  public:
    static const size_t     OBSERVED_EVENTS_COUNT;
    static const size_t     FIRED_EVENTS_COUNT;

    static const size_t     BUFFER_CAP[];
    static const size_t     BUFFER_CAP_SIZE;

    SubProcessLx();
    virtual ~SubProcessLx() noexcept;

    // @throws SubProcess::Exception
    virtual uint64_t get_pid() const noexcept override;
    virtual void execute(const CmdLine& cmd) override;
    virtual void terminate(const bool force) override;

  private:
    mutable std::mutex  proc_lock;

    static constexpr size_t PIPE_READ   = 0;
    static constexpr size_t PIPE_WRITE  = 1;

    bool    enable_spawn    {true};
    pid_t   subproc_id      {-1};

    int poll_fd = -1;

    size_t  out_buffer_cap_idx  = 0;
    size_t  err_buffer_cap_idx  = 0;

    int subproc_stdout_pipe[2];
    int subproc_stderr_pipe[2];
    int wakeup_pipe[2];

    // @throws SubProcess::Exception, std::bad_alloc
    void read_subproc_output();
    // @throws std::bad_alloc
    void read_subproc_fd(
        int&            subproc_fd,
        char* const     read_buffer_ptr,
        std::string&    dst_buffer,
        size_t&         dst_buffer_cap_idx,
        const size_t    max_size
    );
    void throw_if_nonzero(const int rc, const char* const error_msg);
    void close_fd(int& fd) noexcept;

    class SysExecArgs
    {
      public:
        SysExecArgs(const CmdLine& cmd);
        virtual ~SysExecArgs() noexcept;

        virtual char** get_exec_args() noexcept;

      private:
        const size_t arg_count = 0;
        std::unique_ptr<char*[]> arg_array_mgr;

        void cleanup() noexcept;
    };
};

#endif /* SUBPROCESSLX_H */
