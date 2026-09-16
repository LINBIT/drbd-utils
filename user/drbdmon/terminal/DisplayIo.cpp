#include <terminal/DisplayIo.h>
#include <terminal/AnsiControl.h>

#include <cstdarg>
#include <algorithm>
#include <new>

extern "C"
{
    #include <unistd.h>
    #include <errno.h>
}

const uint32_t DisplayIo::MAX_YIELD_LOOP           = 10;
const size_t   DisplayIo::FORMAT_BUFFER_SIZE       = 1024;
const size_t   DisplayIo::OUTPUT_BUFFER_SIZE       = 65536;
// If a single display update should ever exceed this size, it is split into multiple write() calls
// instead of growing the output buffer without any bounds
const size_t   DisplayIo::MAX_OUTPUT_BUFFER_SIZE   = 4194304;

DisplayIo::DisplayIo(const int init_output_fd):
    output_fd(init_output_fd)
{
    format_buffer_mgr = std::unique_ptr<char[]>(new char[FORMAT_BUFFER_SIZE]);
    format_buffer = format_buffer_mgr.get();
    output_buffer.reserve(OUTPUT_BUFFER_SIZE);
}

DisplayIo::~DisplayIo() noexcept
{
}

void DisplayIo::cursor_xy(const uint16_t column, const uint16_t row) const
{
    write_fmt(AnsiControl::ANSI_FMT_CURSOR_POS.c_str(),
              static_cast<unsigned int> (row), static_cast<unsigned int> (column));
}

/**
 * Collects data for output to the output_fd file descriptor
 *
 * The data is not written to the file descriptor until the next flush() call.
 *
 * @param buffer The data to collect for output
 * @param write_length Length of the data in the (possibly larger) buffer
 */
void DisplayIo::write_buffer(const char* const buffer, const size_t write_length) const noexcept
{
    if (write_length >= 1)
    {
        ScreenBuffer* const screen_buffer = screen_buffer_mgr.get();
        if (screen_buffer != nullptr)
        {
            screen_buffer->apply(buffer, write_length);
            return;
        }

        try
        {
            output_buffer.insert(output_buffer.end(), buffer, buffer + write_length);
        }
        catch (std::bad_alloc&)
        {
            // Out of memory, write whatever had been collected so far, then write the current data
            // directly, so that the display keeps working, although it may flicker
            write_fd(output_buffer.data(), output_buffer.size());
            output_buffer.clear();
            write_fd(buffer, write_length);
            return;
        }

        if (output_buffer.size() >= MAX_OUTPUT_BUFFER_SIZE)
        {
            // Guard against unbounded growth of the output buffer
            write_fd(output_buffer.data(), output_buffer.size());
            output_buffer.clear();
        }
    }
}

/**
 * Writes all collected data to the output_fd file descriptor
 */
void DisplayIo::flush() const noexcept
{
    if (frame_level >= 1)
    {
        // A display update is still being generated, transferring it now would show
        // a partially updated screen
        return;
    }

    ScreenBuffer* const screen_buffer = screen_buffer_mgr.get();
    if (screen_buffer != nullptr)
    {
        try
        {
            screen_buffer->render(output_buffer);
        }
        catch (std::bad_alloc&)
        {
            // Out of memory, the update sequence is incomplete. Discard it and update the entire
            // terminal on the next flush() call.
            output_buffer.clear();
            screen_buffer->invalidate();
        }
    }
    if (!output_buffer.empty())
    {
        write_fd(output_buffer.data(), output_buffer.size());
        output_buffer.clear();
    }
}

void DisplayIo::enable_screen_buffer(const uint16_t cols, const uint16_t rows)
{
    if (screen_buffer_mgr == nullptr)
    {
        screen_buffer_mgr = std::unique_ptr<ScreenBuffer>(new ScreenBuffer(cols, rows));
    }
    else
    {
        screen_buffer_mgr->set_dimensions(cols, rows);
    }
}

void DisplayIo::disable_screen_buffer() noexcept
{
    screen_buffer_mgr = nullptr;
}

void DisplayIo::set_screen_dimensions(const uint16_t cols, const uint16_t rows)
{
    if (screen_buffer_mgr != nullptr)
    {
        screen_buffer_mgr->set_dimensions(cols, rows);
    }
}

void DisplayIo::invalidate_screen() noexcept
{
    if (screen_buffer_mgr != nullptr)
    {
        screen_buffer_mgr->invalidate();
    }
}

