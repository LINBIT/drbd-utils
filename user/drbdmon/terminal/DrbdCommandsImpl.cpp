#include <terminal/DrbdCommandsImpl.h>

#include <terminal/DisplayId.h>
#include <terminal/DisplayConsts.h>
#include <subprocess/SubProcessQueue.h>
#include <subprocess/CmdLine.h>
#include <subprocess/DrbdCmdConsts.h>
#include <dsaext.h>
#include <integerparse.h>

const std::string   DrbdCommandsImpl::KEY_CMD_START("START");
const std::string   DrbdCommandsImpl::KEY_CMD_STOP("STOP");
const std::string   DrbdCommandsImpl::KEY_CMD_UP("UP");
const std::string   DrbdCommandsImpl::KEY_CMD_DOWN("DOWN");
const std::string   DrbdCommandsImpl::KEY_CMD_ADJUST("ADJUST");
const std::string   DrbdCommandsImpl::KEY_CMD_PRIMARY("PRIMARY");
const std::string   DrbdCommandsImpl::KEY_CMD_FORCE_PRIMARY("FORCE-PRIMARY");
const std::string   DrbdCommandsImpl::KEY_CMD_SECONDARY("SECONDARY");
const std::string   DrbdCommandsImpl::KEY_CMD_FORCE_SECONDARY("FORCE-SECONDARY");
const std::string   DrbdCommandsImpl::KEY_CMD_CONNECT("CONNECT");
const std::string   DrbdCommandsImpl::KEY_CMD_DISCONNECT("DISCONNECT");
const std::string   DrbdCommandsImpl::KEY_CMD_FORCE_DISCONNECT("FORCE-DISCONNECT");
const std::string   DrbdCommandsImpl::KEY_CMD_ATTACH("ATTACH");
const std::string   DrbdCommandsImpl::KEY_CMD_DETACH("DETACH");
const std::string   DrbdCommandsImpl::KEY_CMD_DISCARD_CONNECT("CONNECT-DISCARD");
const std::string   DrbdCommandsImpl::KEY_CMD_VERIFY("VERIFY");
const std::string   DrbdCommandsImpl::KEY_CMD_INVALIDATE("INVALIDATE");
const std::string   DrbdCommandsImpl::KEY_CMD_INVALIDATE_REMOTE("INVALIDATE-REMOTE");
const std::string   DrbdCommandsImpl::KEY_CMD_PAUSE_SYNC("PAUSE-SYNC");
const std::string   DrbdCommandsImpl::KEY_CMD_RESUME_SYNC("RESUME-SYNC");

const size_t        DrbdCommandsImpl::STRING_PREALLOC_LENGTH    = 450;

