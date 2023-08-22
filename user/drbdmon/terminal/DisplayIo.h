#ifndef DISPLAYIO_H
#define DISPLAYIO_H

#include <default_types.h>
#include <string>
#include <cstring>
#include <memory>

class DisplayIo
{
  public:
    static const uint32_t MAX_YIELD_LOOP;
    static const uint16_t OUTPUT_BUFFER_SIZE;

    DisplayIo(const int init_output_fd);
    virtual ~DisplayIo() noexcept;
    DisplayIo(const DisplayIo& other)                       = delete;
    DisplayIo(DisplayIo&& other)                            = delete;
    virtual DisplayIo& operator=(const DisplayIo& other)    = delete;
    virtual DisplayIo& operator=(DisplayIo&& orig)          = delete;

    virtual void cursor_xy(const uint16_t column, const uint16_t row) const;

    virtual void write_char(const char ch) const noexcept;
    virtual void write_text(const char* const text) const noexcept;
    virtual void write_fmt(const char* const format, ...) const noexcept;
    virtual void write_string_field(const std::string& text, const size_t field_width, const bool fill) const noexcept;
    virtual void write_fill_char(const char fill_char, const size_t fill_length) const noexcept;
    virtual void write_fill_seq(const std::string& seq, const size_t seq_count) const noexcept;

    virtual void write_buffer(const char* buffer, const size_t write_length) const noexcept;

  private:
    const int output_fd;

    char* output_buffer;
    std::unique_ptr<char[]> output_buffer_mgr;

    // 20 ms delay
    struct timespec write_retry_delay {0, 20000000};
};

#endif /* DISPLAYIO_H */
