#ifndef MDSPSTDLISTBASE_H
#define MDSPSTDLISTBASE_H

#include <terminal/MDspBase.h>
#include <terminal/MouseEvent.h>

class MDspStdListBase : public MDspBase
{
  public:
    MDspStdListBase(const ComponentsHub& comp_hub);
    virtual ~MDspStdListBase() noexcept;

    virtual bool is_list_focused();
    virtual void set_list_focus(const bool flag);

    virtual void display_content() override;

    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;

    virtual void enter_command_line_mode() override;
    virtual void leave_command_line_mode() override;
    virtual void enter_page_nav_mode() override;
    virtual void leave_page_nav_mode(const page_change_type change) override;

    virtual void first_page() override;
    virtual void next_page() override;
    virtual void previous_page() override;
    virtual void last_page() override;

    virtual void display_list() = 0;

    virtual bool is_cursor_nav() = 0;
    virtual void reset_cursor_position() = 0;
    virtual void clear_cursor() = 0;
    virtual bool is_selecting() = 0;
    virtual void toggle_select_cursor_item() = 0;
    virtual void clear_selection() = 0;

  private:
    bool list_focused       {false};
};

#endif /* MDSPSTDLISTBASE_H */
