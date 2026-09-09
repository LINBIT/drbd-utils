#include <comparators.h>
#include <subprocess/SubProcessQueue.h>
#include <bounds.h>
#include <exception>
#include <system_error>

extern "C"
{
    #include <signal.h>
}

const uint64_t  SubProcessQueue::TASKQ_NONE         = UINT64_MAX;
const size_t    SubProcessQueue::MAX_ENTRY_COUNT    = 4000;

const size_t    SubProcessQueue::MIN_ACTIVE_COUNT_RANGE = 1;
const size_t    SubProcessQueue::DFLT_ACTIVE_COUNT      = 8;
const size_t    SubProcessQueue::MAX_ACTIVE_COUNT_RANGE = 40;

SubProcessQueue::SubProcessQueue():
    SubProcessQueue::SubProcessQueue(DFLT_ACTIVE_COUNT)
{
}

SubProcessQueue::SubProcessQueue(const size_t concurrency)
{
    sys_api = system_api::create_system_api();
    map = std::unique_ptr<EntryMapType>(new EntryMapType(&comparators::compare<uint64_t>));

    const size_t safe_concurrency = bounds(MIN_ACTIVE_COUNT_RANGE, concurrency, MAX_ACTIVE_COUNT_RANGE);
    crt_pool_size = safe_concurrency;
    tgt_pool_size = safe_concurrency;
    max_sub_proc = safe_concurrency;

    for (size_t ctr = 0; ctr < crt_pool_size; ++ctr)
    {
        ThreadQueue::Node* const q_node = new ThreadQueue::Node();
        inactive_threads.link_node(q_node);
    }
}

SubProcessQueue::~SubProcessQueue() noexcept
{
    {
        std::unique_lock<std::mutex> lock(queue_lock);

        shutdown = true;

        // Force termination of all active external tasks
        SubProcessQueue::Iterator task_iter = active_queue_iterator();
        while (task_iter.has_next())
        {
            SubProcessQueue::Entry* const task_entry = task_iter.next();
            if (task_entry->process_mgr != nullptr)
            {
                task_entry->process_mgr->terminate(true);
            }
        }
    }

    for (ThreadQueue::Node* q_node = active_threads.head; q_node != nullptr; q_node = q_node->next)
    {
        if (q_node->worker_thread.joinable())
        {
            q_node->worker_thread.join();
        }
    }

    EntryMapType::ValuesIterator iter(*map);
    while (iter.has_next())
    {
        Entry* const map_entry = iter.next();
        delete map_entry;
    }
    map->clear();
}

std::mutex& SubProcessQueue::get_queue_lock() noexcept
{
    return queue_lock;
}

// @throws std::bad_alloc, SubProcess::Exception, dsaext::DuplicateInsertException
// The DuplicateInsertException is theoretical, because entry ids are unique
uint64_t SubProcessQueue::add_entry(std::unique_ptr<CmdLine>& command_mgr, const bool activate)
{
    Queue& dst_queue = activate ? ready_queue : wait_queue;

    std::unique_lock<std::mutex> lock(queue_lock);

    const uint64_t entry_id = next_id;
    if (entry_count < MAX_ENTRY_COUNT)
    {
        std::unique_ptr<Entry> queue_entry_mgr(new Entry(entry_id));
        Entry& queue_entry = *queue_entry_mgr;

        map->insert(&(queue_entry.id), &queue_entry);
        dst_queue.append_entry(&queue_entry);

        // Take ownership of the command line object
        queue_entry.command_mgr = std::move(command_mgr);

        queue_entry_mgr.release();

        ++next_id;
        ++entry_count;

        if (observer != nullptr)
        {
            observer->notify_queue_changed();
        }
    }
    else
    {
        throw QueueCapacityException();
    }

    if (activate)
    {
        try
        {
            schedule_threads();
        }
        catch (std::system_error&)
        {
            throw SubProcess::Exception("Thread creation failed. Check operating system limits.");
        }
    }
    return entry_id;
}