DrbdCommandsImpl::DrbdCommandsImpl(const ComponentsHub& comp_hub):
    dsp_comp_hub(comp_hub),
    entry_start(&DrbdCommandsImpl::KEY_CMD_START, &DrbdCommandsImpl::cmd_start),
    entry_stop(&DrbdCommandsImpl::KEY_CMD_STOP, &DrbdCommandsImpl::cmd_stop),
    entry_up(&DrbdCommandsImpl::KEY_CMD_UP, &DrbdCommandsImpl::cmd_start),
    entry_down(&DrbdCommandsImpl::KEY_CMD_DOWN, &DrbdCommandsImpl::cmd_stop),
    entry_adjust(&DrbdCommandsImpl::KEY_CMD_ADJUST, &DrbdCommandsImpl::cmd_adjust),
    entry_primary(&DrbdCommandsImpl::KEY_CMD_PRIMARY, &DrbdCommandsImpl::cmd_primary),
    entry_force_primary(&DrbdCommandsImpl::KEY_CMD_FORCE_PRIMARY, &DrbdCommandsImpl::cmd_force_primary),
    entry_secondary(&DrbdCommandsImpl::KEY_CMD_SECONDARY, &DrbdCommandsImpl::cmd_secondary),
    entry_force_secondary(&DrbdCommandsImpl::KEY_CMD_FORCE_SECONDARY, &DrbdCommandsImpl::cmd_force_secondary),
    entry_connect(&DrbdCommandsImpl::KEY_CMD_CONNECT, &DrbdCommandsImpl::cmd_connect),
    entry_disconnect(&DrbdCommandsImpl::KEY_CMD_DISCONNECT, &DrbdCommandsImpl::cmd_disconnect),
    entry_force_disconnect(&DrbdCommandsImpl::KEY_CMD_FORCE_DISCONNECT, &DrbdCommandsImpl::cmd_force_disconnect),
    entry_attach(&DrbdCommandsImpl::KEY_CMD_ATTACH, &DrbdCommandsImpl::cmd_attach),
    entry_detach(&DrbdCommandsImpl::KEY_CMD_DETACH, &DrbdCommandsImpl::cmd_detach),
    entry_discard_connect(&DrbdCommandsImpl::KEY_CMD_DISCARD_CONNECT, &DrbdCommandsImpl::cmd_discard_connect),
    entry_verify(&DrbdCommandsImpl::KEY_CMD_VERIFY, &DrbdCommandsImpl::cmd_verify),
    entry_invalidate(&DrbdCommandsImpl::KEY_CMD_INVALIDATE, &DrbdCommandsImpl::cmd_invalidate),
    entry_invalidate_remote(&DrbdCommandsImpl::KEY_CMD_INVALIDATE_REMOTE, &DrbdCommandsImpl::cmd_invalidate_remote),
    entry_pause_sync(&DrbdCommandsImpl::KEY_CMD_PAUSE_SYNC, &DrbdCommandsImpl::cmd_pause_sync),
    entry_resume_sync(&DrbdCommandsImpl::KEY_CMD_RESUME_SYNC, &DrbdCommandsImpl::cmd_resume_sync)
{
    add_command(entry_start);
    add_command(entry_stop);
    add_command(entry_up);
    add_command(entry_down);
    add_command(entry_adjust);
    add_command(entry_primary);
    add_command(entry_force_primary);
    add_command(entry_secondary);
    add_command(entry_force_secondary);
    add_command(entry_connect);
    add_command(entry_disconnect);
    add_command(entry_force_disconnect);
    add_command(entry_attach);
    add_command(entry_detach);
    add_command(entry_discard_connect);
    add_command(entry_verify);
    add_command(entry_invalidate);
    add_command(entry_invalidate_remote);
    add_command(entry_pause_sync);
    add_command(entry_resume_sync);
}

DrbdCommandsImpl::~DrbdCommandsImpl() noexcept
{
}

bool DrbdCommandsImpl::execute_command(const std::string& command, StringTokenizer& tokenizer)
{
    bool processed = false;
    if (dsp_comp_hub.enable_drbd_actions)
    {
        Entry* const cmd_entry = cmd_map->get(&command);
        if (cmd_entry != nullptr)
        {
            cmd_func_type cmd_func = cmd_entry->cmd_func;
            try
            {
                dsp_comp_hub.dsp_selector->synchronize_displays();
                processed = (this->*cmd_func)(command, tokenizer);
            }
            catch (SubProcessQueue::QueueCapacityException&)
            {
                dsp_comp_hub.log->add_entry(
                    MessageLog::log_level::ALERT,
                    "Cannot execute command, insufficient queue capacity"
                );
            }
            catch (SubProcess::Exception&)
            {
                dsp_comp_hub.log->add_entry(
                    MessageLog::log_level::ALERT,
                    "Command failed: Sub-process execution error"
                );
            }
        }
    }
    else
    {
        const uint64_t msg_id = dsp_comp_hub.log->add_entry(
            MessageLog::log_level::WARN,
            "DRBD commands are currently disabled"
        );
        dsp_comp_hub.dsp_shared->message_id = msg_id;
        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::MSG_VIEWER);
    }
    return processed;
}

bool DrbdCommandsImpl::complete_command(const std::string& prefix, std::string& completion)
{
    return CommandsBase<DrbdCommandsImpl>::complete_command(prefix, completion);
}

