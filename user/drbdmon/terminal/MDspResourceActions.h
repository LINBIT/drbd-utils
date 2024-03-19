#ifndef MDSPRESOURCEACTIONS_H
#define MDSPRESOURCEACTIONS_H

#include <default_types.h>
#include <terminal/MDspMenuBase.h>

class MDspResourceActions: public MDspMenuBase
{
  public:
    typedef void (MDspResourceActions::*action_func_type)(const std::string& rsc_name);

    static const uint8_t    OPT_LIST_Y;

    MDspResourceActions(const ComponentsHub& comp_hub);
    virtual ~MDspResourceActions() noexcept;

    virtual void display_content() override;
    virtual bool key_pressed(const uint32_t key) override;

    virtual uint64_t get_update_mask() noexcept override;

  private:
    std::function<void()>   cmd_fn_start;
    std::function<void()>   cmd_fn_stop;
    std::function<void()>   cmd_fn_adjust;
    std::function<void()>   cmd_fn_primary;
    std::function<void()>   cmd_fn_secondary;
    std::function<void()>   cmd_fn_connect;
    std::function<void()>   cmd_fn_disconnect;
    std::function<void()>   cmd_fn_verify;
    std::function<void()>   cmd_fn_pause_sync;
    std::function<void()>   cmd_fn_resume_sync;
    std::function<void()>   cmd_fn_connect_discard;
    std::function<void()>   cmd_fn_force_primary;
    std::function<void()>   cmd_fn_invalidate;

    std::unique_ptr<ClickableCommand>   cmd_start;
    std::unique_ptr<ClickableCommand>   cmd_stop;
    std::unique_ptr<ClickableCommand>   cmd_adjust;
    std::unique_ptr<ClickableCommand>   cmd_primary;
    std::unique_ptr<ClickableCommand>   cmd_force_primary;
    std::unique_ptr<ClickableCommand>   cmd_secondary;
    std::unique_ptr<ClickableCommand>   cmd_connect;
    std::unique_ptr<ClickableCommand>   cmd_disconnect;
    std::unique_ptr<ClickableCommand>   cmd_verify;
    std::unique_ptr<ClickableCommand>   cmd_pause_sync;
    std::unique_ptr<ClickableCommand>   cmd_resume_sync;
    std::unique_ptr<ClickableCommand>   cmd_connect_discard;
    std::unique_ptr<ClickableCommand>   cmd_invalidate;

    void show_actions();

    void selection_action(const action_func_type action_func);

    void action_start(const std::string& rsc_name);
    void action_stop(const std::string& rsc_name);
    void action_primary(const std::string& rsc_name);
    void action_force_primary(const std::string& rsc_name);
    void action_secondary(const std::string& rsc_name);
    void action_adjust(const std::string& rsc_name);
    void action_verify(const std::string& rsc_name);
    void action_pause_sync(const std::string& rsc_name);
    void action_resume_sync(const std::string& rsc_name);
    void action_connect(const std::string& rsc_name);
    void action_disconnect(const std::string& rsc_name);
    void action_connect_discard(const std::string& rsc_name);
    void action_invalidate(const std::string& rsc_name);
};

#endif /* MDSPRESOURCEACTIONS_H */
