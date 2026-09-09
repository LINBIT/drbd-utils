#ifndef THREADQUEUE_H
#define THREADQUEUE_H

#include <thread>

class ThreadQueue
{
  public:
    class Node
    {
      public:
        std::thread     worker_thread;

        Node*   prev    {nullptr};
        Node*   next    {nullptr};

        Node();
        virtual ~Node() noexcept;
    };

    Node*   head    {nullptr};
    Node*   tail    {nullptr};

    size_t  size    {0};

    ThreadQueue();
    virtual ~ThreadQueue() noexcept;

    virtual void unlink_node(Node* const q_node) noexcept;
    // The caller must enforce constraints to prevent a numerical overflow of the size variable
    virtual void link_node(Node* const q_node) noexcept;
};

#endif //THREADQUEUE_H