bool DrbdCommandsImpl::exec_for_resources(
    const std::string&  command,
    StringTokenizer&    tokenizer,
    exec_rsc_type       exec_func
)
{
    bool cmd_valid = false;
    if (tokenizer.has_next())
    {
        const std::string obj_arg = tokenizer.next();
        std::string rsc_name;
        get_resource_name(obj_arg, rsc_name);

        if (!rsc_name.empty())
        {
            cmd_valid = true;

            (this->*exec_func)(rsc_name);
        }
    }
    else
    if (can_run_resource_cmd())
    {
        DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
        if (!dsp_comp_hub.dsp_shared->ovrd_resource_selection &&
            (active_page == DisplayId::display_page::RSC_LIST ||
            active_page == DisplayId::display_page::RSC_ACTIONS) &&
            dsp_comp_hub.dsp_shared->have_resources_selection())
        {
            dsp_comp_hub.dsp_common->application_working();

            cmd_valid = true;

            ResourcesMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_resources_map();
            ResourcesMap::KeysIterator rsc_iter(selection_map);

            while (rsc_iter.has_next())
            {
                const std::string& cur_rsc_name = *(rsc_iter.next());
                (this->*exec_func)(cur_rsc_name);
            }
        }
        else
        if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty())
        {
            cmd_valid = true;

            (this->*exec_func)(dsp_comp_hub.dsp_shared->monitor_rsc);
        }
    }
    return cmd_valid;
}

bool DrbdCommandsImpl::exec_for_connections(
    const std::string&  command,
    StringTokenizer&    tokenizer,
    exec_con_type       exec_func
)
{
    bool cmd_valid = false;
    if (tokenizer.has_next())
    {
        const std::string obj_arg = tokenizer.next();
        std::string rsc_name;
        std::string con_name;
        get_resource_name(obj_arg, rsc_name);
        get_connection_name(obj_arg, con_name);

        if (!rsc_name.empty())
        {
            cmd_valid = true;

            (this->*exec_func)(rsc_name, con_name);
        }
    }
    else
    if (can_run_connection_cmd())
    {
        DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
        if ((active_page == DisplayId::display_page::RSC_LIST ||
            active_page == DisplayId::display_page::RSC_ACTIONS) &&
            dsp_comp_hub.dsp_shared->have_resources_selection())
        {
            // On resource list or resource actions with a selection of multiple resources,
            // operates on all connections or all selected resources, overrides any connection selection

            dsp_comp_hub.dsp_common->application_working();

            cmd_valid = true;

            ResourcesMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_resources_map();
            ResourcesMap::KeysIterator rsc_iter(selection_map);

            std::string empty_con_name;
            while (rsc_iter.has_next())
            {
                std::string& cur_rsc_name = *(rsc_iter.next());
                (this->*exec_func)(cur_rsc_name, empty_con_name);
            }
        }
        else
        {
            const std::string& rsc_name = dsp_comp_hub.dsp_shared->monitor_rsc;
            const std::string& con_name = dsp_comp_hub.dsp_shared->monitor_con;
            if (!rsc_name.empty())
            {
                // Have at least a resource name, command is always valid
                cmd_valid = true;

                if (!dsp_comp_hub.dsp_shared->ovrd_connection_selection &&
                    (active_page == DisplayId::display_page::CON_LIST ||
                    active_page == DisplayId::display_page::CON_ACTIONS) &&
                    dsp_comp_hub.dsp_shared->have_connections_selection())
                {
                    // On connection list or connection details with a selection of multiple connections

                    dsp_comp_hub.dsp_common->application_working();

                    ConnectionsMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_connections_map();
                    ConnectionsMap::KeysIterator con_iter(selection_map);

                    while (con_iter.has_next())
                    {
                        const std::string& cur_con_name = *(con_iter.next());
                        (this->*exec_func)(rsc_name, cur_con_name);
                    }
                }
                else
                {
                    if (active_page == DisplayId::display_page::CON_LIST ||
                        active_page == DisplayId::display_page::CON_DETAIL ||
                        active_page == DisplayId::display_page::CON_ACTIONS ||
                        active_page == DisplayId::display_page::PEER_VLM_LIST ||
                        active_page == DisplayId::display_page::PEER_VLM_DETAIL)
                    {
                        // On connection list or connection actions without a selection,
                        // or operating from a page that selects a single connection.
                        // Operates on all connections of a resource or on a single selected connection

                        (this->*exec_func)(rsc_name, con_name);
                    }
                    else
                    {
                        // Operates on all connections of a single resource

                        std::string empty_con_name;
                        (this->*exec_func)(rsc_name, empty_con_name);
                    }
                }
            }
        }
    }
    return cmd_valid;
}

