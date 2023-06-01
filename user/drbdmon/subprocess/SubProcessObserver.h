#ifndef SUBPROCESSOBSERVER_H
#define SUBPROCESSOBSERVER_H

class SubProcessObserver
{
  public:
    virtual ~SubProcessObserver() noexcept
    {
    }

    virtual void notify_queue_changed() noexcept = 0;
    virtual void notify_out_of_memory() noexcept = 0;
};

#endif /* SUBPROCESSOBSERVER_H */
