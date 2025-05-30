#include <terminal/ModularDisplay.h>
#include <terminal/GlobalCommandsImpl.h>
#include <terminal/GlobalCommandConsts.h>
#include <terminal/DisplayConsts.h>
#include <terminal/ColorTable.h>
#include <terminal/CharacterTable.h>
#include <terminal/DisplayId.h>
#include <objects/DrbdResource.h>
#include <objects/DrbdVolume.h>
#include <objects/VolumesContainer.h>
#include <integerparse.h>
#include <dsaext.h>
#include <string_transformations.h>
#include <mutex>

GlobalCommandsImpl::GlobalCommandsImpl(ComponentsHub& comp_hub, Configuration& config_ref):
    dsp_comp_hub(comp_hub),
    config(&config_ref),
    entry_exit(&cmd_names::KEY_CMD_EXIT, &GlobalCommandsImpl::cmd_exit),
    entry_display(&cmd_names::KEY_CMD_DISPLAY, &GlobalCommandsImpl::cmd_display),
    entry_page(&cmd_names::KEY_CMD_PAGE, &GlobalCommandsImpl::local_command),
    entry_refresh(&cmd_names::KEY_CMD_REFRESH, &GlobalCommandsImpl::cmd_refresh),
    entry_colors(&cmd_names::KEY_CMD_COLORS, &GlobalCommandsImpl::cmd_colors),
    entry_charset(&cmd_names::KEY_CMD_CHARSET, &GlobalCommandsImpl::cmd_charset),
    entry_select(&cmd_names::KEY_CMD_SELECT, &GlobalCommandsImpl::local_command),
    entry_deselect(&cmd_names::KEY_CMD_DESELECT, &GlobalCommandsImpl::local_command),
    entry_select_all(&cmd_names::KEY_CMD_SELECT_ALL, &GlobalCommandsImpl::cmd_select_all),
    entry_deselect_all(&cmd_names::KEY_CMD_DESELECT_ALL, &GlobalCommandsImpl::cmd_deselect_all),
    entry_clear_selection(&cmd_names::KEY_CMD_CLEAR_SELECTION, &GlobalCommandsImpl::cmd_deselect_all),
    entry_cursor(&cmd_names::KEY_CMD_CURSOR, &GlobalCommandsImpl::local_command),
    entry_resource(&cmd_names::KEY_CMD_RESOURCE, &GlobalCommandsImpl::cmd_resource),
    entry_connection(&cmd_names::KEY_CMD_CONNECTION, &GlobalCommandsImpl::cmd_connection),
    entry_volume(&cmd_names::KEY_CMD_VOLUME, &GlobalCommandsImpl::cmd_volume),
    entry_minor_nr(&cmd_names::KEY_CMD_MINOR_NR, &GlobalCommandsImpl::cmd_minor_nr),
    entry_close(&cmd_names::KEY_CMD_CLOSE, &GlobalCommandsImpl::cmd_close)
{
    add_command(entry_exit);
    add_command(entry_display);
    add_command(entry_page);
    add_command(entry_refresh);
    add_command(entry_colors);
    add_command(entry_charset);
    add_command(entry_select);
    add_command(entry_deselect);
    add_command(entry_select_all);
    add_command(entry_deselect_all);
    add_command(entry_clear_selection);
    add_command(entry_cursor);
    add_command(entry_resource);
    add_command(entry_connection);
    add_command(entry_volume);
    add_command(entry_minor_nr);
    add_command(entry_close);
}

GlobalCommandsImpl::~GlobalCommandsImpl() noexcept
{
}

bool GlobalCommandsImpl::execute_command(const std::string& command, StringTokenizer& tokenizer)
{
    bool processed = false;
    Entry* const cmd_entry = cmd_map->get(&command);
    if (cmd_entry != nullptr)
    {
        cmd_func_type cmd_func = cmd_entry->cmd_func;
        processed = (this->*cmd_func)(command, tokenizer);
    }
    return processed;
}

bool GlobalCommandsImpl::complete_command(const std::string& prefix, std::string& completion)
{
    return CommandsBase<GlobalCommandsImpl>::complete_command(prefix, completion);
}