bool DrbdCommandsImpl::exec_for_volumes(
    const std::string&  command,
    StringTokenizer&    tokenizer,
    exec_vlm_type       exec_func
)
{
    bool cmd_valid = false;
    if (tokenizer.has_next())
    {
        const std::string obj_arg = tokenizer.next();
        std::string rsc_name;
        uint16_t vlm_nr = DisplayConsts::VLM_NONE;
        get_resource_name(obj_arg, rsc_name);
        get_volume_number(obj_arg, vlm_nr);

        if (!rsc_name.empty() && vlm_nr != DisplayConsts::VLM_NONE)
        {
            cmd_valid = true;

            (this->*exec_func)(rsc_name, vlm_nr);
        }
    }
    else
    if (can_run_volume_cmd())
    {
        const std::string& rsc_name = dsp_comp_hub.dsp_shared->monitor_rsc;
        const uint16_t vlm_nr = dsp_comp_hub.dsp_shared->monitor_vlm;
        if (!rsc_name.empty())
        {
            DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
            if (!dsp_comp_hub.dsp_shared->ovrd_volume_selection &&
                (active_page == DisplayId::display_page::VLM_LIST ||
                active_page == DisplayId::display_page::VLM_ACTIONS) &&
                dsp_comp_hub.dsp_shared->have_volumes_selection())
            {
                dsp_comp_hub.dsp_common->application_working();

                cmd_valid = true;

                VolumesMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_volumes_map();
                VolumesMap::KeysIterator vlm_iter(selection_map);

                while (vlm_iter.has_next())
                {
                    const uint16_t cur_vlm_nr = *(vlm_iter.next());
                    (this->*exec_func)(rsc_name, cur_vlm_nr);
                }
            }
            else
            if (vlm_nr != DisplayConsts::VLM_NONE)
            {
                cmd_valid = true;

                if (active_page == DisplayId::display_page::VLM_LIST ||
                    active_page == DisplayId::display_page::VLM_ACTIONS ||
                    active_page == DisplayId::display_page::VLM_DETAIL)
                {
                    (this->*exec_func)(rsc_name, vlm_nr);
                }
                // else no-op; volume commands are not run for all volumes of a resource
            }
        }
    }
    return cmd_valid;
}

bool DrbdCommandsImpl::exec_for_peer_volumes(
    const std::string&  command,
    StringTokenizer&    tokenizer,
    exec_peer_vlm_type  exec_func
)
{
    bool cmd_valid = false;
    if (tokenizer.has_next())
    {
        const std::string obj_arg = tokenizer.next();
        std::string rsc_name;
        std::string con_name;
        uint16_t vlm_nr = DisplayConsts::VLM_NONE;

        get_resource_name(obj_arg, rsc_name);
        get_connection_name(obj_arg, con_name);
        get_volume_number(obj_arg, vlm_nr);

        if (!rsc_name.empty() && !con_name.empty() && vlm_nr != DisplayConsts::VLM_NONE)
        {
            cmd_valid = true;

            (this->*exec_func)(rsc_name, con_name, vlm_nr);
        }
    }
    else
    if (can_run_peer_volume_cmd())
    {
        const std::string& rsc_name = dsp_comp_hub.dsp_shared->monitor_rsc;
        const std::string& con_name = dsp_comp_hub.dsp_shared->monitor_con;
        const uint16_t vlm_nr = dsp_comp_hub.dsp_shared->monitor_peer_vlm;

        DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
        if (!rsc_name.empty() && !con_name.empty())
        {
            if (!dsp_comp_hub.dsp_shared->ovrd_peer_volume_selection &&
                (active_page == DisplayId::display_page::PEER_VLM_LIST ||
                active_page == DisplayId::display_page::PEER_VLM_ACTIONS) &&
                dsp_comp_hub.dsp_shared->have_peer_volumes_selection())
            {
                dsp_comp_hub.dsp_common->application_working();

                cmd_valid = true;

                VolumesMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_peer_volumes_map();
                VolumesMap::KeysIterator vlm_iter(selection_map);

                while (vlm_iter.has_next())
                {
                    const uint16_t cur_vlm_nr = *(vlm_iter.next());
                    (this->*exec_func)(rsc_name, con_name, cur_vlm_nr);
                }
            }
            else
            if (vlm_nr != DisplayConsts::VLM_NONE)
            {
                cmd_valid = true;

                if (active_page == DisplayId::display_page::PEER_VLM_LIST ||
                    active_page == DisplayId::display_page::PEER_VLM_ACTIONS ||
                    active_page == DisplayId::display_page::PEER_VLM_DETAIL)
                {
                    (this->*exec_func)(rsc_name, con_name, vlm_nr);
                }
                // else no-op; peer volume commands for all peer volumes from a resource/connection page not allowed
            }
        }
    }
    return cmd_valid;
}

