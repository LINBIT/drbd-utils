#include <DrbdMonConsts.h>
#include <terminal/DisplayController.h>
#include <terminal/DisplayCommon.h>
#include <terminal/KeyCodes.h>
#include <terminal/MDspMainMenu.h>
#include <terminal/MDspResources.h>
#include <terminal/MDspResourceDetail.h>
#include <terminal/MDspResourceActions.h>
#include <terminal/MDspVolumes.h>
#include <terminal/MDspVolumeDetail.h>
#include <terminal/MDspVolumeActions.h>
#include <terminal/MDspConnections.h>
#include <terminal/MDspConnectionDetail.h>
#include <terminal/MDspConnectionActions.h>
#include <terminal/MDspPeerVolumes.h>
#include <terminal/MDspPeerVolumeDetail.h>
#include <terminal/MDspPeerVolumeActions.h>
#include <terminal/MDspTaskQueue.h>
#include <terminal/MDspTaskDetail.h>
#include <terminal/MDspHelpIndex.h>
#include <terminal/MDspHelp.h>
#include <terminal/MDspLogViewer.h>
#include <terminal/MDspMessage.h>
#include <terminal/MDspPgmInfo.h>
#include <terminal/MDspConfiguration.h>
#include <terminal/InputField.h>
#include <terminal/DisplayConsts.h>
#include <terminal/DisplayUpdateEvent.h>
#include <subprocess/CmdLine.h>
#include <string_transformations.h>
#include <comparators.h>
#include <bounds.h>

extern "C"
{
    #include <unistd.h>
}

const std::string DisplayController::INITIAL_WAIT_MSG   = "Reading initial DRBD state";

