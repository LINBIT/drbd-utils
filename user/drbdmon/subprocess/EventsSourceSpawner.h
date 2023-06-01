#ifndef EVENTSSOURCESPAWNER_H
#define	EVENTSSOURCESPAWNER_H

#include <new>
#include <stdexcept>

#include <MessageLog.h>
#include <exceptions.h>

extern "C"
{
    #include <sys/types.h>
}

class EventsSourceSpawner
{
  public:
    static const char* EVENTS_PROGRAM;
    static const char* EVENTS_PROGRAM_ARGS[];

    EventsSourceSpawner(MessageLog& logRef);
    virtual ~EventsSourceSpawner();

    EventsSourceSpawner(const EventsSourceSpawner& orig) = delete;
    EventsSourceSpawner& operator=(const EventsSourceSpawner& orig) = delete;
    EventsSourceSpawner(EventsSourceSpawner&& orig) = default;
    EventsSourceSpawner& operator=(EventsSourceSpawner&& orig) = default;

    virtual pid_t get_process_id();
    virtual int get_events_out_fd();
    virtual int get_events_err_fd();

    // @throws std::bad_alloc, EventSourceException
    virtual void spawn_source();

    // @throws EventsSourceException
    virtual void cleanup_child_processes();
  private:
    MessageLog& log;

    void close_pipe();

    void set_nonblocking(int fd);

    // @throws EventsSourceException
    void checked_int_rc(int rc) const;

    // @throws std::bad_alloc
    char** init_spawn_args() const;
    void destroy_spawn_args(char** spawn_args) const noexcept;

    // @throws EventsSourceException
    void cleanup_child_processes_impl();

    void terminate_child_process() noexcept;

    pid_t spawned_pid {-1};
    int   out_pipe_fd[2]  {-1, -1};
    int   err_pipe_fd[2]  {-1, -1};
};

#endif	/* EVENTSSOURCESPAWNER_H */