bool DrbdCommandsImpl::cmd_start(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_resources(command, tokenizer, &DrbdCommandsImpl::exec_start);
}

bool DrbdCommandsImpl::cmd_stop(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_resources(command, tokenizer, &DrbdCommandsImpl::exec_stop);
}

bool DrbdCommandsImpl::cmd_adjust(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_resources(command, tokenizer, &DrbdCommandsImpl::exec_adjust);
}

bool DrbdCommandsImpl::cmd_primary(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_resources(command, tokenizer, &DrbdCommandsImpl::exec_primary);
}

bool DrbdCommandsImpl::cmd_force_primary(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_resources(command, tokenizer, &DrbdCommandsImpl::exec_force_primary);
}

bool DrbdCommandsImpl::cmd_secondary(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_resources(command, tokenizer, &DrbdCommandsImpl::exec_secondary);
}

bool DrbdCommandsImpl::cmd_force_secondary(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_resources(command, tokenizer, &DrbdCommandsImpl::exec_force_secondary);
}

bool DrbdCommandsImpl::cmd_connect(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_connections(command, tokenizer, &DrbdCommandsImpl::exec_connect);
}

bool DrbdCommandsImpl::cmd_disconnect(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_connections(command, tokenizer, &DrbdCommandsImpl::exec_disconnect);
}

bool DrbdCommandsImpl::cmd_force_disconnect(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_connections(command, tokenizer, &DrbdCommandsImpl::exec_force_disconnect);
}

bool DrbdCommandsImpl::cmd_attach(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_volumes(command, tokenizer, &DrbdCommandsImpl::exec_attach);
}

bool DrbdCommandsImpl::cmd_detach(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_volumes(command, tokenizer, &DrbdCommandsImpl::exec_detach);
}

bool DrbdCommandsImpl::cmd_discard_connect(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_connections(command, tokenizer, &DrbdCommandsImpl::exec_discard_connect);
}

bool DrbdCommandsImpl::cmd_verify(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_peer_volumes(command, tokenizer, &DrbdCommandsImpl::exec_verify);
}

bool DrbdCommandsImpl::cmd_invalidate(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_volumes(command, tokenizer, &DrbdCommandsImpl::exec_invalidate);
}

bool DrbdCommandsImpl::cmd_invalidate_remote(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_peer_volumes(command, tokenizer, &DrbdCommandsImpl::exec_invalidate_remote);
}

bool DrbdCommandsImpl::cmd_pause_sync(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_peer_volumes(command, tokenizer, &DrbdCommandsImpl::exec_pause_sync);
}

bool DrbdCommandsImpl::cmd_resume_sync(const std::string& command, StringTokenizer& tokenizer)
{
    return exec_for_peer_volumes(command, tokenizer, &DrbdCommandsImpl::exec_resume_sync);
}

void DrbdCommandsImpl::exec_start(const std::string& rsc_name)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    description.append("Start resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->set_description(description);
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_START);
    command->add_argument(rsc_name);

    queue_command(command);
}

void DrbdCommandsImpl::exec_stop(const std::string& rsc_name)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    description.append("Stop resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->set_description(description);
    command->add_argument(drbdcmd::DRBDSETUP_CMD);
    command->add_argument(drbdcmd::ARG_STOP);
    command->add_argument(rsc_name);

    queue_command(command);
}

void DrbdCommandsImpl::exec_adjust(const std::string& rsc_name)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    description.append("Adjust resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->set_description(description);
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_ADJUST);
    command->add_argument(rsc_name);

    queue_command(command);
}

void DrbdCommandsImpl::exec_primary(const std::string& rsc_name)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    description.append("Make primary: Resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->set_description(description);
    command->add_argument(drbdcmd::DRBDSETUP_CMD);
    command->add_argument(drbdcmd::ARG_PRIMARY);
    command->add_argument(rsc_name);

    queue_command(command);
}

void DrbdCommandsImpl::exec_force_primary(const std::string& rsc_name)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    description.append("Force make primary: Resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->set_description(description);
    command->add_argument(drbdcmd::DRBDSETUP_CMD);
    command->add_argument(drbdcmd::ARG_PRIMARY);
    command->add_argument(drbdcmd::ARG_FORCE);
    command->add_argument(rsc_name);

    queue_command(command);
}