// @throws std::bad_alloc, std::logic_error
DisplayController::DisplayController(
    DrbdMonCore&                core_instance_ref,
    SystemApi&                  sys_api_ref,
    SubProcessObserver&         sub_proc_obs_ref,
    ResourcesMap&               rsc_map_ref,
    ResourcesMap&               prb_rsc_map_ref,
    MessageLog&                 log_ref,
    Configuration&              config_ref,
    const std::string* const    node_name_ptr
):
    core_instance(core_instance_ref)
{
    SystemApi& sys_api = core_instance.get_system_api();
    dsp_map = std::unique_ptr<DisplayMap>(new DisplayMap(&comparators::compare_string));

    term_ctl_mgr = sys_api.create_terminal_control();

    dsp_comp_hub_mgr = std::unique_ptr<ComponentsHub>(new ComponentsHub());
    dsp_io_mgr = std::unique_ptr<DisplayIo>(new DisplayIo(STDOUT_FILENO));
    dsp_shared_mgr = std::unique_ptr<SharedData>(new SharedData());
    term_size_mgr = std::unique_ptr<PosixTermSize>(new PosixTermSize());
    dsp_styles_mgr = std::unique_ptr<DisplayStyleCollection>(new DisplayStyleCollection());
    ansi_ctl_mgr = std::unique_ptr<AnsiControl>(new AnsiControl());
    sub_proc_queue_mgr = std::unique_ptr<SubProcessQueue>(new SubProcessQueue());

    dsp_comp_hub_mgr->core_instance     = &core_instance;
    dsp_comp_hub_mgr->sys_api           = &sys_api_ref;
    dsp_comp_hub_mgr->dsp_selector      = dynamic_cast<DisplaySelector*> (this);
    dsp_comp_hub_mgr->dsp_io            = dsp_io_mgr.get();
    dsp_comp_hub_mgr->dsp_shared        = dsp_shared_mgr.get();
    dsp_comp_hub_mgr->term_size         = dynamic_cast<TermSize*> (term_size_mgr.get());
    dsp_comp_hub_mgr->rsc_map           = &rsc_map_ref;
    dsp_comp_hub_mgr->prb_rsc_map       = &prb_rsc_map_ref;
    dsp_comp_hub_mgr->log               = &log_ref;
    dsp_comp_hub_mgr->node_name         = node_name_ptr;
    dsp_comp_hub_mgr->style_coll        = dsp_styles_mgr.get();
    dsp_comp_hub_mgr->ansi_ctl          = ansi_ctl_mgr.get();
    dsp_comp_hub_mgr->sub_proc_queue    = sub_proc_queue_mgr.get();
    dsp_comp_hub_mgr->config            = &config_ref;
    dsp_comp_hub_mgr->have_term_size    = false;
    dsp_comp_hub_mgr->term_cols         = 100;
    dsp_comp_hub_mgr->term_rows         = 30;

    {
        const DisplayStyleCollection::ColorStyle selected_color_style =
            dsp_styles_mgr->get_color_style_by_numeric_id(config_ref.color_scheme);
        dsp_comp_hub_mgr->active_color_table =
            &(dsp_styles_mgr->get_color_table(selected_color_style));

        const DisplayStyleCollection::CharacterStyle selected_character_style =
            dsp_styles_mgr->get_character_style_by_numeric_id(config_ref.character_set);
        dsp_comp_hub_mgr->active_character_table =
            &(dsp_styles_mgr->get_character_table(selected_character_style));
    }

    command_line_mgr = std::unique_ptr<InputField>(
        new InputField(*(dsp_comp_hub_mgr.get()), DisplayConsts::MAX_CMD_LENGTH)
    );
    dsp_comp_hub_mgr->command_line      = command_line_mgr.get();

    dsp_common_mgr = std::unique_ptr<DisplayCommonImpl>(new DisplayCommonImpl(*dsp_comp_hub_mgr));
    dsp_comp_hub_mgr->dsp_common        = dynamic_cast<DisplayCommon*> (dsp_common_mgr.get());

    drbd_cmd_exec_mgr = std::unique_ptr<DrbdCommandsImpl>(new DrbdCommandsImpl(*dsp_comp_hub_mgr));
    dsp_comp_hub_mgr->drbd_cmd_exec     = dynamic_cast<DrbdCommands*> (drbd_cmd_exec_mgr.get());

    global_cmd_exec_mgr = std::unique_ptr<GlobalCommandsImpl>(
        new GlobalCommandsImpl(*dsp_comp_hub_mgr, config_ref)
    );
    dsp_comp_hub_mgr->global_cmd_exec   = dynamic_cast<GlobalCommands*> (global_cmd_exec_mgr.get());

    dsp_comp_hub_mgr->sub_proc_queue->set_observer(&sub_proc_obs_ref);

    // Sub process queue configuration
    sub_proc_queue_mgr->set_discard_succeeded_tasks(config_ref.discard_succ_tasks);
    sub_proc_queue_mgr->set_discard_finished_tasks(config_ref.discard_fail_tasks);
    dsp_shared_mgr->activate_tasks = !(config_ref.suspend_new_tasks);

    dsp_stack = std::unique_ptr<DisplayStack>(new DisplayStack(&DisplayController::compare_display_id));

    {
        const ComponentsHub& dsp_comp_hub = *(dsp_comp_hub_mgr.get());
        main_menu_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspMainMenu(dsp_comp_hub))
        );
        resource_view_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspResources(dsp_comp_hub))
        );
        resource_detail_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspResourceDetail(dsp_comp_hub))
        );
        resource_actions_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspResourceActions(dsp_comp_hub))
        );
        volume_view_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspVolumes(dsp_comp_hub))
        );
        volume_detail_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspVolumeDetail(dsp_comp_hub))
        );
        volume_actions_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspVolumeActions(dsp_comp_hub))
        );
        connections_view_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspConnections(dsp_comp_hub))
        );
        connection_detail_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspConnectionDetail(dsp_comp_hub))
        );
        connection_actions_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspConnectionActions(dsp_comp_hub))
        );
        peer_volume_view_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspPeerVolumes(dsp_comp_hub))
        );
        peer_volume_detail_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspPeerVolumeDetail(dsp_comp_hub))
        );
        peer_volume_actions_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspPeerVolumeActions(dsp_comp_hub))
        );
        active_tasks_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (
                new MDspTaskQueue(
                    dsp_comp_hub,
                    *sub_proc_queue_mgr,
                    DisplayConsts::task_queue_type::ACTIVE_QUEUE,
                    *(dsp_comp_hub.dsp_shared->selected_actq_entries),
                    &SubProcessQueue::active_queue_iterator,
                    &SubProcessQueue::active_queue_offset_iterator,
                    &SubProcessQueue::get_active_queue_selected_id,
                    &SubProcessQueue::set_active_queue_selected_id
                )
            )
        );
        pending_tasks_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (
                new MDspTaskQueue(
                    dsp_comp_hub,
                    *sub_proc_queue_mgr,
                    DisplayConsts::task_queue_type::PENDING_QUEUE,
                    *(dsp_comp_hub.dsp_shared->selected_pndq_entries),
                    &SubProcessQueue::pending_queue_iterator,
                    &SubProcessQueue::pending_queue_offset_iterator,
                    &SubProcessQueue::get_pending_queue_selected_id,
                    &SubProcessQueue::set_pending_queue_selected_id
                )
            )
        );
        suspended_tasks_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (
                new MDspTaskQueue(
                    dsp_comp_hub,
                    *sub_proc_queue_mgr,
                    DisplayConsts::task_queue_type::SUSPENDED_QUEUE,
                    *(dsp_comp_hub.dsp_shared->selected_sspq_entries),
                    &SubProcessQueue::suspended_queue_iterator,
                    &SubProcessQueue::suspended_queue_offset_iterator,
                    &SubProcessQueue::get_suspended_queue_selected_id,
                    &SubProcessQueue::set_suspended_queue_selected_id
                )
            )
        );
        finished_tasks_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (
                new MDspTaskQueue(
                    dsp_comp_hub,
                    *sub_proc_queue_mgr,
                    DisplayConsts::task_queue_type::FINISHED_QUEUE,
                    *(dsp_comp_hub.dsp_shared->selected_finq_entries),
                    &SubProcessQueue::finished_queue_iterator,
                    &SubProcessQueue::finished_queue_offset_iterator,
                    &SubProcessQueue::get_finished_queue_selected_id,
                    &SubProcessQueue::set_finished_queue_selected_id
                )
            )
        );
        task_detail_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspTaskDetail(dsp_comp_hub))
        );
        help_idx_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspHelpIndex(dsp_comp_hub))
        );
        help_view_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspHelp(dsp_comp_hub))
        );
        log_view_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspLogViewer(dsp_comp_hub))
        );
        msg_view_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspMessage(dsp_comp_hub))
        );
        pgm_info_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspPgmInfo(dsp_comp_hub))
        );
        // Pass a mutable components hub to the configuration display
        config_mgr = std::unique_ptr<ModularDisplay>(
            dynamic_cast<ModularDisplay*> (new MDspConfiguration(*dsp_comp_hub_mgr, config_ref))
        );

        wait_msg_mgr = std::unique_ptr<MDspWaitMsg>(new MDspWaitMsg(dsp_comp_hub));

    }

    {
        AnsiControl* ansi_ctl = dsp_comp_hub_mgr->ansi_ctl;
        DisplayIo* dsp_io = dsp_comp_hub_mgr->dsp_io;
        dsp_io->write_text(ansi_ctl->ANSI_ALTBFR_ON.c_str());
        dsp_io->write_text(ansi_ctl->ANSI_CURSOR_OFF.c_str());
        if (config_ref.enable_mouse_nav)
        {
            dsp_io->write_text(ansi_ctl->ANSI_MOUSE_ON.c_str());
        }
        dsp_io->write_text(ansi_ctl->ANSI_CLEAR_SCREEN.c_str());
    }

    dsp_comp_hub_mgr->verify();

    dspid::initialize_display_ids(*dsp_map);

    MDspWaitMsg* const wait_msg = wait_msg_mgr.get();
    wait_msg->set_wait_msg("Reading initial DRBD state");

    active_display = dynamic_cast<ModularDisplay*> (wait_msg);
    update_mask = active_display->get_update_mask();
    active_display->display_activated();
}

