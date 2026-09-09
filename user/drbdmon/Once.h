#ifndef ONCE_H
#define ONCE_H

#include <atomic>
#include <mutex>
#include <functional>

class Once
{
  private:
    std::atomic<bool>   expended    {false};
    std::mutex          exec_lock;

  public:
    virtual ~Once() noexcept;
    virtual void executeOnce(const std::function<void()> exec_func);
};

#endif // ONCE_H
