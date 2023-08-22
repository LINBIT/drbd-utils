#ifndef MDSPMENUBASE_H
#define MDSPMENUBASE_H

#include <default_types.h>
#include <terminal/MDspBase.h>
#include <terminal/ClickableCommand.h>
#include <terminal/InputField.h>
#include <memory>

class MDspMenuBase : public MDspBase
{
  public:
    MDspMenuBase(const ComponentsHub& comp_hub);
    ~MDspMenuBase() noexcept;

    virtual void display_option(
        const char* const       key_text,
        const char* const       text,
        const ClickableCommand& cmd,
        const std::string&      text_color
    );
    virtual void display_option_query(const uint16_t coord_x, const uint16_t coord_y);

    virtual void display_activated() override;
    virtual void display_deactivated() override;
    virtual void display_closed() override;
    virtual void reset_display() override;
    virtual void synchronize_data() override;

    virtual void text_cursor_ops() override;

    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;

    virtual void enter_command_line_mode() override;
    virtual void leave_command_line_mode() override;
    virtual void enter_page_nav_mode() override;
    virtual void leave_page_nav_mode(const page_change_type change) override;

    virtual void cursor_to_next_item() override;
    virtual void cursor_to_previous_item() override;

    virtual InputField& get_option_field();
    virtual void add_option_command(ClickableCommand& cmd);
    virtual void add_option_clickable(ClickableCommand& cmd);
    virtual void add_option(ClickableCommand& cmd);
    virtual void clear_option_commands() noexcept;
    virtual void clear_option_clickables() noexcept;
    virtual void clear_options() noexcept;

    virtual void delegate_focus(const bool flag);
    virtual bool is_focus_delegated();

  private:
    bool    content_focus   {true};
    bool    focus_delegated {false};

    std::unique_ptr<InputField> mb_option_field;

    std::unique_ptr<ClickableCommand::CommandMap>   cmd_map;
    std::unique_ptr<ClickableCommand::ClickableMap> click_map;
};

#endif /* MDSPMENUBASE_H */
