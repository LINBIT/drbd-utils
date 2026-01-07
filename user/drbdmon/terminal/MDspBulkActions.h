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
    virtual void text_cursor_ops() override;
    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;
    virtual void cursor_to_next_item() override;
    virtual void cursor_to_previous_item() override;
    virtual uint64_t get_update_mask() noexcept override;

  private:
    class RangeSpec
    {
      public:
        uint32_t    skip_count  {0};
        uint32_t    apply_count {0};
    };

    std::unique_ptr<InputField> skip_count_input;
    std::unique_ptr<InputField> apply_count_input;
    std::string                 range_info_msg;
    std::string                 range_error_msg;
    bool                        keep_range      {false};

    uint32_t                    range_page      {0};
    InputField*                 active_input    {nullptr};

    std::function<void()>   cmd_fn_rsc_start;
    std::function<void()>   cmd_fn_rsc_stop;
    std::function<void()>   cmd_fn_rsc_adjust;
    std::function<void()>   cmd_fn_rsc_adjust_skip_disk;
    std::function<void()>   cmd_fn_rsc_adjust_skip_net;
    std::function<void()>   cmd_fn_rsc_adjust_skip_disk_net;
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

    std::function<void()>   cmd_fn_toggle_keep_range;

    std::unique_ptr<ClickableCommand>   cmd_rsc_start;
    std::unique_ptr<ClickableCommand>   cmd_rsc_stop;
    std::unique_ptr<ClickableCommand>   cmd_rsc_adjust;
    std::unique_ptr<ClickableCommand>   cmd_rsc_adjust_skip_disk;
    std::unique_ptr<ClickableCommand>   cmd_rsc_adjust_skip_net;
    std::unique_ptr<ClickableCommand>   cmd_rsc_adjust_skip_disk_net;
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

    std::unique_ptr<ClickableCommand>   cmd_toggle_keep_range;

    void display_actions();
    void display_resource_actions();
    void display_volume_actions();
    void display_connection_actions();
    void display_peer_volume_actions();
    void display_range_options();

    void toggle_keep_range();

    void setup_cmd_functions();
    void setup_pages();

    void execute_resource_actions(DrbdCommands::resource_action_fn action);
    void execute_volume_actions(DrbdCommands::volume_action_fn action);
    void execute_connection_actions(DrbdCommands::connection_action_fn action);
    void execute_peer_volume_actions(DrbdCommands::peer_volume_action_fn action);

    // @throws SubProcessQueue::QueueCapacityException
    void action_loop_for_resources(
        DrbdCommands::resource_action_fn    action,
        bool&                               range_completed,
        RangeSpec&                          range,
        uint32_t&                           skip_ctr,
        uint32_t&                           apply_ctr
    );
    // @throws SubProcessQueue::QueueCapacityException
    void action_loop_for_volumes(
        DrbdCommands::volume_action_fn      action,
        bool&                               range_completed,
        RangeSpec&                          range,
        uint32_t&                           skip_ctr,
        uint32_t&                           apply_ctr
    );
    // @throws SubProcessQueue::QueueCapacityException
    void action_loop_for_connections(
        DrbdCommands::connection_action_fn  action,
        bool&                               range_completed,
        RangeSpec&                          range,
        uint32_t&                           skip_ctr,
        uint32_t&                           apply_ctr
    );
    // @throws SubProcessQueue::QueueCapacityException
    void action_loop_for_peer_volumes(
        DrbdCommands::peer_volume_action_fn action,
        bool&                               range_completed,
        RangeSpec&                          range,
        uint32_t&                           skip_ctr,
        uint32_t&                           apply_ctr
    );

    template<typename A>
    void execute_for_range(std::function<void(A, bool&, RangeSpec&, uint32_t&, uint32_t&)> action_loop, A action)
    {
        bool updated_range = false;
        try
        {
            range_info_msg.clear();
            range_error_msg.clear();
            RangeSpec range = get_exec_range();

            uint32_t skip_ctr = 0;
            uint32_t apply_ctr = 0;

            bool range_completed = false;
            try
            {
                action_loop(action, range_completed, range, skip_ctr, apply_ctr);
                range_completed = true;
            }
            catch (SubProcessQueue::QueueCapacityException&)
            {
                log_insufficient_qcap_error();
            }

            if ((!range_completed || range.apply_count != 0) && apply_ctr > 0)
            {
                const uint32_t updated_skip_count = range.skip_count + apply_ctr;
                const std::string skip_count_text = std::to_string(updated_skip_count);
                skip_count_input->set_text(skip_count_text);
                updated_range = true;
            }
        }
        catch (dsaext::NumberFormatException&)
        {
            // Error message set by get_exec_range
        }

        if (!range_error_msg.empty())
        {
            set_page_nr(range_page);
            dsp_comp_hub.dsp_selector->refresh_display();
        }
        else
        if (updated_range)
        {
            dsp_comp_hub.dsp_selector->refresh_display();
        }
        else
        {
            dsp_comp_hub.dsp_selector->leave_display();
        }
    }

    // @throws NumberFormatException
    RangeSpec get_exec_range();

    void log_subprocess_error(const std::string& rsc_name);
    void log_insufficient_qcap_error();
};

#endif /* MDSPBULKACTIONS_H */
