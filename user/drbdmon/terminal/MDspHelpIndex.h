#ifndef MDSPHELPINDEX_H
#define MDSPHELPINDEX_H

#include <default_types.h>
#include <terminal/MDspMenuBase.h>
#include <terminal/ClickableCommand.h>
#include <memory>

class MDspHelpIndex : public MDspMenuBase
{
  public:
    std::unique_ptr<ClickableCommand>   cmd_general_help;
    std::unique_ptr<ClickableCommand>   cmd_rsc_list;
    std::unique_ptr<ClickableCommand>   cmd_rsc_detail;
    std::unique_ptr<ClickableCommand>   cmd_rsc_actions;
    std::unique_ptr<ClickableCommand>   cmd_vlm_list;
    std::unique_ptr<ClickableCommand>   cmd_vlm_detail;
    std::unique_ptr<ClickableCommand>   cmd_vlm_actions;
    std::unique_ptr<ClickableCommand>   cmd_con_list;
    std::unique_ptr<ClickableCommand>   cmd_con_detail;
    std::unique_ptr<ClickableCommand>   cmd_con_actions;
    std::unique_ptr<ClickableCommand>   cmd_pvlm_list;
    std::unique_ptr<ClickableCommand>   cmd_pvlm_detail;
    std::unique_ptr<ClickableCommand>   cmd_pvlm_actions;
    std::unique_ptr<ClickableCommand>   cmd_msg_log;
    std::unique_ptr<ClickableCommand>   cmd_msg_detail;
    std::unique_ptr<ClickableCommand>   cmd_taskq;
    std::unique_ptr<ClickableCommand>   cmd_task_detail;
    std::unique_ptr<ClickableCommand>   cmd_global_cmd;
    std::unique_ptr<ClickableCommand>   cmd_drbd_cmd;
    std::unique_ptr<ClickableCommand>   cmd_conf;


    MDspHelpIndex(const ComponentsHub& comp_hub);
    virtual ~MDspHelpIndex() noexcept;

    virtual void display_content() override;

    virtual uint64_t get_update_mask() noexcept override;

  private:
    std::function<void()>     cmd_fn_general_help;
    std::function<void()>     cmd_fn_rsc_list;
    std::function<void()>     cmd_fn_rsc_detail;
    std::function<void()>     cmd_fn_rsc_actions;
    std::function<void()>     cmd_fn_vlm_list;
    std::function<void()>     cmd_fn_vlm_detail;
    std::function<void()>     cmd_fn_vlm_actions;
    std::function<void()>     cmd_fn_con_list;
    std::function<void()>     cmd_fn_con_detail;
    std::function<void()>     cmd_fn_con_actions;
    std::function<void()>     cmd_fn_pvlm_list;
    std::function<void()>     cmd_fn_pvlm_detail;
    std::function<void()>     cmd_fn_pvlm_actions;
    std::function<void()>     cmd_fn_msg_log;
    std::function<void()>     cmd_fn_msg_detail;
    std::function<void()>     cmd_fn_taskq;
    std::function<void()>     cmd_fn_task_detail;
    std::function<void()>     cmd_fn_global_cmd;
    std::function<void()>     cmd_fn_drbd_cmd;
    std::function<void()>     cmd_fn_conf;
};

#endif /* MDSPHELPINDEX_H */