bool GlobalCommandsImpl::cmd_exit(const std::string& command, StringTokenizer& tokenizer)
{
    dsp_comp_hub.core_instance->shutdown(DrbdMonCore::finish_action::TERMINATE);
    return true;
}

bool GlobalCommandsImpl::cmd_display(const std::string& command, StringTokenizer& tokenizer)
{
    bool cmd_valid = false;
    if (tokenizer.has_next())
    {
        std::string cmd_arg = tokenizer.next();
        std::string upper_display_name = string_transformations::uppercase_copy_of(cmd_arg);
        cmd_valid = dsp_comp_hub.dsp_selector->switch_to_display(upper_display_name);
    }
    return cmd_valid;
}

bool GlobalCommandsImpl::cmd_refresh(const std::string& command, StringTokenizer& tokenizer)
{
    // Command does not take any arguments
    bool cmd_valid = !tokenizer.has_next();
    dsp_comp_hub.dsp_selector->refresh_display();
    return cmd_valid;
}

bool GlobalCommandsImpl::cmd_colors(const std::string& command, StringTokenizer& tokenizer)
{
    bool cmd_valid = false;
    if (tokenizer.has_next())
    {
        const std::string cmd_arg = tokenizer.next();
        const std::string upper_color_style = string_transformations::uppercase_copy_of(cmd_arg);

        DisplayStyleCollection::ColorStyle style_key = DisplayStyleCollection::ColorStyle::DEFAULT;
        if (upper_color_style == "DARK256")
        {
            style_key = DisplayStyleCollection::ColorStyle::DARK_BG_256_COLORS;
            cmd_valid = true;
        }
        else
        if (upper_color_style == "LIGHT256")
        {
            style_key = DisplayStyleCollection::ColorStyle::LIGHT_BG_256_COLORS;
            cmd_valid = true;
        }
        else
        if (upper_color_style == "DARK16")
        {
            style_key = DisplayStyleCollection::ColorStyle::DARK_BG_16_COLORS;
            cmd_valid = true;
        }
        else
        if (upper_color_style == "LIGHT16")
        {
            style_key = DisplayStyleCollection::ColorStyle::LIGHT_BG_16_COLORS;
            cmd_valid = true;
        }
        if (upper_color_style == "DEFAULT")
        {
            cmd_valid = true;
        }

        if (cmd_valid)
        {
            const ColorTable& new_color_table = dsp_comp_hub.style_coll->get_color_table(style_key);
            dsp_comp_hub.active_color_table = &new_color_table;
            config->color_scheme = static_cast<uint16_t> (style_key);
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
    return cmd_valid;
}

bool GlobalCommandsImpl::cmd_charset(const std::string& command, StringTokenizer& tokenizer)
{
    bool cmd_valid = false;
    if (tokenizer.has_next())
    {
        const std::string cmd_arg = tokenizer.next();
        const std::string upper_char_style = string_transformations::uppercase_copy_of(cmd_arg);

        DisplayStyleCollection::CharacterStyle style_key = DisplayStyleCollection::CharacterStyle::DEFAULT;
        if (upper_char_style == "UNICODE" || upper_char_style == "UTF-8")
        {
            style_key = DisplayStyleCollection::CharacterStyle::UNICODE;
            cmd_valid = true;
        }
        else
        if (upper_char_style == "ASCII")
        {
            style_key = DisplayStyleCollection::CharacterStyle::ASCII;
            cmd_valid = true;
        }
        else
        if (upper_char_style == "DEFAULT")
        {
            cmd_valid = true;
        }

        if (cmd_valid)
        {
            const CharacterTable& new_char_table = dsp_comp_hub.style_coll->get_character_table(style_key);
            dsp_comp_hub.active_character_table = &new_char_table;
            config->character_set = static_cast<uint16_t> (style_key);
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
    return cmd_valid;
}

bool GlobalCommandsImpl::cmd_select_all(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = false;
    DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
    if (active_page == DisplayId::display_page::LOG_VIEWER)
    {
        MessageLog* const log = active_page == DisplayId::display_page::LOG_VIEWER ?
            dsp_comp_hub.log : dsp_comp_hub.debug_log;
        MessageMap& selection = active_page == DisplayId::display_page::LOG_VIEWER ?
            *(dsp_comp_hub.dsp_shared->selected_log_entries) :
            *(dsp_comp_hub.dsp_shared->selected_debug_log_entries);
        dsp_comp_hub.dsp_common->application_working();
        {
            std::unique_lock<std::recursive_mutex> lock(log->queue_lock);

            MessageLog::Queue::ValuesIterator entry_iter = log->iterator();
            while (entry_iter.has_next())
            {
                MessageLog::Entry* const entry = entry_iter.next();
                const uint64_t entry_id = entry->get_id();
                dsp_comp_hub.dsp_shared->select_log_entry(selection, entry_id);
            }
        }
        dsp_comp_hub.dsp_selector->refresh_display();
        accepted = true;
    }
    return accepted;
}

bool GlobalCommandsImpl::cmd_deselect_all(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = false;
    DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
    if (active_page == DisplayId::display_page::LOG_VIEWER ||
        active_page == DisplayId::display_page::DEBUG_LOG_VIEWER)
    {
        MessageMap& selection = active_page == DisplayId::display_page::LOG_VIEWER ?
            *(dsp_comp_hub.dsp_shared->selected_log_entries) :
            *(dsp_comp_hub.dsp_shared->selected_debug_log_entries);
        dsp_comp_hub.dsp_common->application_working();
        dsp_comp_hub.dsp_shared->clear_log_entry_selection(selection);
        dsp_comp_hub.dsp_selector->refresh_display();
        accepted = true;
    }
    return accepted;
}

bool GlobalCommandsImpl::cmd_resource(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = false;
    if (tokenizer.has_next())
    {
        const std::string cmd_arg = tokenizer.next();
        if (!cmd_arg.empty())
        {
            DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
            if (active_page == DisplayId::display_page::RSC_LIST ||
                active_page == DisplayId::display_page::RSC_DETAIL ||
                active_page == DisplayId::display_page::RSC_ACTIONS ||
                active_page == DisplayId::display_page::CON_LIST ||
                active_page == DisplayId::display_page::CON_DETAIL ||
                active_page == DisplayId::display_page::CON_ACTIONS ||
                active_page == DisplayId::display_page::VLM_LIST ||
                active_page == DisplayId::display_page::VLM_DETAIL ||
                active_page == DisplayId::display_page::VLM_ACTIONS ||
                active_page == DisplayId::display_page::PEER_VLM_LIST ||
                active_page == DisplayId::display_page::PEER_VLM_DETAIL ||
                active_page == DisplayId::display_page::PEER_VLM_ACTIONS)
            {
                dsp_comp_hub.dsp_shared->update_monitor_rsc(cmd_arg);
                ModularDisplay& active_display = dsp_comp_hub.dsp_selector->get_active_display();
                active_display.notify_data_updated();
                accepted = true;
            }
        }
    }
    return accepted;
}

bool GlobalCommandsImpl::cmd_connection(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = false;
    if (tokenizer.has_next())
    {
        const std::string cmd_arg = tokenizer.next();
        if (!cmd_arg.empty())
        {
            DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
            if (active_page == DisplayId::display_page::CON_LIST ||
                active_page == DisplayId::display_page::CON_DETAIL ||
                active_page == DisplayId::display_page::CON_ACTIONS ||
                active_page == DisplayId::display_page::PEER_VLM_LIST ||
                active_page == DisplayId::display_page::PEER_VLM_DETAIL ||
                active_page == DisplayId::display_page::PEER_VLM_ACTIONS)
            {
                dsp_comp_hub.dsp_shared->update_monitor_con(cmd_arg);
                ModularDisplay& active_display = dsp_comp_hub.dsp_selector->get_active_display();
                active_display.notify_data_updated();
                accepted = true;
            }
        }
    }
    return accepted;
}

bool GlobalCommandsImpl::cmd_volume(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = false;
    if (tokenizer.has_next())
    {
        const std::string cmd_arg = tokenizer.next();
        uint16_t vlm_nr = DisplayConsts::VLM_NONE;
        try
        {
            vlm_nr = dsaext::parse_unsigned_int16(cmd_arg);
        }
        catch (dsaext::NumberFormatException&)
        {
            // ignored
        }

        if (vlm_nr != DisplayConsts::VLM_NONE)
        {
            DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
            if (active_page == DisplayId::display_page::VLM_LIST ||
                active_page == DisplayId::display_page::VLM_DETAIL ||
                active_page == DisplayId::display_page::VLM_ACTIONS)
            {
                dsp_comp_hub.dsp_shared->update_monitor_vlm(vlm_nr);
                accepted = true;
            }
            else
            if (active_page == DisplayId::display_page::PEER_VLM_LIST ||
                active_page == DisplayId::display_page::PEER_VLM_DETAIL ||
                active_page == DisplayId::display_page::PEER_VLM_ACTIONS)
            {
                dsp_comp_hub.dsp_shared->update_monitor_peer_vlm(vlm_nr);
                accepted = true;
            }

            if (accepted)
            {
                ModularDisplay& active_display = dsp_comp_hub.dsp_selector->get_active_display();
                active_display.notify_data_updated();
            }
        }
    }
    return accepted;
}

bool GlobalCommandsImpl::cmd_minor_nr(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = false;
    if (tokenizer.has_next())
    {
        const std::string cmd_arg = tokenizer.next();
        int32_t req_minor_nr = -1;

        DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
        if (active_page == DisplayId::display_page::RSC_LIST ||
            active_page == DisplayId::display_page::RSC_DETAIL ||
            active_page == DisplayId::display_page::RSC_ACTIONS ||
            active_page == DisplayId::display_page::VLM_LIST ||
            active_page == DisplayId::display_page::VLM_DETAIL ||
            active_page == DisplayId::display_page::VLM_ACTIONS ||
            active_page == DisplayId::display_page::CON_LIST ||
            active_page == DisplayId::display_page::CON_DETAIL ||
            active_page == DisplayId::display_page::CON_ACTIONS ||
            active_page == DisplayId::display_page::PEER_VLM_LIST ||
            active_page == DisplayId::display_page::PEER_VLM_DETAIL ||
            active_page == DisplayId::display_page::PEER_VLM_ACTIONS)
        {
            try
            {
                req_minor_nr = dsaext::parse_signed_int32(cmd_arg);
            }
            catch (dsaext::NumberFormatException&)
            {
                // ignored
            }

            bool have_match = false;
            if (req_minor_nr >= 0 && req_minor_nr < static_cast<int32_t> (0x100000L))
            {
                ResourcesMap::ValuesIterator rsc_iter(*(dsp_comp_hub.rsc_map));
                while (rsc_iter.has_next() && !have_match)
                {
                    DrbdResource* const rsc = rsc_iter.next();
                    VolumesContainer::VolumesIterator vlm_iter = rsc->volumes_iterator();
                    while (vlm_iter.has_next() && !have_match)
                    {
                        DrbdVolume* const vlm = vlm_iter.next();
                        const int32_t vlm_minor_nr = vlm->get_minor_nr();
                        have_match = vlm_minor_nr == req_minor_nr;
                        if (have_match)
                        {
                            const std::string& rsc_name = rsc->get_name();
                            const uint16_t vlm_nr = vlm->get_volume_nr();
                            dsp_comp_hub.dsp_shared->update_monitor_rsc(rsc_name);
                            dsp_comp_hub.dsp_shared->update_monitor_vlm(vlm_nr);
                            dsp_comp_hub.dsp_shared->update_monitor_peer_vlm(vlm_nr);
                            ModularDisplay& active_dsp = dsp_comp_hub.dsp_selector->get_active_display();
                            active_dsp.notify_data_updated();
                            dsp_comp_hub.dsp_selector->refresh_display();
                            accepted = true;
                        }
                    }
                }
            }
        }
    }
    return accepted;
}

bool GlobalCommandsImpl::cmd_close(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = true;

    dsp_comp_hub.dsp_selector->leave_display();

    return accepted;
}

// Dummy method for display local commands
bool GlobalCommandsImpl::local_command(const std::string& command, StringTokenizer& tokenizer)
{
    // Not accepting a global command results in the command being sent to the active display instead
    return false;
}
