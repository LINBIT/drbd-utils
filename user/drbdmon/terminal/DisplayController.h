#ifndef DISPLAYCONTROLLER_H
#define DISPLAYCONTROLLER_H

#include <default_types.h>
#include <terminal/GenericDisplay.h>
#include <terminal/DisplaySelector.h>
#include <terminal/ModularDisplay.h>
#include <terminal/TerminalControl.h>
#include <terminal/ComponentsHub.h>
#include <terminal/DisplayId.h>
#include <terminal/DisplayIdMap.h>
#include <terminal/DisplayIo.h>
#include <terminal/DisplayCommonImpl.h>
#include <terminal/SharedData.h>
#include <terminal/DisplayStyleCollection.h>
#include <terminal/AnsiControl.h>
#include <terminal/MouseEvent.h>
#include <terminal/TermSize.h>
#include <terminal/PosixTermSize.h>
#include <terminal/GlobalCommandsImpl.h>
#include <terminal/DrbdCommandsImpl.h>
#include <terminal/MDspWaitMsg.h>
#include <subprocess/SubProcessQueue.h>
#include <subprocess/SubProcessObserver.h>
#include <configuration/Configuration.h>
#include <platform/SystemApi.h>
#include <DrbdMonCore.h>
#include <MonitorEnvironment.h>
#include <MessageLog.h>
#include <map_types.h>
#include <VList.h>
#include <memory>
#include <string>

class DisplayController : public GenericDisplay, public DisplaySelector
{
  public:
    static constexpr uint16_t   MIN_SIZE_X  = 100;
    static constexpr uint16_t   MIN_SIZE_Y  = 20;
    static constexpr uint16_t   MAX_SIZE_X  = 400;
    static constexpr uint16_t   MAX_SIZE_Y  = 400;

    static constexpr uint8_t    MAX_DSP_REFRESH_LOOPS   = 10;

    static constexpr size_t     MAX_DSP_STACK_SIZE  = 10;

    static const std::string    INITIAL_WAIT_MSG;

    // @throws std::bad_alloc, std::logic_error
    DisplayController(
        DrbdMonCore&                core_instance_ref,
        MonitorEnvironment&         mon_env_ref,
        SubProcessObserver&         sub_proc_obs_ref,
        ResourcesMap&               rsc_map_ref,
        ResourcesMap&               prb_rsc_map_ref
    );
    virtual ~DisplayController() noexcept;
    DisplayController(const DisplayController& other) = delete;
    virtual DisplayController& operator=(const DisplayController& other) = delete;
    DisplayController(DisplayController&& orig) = delete;
    virtual DisplayController& operator=(DisplayController&& orig) = delete;

    virtual void initialize() override;
    virtual void enter_initial_display() override;
    virtual void exit_initial_display() override;
    virtual bool notify_drbd_changed() override;
    virtual bool notify_task_queue_changed() override;
    virtual bool notify_message_log_changed() override;
    virtual void terminal_size_changed() override;
    virtual void key_pressed(const uint32_t key) override;
    virtual void mouse_action(MouseEvent& mouse) override;

    virtual bool switch_to_display(const std::string& display_name) override;
    virtual void switch_to_display(const DisplayId::display_page display_id) override;
    virtual void leave_display() override;
    virtual bool can_leave_display() override;
    virtual DisplayId::display_page get_active_page() noexcept override;
    virtual ModularDisplay& get_active_display() noexcept override;
    virtual void synchronize_displays() override;

    virtual void refresh_display() override;

  private:
    using DisplayStack = VList<DisplayId::display_page>;

    DrbdMonCore&            core_instance;
    MonitorEnvironment&     mon_env;

    std::unique_ptr<DisplayMap> dsp_map;

    std::unique_ptr<TerminalControl> term_ctl_mgr;

    std::unique_ptr<ComponentsHub> dsp_comp_hub_mgr;

