#ifndef MDSPBULKACTIONS_H
#define MDSPBULKACTIONS_H

#include <default_types.h>
#include <terminal/MDspMenuBase.h>
#include <terminal/DrbdCommands.h>
#include <objects/DrbdResource.h>
#include <objects/DrbdVolume.h>
#include <objects/DrbdConnection.h>

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

    std::unique_ptr<InputField> rsc_program_input;
    std::unique_ptr<InputField> vlm_program_input;
    std::unique_ptr<InputField> con_program_input;
    std::unique_ptr<InputField> peer_vlm_program_input;

    uint32_t                    last_page       {0};
    uint32_t                    range_page      {0};
    uint32_t                    rsc_page        {0};
    uint32_t                    vlm_page        {0};
    uint32_t                    con_page        {0};
    uint32_t                    peer_vlm_page   {0};
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

    std::function<void()>   cmd_fn_rsc_program;
    std::function<void()>   cmd_fn_vlm_program;
    std::function<void()>   cmd_fn_con_program;
    std::function<void()>   cmd_fn_peer_vlm_program;

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

    std::unique_ptr<ClickableCommand>   cmd_rsc_program;
    std::unique_ptr<ClickableCommand>   cmd_vlm_program;
    std::unique_ptr<ClickableCommand>   cmd_con_program;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_program;

    void display_actions();
    void display_resource_actions();
    void display_volume_actions();
    void display_connection_actions();
    void display_peer_volume_actions();
    void display_range_options();

    void toggle_keep_range();

    void setup_cmd_functions();
    void setup_pages();

    using rsc_function = std::function<void(const std::string&, const DrbdResource* const)>;
    using con_function = std::function<void(const std::string&, const DrbdResource* const,
                                            const std::string&, const DrbdConnection* const)>;
    using vlm_function = std::function<void(const std::string&, const DrbdResource* const,
                                            const uint16_t, const DrbdVolume* const)>;
    using peer_vlm_function = std::function<void(const std::string&, const DrbdResource* const,
                                                 const std::string&, const DrbdConnection* const,
                                                 const uint16_t, const DrbdVolume* const)>;

    void execute_resource_actions(rsc_function& action, bool attach_objects = false);
    void execute_volume_actions(vlm_function& action, bool attach_objects = false);
    void execute_connection_actions(con_function& action, bool attach_objects = false);
    void execute_peer_volume_actions(peer_vlm_function& action, bool attach_objects = false);

    // @throws SubProcessQueue::QueueCapacityException
    void action_loop_for_resources(
        rsc_function&                       action,
        bool&                               range_completed,
        RangeSpec&                          range,
        uint32_t&                           skip_ctr,
        uint32_t&                           apply_ctr,
        bool                                attach_objects
    );
    // @throws SubProcessQueue::QueueCapacityException
    void action_loop_for_volumes(
        vlm_function&                       action,
        bool&                               range_completed,
        RangeSpec&                          range,
        uint32_t&                           skip_ctr,
        uint32_t&                           apply_ctr,
        bool                                attach_objects
    );
    // @throws SubProcessQueue::QueueCapacityException
    void action_loop_for_connections(
        con_function&                       action,
        bool&                               range_completed,
        RangeSpec&                          range,
        uint32_t&                           skip_ctr,
        uint32_t&                           apply_ctr,
        bool                                attach_objects
    );
    // @throws SubProcessQueue::QueueCapacityException
    void action_loop_for_peer_volumes(
        peer_vlm_function&                  action,
        bool&                               range_completed,
        RangeSpec&                          range,
        uint32_t&                           skip_ctr,
        uint32_t&                           apply_ctr,
        bool                                attach_objects
    );

    template<typename A>
    void execute_for_range(
        std::function<void(A&, bool&, RangeSpec&, uint32_t&, uint32_t&, bool)> action_loop,
        A& action,
        bool attach_objects
    )
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
                action_loop(action, range_completed, range, skip_ctr, apply_ctr, attach_objects);
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

    void switch_program_input(const std::unique_ptr<InputField>& program_input);
    bool mouse_action_program_input(const std::unique_ptr<InputField>& program_input, MouseEvent& mouse);

    rsc_function rsc_function_for_action(DrbdCommands::resource_action_fn action);
    vlm_function vlm_function_for_action(DrbdCommands::volume_action_fn action);
    con_function con_function_for_action(DrbdCommands::connection_action_fn action);
    peer_vlm_function peer_vlm_function_for_action(DrbdCommands::peer_volume_action_fn action);

    void exec_rsc_program();
    void exec_vlm_program();
    void exec_con_program();
    void exec_peer_vlm_program();
};

#endif /* MDSPBULKACTIONS_H */