void DrbdCommandsImpl::exec_secondary(const std::string& rsc_name)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    description.append("Make secondary: Resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->set_description(description);
    command->add_argument(drbdcmd::DRBDSETUP_CMD);
    command->add_argument(drbdcmd::ARG_SECONDARY);
    command->add_argument(rsc_name);

    queue_command(command);
}

void DrbdCommandsImpl::exec_force_secondary(const std::string& rsc_name)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    description.append("Force secondary: Resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->set_description(description);
    command->add_argument(drbdcmd::DRBDSETUP_CMD);
    command->add_argument(drbdcmd::ARG_SECONDARY);
    command->add_argument(drbdcmd::ARG_FORCE);
    command->add_argument(rsc_name);

    queue_command(command);
}

void DrbdCommandsImpl::exec_connect(const std::string& rsc_name, const std::string& con_name)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    description.append("Connect resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_CONNECT);
    if (con_name.empty())
    {
        command->add_argument(rsc_name);
    }
    else
    {
        description.append(", connection ");
        description.append(con_name);

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        command->add_argument(target_obj);
    }
    command->set_description(description);

    queue_command(command);
}

void DrbdCommandsImpl::exec_disconnect(const std::string& rsc_name, const std::string& con_name)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    description.append("Disconnect resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_DISCONNECT);
    if (con_name.empty())
    {
        command->add_argument(rsc_name);
    }
    else
    {
        description.append(", connection ");
        description.append(con_name);

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        command->add_argument(target_obj);
    }
    command->set_description(description);

    queue_command(command);
}

void DrbdCommandsImpl::exec_force_disconnect(const std::string& rsc_name, const std::string& con_name)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    description.append("Force disconnect resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_DISCONNECT);
    command->add_argument(drbdcmd::ARG_FORCE);
    if (con_name.empty())
    {
        command->add_argument(rsc_name);
    }
    else
    {
        description.append(", connection ");
        description.append(con_name);

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        command->add_argument(target_obj);
    }
    command->set_description(description);

    queue_command(command);
}

void DrbdCommandsImpl::exec_attach(const std::string& rsc_name, const uint16_t vlm_nr)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    std::string vlm_nr_str(std::to_string(static_cast<unsigned int> (vlm_nr)));

    description.append("Attach resource ");
    description.append(rsc_name);
    description.append(", volume ");
    description.append(vlm_nr_str);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_ATTACH);

    std::string target_obj;
    target_obj.reserve(STRING_PREALLOC_LENGTH);

    target_obj.append(rsc_name);
    target_obj.append("/");
    target_obj.append(vlm_nr_str);
    command->add_argument(target_obj);

    command->set_description(description);

    queue_command(command);
}

void DrbdCommandsImpl::exec_detach(const std::string& rsc_name, const uint16_t vlm_nr)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    std::string vlm_nr_str(std::to_string(static_cast<unsigned int> (vlm_nr)));

    description.append("Detach resource ");
    description.append(rsc_name);
    description.append(", volume ");
    description.append(vlm_nr_str);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_DETACH);

    std::string target_obj;
    target_obj.reserve(STRING_PREALLOC_LENGTH);

    target_obj.append(rsc_name);
    target_obj.append("/");
    target_obj.append(vlm_nr_str);
    command->add_argument(target_obj);

    command->set_description(description);

    queue_command(command);
}

void DrbdCommandsImpl::exec_discard_connect(const std::string& rsc_name, const std::string& con_name)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    description.append("Discard data & connect resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_CONNECT);
    command->add_argument(drbdcmd::ARG_DISCARD);
    if (con_name.empty())
    {
        command->add_argument(rsc_name);
    }
    else
    {
        description.append(", connection ");
        description.append(con_name);

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        command->add_argument(target_obj);
    }
    command->set_description(description);

    queue_command(command);
}