DisplayController::~DisplayController() noexcept
{
    AnsiControl* ansi_ctl = dsp_comp_hub_mgr->ansi_ctl;
    DisplayIo* dsp_io = dsp_comp_hub_mgr->dsp_io;

    dsp_io->write_text(ansi_ctl->ANSI_CURSOR_ON.c_str());
    dsp_io->write_text(ansi_ctl->ANSI_MOUSE_OFF.c_str());
    dsp_io->write_text(ansi_ctl->ANSI_ALTBFR_OFF.c_str());

    dsp_io->write_text(ansi_ctl->ANSI_CLEAR_SCREEN.c_str());

    DisplayStack::ValuesIterator iter(*dsp_stack);
    while (iter.has_next())
    {
        DisplayId::display_page* const page_entry = iter.next();
        delete page_entry;
    }
    dsp_stack->clear();

    SubProcessQueue* const subproc_queue = dsp_comp_hub_mgr->sub_proc_queue;
    std::mutex& queue_lock = subproc_queue->get_queue_lock();
    {
        std::unique_lock<std::mutex> lock(queue_lock);

        SubProcessQueue::Iterator task_iter = subproc_queue->active_queue_iterator();
        if (task_iter.get_size() >= 1)
        {
            dsp_io->write_text("The following external processes will be terminated:\n");
            while (task_iter.has_next())
            {
                SubProcessQueue::Entry* const task_entry = task_iter.next();
                SubProcess* const subproc = task_entry->get_process();
                const CmdLine* const cmd = task_entry->get_command();
                if (subproc != nullptr && cmd != nullptr)
                {
                    const uint64_t proc_id = subproc->get_pid();
                    dsp_io->write_fmt("  %20llu ", static_cast<unsigned long long> (proc_id));

                    const std::string& description = cmd->get_description();
                    dsp_io->write_text(description.c_str());
                    dsp_io->write_char('\n');
                }
            }
        }
    }

    dsp_io->write_text("Waiting for the external processes to exit: ");
    sub_proc_queue_mgr = nullptr;
    dsp_io->write_text("done.\n");
}

