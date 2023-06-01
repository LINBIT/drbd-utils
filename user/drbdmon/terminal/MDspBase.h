#ifndef MDSPBASE_H
#define MDSPBASE_H

#include <terminal/ModularDisplay.h>
#include <terminal/ComponentsHub.h>
#include <terminal/MouseEvent.h>
#include <terminal/ClickableCommand.h>

class MDspBase : public ModularDisplay
{
  public:
    enum class page_change_type : uint8_t
    {
        PG_CHG_CANCELED,
        PG_CHG_SAME,
        PG_CHG_CHANGED
    };

    const ComponentsHub& dsp_comp_hub;

    MDspBase(const ComponentsHub& comp_hub);
    virtual ~MDspBase() noexcept;

    virtual void display_activated() override;
    virtual void display_deactivated() override;
    virtual void display_closed() override;
    virtual void reset_display() override;

    virtual void display() override;
    virtual void display_content() = 0;
    virtual void reposition_text_cursor() final;

    // To be overridden by subclasses. Do not call this method directly, call reposition_text_cursor instead.
    virtual void text_cursor_ops();

    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;
    bool execute_command(const std::string& command, StringTokenizer& tokenizer) override;

    virtual void enter_command_line_mode() = 0;
    virtual void leave_command_line_mode() = 0;
    virtual void enter_page_nav_mode() = 0;
    virtual void leave_page_nav_mode(const page_change_type change) = 0;

    virtual void first_page();
    virtual void next_page();
    virtual void previous_page();
    virtual void last_page();
    virtual void cursor_to_next_item() = 0;
    virtual void cursor_to_previous_item() = 0;

    virtual uint32_t get_page_nr() noexcept;
    virtual uint32_t get_line_offset() noexcept;
    virtual uint32_t get_page_count() noexcept;
    virtual void set_page_nr(const uint32_t new_page_nr);
    virtual void set_line_offset(const uint32_t new_line_offset);
    virtual void set_page_count(const uint32_t new_page_count);

    virtual ClickableCommand* get_command_by_name(
        ClickableCommand::CommandMap&   cmd_map,
        const std::string&              name
    );
    virtual ClickableCommand* get_command_by_position(
        ClickableCommand::ClickableMap& click_map,
        const uint16_t                  page_nr,
        MouseEvent&                     event
    );

  protected:
    bool        autoscroll          {false};

  private:
    enum class base_input_mode_type : uint8_t
    {
        // Intercepts global keys like the exit, command line, page navigation keys
        GLOBAL_KEYS = 0,
        // Sends keys to the command line
        COMMAND     = 1,
        // Sends keys to the page number entry handler
        PAGE_NAV    = 2
    };

    base_input_mode_type    base_input_mode {base_input_mode_type::GLOBAL_KEYS};

    uint32_t    base_page_nr        {1};
    uint32_t    base_line_offset    {0};
    uint32_t    base_page_count     {1};
    uint32_t    base_input_page_nr  {0};

    void base_enter_command_line_mode();
    void base_enter_page_nav_mode();
};

#endif /* MDSPBASE_H */
