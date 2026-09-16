#ifndef DISPLAYIO_H
#define DISPLAYIO_H

#include <default_types.h>
#include <terminal/ScreenBuffer.h>
#include <string>
#include <cstring>
#include <memory>
#include <vector>

class DisplayIo
{
  public:
    static const uint32_t MAX_YIELD_LOOP;
    static const size_t   FORMAT_BUFFER_SIZE;
    static const size_t   OUTPUT_BUFFER_SIZE;
    static const size_t   MAX_OUTPUT_BUFFER_SIZE;

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

    // Transfers everything that was collected by the write_* methods to the output_fd file descriptor.
    // Output is collected until this method is called, so that a complete display update is transferred
    // to the terminal by a single write() system call, which avoids the terminal rendering
    // partially updated screens.
    virtual void flush() const noexcept;

    // Routes the output of the write_* methods through a ScreenBuffer, so that flush() only updates
    // those parts of the terminal that actually changed. Must be disabled while sequences that the
    // ScreenBuffer does not interpret are written, which is the case while the terminal is set up
    // and while it is reset.
    // @throws std::bad_alloc
    virtual void enable_screen_buffer(const uint16_t cols, const uint16_t rows);
    virtual void disable_screen_buffer() noexcept;

    // @throws std::bad_alloc
    virtual void set_screen_dimensions(const uint16_t cols, const uint16_t rows);

    // Forces the next flush() call to update the entire terminal
    virtual void invalidate_screen() noexcept;

    // Marks the beginning and the end of the generation of a display update.
    // flush() does nothing while a display update is being generated, so that a flush() call from
    // within the code that generates the update cannot transfer a partially generated screen
    // to the terminal.
    virtual void begin_frame() noexcept;
    virtual void end_frame() noexcept;

  private:
    const int output_fd;

    char* format_buffer;
    std::unique_ptr<char[]> format_buffer_mgr;

    // Collects the output of the write_* methods until the next flush() call.
    // Mutable, because the write_* and flush methods are const, as they do not modify the state of the
    // display, they merely collect and transfer output.
    mutable std::vector<char> output_buffer;

    // If present, the output of the write_* methods is painted into this buffer instead of being
    // collected for a verbatim transfer to the terminal
    std::unique_ptr<ScreenBuffer> screen_buffer_mgr;

    // Nesting level of begin_frame()/end_frame(), nonzero while a display update is being generated.
    // Mutable for the same reason as output_buffer.
    mutable uint32_t frame_level {0};

    // 20 ms delay
    struct timespec write_retry_delay {0, 20000000};

    void write_fd(const char* const buffer, const size_t write_length) const noexcept;
};

#endif /* DISPLAYIO_H */