void DisplayController::initialize()
{
    terminal_size_changed_impl();

    try
    {
        term_ctl_mgr->adjust_terminal();
    }
    catch (TerminalControl::Exception&)
    {
        dsp_comp_hub_mgr->log->add_entry(
            MessageLog::log_level::WARN,
            "The system call to adjust the terminal mode failed. The user interface may not work properly."
        );
    }
}

void DisplayController::enter_initial_display()
{
    // Already initialized, display the wait message
    display();
}

void DisplayController::exit_initial_display()
{
    if (active_display == wait_msg_mgr.get())
    {
        switch_active_display(resource_view_mgr.get(), DisplayId::display_page::RSC_LIST, true);
    }
}

bool DisplayController::notify_drbd_changed()
{
    bool update_flag = false;
    if ((update_mask & update_event::UPDATE_FLAG_DRBD) == update_event::UPDATE_FLAG_DRBD)
    {
        display();
        update_flag = true;
    }
    return update_flag;
}

bool DisplayController::notify_task_queue_changed()
{
    bool update_flag = false;
    if ((update_mask & update_event::UPDATE_FLAG_TASK_QUEUE) == update_event::UPDATE_FLAG_TASK_QUEUE)
    {
        display();
        update_flag = true;
    }
    return update_flag;
}

bool DisplayController::notify_message_log_changed()
{
    bool update_flag = false;
    if ((update_mask & update_event::UPDATE_FLAG_MESSAGE_LOG) == update_event::UPDATE_FLAG_MESSAGE_LOG)
    {
        display();
        update_flag = true;
    }
    return update_flag;
}