// An entry can never be removed from the active queue, because another thread is still accessing its data structures
bool SubProcessQueue::remove_entry(const uint64_t entry_id)
{
    std::unique_lock<std::mutex> lock(queue_lock);

    bool is_removed = false;
    Entry* const queue_entry = map->get(&entry_id);
    if (queue_entry != nullptr)
    {
        is_removed = remove_entry_impl(queue_entry);
    }
    return is_removed;
}

bool SubProcessQueue::remove_entry(Entry* const queue_entry)
{
    std::unique_lock<std::mutex> lock(queue_lock);

    return remove_entry_impl(queue_entry);
}

// Caller must hold queue_lock
bool SubProcessQueue::remove_entry_impl(Entry* const queue_entry)
{
    bool is_removed = false;
    if (queue_entry->owning_queue != &active_queue)
    {
        map->remove(&(queue_entry->id));
        queue_entry->owning_queue->remove_entry(queue_entry);
        delete queue_entry;
        --entry_count;
        is_removed = true;
        if (observer != nullptr)
        {
            observer->notify_queue_changed();
        }
    }
    return is_removed;
}

// Caller must hold queue_lock
SubProcessQueue::Entry* SubProcessQueue::get_entry(const uint64_t entry_id)
{
    return map->get(&entry_id);
}

// @throws SubProcess::Exception
bool SubProcessQueue::activate_entry(const uint64_t entry_id)
{
    bool is_activated = false;

    std::unique_lock<std::mutex> lock(queue_lock);

    Entry* const queue_entry = map->get(&entry_id);
    if (queue_entry != nullptr)
    {
        if (queue_entry->owning_queue == &wait_queue)
        {
            is_activated = move_entry(queue_entry, ready_queue);
            try
            {
                schedule_threads();
            }
            catch (std::system_error&)
            {
                throw SubProcess::Exception("Thread creation failed. Check operating system limits.");
            }

            if (is_activated && observer != nullptr)
            {
                observer->notify_queue_changed();
            }
        }
    }
    return is_activated;
}

bool SubProcessQueue::inactivate_entry(const uint64_t entry_id)
{
    bool is_inactivated = false;

    std::unique_lock<std::mutex> lock(queue_lock);

    Entry* const queue_entry = map->get(&entry_id);
    if (queue_entry != nullptr)
    {
        if (queue_entry->owning_queue == &ready_queue)
        {
            move_entry(queue_entry, wait_queue);
            is_inactivated = true;

            if (is_inactivated && observer != nullptr)
            {
                observer->notify_queue_changed();
            }
        }
    }
    return is_inactivated;
}

void SubProcessQueue::terminate_task(const uint64_t entry_id, const bool force)
{
    std::unique_lock<std::mutex> lock(queue_lock);

    Entry* const queue_entry = map->get(&entry_id);
    if (queue_entry != nullptr)
    {
        if (queue_entry->owning_queue == &active_queue)
        {
            if (queue_entry->process_mgr != nullptr)
            {
                queue_entry->process_mgr->terminate(force);
            }
        }
    }
}

void SubProcessQueue::set_discard_finished_tasks(const bool discard_flag)
{
    std::unique_lock<std::mutex> lock(queue_lock);

    discard_finished_tasks = discard_flag;
}

void SubProcessQueue::set_discard_succeeded_tasks(const bool discard_flag)
{
    std::unique_lock<std::mutex> lock(queue_lock);

    discard_succeeded_tasks = discard_flag;
}

// Caller must hold queue_lock
uint64_t SubProcessQueue::get_active_queue_selected_id()
{
    return active_queue.selected_id;
}

// Caller must hold queue_lock
uint64_t SubProcessQueue::get_pending_queue_selected_id()
{
    return ready_queue.selected_id;
}

// Caller must hold queue_lock
uint64_t SubProcessQueue::get_suspended_queue_selected_id()
{
    return wait_queue.selected_id;
}

