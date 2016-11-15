#ifndef EVENTSSOURCESPAWNER_H
#define	EVENTSSOURCESPAWNER_H

#include <new>
#include <stdexcept>

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

    static const int PIPE_READ_SIDE;
    static const int PIPE_WRITE_SIDE;

    EventsSourceSpawner();
    virtual ~EventsSourceSpawner();

    EventsSourceSpawner(const EventsSourceSpawner& orig) = delete;
    EventsSourceSpawner& operator=(const EventsSourceSpawner& orig) = delete;
    EventsSourceSpawner(EventsSourceSpawner&& orig) = default;
    EventsSourceSpawner& operator=(EventsSourceSpawner&& orig) = default;

    virtual pid_t get_process_id();
    virtual int get_events_source_fd();

    // @throws std::bad_alloc, EventSourceException
    virtual int spawn_source();

    // @throws EventsSourceException
    virtual void cleanup_child_processes();
  private:
    void close_pipe();

    // @throws EventsSourceException
    void checked_int_rc(int rc) const;

    // @throws std::bad_alloc
    char** init_spawn_args() const;
    void destroy_spawn_args(char** spawn_args) const noexcept;

    // @throws EventsSourceException
    void cleanup_child_processes_impl();

    void terminate_child_process() noexcept;

    pid_t spawned_pid {-1};
    int   pipe_fd[2]  {-1, -1};
};

class EventsSourceException : public EventException
{
  public:
    EventsSourceException() = default;
    virtual ~EventsSourceException() noexcept = default;

    EventsSourceException(const EventsSourceException& orig) = delete;
    EventsSourceException& operator=(const EventsSourceException& orig) = delete;
    EventsSourceException(EventsSourceException&& orig) = default;
    EventsSourceException& operator=(EventsSourceException&& orig) = default;
};

#endif	/* EVENTSSOURCESPAWNER_H */