void DisplayController::terminal_size_changed()
{
    terminal_size_changed_impl();
    display();
}

void DisplayController::terminal_size_changed_impl() noexcept
{
    ComponentsHub& dsp_comp_hub = *(dsp_comp_hub_mgr.get());

    if (dsp_comp_hub.term_size->probe_terminal_size())
    {
        const uint16_t unsafe_size_x = dsp_comp_hub.term_size->get_size_x();
        const uint16_t unsafe_size_y = dsp_comp_hub.term_size->get_size_y();
        dsp_comp_hub.term_cols = bounds(MIN_SIZE_X, unsafe_size_x, MAX_SIZE_X);
        dsp_comp_hub.term_rows = bounds(MIN_SIZE_Y, unsafe_size_y, MAX_SIZE_Y);
        dsp_comp_hub.have_term_size = (unsafe_size_x >= MIN_SIZE_X && unsafe_size_y >= MIN_SIZE_Y);
        dsp_comp_hub.command_line->set_position(
            DisplayConsts::CMD_LINE_X,
            dsp_comp_hub.term_rows - DisplayConsts::CMD_LINE_Y
        );
        dsp_comp_hub.command_line->set_field_length(dsp_comp_hub.term_cols - DisplayConsts::CMD_LINE_X + 1);
    }
}

void DisplayController::key_pressed(const uint32_t key)
{
    if (key == KeyCodes::FUNC_03)
    {
        core_instance.shutdown(DrbdMonCore::finish_action::TERMINATE);
    }
    else
    if (key == KeyCodes::FUNC_05)
    {
        display();
    }
    else
    if (dsp_comp_hub_mgr->have_term_size)
    {
        active_display->key_pressed(key);
        cond_refresh_display();
    }
}

void DisplayController::mouse_action(MouseEvent& mouse)
{
    if (dsp_comp_hub_mgr->have_term_size && dsp_comp_hub_mgr->config->enable_mouse_nav)
    {
        active_display->mouse_action(mouse);
        cond_refresh_display();
    }
}

void DisplayController::display()
{
    ComponentsHub& dsp_comp_hub = *dsp_comp_hub_mgr;
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CURSOR_OFF.c_str());
    if (dsp_comp_hub.have_term_size)
    {
        uint8_t loop_guard = 0;
        do
        {
            refresh_display_flag = false;
            active_display->display();
            ++loop_guard;
        }
        while (refresh_display_flag && loop_guard < MAX_DSP_REFRESH_LOOPS);
    }
    else
    {
        terminal_size_error();
    }
}

void DisplayController::terminal_size_error()
{
    ComponentsHub& dsp_comp_hub = *dsp_comp_hub_mgr;
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_SCREEN.c_str());
    dsp_comp_hub.dsp_io->cursor_xy(1, 1);
    dsp_comp_hub.dsp_io->write_text(DrbdMonConsts::PROGRAM_NAME.c_str());
    dsp_comp_hub.dsp_io->write_text(": Terminal dimensions too small\n");
    dsp_comp_hub.dsp_io->write_fmt(
        "    Minimum dimensions: %d columns, %d rows",
        static_cast<unsigned int> (MIN_SIZE_X),
        static_cast<unsigned int> (MIN_SIZE_Y)
    );
}

void DisplayController::cond_refresh_display()
{
    if (refresh_display_flag)
    {
        display();
    }
}

void DisplayController::refresh_display()
{
    refresh_display_flag = true;
}

int DisplayController::compare_display_id(
    const DisplayId::display_page* const key,
    const DisplayId::display_page* const other
) noexcept
{
    int result = 0;
    if (key != nullptr && other != nullptr)
    {
        const uint16_t key_num = static_cast<uint16_t> (*key);
        const uint16_t other_num = static_cast<uint16_t> (*other);

        if (key_num < other_num)
        {
            result = -1;
        }
        else
        if (key_num > other_num)
        {
            result = 1;
        }
    }
    else
    {
        if (key == nullptr && other != nullptr)
        {
            result = -1;
        }
        else
        if (key != nullptr && other == nullptr)
        {
            result = 1;
        }
    }
    return result;
}