// Caller must hold queue_lock
uint64_t SubProcessQueue::get_finished_queue_selected_id()
{
    return ended_queue.selected_id;
}

// Caller must hold queue_lock
void SubProcessQueue::set_active_queue_selected_id(const uint64_t id)
{
    active_queue.selected_id = id;
}

// Caller must hold queue_lock
void SubProcessQueue::set_pending_queue_selected_id(const uint64_t id)
{
    ready_queue.selected_id = id;
}

// Caller must hold queue_lock
void SubProcessQueue::set_suspended_queue_selected_id(const uint64_t id)
{
    wait_queue.selected_id = id;
}

// Caller must hold queue_lock
void SubProcessQueue::set_finished_queue_selected_id(const uint64_t id)
{
    ended_queue.selected_id = id;
}

SubProcessQueue::Iterator SubProcessQueue::active_queue_iterator()
{
    return Iterator(active_queue);
}

SubProcessQueue::Iterator SubProcessQueue::active_queue_offset_iterator(Entry* const start_entry)
{
    return Iterator(active_queue, start_entry);
}

SubProcessQueue::Iterator SubProcessQueue::pending_queue_iterator()
{
    return Iterator(ready_queue);
}

SubProcessQueue::Iterator SubProcessQueue::pending_queue_offset_iterator(Entry* const start_entry)
{
    return Iterator(ready_queue, start_entry);
}

SubProcessQueue::Iterator SubProcessQueue::suspended_queue_iterator()
{
    return Iterator(wait_queue);
}

SubProcessQueue::Iterator SubProcessQueue::suspended_queue_offset_iterator(Entry* const start_entry)
{
    return Iterator(wait_queue, start_entry);
}

SubProcessQueue::Iterator SubProcessQueue::finished_queue_iterator()
{
    return Iterator(ended_queue);
}

SubProcessQueue::Iterator SubProcessQueue::finished_queue_offset_iterator(Entry* const start_entry)
{
    return Iterator(ended_queue, start_entry);
}

void SubProcessQueue::set_observer(SubProcessObserver* const new_observer)
{
    std::unique_lock<std::mutex> lock(queue_lock);

    observer = new_observer;
}

// @throws std::bad_alloc, SubProcess::Exception
void SubProcessQueue::change_sub_proc_concurrency(const size_t concurrency)
{
    std::unique_lock<std::mutex> lock(queue_lock);

    const size_t safe_concurrency = bounds(MIN_ACTIVE_COUNT_RANGE, concurrency, MAX_ACTIVE_COUNT_RANGE);
    if (safe_concurrency != crt_pool_size)
    {
        change_pool = true;
        tgt_pool_size = safe_concurrency;

        if (tgt_pool_size > crt_pool_size)
        {
            increase_pool_size_impl(false);
        }
        else
        {
            max_sub_proc = tgt_pool_size;

            decrease_pool_size_impl();
        }
    }
}

size_t SubProcessQueue::get_current_sub_proc_concurrency()
{
    size_t concurrency = 0;
    {
        std::unique_lock<std::mutex> lock(queue_lock);
        concurrency = max_sub_proc;
    }
    return concurrency;
}

size_t SubProcessQueue::get_target_sub_proc_concurrency()
{
    size_t concurrency = 0;
    {
        std::unique_lock<std::mutex> lock(queue_lock);
        concurrency = tgt_pool_size;
    }
    return concurrency;
}

// Caller must hold queue_lock
void SubProcessQueue::decrease_pool_size_impl()
{
    if (active_count <= tgt_pool_size)
    {
        if (crt_pool_size > tgt_pool_size)
        {
            const size_t discard = crt_pool_size - tgt_pool_size;
            size_t ctr = 0;
            ThreadQueue::Node* q_node = inactive_threads.head;
            while (q_node != nullptr && ctr < discard)
            {
                ThreadQueue::Node* const next = q_node->next;
                if (q_node->worker_thread.joinable())
                {
                    try
                    {
                        q_node->worker_thread.join();
                    }
                    catch (std::system_error&)
                    {
                        // Cannot fix it anyway, handling is a no-op
                    }
                }
                inactive_threads.unlink_node(q_node);
                --crt_pool_size;
                delete q_node;
                q_node = next;
                ++ctr;
            }
        }

        if (crt_pool_size == tgt_pool_size)
        {
            change_pool = false;
        }
    }
}

