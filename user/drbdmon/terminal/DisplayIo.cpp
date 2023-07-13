#include <terminal/DisplayIo.h>
#include <terminal/AnsiControl.h>

#include <cstdarg>

extern "C"
{
    #include <unistd.h>
    #include <errno.h>
}

const uint32_t DisplayIo::MAX_YIELD_LOOP       = 10;
const uint16_t DisplayIo::OUTPUT_BUFFER_SIZE   = 1024;

DisplayIo::DisplayIo(const int init_output_fd):
    output_fd(init_output_fd)
{
    output_buffer_mgr = std::unique_ptr<char[]>(new char[OUTPUT_BUFFER_SIZE]);
    output_buffer = output_buffer_mgr.get();
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
 * Writes buffered data to the output_fd file descriptor
 *
 * Write attempts that fail temporarily or are only partially successful are retried until the
 * all the buffered data has been written.
 *
 * @param buffer The buffered data to write
 * @param length Length of the buffered data in the (possibly larger) buffer
 */
void DisplayIo::write_buffer(const char* const buffer, const size_t write_length) const noexcept
{
    size_t length = write_length;
    uint32_t loop_guard {0};
    ssize_t written {0};
    while (length > 0)
    {
        // Repeat temporarily failing write() calls until the entire contents of the buffer have been written
        errno = 0;
        written = write(output_fd, static_cast<const void*> (buffer), length);
        if (written > 0)
        {
            length -= written;
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
 * Writes a single character to the output_fd file descriptor
 *
 * @param ch The character to write
 */
void DisplayIo::write_char(const char ch) const noexcept
{
    // Repeat temporarily failing write() calls until the byte has been written
    uint32_t loop_guard {0};
    ssize_t write_count = 0;
    do
    {
        errno = 0;
        write_count = write(output_fd, static_cast<const void*> (&ch), 1);
        if (write_count != 1)
        {
            if (write_count == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            {
                // I/O error
                break;
            }

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
    while (write_count != 1);
}

/**
 * Writes a text string to the output_fd file descriptor
 *
 * @param text The text string to write
 */
void DisplayIo::write_text(const char* const text) const noexcept
{
    size_t length = std::strlen(text);
    write_buffer(text, length);
}

/**
 * Formats a text string and writes the result to the output_fd file descriptor
 *
 * @param format Format string
 * @param ... Arguments for the format string
 */
void DisplayIo::write_fmt(const char* const format, ...) const noexcept
{
    va_list vars;
    va_start(vars, format);
    size_t safe_length = 0;
    {
        size_t unsafe_length = vsnprintf(output_buffer, OUTPUT_BUFFER_SIZE, format, vars);
        safe_length = unsafe_length < OUTPUT_BUFFER_SIZE ? unsafe_length : OUTPUT_BUFFER_SIZE;
    }
    va_end(vars);
    write_buffer(output_buffer, safe_length);
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
        if (text_length < field_width)
        {
            const size_t fill_length = std::min(
                static_cast<size_t> (field_width - text_length),
                static_cast<size_t> (OUTPUT_BUFFER_SIZE)
            );
            if (fill)
            {
                for (size_t idx = 0; idx < fill_length; ++idx)
                {
                    output_buffer[idx] = ' ';
                }
                write_buffer(output_buffer, fill_length);
            }
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
    const size_t prepare_length = std::min(fill_length, static_cast<size_t> (OUTPUT_BUFFER_SIZE));
    for (size_t idx = 0; idx < prepare_length; ++idx)
    {
        output_buffer[idx] = fill_char;
    }
    size_t remain_length = fill_length;
    while (remain_length > 0)
    {
        const size_t write_length = std::min(prepare_length, remain_length);
        write_buffer(output_buffer, write_length);
        remain_length -= write_length;
    }
}

void DisplayIo::write_fill_seq(const std::string& seq, const size_t seq_count) const noexcept
{
    if (seq_count >= 1)
    {
        const size_t seq_length = seq.length();
        if (seq_length >= 1 && seq_length < OUTPUT_BUFFER_SIZE)
        {
            const char* const seq_chars = seq.c_str();
            const size_t max_prepare_count = OUTPUT_BUFFER_SIZE / seq_length;
            const size_t prepare_count = std::min(max_prepare_count, seq_count);
            for (size_t idx = 0; idx < prepare_count; ++idx)
            {
                for (size_t seq_idx = 0; seq_idx < seq_length; ++seq_idx)
                {
                    output_buffer[(idx * seq_length) + seq_idx] = seq_chars[seq_idx];
                }
            }
            const size_t write_cycles = seq_count / prepare_count;
            const size_t write_length = prepare_count * seq_length;
            for (size_t write_ctr = 0; write_ctr < write_cycles; ++write_ctr)
            {
                write_buffer(output_buffer, write_length);
            }
            const size_t remain_seq_count = seq_count % prepare_count;
            const size_t remain_write_length = remain_seq_count * seq_length;
            if (remain_write_length >= 1)
            {
                write_buffer(output_buffer, remain_write_length);
            }
        }
    }
}