bool DisplayController::switch_to_display(const std::string& display_name)
{
    bool switched = false;
    std::string display_name_upper = string_transformations::uppercase_copy_of(display_name);
    const DisplayId* const display_id = dsp_map->get(&display_name_upper);
    if (display_id != nullptr)
    {
        switch_to_display(display_id->page_id);
        switched = true;
    }
    return switched;
}

void DisplayController::switch_to_display(const DisplayId::display_page display_id)
{
    if (dsp_comp_hub_mgr->have_term_size)
    {
        dsp_comp_hub_mgr->dsp_common->application_working();
    }

    DisplayId::display_page next_dsp_id = DisplayId::display_page::MAIN_MENU;
    ModularDisplay* next_display = main_menu_mgr.get();

    get_display(display_id, next_display);
    if (next_display != main_menu_mgr.get())
    {
        next_dsp_id = display_id;
    }

    if (next_display != active_display)
    {
        DisplayStack::NodesIterator iter(*dsp_stack);
        while (iter.has_next())
        {
            DisplayStack::Node* dsp_node = iter.next();
            DisplayId::display_page* const page_entry = dsp_node->get_value();
            DisplayId::display_page const cur_dsp_id = *(page_entry);
            if (cur_dsp_id == next_dsp_id)
            {
                delete page_entry;
                dsp_stack->remove_node(dsp_node);
                break;
            }
        }

        if (!(active_display == main_menu_mgr.get()))
        {
            push_onto_display_stack(active_page);
        }

        switch_active_display(next_display, next_dsp_id, false);
    }
    refresh_display_flag = true;
}

void DisplayController::leave_display()
{
    DisplayId::display_page dsp_id = pop_from_display_stack();

    ModularDisplay* const main_menu = main_menu_mgr.get();

    DisplayId::display_page prev_dsp_id = DisplayId::display_page::MAIN_MENU;
    ModularDisplay* prev_display = main_menu;

    get_display(dsp_id, prev_display);
    if (prev_display != main_menu)
    {
        prev_dsp_id = dsp_id;
    }

    switch_active_display(prev_display, prev_dsp_id, true);
}

bool DisplayController::can_leave_display()
{
    return dsp_stack->get_size() >= 1 || active_page != DisplayId::display_page::MAIN_MENU;
}

DisplayId::display_page DisplayController::get_active_page() noexcept
{
    return active_page;
}

ModularDisplay& DisplayController::get_active_display() noexcept
{
    return *active_display;
}

void DisplayController::synchronize_displays()
{
    active_display->synchronize_data();
}

void DisplayController::switch_active_display(
    ModularDisplay* next_display,
    DisplayId::display_page next_dsp_id,
    const bool close_flag
)
{
    active_display->synchronize_data();
    active_display->display_deactivated();
    if (close_flag)
    {
        active_display->display_closed();
    }

    active_display = next_display;
    active_page = next_dsp_id;

    update_mask = active_display->get_update_mask();
    active_display->display_activated();

    refresh_display_flag = true;
}

void DisplayController::push_onto_display_stack(const DisplayId::display_page page_id)
{
    if (dsp_stack->get_size() >= MAX_DSP_STACK_SIZE)
    {
        DisplayStack::Node* const head_node = dsp_stack->get_head_node();
        if (head_node != nullptr)
        {
            const DisplayId::display_page* const head_page_entry = head_node->get_value();

            // Close the display that is dropped from the display stack
            ModularDisplay* dropped_display = nullptr;
            get_display(*head_page_entry, dropped_display);
            if (dropped_display != nullptr)
            {
                dropped_display->display_closed();
            }

            delete head_page_entry;
            dsp_stack->remove_node(head_node);
        }
    }

    std::unique_ptr<DisplayId::display_page> page_entry(new DisplayId::display_page);
    *page_entry = page_id;
    dsp_stack->append(page_entry.get());
    page_entry.release();
}