// Caller must hold queue_lock
void SubProcessQueue::increase_pool_size_impl(const bool defer_exc)
{
    std::unique_ptr<ThreadQueue::Node> q_node_mgr;
    while (tgt_pool_size > crt_pool_size)
    {
        try
        {
            q_node_mgr = std::unique_ptr<ThreadQueue::Node>(new ThreadQueue::Node());
        }
        catch (std::bad_alloc&)
        {
            if (defer_exc)
            {
                observer->notify_out_of_memory();
                break;
            }
            else
            {
                throw;
            }
        }
        if (q_node_mgr != nullptr)
        {
            inactive_threads.link_node(q_node_mgr.get());
            q_node_mgr.release();
            ++crt_pool_size;
            max_sub_proc = crt_pool_size;
        }
    }

    try
    {
        schedule_threads();
        change_pool = false;
    }
    catch (std::system_error&)
    {
        if (!defer_exc)
        {
            throw SubProcess::Exception("Thread creation failed. Check operating system limits.");
        }
        // else the exception is suppressed, retrying again later when called from a thread.
    }
}

// Caller must hold queue_lock
bool SubProcessQueue::move_entry(Entry* const queue_entry, Queue& dst_queue)
{
    bool outcome = false;
    Queue* const org_queue = queue_entry->owning_queue;
    if (org_queue != nullptr)
    {
        org_queue->remove_entry(queue_entry);
        dst_queue.append_entry(queue_entry);
        outcome = true;
    }
    return outcome;
}

// Caller must hold queue_lock
// @throws std::system_error
void SubProcessQueue::schedule_threads()
{
    if (!shutdown)
    {
        const size_t queue_count = std::min(ready_queue.size + active_queue.size, max_sub_proc);
        while (queue_count > active_count)
        {
            ThreadQueue::Node *q_node = inactive_threads.head;
            if (q_node != nullptr)
            {
                // This is required to clean up a thread object that was previously used for another thread
                if (q_node->worker_thread.joinable())
                {
                    q_node->worker_thread.join();
                }
            }
            else
            {
                // Safeguard; can only happen if the queue sizes are incorrect
                break;
            }

            sys_api->pre_thread_invocation();
            q_node->worker_thread = std::thread(&SubProcessQueue::invoke_thread, this, q_node);
            sys_api->post_thread_invocation();

            inactive_threads.unlink_node(q_node);
            active_threads.link_node(q_node);
            ++active_count;
        }
    }
}

