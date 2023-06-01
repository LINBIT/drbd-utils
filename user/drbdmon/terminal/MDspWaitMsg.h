#ifndef MDSPWAITMSG_H
#define MDSPWAITMSG_H

#include <terminal/MDspBase.h>
#include <string>

class MDspWaitMsg : public MDspBase
{
  public:
    MDspWaitMsg(const ComponentsHub& comp_hub);
    virtual ~MDspWaitMsg() noexcept;

    virtual void display_content() override;

    virtual void enter_command_line_mode() override;
    virtual void leave_command_line_mode() override;
    virtual void enter_page_nav_mode() override;
    virtual void leave_page_nav_mode(const page_change_type change) override;

    virtual void cursor_to_next_item() override;
    virtual void cursor_to_previous_item() override;

    virtual void display_deactivated() override;
    virtual void reset_display() override;
    virtual void synchronize_data() override;
    virtual uint64_t get_update_mask() noexcept override;

    virtual void set_wait_msg(const std::string& message);

  private:
    std::string wait_msg;
};


#endif /* MDSPWAITMSG_H */