DisplayId::display_page DisplayController::pop_from_display_stack()
{
    DisplayId::display_page page_id = DisplayId::display_page::MAIN_MENU;
    DisplayStack::Node* const dsp_node = dsp_stack->get_tail_node();
    if (dsp_node != nullptr)
    {
        const DisplayId::display_page* const page_entry = dsp_node->get_value();
        page_id = *page_entry;
        delete page_entry;
        dsp_stack->remove_node(dsp_node);
    }
    return page_id;
}

void DisplayController::get_display(
    const DisplayId::display_page   display_id,
    ModularDisplay*&                dsp_obj
) const noexcept
{
    switch (display_id)
    {
        case DisplayId::display_page::RSC_LIST:
            dsp_obj = resource_view_mgr.get();
            break;
        case DisplayId::display_page::RSC_DETAIL:
            dsp_obj = resource_detail_mgr.get();
            break;
        case DisplayId::display_page::VLM_LIST:
            dsp_obj = volume_view_mgr.get();
            break;
        case DisplayId::display_page::VLM_DETAIL:
            dsp_obj = volume_detail_mgr.get();
            break;
        case DisplayId::display_page::CON_LIST:
            dsp_obj = connections_view_mgr.get();
            break;
        case DisplayId::display_page::CON_DETAIL:
            dsp_obj = connection_detail_mgr.get();
            break;
        case DisplayId::display_page::PEER_VLM_LIST:
            dsp_obj = peer_volume_view_mgr.get();
            break;
        case DisplayId::display_page::PEER_VLM_DETAIL:
            dsp_obj = peer_volume_detail_mgr.get();
            break;
        case DisplayId::display_page::MAIN_MENU:
            dsp_obj = main_menu_mgr.get();
            break;
        case DisplayId::display_page::TASKQ_ACT:
            dsp_obj = active_tasks_mgr.get();
            break;
        case DisplayId::display_page::TASKQ_SSP:
            dsp_obj = suspended_tasks_mgr.get();
            break;
        case DisplayId::display_page::TASKQ_PND:
            dsp_obj = pending_tasks_mgr.get();
            break;
        case DisplayId::display_page::TASKQ_FIN:
            dsp_obj = finished_tasks_mgr.get();
            break;
        case DisplayId::display_page::TASK_DETAIL:
            dsp_obj = task_detail_mgr.get();
            break;
        case DisplayId::display_page::HELP_IDX:
            dsp_obj = help_idx_mgr.get();
            break;
        case DisplayId::display_page::HELP_VIEWER:
            dsp_obj = help_view_mgr.get();
            break;
        case DisplayId::display_page::SYNC_LIST:
            break;
        case DisplayId::display_page::LOG_VIEWER:
            dsp_obj = log_view_mgr.get();
            break;
        case DisplayId::display_page::MSG_VIEWER:
            dsp_obj = msg_view_mgr.get();
            break;
        case DisplayId::display_page::RSC_ACTIONS:
            dsp_obj = resource_actions_mgr.get();
            break;
        case DisplayId::display_page::VLM_ACTIONS:
            dsp_obj = volume_actions_mgr.get();
            break;
        case DisplayId::display_page::CON_ACTIONS:
            dsp_obj = connection_actions_mgr.get();
            break;
        case DisplayId::display_page::PEER_VLM_ACTIONS:
            dsp_obj = peer_volume_actions_mgr.get();
            break;
        case DisplayId::display_page::PGM_INFO:
            dsp_obj = pgm_info_mgr.get();
            break;
        case DisplayId::display_page::CONFIGURATION:
            dsp_obj = config_mgr.get();
            break;
        default:
            break;
    }
}