void SubProcessQueue::invoke_thread(ThreadQueue::Node* const q_node)
{
    std::unique_lock<std::mutex> lock(queue_lock);
    try
    {
        WorkerGuard guard(*this, q_node);
        while (!shutdown && active_count <= max_sub_proc && ready_queue.head != nullptr)
        {
            Entry* const selected_entry = ready_queue.head;
            move_entry(selected_entry, active_queue);

            if (observer != nullptr)
            {
                observer->notify_queue_changed();
            }

            std::exception_ptr stored_exc;

            try
            {
                selected_entry->process_mgr = sys_api->create_subprocess_handler(observer);
            }
            catch (std::bad_alloc& exc)
            {
                stored_exc = std::current_exception();
            }

            if (selected_entry->process_mgr != nullptr)
            {
                lock.unlock();

                try
                {
                    selected_entry->process_mgr->execute(*(selected_entry->command_mgr));
                }
                catch (std::bad_alloc& exc)
                {
                    stored_exc = std::current_exception();
                }
                catch (SubProcess::Exception& exc)
                {
                    try
                    {
                        selected_entry->process_mgr->set_error_message(exc.get_error_message());
                    }
                    catch (std::bad_alloc&)
                    {
                        stored_exc = std::current_exception();
                    }
                }

                lock.lock();
            }

            move_entry(selected_entry, ended_queue);

            bool discard_flag = false;
            if (selected_entry->process_mgr)
            {
                discard_flag = discard_finished_tasks;
                if (!discard_finished_tasks && discard_succeeded_tasks)
                {
                    const int exit_status = selected_entry->process_mgr->get_exit_status();
                    discard_flag = exit_status == 0;
                }
            }

            if (discard_flag)
            {
                remove_entry_impl(selected_entry);
            }

            if (observer != nullptr)
            {
                observer->notify_queue_changed();
            }

            if (stored_exc != nullptr)
            {
                std::rethrow_exception(stored_exc);
            }

            if (change_pool)
            {
                if (tgt_pool_size < crt_pool_size)
                {
                    decrease_pool_size_impl();
                }
                else
                if (tgt_pool_size > crt_pool_size)
                {
                    // std::bad_alloc reported via SubProcessObserver
                    increase_pool_size_impl(true);
                }
                else
                {
                    change_pool = false;
                }
            }
        }
    }
    catch (std::bad_alloc&)
    {
        if (observer != nullptr)
        {
            observer->notify_out_of_memory();
        }
    }
}

SubProcessQueue::Entry::Entry(const uint64_t entry_id)
{
    id = entry_id;
}

SubProcessQueue::Entry::~Entry() noexcept
{
}

uint64_t SubProcessQueue::Entry::get_id() const noexcept
{
    return id;
}

const uint64_t& SubProcessQueue::Entry::get_id_ref() const noexcept
{
    return id;
}

SubProcessQueue::Entry* SubProcessQueue::Entry::get_previous_entry() noexcept
{
    return prev_entry;
}

SubProcessQueue::Entry* SubProcessQueue::Entry::get_next_entry() noexcept
{
    return next_entry;
}

SubProcessQueue::entry_state_type SubProcessQueue::get_entry_state(const uint64_t entry_id)
{
    entry_state_type state = entry_state_type::INVALID_ID;
    Entry* const queue_entry = map->get(&entry_id);
    if (queue_entry != nullptr)
    {
        state = get_entry_state(queue_entry);
    }
    return state;
}

SubProcessQueue::entry_state_type SubProcessQueue::get_entry_state(const Entry* const queue_entry)
{
    entry_state_type state = entry_state_type::ENDED;
    if (queue_entry->owning_queue == &wait_queue)
    {
        state = entry_state_type::WAITING;
    }
    else
    if (queue_entry->owning_queue == &ready_queue)
    {
        state = entry_state_type::READY;
    }
    else
    if (queue_entry->owning_queue == &active_queue)
    {
        state = entry_state_type::ACTIVE;
    }
    return state;
}

const CmdLine* SubProcessQueue::Entry::get_command() const
{
    return command_mgr.get();
}

SubProcess* SubProcessQueue::Entry::get_process()
{
    return process_mgr.get();
}

SubProcessQueue::Queue::Queue()
{
}

SubProcessQueue::Queue::~Queue() noexcept
{
}

// @throws dsaext::DuplicateInsertException, std::bad_alloc
void SubProcessQueue::Queue::prepend_entry(Entry* const queue_entry)
{
    queue_entry->owning_queue = this;
    if (tail == nullptr)
    {
        tail = queue_entry;
    }
    else
    {
        queue_entry->next_entry = head;
        head->prev_entry = queue_entry;
    }
    head = queue_entry;
    ++size;
}

