#ifndef MDSPMAINMENU_H
#define MDSPMAINMENU_H

#include <terminal/MDspMenuBase.h>
#include <terminal/InputField.h>
#include <terminal/MouseEvent.h>
#include <terminal/ClickableCommand.h>
#include <memory>

class MDspMainMenu : public MDspMenuBase
{
  public:
    std::unique_ptr<ClickableCommand>   cmd_rsc_ovw;
    std::unique_ptr<ClickableCommand>   cmd_log;
    std::unique_ptr<ClickableCommand>   cmd_act_tsk;
    std::unique_ptr<ClickableCommand>   cmd_pnd_tsk;
    std::unique_ptr<ClickableCommand>   cmd_ssp_tsk;
    std::unique_ptr<ClickableCommand>   cmd_fin_tsk;
    std::unique_ptr<ClickableCommand>   cmd_help_idx;
    std::unique_ptr<ClickableCommand>   cmd_about;
    std::unique_ptr<ClickableCommand>   cmd_configuration;
    std::unique_ptr<ClickableCommand>   cmd_start_all_rsc;
    std::unique_ptr<ClickableCommand>   cmd_stop_all_rsc;
    std::unique_ptr<ClickableCommand>   cmd_exit;

    MDspMainMenu(const ComponentsHub& comp_hub);
    virtual ~MDspMainMenu() noexcept;

    virtual void display_content() override;

    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;

    virtual uint64_t get_update_mask() noexcept override;

  private:
    std::function<void()>   cmd_fn_rsc_ovw;
    std::function<void()>   cmd_fn_log;
    std::function<void()>   cmd_fn_act_tsk;
    std::function<void()>   cmd_fn_pnd_tsk;
    std::function<void()>   cmd_fn_ssp_tsk;
    std::function<void()>   cmd_fn_fin_tsk;
    std::function<void()>   cmd_fn_help_idx;
    std::function<void()>   cmd_fn_about;
    std::function<void()>   cmd_fn_configuration;
    std::function<void()>   cmd_fn_start_all_rsc;
    std::function<void()>   cmd_fn_stop_all_rsc;
    std::function<void()>   cmd_fn_exit;

    void opt_resource_overview();
    void opt_log();
    void opt_active_tasks();
    void opt_pending_tasks();
    void opt_suspended_tasks();
    void opt_finished_tasks();
    void opt_help_index();
    void opt_about_drbdmon();
    void opt_configuration();
    void opt_start_all_resources();
    void opt_stop_all_resources();
    void opt_exit();
};

#endif /* MDSPMAINMENU_H */