void DisplayIo::begin_frame() noexcept
{
    ++frame_level;
}

void DisplayIo::end_frame() noexcept
{
    if (frame_level >= 1)
    {
        --frame_level;
    }
}

/**
 * Writes data to the output_fd file descriptor
 *
 * Write attempts that fail temporarily or are only partially successful are retried until
 * all the data has been written.
 *
 * @param buffer The data to write
 * @param write_length Length of the data in the (possibly larger) buffer
 */
void DisplayIo::write_fd(const char* const buffer, const size_t write_length) const noexcept
{
    const char* pos = buffer;
    size_t length = write_length;
    uint32_t loop_guard {0};
    ssize_t written {0};
    while (length > 0)
    {
        // Repeat temporarily failing write() calls until the entire contents of the buffer have been written
        errno = 0;
        written = write(output_fd, static_cast<const void*> (pos), length);
        if (written > 0)
        {
            pos += written;
            length -= written;
            loop_guard = 0;
        }
        else
        if (written == -1 && (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
        {
            break;
        }

        if (written <= 0)
        {
            if (loop_guard < MAX_YIELD_LOOP)
            {
                // Attempt to yield to other processes before retrying
                static_cast<void> (sched_yield());
                ++loop_guard;
            }
            else
            {
                // If yielding to other processes did not lead to any progress,
                // suspend for a while
                static_cast<void> (nanosleep(&write_retry_delay, nullptr));
            }
        }
    }
}

/**
 * Collects a single character for output to the output_fd file descriptor
 *
 * @param ch The character to write
 */
void DisplayIo::write_char(const char ch) const noexcept
{
    write_buffer(&ch, 1);
}

/**
 * Collects a text string for output to the output_fd file descriptor
 *
 * @param text The text string to write
 */
void DisplayIo::write_text(const char* const text) const noexcept
{
    size_t length = std::strlen(text);
    write_buffer(text, length);
}

/**
 * Formats a text string and collects the result for output to the output_fd file descriptor
 *
 * @param format Format string
 * @param ... Arguments for the format string
 */
void DisplayIo::write_fmt(const char* const format, ...) const noexcept
{
    va_list vars;
    va_start(vars, format);
    const int fmt_length = vsnprintf(format_buffer, FORMAT_BUFFER_SIZE, format, vars);
    va_end(vars);
    if (fmt_length > 0)
    {
        const size_t safe_length = std::min(static_cast<size_t> (fmt_length), FORMAT_BUFFER_SIZE - 1);
        write_buffer(format_buffer, safe_length);
    }
}

void DisplayIo::write_string_field(
    const std::string& text,
    const size_t field_width,
    const bool fill
) const noexcept
{
    const size_t text_length = text.length();
    if (text_length <= field_width)
    {
        write_buffer(text.c_str(), text_length);
        if (fill && text_length < field_width)
        {
            write_fill_char(' ', field_width - text_length);
        }
    }
    else
    {
        // Print truncation indicator only in fields with a length of at least 6 bytes
        if (field_width >= 6)
        {
            // Print truncated text and truncation indicator
            write_buffer(text.c_str(), field_width - 3);
            write_buffer("...", 3);
        }
        else
        {
            // Print truncated text without truncation indicator
            write_buffer(text.c_str(), field_width);
        }
    }
}

void DisplayIo::write_fill_char(const char fill_char, const size_t fill_length) const noexcept
{
    if (fill_length >= 1)
    {
        const size_t chunk_length = std::min(fill_length, FORMAT_BUFFER_SIZE);
        for (size_t idx = 0; idx < chunk_length; ++idx)
        {
            format_buffer[idx] = fill_char;
        }
        size_t remain_length = fill_length;
        while (remain_length > 0)
        {
            const size_t write_length = std::min(chunk_length, remain_length);
            write_buffer(format_buffer, write_length);
            remain_length -= write_length;
        }
    }
}

void DisplayIo::write_fill_seq(const std::string& seq, const size_t seq_count) const noexcept
{
    const size_t seq_length = seq.length();
    if (seq_count >= 1 && seq_length >= 1)
    {
        const char* const seq_chars = seq.c_str();
        for (size_t seq_ctr = 0; seq_ctr < seq_count; ++seq_ctr)
        {
            write_buffer(seq_chars, seq_length);
        }
    }
}
