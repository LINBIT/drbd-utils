#ifndef MESSAGELOG_H
#define	MESSAGELOG_H

#include <cstdint>
#include <new>
#include <memory>
#include <stdexcept>

// https://github.com/raltnoeder/cppdsaext
#include <dsaext.h>

extern "C"
{
    #include <sys/time.h>
    #include <time.h>
}

class MessageLog
{
  public:
    static const char* MESSAGES_HEADER;

    static const char* F_ALERT_MARK;
    static const char* F_WARN_MARK;
    static const char* F_INFO_MARK;
    static const char* F_ALERT;
    static const char* F_WARN;
    static const char* F_INFO;
    static const char* F_RESET;

    static const size_t DATE_BUFFER_SIZE;
    static const char*  DATE_FORMAT;
    static const size_t DATE_LENGTH;

    static const std::string CAPACITY_ERROR;

    enum class log_level : uint32_t
    {
        INFO,
        WARN,
        ALERT
    };

    typedef struct entry_s
    {
        log_level    level;
        std::string* message;
    }
    entry;

    class EntriesIterator : dsaext::QIterator<entry>
    {
      public:
        EntriesIterator(const MessageLog& log_ref);
        EntriesIterator(const EntriesIterator& orig) = delete;
        EntriesIterator& operator=(const EntriesIterator& orig) = delete;
        EntriesIterator(EntriesIterator&& orig) = default;
        EntriesIterator& operator=(EntriesIterator&& orig) = default;
        virtual ~EntriesIterator() noexcept override;

        virtual entry* next() override;
        virtual size_t get_size() const override;
        virtual bool has_next() const override;

      private:
        const MessageLog& log_obj;
        size_t iter_index {0};
        bool circular_mode {false};
    };

    // @throws std::bad_alloc, std::out_of_range
    explicit MessageLog(size_t entries);
    MessageLog(const MessageLog& orig) = delete;
    MessageLog& operator=(const MessageLog& orig) = delete;
    MessageLog(MessageLog&& orig) = default;
    MessageLog& operator=(MessageLog&& orig) = delete;
    virtual ~MessageLog() noexcept;

    // @throws std::bad_alloc
    virtual bool has_entries() const;
    virtual void add_entry(log_level level, const std::string& message);
    virtual void add_entry(log_level level, const char* message);
    virtual void clear();
    virtual void display_messages(std::ostream& out) const;

  private:
    size_t index    {0};
    bool   filled   {false};
    size_t capacity {0};

    struct timeval utc_time;
    struct tm time_fields;

    std::unique_ptr<entry*[]> log_entries;
    std::unique_ptr<char[]> date_buffer;

    void clear_impl() noexcept;
    bool format_date() noexcept;
};

#endif	/* MESSAGELOG_H */
