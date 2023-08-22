#ifndef MDSPVOLUMEDETAIL_H
#define MDSPVOLUMEDETAIL_H

#include <default_types.h>
#include <terminal/MDspMenuBase.h>

class MDspVolumeDetail : public MDspMenuBase
{
  public:
    MDspVolumeDetail(const ComponentsHub& comp_hub);
    virtual ~MDspVolumeDetail() noexcept;

    virtual void display_content() override;
    virtual uint64_t get_update_mask() noexcept override;
    virtual bool key_pressed(const uint32_t key) override;

    virtual void text_cursor_ops() override;

    virtual void display_activated() override;
    virtual void display_deactivated() override;

  private:
    bool    options_configured  {false};

    uint32_t saved_options_line {0};

    std::function<void()>   cmd_fn_actions;

    std::unique_ptr<ClickableCommand>   cmd_actions;

    void opt_actions();
};

#endif /* MDSPVOLUMEDETAIL_H */
