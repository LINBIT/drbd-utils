#ifndef DISPLAYCOMMONIMPL_H
#define DISPLAYCOMMONIMPL_H

#include <default_types.h>
#include <terminal/DisplayCommon.h>
#include <terminal/ModularDisplay.h>
#include <terminal/DisplayIo.h>
#include <terminal/ComponentsHub.h>
#include <string>

class DisplayCommonImpl : public DisplayCommon
{
  public:
    DisplayCommonImpl(const ComponentsHub& comp_hub);
    virtual ~DisplayCommonImpl() noexcept;

    virtual void display_header() const override;
    virtual void display_page_id(const std::string& page_id) const override;
    virtual void display_command_line() const override;
    virtual void display_std_hotkeys() const override;
    virtual void display_page_or_cursor_nav(const bool is_cursor_tracking) const override;
    virtual void display_page_numbers(
        const bool      is_focused,
        const uint32_t  page_nr,
        const uint32_t  page_count,
        const uint32_t  line_offset,
        const uint32_t  input_page_nr
    ) const override;
    virtual void display_resource_header(uint32_t& current_line) const override;
    virtual void display_connection_header(uint32_t& current_line) const override;
    virtual void display_resource_line(uint32_t& current_line) const override;
    virtual void display_connection_line(uint32_t& current_line) const override;
    virtual uint32_t calculate_page_count(const uint32_t lines, const uint32_t lines_per_page) const override;
    virtual void page_navigation_cursor() const override;
    virtual void display_selection_mode_label(const bool is_enabled) const;
    virtual void display_problem_mode_label(const bool using_problem_mode) const override;
    virtual problem_mode_type get_problem_mode() const noexcept;
    virtual void toggle_problem_mode() noexcept;
    virtual command_state_type command_line_key_pressed(
        const uint32_t  key,
        ModularDisplay& display
    ) const override;
    virtual void activate_command_line() const;
    virtual bool execute_command(ModularDisplay& display) const override;
    virtual void application_idle() const override;
    virtual void application_working() const override;

  private:
    const ComponentsHub& dsp_comp_hub;
    const DisplayIo& dsp_io;

    std::string program_version;

    problem_mode_type   problem_mode    {OFF};

    void write_hotkey(const ColorTable& clr, const char* const key, const char* const label) const;
    void hotkey_spacer() const;
    void command_completion(
        const std::string& command,
        const std::string& arguments,
        const std::string& prefix,
        const bool cmd_scope_drbd
    ) const;
};

#endif /* DISPLAYCOMMONIMPL_H */
