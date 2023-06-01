#ifndef MDSPPGMINFO_H
#define MDSPPGMINFO_H

#include <cstddef>
#include <terminal/MDspBase.h>
#include <terminal/TextColumn.h>

class MDspPgmInfo : public MDspBase
{
  public:
    static const char* const    PGM_INFO_TEXT;
    static const size_t         RESERVE_CAPACITY;

    MDspPgmInfo(const ComponentsHub& comp_hub);
    virtual ~MDspPgmInfo() noexcept;

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

  private:
    TextColumn format_text;
};

#endif /* MDSPPGMINFO_H */