void DrbdCommandsImpl::exec_verify(const std::string& rsc_name, const std::string& con_name, const uint16_t vlm_nr)
{
    // TODO: Check whether this command actually works like that
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    std::string vlm_nr_str(std::to_string(static_cast<unsigned int> (vlm_nr)));

    description.append("Verify resource ");
    description.append(rsc_name);
    description.append(", connection ");
    description.append(con_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_VERIFY);

    if (vlm_nr != DisplayConsts::VLM_NONE)
    {
        description.append(", volume ");
        description.append(vlm_nr_str);

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        target_obj.append("/");
        target_obj.append(vlm_nr_str);
        command->add_argument(target_obj);
    }
    else
    {
        description.append(", all volumes");

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        command->add_argument(target_obj);
    }
    command->set_description(description);

    queue_command(command);
}

void DrbdCommandsImpl::exec_invalidate(const std::string& rsc_name, const uint16_t vlm_nr)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    std::string vlm_nr_str(std::to_string(static_cast<unsigned int> (vlm_nr)));

    description.append("Invalidate resource ");
    description.append(rsc_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_INVALIDATE);

    if (vlm_nr != DisplayConsts::VLM_NONE)
    {
        description.append(", volume ");
        description.append(vlm_nr_str);

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append("/");
        target_obj.append(vlm_nr_str);
        command->add_argument(target_obj);
    }
    else
    {
        description.append(", all volumes");
        command->add_argument(rsc_name);
    }
    command->set_description(description);

    queue_command(command);
}

void DrbdCommandsImpl::exec_invalidate_remote(
    const std::string& rsc_name,
    const std::string& con_name,
    const uint16_t vlm_nr
)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    std::string vlm_nr_str(std::to_string(static_cast<unsigned int> (vlm_nr)));

    description.append("Invalidate remote resource ");
    description.append(rsc_name);
    description.append(", connection ");
    description.append(con_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_INVALIDATE_REMOTE);

    if (vlm_nr != DisplayConsts::VLM_NONE)
    {
        description.append(", peer volume ");
        description.append(vlm_nr_str);

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        target_obj.append("/");
        target_obj.append(vlm_nr_str);
        command->add_argument(target_obj);
    }
    else
    {
        description.append(", all volumes");

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        command->add_argument(target_obj);
    }
    command->set_description(description);

    queue_command(command);
}

void DrbdCommandsImpl::exec_pause_sync(
    const std::string& rsc_name,
    const std::string& con_name,
    const uint16_t vlm_nr
)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    std::string vlm_nr_str(std::to_string(static_cast<unsigned int> (vlm_nr)));

    description.append("Pause resynchronization, resource ");
    description.append(rsc_name);
    description.append(", connection ");
    description.append(con_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_PAUSE_SYNC);

    if (vlm_nr != DisplayConsts::VLM_NONE)
    {
        description.append(", peer volume ");
        description.append(vlm_nr_str);

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        target_obj.append("/");
        target_obj.append(vlm_nr_str);
        command->add_argument(target_obj);
    }
    else
    {
        description.append(", all volumes");

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        command->add_argument(target_obj);
    }
    command->set_description(description);

    queue_command(command);
}

void DrbdCommandsImpl::exec_resume_sync(
    const std::string& rsc_name,
    const std::string& con_name,
    const uint16_t vlm_nr
)
{
    std::string description;
    description.reserve(STRING_PREALLOC_LENGTH);

    std::string vlm_nr_str(std::to_string(static_cast<unsigned int> (vlm_nr)));

    description.append("Resume resynchronization, resource ");
    description.append(rsc_name);
    description.append(", connection ");
    description.append(con_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_RESUME_SYNC);

    if (vlm_nr != DisplayConsts::VLM_NONE)
    {
        description.append(", peer volume ");
        description.append(vlm_nr_str);

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        target_obj.append("/");
        target_obj.append(vlm_nr_str);
        command->add_argument(target_obj);
    }
    else
    {
        description.append(", all volumes");

        std::string target_obj;
        target_obj.reserve(STRING_PREALLOC_LENGTH);

        target_obj.append(rsc_name);
        target_obj.append(":");
        target_obj.append(con_name);
        command->add_argument(target_obj);
    }
    command->set_description(description);

    queue_command(command);
}

