#ifndef MDSPCONNECTIONDETAIL_H
#define MDSPCONNECTIONDETAIL_H

#include <default_types.h>
#include <terminal/MDspMenuBase.h>

class MDspConnectionDetail : public MDspMenuBase
{
  public:
    MDspConnectionDetail(const ComponentsHub& comp_hub);
    virtual ~MDspConnectionDetail() noexcept;

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
    std::function<void()>   cmd_fn_peer_vlm;

    std::unique_ptr<ClickableCommand>   cmd_actions;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm;

    void opt_actions();
    void opt_peer_vlm();
};

#endif /* MDSPCONNECTIONDETAIL_H */
