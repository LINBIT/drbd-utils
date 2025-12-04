#ifndef MDSPBULKACTIONS_H
#define MDSPBULKACTIONS_H

#include <default_types.h>
#include <terminal/MDspMenuBase.h>
#include <terminal/DrbdCommands.h>

class MDspBulkActions : public MDspMenuBase
{
  public:
    MDspBulkActions(const ComponentsHub& comp_hub);
    virtual ~MDspBulkActions() noexcept;

    virtual void display_content() override;
    virtual void display_closed() override;
    virtual bool key_pressed(const uint32_t key) override;
    virtual uint64_t get_update_mask() noexcept override;

  private:
    std::unique_ptr<InputField> skip_count_input;
    std::unique_ptr<InputField> apply_count_input;

    std::function<void()>   cmd_fn_rsc_start;
    std::function<void()>   cmd_fn_rsc_stop;
    std::function<void()>   cmd_fn_rsc_adjust;
    std::function<void()>   cmd_fn_rsc_primary;
    std::function<void()>   cmd_fn_rsc_secondary;
    std::function<void()>   cmd_fn_rsc_force_primary;
    std::function<void()>   cmd_fn_rsc_force_secondary;

    std::function<void()>   cmd_fn_vlm_attach;
    std::function<void()>   cmd_fn_vlm_detach;
    // Not implemented in DrbdCommands yet
    // std::function<void()>   cmd_fn_vlm_resize;
    std::function<void()>   cmd_fn_vlm_invalidate;

    std::function<void()>   cmd_fn_con_connect;
    std::function<void()>   cmd_fn_con_disconnect;
    std::function<void()>   cmd_fn_con_discard;

    std::function<void()>   cmd_fn_peer_vlm_pause_sync;
    std::function<void()>   cmd_fn_peer_vlm_resume_sync;
    std::function<void()>   cmd_fn_peer_vlm_verify;
    std::function<void()>   cmd_fn_peer_vlm_invalidate_remote;

    std::unique_ptr<ClickableCommand>   cmd_rsc_start;
    std::unique_ptr<ClickableCommand>   cmd_rsc_stop;
    std::unique_ptr<ClickableCommand>   cmd_rsc_adjust;
    std::unique_ptr<ClickableCommand>   cmd_rsc_primary;
    std::unique_ptr<ClickableCommand>   cmd_rsc_secondary;
    std::unique_ptr<ClickableCommand>   cmd_rsc_force_primary;
    std::unique_ptr<ClickableCommand>   cmd_rsc_force_secondary;

    std::unique_ptr<ClickableCommand>   cmd_vlm_attach;
    std::unique_ptr<ClickableCommand>   cmd_vlm_detach;
    // Not implemented in DrbdCommands yet
    // std::unique_ptr<ClickableCommand>   cmd_vlm_resize;
    std::unique_ptr<ClickableCommand>   cmd_vlm_invalidate;

    std::unique_ptr<ClickableCommand>   cmd_con_connect;
    std::unique_ptr<ClickableCommand>   cmd_con_disconnect;
    std::unique_ptr<ClickableCommand>   cmd_con_discard;

    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_pause_sync;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_resume_sync;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_verify;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_invalidate_remote;

    void display_actions();
    void display_resource_actions();
    void display_volume_actions();
    void display_connection_actions();
    void display_peer_volume_actions();
    void display_range_options();

    void setup_cmd_functions();
    void setup_pages();

    void execute_resource_actions(DrbdCommands::resource_action_fn action);
    void execute_volume_actions(DrbdCommands::volume_action_fn action);
    void execute_connection_actions(DrbdCommands::connection_action_fn action);
    void execute_peer_volume_actions(DrbdCommands::peer_volume_action_fn action);

    void log_subprocess_error(const std::string& rsc_name);
    void log_insufficient_qcap_error();
};

#endif /* MDSPBULKACTIONS_H */