void DrbdCommandsImpl::get_resource_name(const std::string& argument, std::string& rsc_name)
{
    rsc_name.clear();
    const size_t vlm_split_idx = argument.find("/");
    const size_t con_split_idx = argument.find(":");

    if (vlm_split_idx != std::string::npos || con_split_idx != std::string::npos)
    {
        size_t split_idx = argument.length();
        if (vlm_split_idx != std::string::npos && con_split_idx != std::string::npos)
        {
            split_idx = std::min(vlm_split_idx, con_split_idx);
        }
        else
        if (vlm_split_idx != std::string::npos)
        {
            split_idx = vlm_split_idx;
        }
        else
        {
            split_idx = con_split_idx;
        }
        rsc_name.append(argument, 0, split_idx);
    }
    else
    {
        rsc_name = argument;
    }
}

void DrbdCommandsImpl::get_connection_name(const std::string& argument, std::string& con_name)
{
    con_name.clear();
    const size_t con_split_idx = argument.find(":");
    if (con_split_idx != std::string::npos)
    {
        const size_t vlm_split_idx = argument.find("/");
        if (vlm_split_idx != std::string::npos && vlm_split_idx > con_split_idx)
        {
            con_name.append(argument, con_split_idx + 1, vlm_split_idx - con_split_idx - 1);
        }
        else
        {
            con_name.append(argument, con_split_idx + 1, argument.length() - con_split_idx - 1);
        }
    }
}

void DrbdCommandsImpl::get_volume_number(const std::string& argument, uint16_t& vlm_nr)
{
    vlm_nr = DisplayConsts::VLM_NONE;
    const size_t vlm_split_idx = argument.find("/");
    if (vlm_split_idx != std::string::npos)
    {
        std::string vlm_nr_str;
        const size_t con_split_idx = argument.find(":");
        if (con_split_idx != std::string::npos && con_split_idx > vlm_split_idx)
        {
            vlm_nr_str.append(argument, vlm_split_idx + 1, con_split_idx - vlm_split_idx - 1);
        }
        else
        {
            vlm_nr_str.append(argument, vlm_split_idx + 1, argument.length() - vlm_split_idx - 1);
        }

        try
        {
            vlm_nr = dsaext::parse_unsigned_int16(vlm_nr_str);
        }
        catch (dsaext::NumberFormatException&)
        {
            // Volume number unparsable
        }
    }
}

bool DrbdCommandsImpl::can_run_resource_cmd()
{
    DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
    return (active_page == DisplayId::display_page::RSC_LIST ||
            active_page == DisplayId::display_page::CON_LIST ||
            active_page == DisplayId::display_page::VLM_LIST ||
            active_page == DisplayId::display_page::PEER_VLM_LIST ||
            active_page == DisplayId::display_page::RSC_DETAIL ||
            active_page == DisplayId::display_page::CON_DETAIL ||
            active_page == DisplayId::display_page::VLM_DETAIL ||
            active_page == DisplayId::display_page::PEER_VLM_DETAIL ||
            active_page == DisplayId::display_page::RSC_ACTIONS ||
            active_page == DisplayId::display_page::CON_ACTIONS ||
            active_page == DisplayId::display_page::VLM_ACTIONS ||
            active_page == DisplayId::display_page::PEER_VLM_ACTIONS);
}

bool DrbdCommandsImpl::can_run_connection_cmd()
{
    DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
    return (active_page == DisplayId::display_page::RSC_LIST ||
            active_page == DisplayId::display_page::CON_LIST ||
            active_page == DisplayId::display_page::PEER_VLM_LIST ||
            active_page == DisplayId::display_page::RSC_DETAIL ||
            active_page == DisplayId::display_page::CON_DETAIL ||
            active_page == DisplayId::display_page::PEER_VLM_DETAIL ||
            active_page == DisplayId::display_page::RSC_ACTIONS ||
            active_page == DisplayId::display_page::CON_ACTIONS ||
            active_page == DisplayId::display_page::PEER_VLM_ACTIONS);
}

bool DrbdCommandsImpl::can_run_volume_cmd()
{
    DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
    return (active_page == DisplayId::display_page::VLM_LIST ||
            active_page == DisplayId::display_page::VLM_DETAIL ||
            active_page == DisplayId::display_page::VLM_ACTIONS);
}

bool DrbdCommandsImpl::can_run_peer_volume_cmd()
{
    DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
    return (active_page == DisplayId::display_page::PEER_VLM_LIST ||
            active_page == DisplayId::display_page::PEER_VLM_DETAIL ||
            active_page == DisplayId::display_page::PEER_VLM_ACTIONS);
}

void DrbdCommandsImpl::queue_command(std::unique_ptr<CmdLine>& command)
{
    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}
