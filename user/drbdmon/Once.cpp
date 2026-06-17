#include <Once.h>

Once::~Once() noexcept
{
}

void Once::executeOnce(const std::function<void()> exec_func)
{
    if (!expended.load(std::memory_order_acquire))
    {
        std::lock_guard<std::mutex> lock(exec_lock);
        if (!expended.load(std::memory_order_relaxed))
        {
            exec_func();
            expended.store(true, std::memory_order_release);
        }
    }
}