// @throws dsaext::DuplicateInsertException, std::bad_alloc
// The DuplicateInsertException is theoretical, because entry ids are unique
void SubProcessQueue::Queue::append_entry(Entry* const queue_entry)
{
    queue_entry->owning_queue = this;
    if (head == nullptr)
    {
        head = queue_entry;
    }
    else
    {
        queue_entry->prev_entry = tail;
        tail->next_entry = queue_entry;
    }
    tail = queue_entry;
    ++size;
}

void SubProcessQueue::Queue::remove_entry(Entry* const queue_entry)
{
    Queue* const owning_queue = queue_entry->owning_queue;
    if (owning_queue != nullptr)
    {
        if (owning_queue->head == queue_entry)
        {
            owning_queue->head = queue_entry->next_entry;
        }
        else
        {
            queue_entry->prev_entry->next_entry = queue_entry->next_entry;
        }
        if (owning_queue->tail == queue_entry)
        {
            owning_queue->tail = queue_entry->prev_entry;
        }
        else
        {
            queue_entry->next_entry->prev_entry = queue_entry->prev_entry;
        }
        --owning_queue->size;

        if (owning_queue->selected_id == queue_entry->id)
        {
            // Update selected entry ID
            if (queue_entry->next_entry != nullptr)
            {
                owning_queue->selected_id = queue_entry->next_entry->id;
            }
            else
            if (queue_entry->prev_entry != nullptr)
            {
                owning_queue->selected_id = queue_entry->prev_entry->id;
            }
            else
            {
                owning_queue->selected_id = TASKQ_NONE;
            }
        }
    }
    queue_entry->next_entry = nullptr;
    queue_entry->prev_entry = nullptr;
    queue_entry->owning_queue = nullptr;
}

SubProcessQueue::Entry* SubProcessQueue::Queue::pop_entry()
{
    Entry* const queue_entry = head;
    if (queue_entry != nullptr)
    {
        head = queue_entry->next_entry;
        if (head != nullptr)
        {
            head->prev_entry = nullptr;
        }
        queue_entry->next_entry = nullptr;
        queue_entry->prev_entry = nullptr;
        queue_entry->owning_queue = nullptr;
        --size;
    }
    return queue_entry;
}

SubProcessQueue::Iterator SubProcessQueue::Queue::iterator()
{
    return SubProcessQueue::Iterator(*this);
}

SubProcessQueue::Iterator SubProcessQueue::Queue::iterator(SubProcessQueue::Entry* const start_entry)
{
    return SubProcessQueue::Iterator(*this, start_entry);
}

SubProcessQueue::Iterator::Iterator(SubProcessQueue::Queue& iter_queue_ref):
    iter_queue(iter_queue_ref)
{
    next_entry = iter_queue.head;
}

SubProcessQueue::Iterator::Iterator(
    SubProcessQueue::Queue&         iter_queue_ref,
    SubProcessQueue::Entry* const   start_entry
):
    iter_queue(iter_queue_ref)
{
    next_entry = start_entry;
}

SubProcessQueue::Iterator::~Iterator() noexcept
{
}

SubProcessQueue::Entry* SubProcessQueue::Iterator::next()
{
    Entry* const cur_entry = next_entry;
    if (cur_entry != nullptr)
    {
        next_entry = cur_entry->next_entry;
    }
    return cur_entry;
}

bool SubProcessQueue::Iterator::has_next() const
{
    return next_entry != nullptr;
}

size_t SubProcessQueue::Iterator::get_size() const
{
    return iter_queue.size;
}

SubProcessQueue::QueueCapacityException::QueueCapacityException()
{
}

SubProcessQueue::QueueCapacityException::~QueueCapacityException() noexcept
{
}

SubProcessQueue::WorkerGuard::WorkerGuard(SubProcessQueue& instance, ThreadQueue::Node* const q_node_ptr):
    container(instance),
    q_node(q_node_ptr)
{
}

SubProcessQueue::WorkerGuard::~WorkerGuard() noexcept
{
    --container.active_count;
    container.active_threads.unlink_node(q_node);
    container.inactive_threads.link_node(q_node);
}
