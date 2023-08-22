#ifndef MDSPHELP_H
#define MDSPHELP_H

#include <default_types.h>
#include <terminal/MDspBase.h>
#include <terminal/TextColumn.h>

class MDspHelp : public MDspBase
{
  public:
    static const uint16_t   MAX_HELP_TEXT_WIDTH;

    MDspHelp(const ComponentsHub& comp_hub);
    virtual ~MDspHelp() noexcept;

    virtual void display_content() override;

    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;

    virtual void enter_command_line_mode() override;
    virtual void leave_command_line_mode() override;
    virtual void enter_page_nav_mode() override;
    virtual void leave_page_nav_mode(const page_change_type change) override;

    virtual void display_activated() override;
    virtual void display_deactivated() override;
    virtual void display_closed() override;
    virtual void reset_display() override;
    virtual void synchronize_data() override;

    virtual void cursor_to_next_item() override;
    virtual void cursor_to_previous_item() override;

    virtual uint64_t get_update_mask() noexcept override;

  private:
    uint16_t    saved_term_cols     {0};
    uint16_t    saved_term_rows     {0};
    TextColumn  format_text;

    uint32_t get_lines_per_page() noexcept;
};

#endif /* MDSPHELP_H */
