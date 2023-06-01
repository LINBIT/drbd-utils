#ifndef SUBPROCESSQUEUE_H
#define SUBPROCESSQUEUE_H

#include <memory>
#include <mutex>
#include <thread>
#include <stdexcept>

#include <QTree.h>
#include <dsaext.h>
#include <subprocess/CmdLine.h>
#include <subprocess/SubProcessObserver.h>
#include <subprocess/SubProcess.h>
#include <platform/SystemApi.h>

class SubProcessQueue
{
  private:
    class Queue;
  public:
    // ID to indicate that no entry is selected
    static const uint64_t   TASKQ_NONE;

    // Maximum aggregate number of entries
    static const size_t MAX_ENTRY_COUNT;

    // Maximum number of concurrently active sub-processes
    static const size_t MAX_ACTIVE_COUNT;

    enum class entry_state_type : uint8_t
    {
        WAITING     = 0,
        READY,
        ACTIVE,
        ENDED,
        INVALID_ID
    };

    class Entry
    {
        friend class SubProcessQueue;

      public:
        Entry(const uint64_t entry_id);
        virtual ~Entry() noexcept;

        virtual uint64_t get_id() const noexcept;
        virtual const uint64_t& get_id_ref() const noexcept;
        virtual const CmdLine* get_command() const;
        virtual SubProcess* get_process();
        virtual Entry* get_previous_entry() noexcept;
        virtual Entry* get_next_entry() noexcept;

      private:
        uint64_t                    id              {0};
        std::unique_ptr<CmdLine>    command_mgr;
        std::unique_ptr<SubProcess> process_mgr;

        Queue*                      owning_queue    {nullptr};
        Entry*                      next_entry      {nullptr};
        Entry*                      prev_entry      {nullptr};
    };

    class Iterator : public dsaext::QIterator<Entry>
    {
      public:
        Iterator(Queue& iter_queue_ref);
        Iterator(Queue& iter_queue_ref, Entry* const start_entry);
        virtual ~Iterator() noexcept;
        virtual Entry* next() override;
        virtual bool has_next() const override;
        virtual size_t get_size() const override;
      private:
        Queue& iter_queue;
        Entry* next_entry   {nullptr};
    };

    class QueueCapacityException : public SubProcess::Exception
    {
      public:
        QueueCapacityException();
        virtual ~QueueCapacityException() noexcept;
    };

    SubProcessQueue();
    virtual ~SubProcessQueue() noexcept;

    virtual std::mutex& get_queue_lock() noexcept;
    virtual uint64_t add_entry(std::unique_ptr<CmdLine>& command_mgr, const bool activate);
    virtual bool remove_entry(const uint64_t entry_id);
    virtual bool remove_entry(Entry* const queue_entry);
    virtual Entry* get_entry(const uint64_t entry_id);
    virtual entry_state_type get_entry_state(const uint64_t entry_id);
    virtual entry_state_type get_entry_state(const Entry* const queue_entry);
    virtual bool activate_entry(const uint64_t entry_id);
    virtual bool inactivate_entry(const uint64_t entry_id);

    virtual void terminate_task(const uint64_t entry_id, const bool force);

    virtual void set_discard_finished_tasks(const bool discard_flag);
    virtual void set_discard_succeeded_tasks(const bool discard_flag);

    virtual uint64_t get_active_queue_selected_id();
    virtual uint64_t get_pending_queue_selected_id();
    virtual uint64_t get_suspended_queue_selected_id();
    virtual uint64_t get_finished_queue_selected_id();

    virtual void set_active_queue_selected_id(const uint64_t id);
    virtual void set_pending_queue_selected_id(const uint64_t id);
    virtual void set_suspended_queue_selected_id(const uint64_t id);
    virtual void set_finished_queue_selected_id(const uint64_t id);

    virtual Iterator active_queue_iterator();
    virtual Iterator active_queue_offset_iterator(Entry* const start_entry);
    virtual Iterator pending_queue_iterator();
    virtual Iterator pending_queue_offset_iterator(Entry* const start_entry);
    virtual Iterator suspended_queue_iterator();
    virtual Iterator suspended_queue_offset_iterator(Entry* const start_entry);
    virtual Iterator finished_queue_iterator();
    virtual Iterator finished_queue_offset_iterator(Entry* const start_entry);

    virtual void set_observer(SubProcessObserver* const new_observer);

  private:
    using EntryMapType = QTree<uint64_t, Entry>;

    class Queue
    {
        friend class Iterator;

      public:
        size_t      size            {0};
        Entry*      head            {nullptr};
        Entry*      tail            {nullptr};
        uint64_t    selected_id     {UINT64_MAX};

        Queue();
        virtual ~Queue() noexcept;

        virtual void prepend_entry(Entry* const queue_entry);
        virtual void append_entry(Entry* const queue_entry);
        virtual void remove_entry(Entry* const queue_entry);
        virtual Entry* pop_entry();
        virtual Iterator iterator();
        virtual Iterator iterator(Entry* const start_entry);
    };

    class WorkerGuard
    {
      public:
        WorkerGuard(SubProcessQueue& instance, const size_t slot_idx);
        virtual ~WorkerGuard() noexcept;

      private:
        SubProcessQueue&    container;
        const size_t        worker_slot_idx;
    };

    // This lock must be held by any thread that works with any of the queues
    std::mutex  queue_lock;

    bool        shutdown        {false};

    uint64_t    next_id         {0};

    uint64_t    entry_count     {0};
    uint64_t    active_count    {0};

    Queue       wait_queue;
    Queue       ready_queue;
    Queue       active_queue;
    Queue       ended_queue;

    bool        discard_finished_tasks  {false};
    bool        discard_succeeded_tasks {false};

    std::unique_ptr<SystemApi>      sys_api;

    std::unique_ptr<EntryMapType>   map;

    std::unique_ptr<std::thread[]>  worker_thread_mgr;
    std::unique_ptr<size_t[]>       worker_slot_mgr;

    SubProcessObserver*             observer;

    bool remove_entry_impl(Entry* const queue_entry);
    bool move_entry(Entry* const queue_entry, Queue& dst_queue);
    void schedule_threads();
    void invoke_thread(const size_t slot_idx);
};

#endif /* SUBPROCESSQUEUE_H */
