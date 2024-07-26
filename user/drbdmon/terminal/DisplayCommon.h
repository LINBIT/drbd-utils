#ifndef DISPLAYCOMMON_H
#define DISPLAYCOMMON_H

#include <default_types.h>
#include <terminal/ModularDisplay.h>

class DisplayCommon
{
  public:
    enum problem_mode_type : uint8_t
    {
        OFF     = 0,
        AUTO    = 1,
        LOCK    = 2
    };

    enum command_state_type : uint8_t
    {
        INPUT       = 0,
        CANCEL      = 1
    };

    virtual ~DisplayCommon() noexcept
    {
    }

    virtual void display_header() const = 0;
    virtual void display_page_id(const std::string& page_id) const = 0;
    virtual void display_command_line() const = 0;
    virtual void display_std_hotkeys() const = 0;
    virtual void display_page_or_cursor_nav(const bool is_cursor_tracking) const = 0;
    virtual void display_page_numbers(
        const bool      is_focused,
        const uint32_t  page_nr,
        const uint32_t  page_count,
        const uint32_t  line_offset,
        const uint32_t  input_page_nr
    ) const = 0;
    virtual void display_resource_header(uint32_t& current_line) const = 0;
    virtual void display_connection_header(uint32_t& current_line) const = 0;
    virtual void display_resource_line(uint32_t& current_line) const = 0;
    virtual void display_connection_line(uint32_t& current_line) const = 0;
    virtual uint32_t calculate_page_count(const uint32_t lines, const uint32_t lines_per_page) const = 0;
    virtual void page_navigation_cursor() const = 0;
    virtual void display_selection_mode_label(const bool is_enabled) const = 0;
    virtual void display_problem_mode_label(const bool using_problem_mode) const = 0;
    virtual problem_mode_type get_problem_mode() const noexcept = 0;
    virtual void toggle_problem_mode() noexcept = 0;
    virtual command_state_type command_line_key_pressed(
        const uint32_t  key,
        ModularDisplay& display
    ) const = 0;
    virtual void activate_command_line() const = 0;
    virtual bool execute_command(ModularDisplay& display) const = 0;
    virtual void application_idle() const = 0;
    virtual void application_working() const = 0;
};

#endif /* DISPLAYCOMMON_H */