    std::unique_ptr<DisplayIo> dsp_io_mgr;
    std::unique_ptr<DisplayCommonImpl> dsp_common_mgr;
    std::unique_ptr<SharedData> dsp_shared_mgr;
    std::unique_ptr<InputField> command_line_mgr;
    std::unique_ptr<PosixTermSize> term_size_mgr;
    std::unique_ptr<DisplayStyleCollection> dsp_styles_mgr;
    std::unique_ptr<AnsiControl> ansi_ctl_mgr;
    std::unique_ptr<SubProcessQueue> sub_proc_queue_mgr;
    std::unique_ptr<GlobalCommandsImpl> global_cmd_exec_mgr;
    std::unique_ptr<DrbdCommandsImpl> drbd_cmd_exec_mgr;

    ModularDisplay*         active_display  {nullptr};
    uint64_t                update_mask     {static_cast<uint64_t> (0xFFFFFFFFFFFFFFFFULL)};

    DisplayId::display_page active_page = DisplayId::display_page::RSC_LIST;
    std::unique_ptr<DisplayStack> dsp_stack;

    std::unique_ptr<ModularDisplay>     main_menu_mgr;
    std::unique_ptr<ModularDisplay>     resource_view_mgr;
    std::unique_ptr<ModularDisplay>     resource_detail_mgr;
    std::unique_ptr<ModularDisplay>     resource_actions_mgr;
    std::unique_ptr<ModularDisplay>     volume_view_mgr;
    std::unique_ptr<ModularDisplay>     volume_detail_mgr;
    std::unique_ptr<ModularDisplay>     volume_actions_mgr;
    std::unique_ptr<ModularDisplay>     connections_view_mgr;
    std::unique_ptr<ModularDisplay>     connection_detail_mgr;
    std::unique_ptr<ModularDisplay>     connection_actions_mgr;
    std::unique_ptr<ModularDisplay>     peer_volume_view_mgr;
    std::unique_ptr<ModularDisplay>     peer_volume_detail_mgr;
    std::unique_ptr<ModularDisplay>     peer_volume_actions_mgr;
    std::unique_ptr<ModularDisplay>     active_tasks_mgr;
    std::unique_ptr<ModularDisplay>     pending_tasks_mgr;
    std::unique_ptr<ModularDisplay>     suspended_tasks_mgr;
    std::unique_ptr<ModularDisplay>     finished_tasks_mgr;
    std::unique_ptr<ModularDisplay>     task_detail_mgr;
    std::unique_ptr<ModularDisplay>     help_idx_mgr;
    std::unique_ptr<ModularDisplay>     help_view_mgr;
    std::unique_ptr<ModularDisplay>     log_view_mgr;
    std::unique_ptr<ModularDisplay>     debug_log_view_mgr;
    std::unique_ptr<ModularDisplay>     msg_view_mgr;
    std::unique_ptr<ModularDisplay>     debug_msg_view_mgr;
    std::unique_ptr<ModularDisplay>     pgm_info_mgr;
    std::unique_ptr<ModularDisplay>     config_mgr;

    std::unique_ptr<MDspWaitMsg>        wait_msg_mgr;

    static int compare_display_id(
        const DisplayId::display_page* const key,
        const DisplayId::display_page* const other
    ) noexcept;

    bool refresh_display_flag   {false};

    void display();
    void cond_refresh_display();
    void terminal_size_changed_impl() noexcept;
    void terminal_size_error();
    void switch_active_display(
        ModularDisplay* next_display,
        DisplayId::display_page next_dsp_id,
        const bool close_flag
    );
    void push_onto_display_stack(const DisplayId::display_page page_id);
    DisplayId::display_page pop_from_display_stack();
    void get_display(
        const DisplayId::display_page   display_id,
        ModularDisplay*&                dsp_obj
    ) const noexcept;
};

#endif /* DISPLAYCONTROLLER_H */
