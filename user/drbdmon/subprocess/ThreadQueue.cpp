#include <subprocess/ThreadQueue.h>

#include <system_error>

ThreadQueue::ThreadQueue()
{
}

ThreadQueue::~ThreadQueue() noexcept
{
    Node* q_node = tail;
    while (q_node != nullptr)
    {
        Node* const prev = q_node->prev;
        if (q_node->worker_thread.joinable())
        {
            try
            {
                q_node->worker_thread.join();
            }
            catch (std::system_error&)
            {
                // Not supposed to happen, but there is also nothing that
                // could be done about it, so handling is a no-op
            }
        }
        delete q_node;
        q_node = prev;
    }
}

// The caller must enforce constraints to prevent a numerical overflow of the size variable
void ThreadQueue::link_node(Node* const q_node) noexcept
{
    if (head == nullptr)
    {
        head = q_node;
        tail = q_node;
    }
    else
    {
        head->prev = q_node;
        q_node->next = head;
        head = q_node;
    }

    ++size;
}

void ThreadQueue::unlink_node(Node* const q_node) noexcept
{
    if (q_node->prev == nullptr)
    {
        head = q_node->next;
    }
    else
    {
        q_node->prev->next = q_node->next;
    }

    if (q_node->next == nullptr)
    {
        tail = q_node->prev;
    }
    else
    {
        q_node->next->prev = q_node->prev;
    }

    q_node->prev = nullptr;
    q_node->next = nullptr;

    --size;
}

ThreadQueue::Node::Node()
{
}

ThreadQueue::Node::~Node() noexcept
{
}
