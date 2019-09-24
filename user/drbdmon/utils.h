#ifndef UTILS_H
#define	UTILS_H

#include <string>
#include <cstdlib>
#include <stdexcept>

#include <exceptions.h>

extern "C"
{
    #include <spawn.h>
    #include <sys/ioctl.h>
}

class TermSize
{
  public:
    TermSize();
    TermSize(const TermSize& orig) = default;
    TermSize& operator=(const TermSize& orig) = default;
    TermSize(TermSize&& orig) = default;
    TermSize& operator=(TermSize&& orig) = default;
    virtual ~TermSize() noexcept
    {
    }

    virtual bool probe_terminal_size();
    virtual bool is_valid() const;
    virtual uint16_t get_size_x() const;
    virtual uint16_t get_size_y() const;

  private:
    struct winsize term_size;
    bool valid {false};
};

namespace posix
{
    extern const size_t PIPE_READ_SIDE;
    extern const size_t PIPE_WRITE_SIDE;

    void close_fd(int& fd) noexcept;

    class SpawnFileActions
    {
      public:
        std::unique_ptr<posix_spawn_file_actions_t> actions_mgr;
        posix_spawn_file_actions_t* actions;

        // @throws std::bad_alloc, EventsSourceException
        SpawnFileActions();
        virtual ~SpawnFileActions() noexcept;
        SpawnFileActions(const SpawnFileActions& other) = delete;
        SpawnFileActions(SpawnFileActions&& orig) = delete;
        virtual SpawnFileActions& operator=(const SpawnFileActions& other) = delete;
        virtual SpawnFileActions& operator=(SpawnFileActions&& orig) = delete;
    };

    class SpawnAttr
    {
      public:
        std::unique_ptr<posix_spawnattr_t> attr_mgr;
        posix_spawnattr_t* attr;

        // @throws std::bad_alloc, EventsSourceException
        SpawnAttr();
        virtual ~SpawnAttr() noexcept;
        SpawnAttr(const SpawnAttr& orig) = delete;
        SpawnAttr(SpawnAttr&& orig) = delete;
        virtual SpawnAttr& operator=(const SpawnAttr& orig) = delete;
        virtual SpawnAttr& operator=(SpawnAttr&& orig) = delete;
    };

    class SpawnArgs
    {
      public:
        char** args {nullptr};

        // @throws std::bad_alloc
        SpawnArgs(const char* const arg_list[]);
        virtual ~SpawnArgs() noexcept;
        SpawnArgs(const SpawnArgs& orig) = delete;
        SpawnArgs(SpawnArgs&& orig) = delete;
        virtual SpawnArgs& operator=(const SpawnArgs& orig) = delete;
        virtual SpawnArgs& operator=(SpawnArgs&& orig) = delete;

      private:
        size_t args_size {0};
        void destroy() noexcept;
    };

    class Pipe
    {
      public:
        // @throws EventsSourceException
        Pipe(int (*pipe_fd)[2]);
        ~Pipe() noexcept;
        Pipe(const Pipe& orig) = delete;
        Pipe(Pipe&& orig) = delete;
        virtual Pipe& operator=(const Pipe& orig) = delete;
        virtual Pipe& operator=(Pipe&& orig) = delete;
        virtual void release() noexcept;
        virtual void close_read_side() noexcept;
        virtual void close_write_side() noexcept;
      private:
        int (*managed_pipe)[2];
    };
}

#endif	/* UTILS_H */
