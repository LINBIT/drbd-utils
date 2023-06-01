#include <terminal/GlobalCommandsImpl.h>
#include <terminal/GlobalCommandConsts.h>
#include <terminal/ColorTable.h>
#include <terminal/CharacterTable.h>
#include <terminal/DisplayId.h>
#include <integerparse.h>
#include <dsaext.h>
#include <string_transformations.h>
#include <mutex>

GlobalCommandsImpl::GlobalCommandsImpl(ComponentsHub& comp_hub, Configuration& config_ref):
    dsp_comp_hub(comp_hub),
    config(&config_ref),
    entry_exit(&cmd_names::KEY_CMD_EXIT, &GlobalCommandsImpl::cmd_exit),
    entry_display(&cmd_names::KEY_CMD_DISPLAY, &GlobalCommandsImpl::cmd_display),
    entry_colors(&cmd_names::KEY_CMD_COLORS, &GlobalCommandsImpl::cmd_colors),
    entry_charset(&cmd_names::KEY_CMD_CHARSET, &GlobalCommandsImpl::cmd_charset),
    entry_select_all(&cmd_names::KEY_CMD_SELECT_ALL, &GlobalCommandsImpl::cmd_select_all),
    entry_deselect(&cmd_names::KEY_CMD_DESELECT, &GlobalCommandsImpl::cmd_deselect),
    entry_cursor(&cmd_names::KEY_CMD_CURSOR, &GlobalCommandsImpl::local_command)
{
    add_command(entry_exit);
    add_command(entry_display);
    add_command(entry_colors);
    add_command(entry_charset);
    add_command(entry_select_all);
    add_command(entry_deselect);
    add_command(entry_cursor);
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
        std::unique_lock<std::recursive_mutex> lock(dsp_comp_hub.log->queue_lock);

        MessageLog::Queue::ValuesIterator entry_iter = dsp_comp_hub.log->iterator();
        while (entry_iter.has_next())
        {
            MessageLog::Entry* const entry = entry_iter.next();
            const uint64_t entry_id = entry->get_id();
            dsp_comp_hub.dsp_shared->select_log_entry(*(dsp_comp_hub.dsp_shared->selected_log_entries), entry_id);
        }
        dsp_comp_hub.dsp_selector->refresh_display();
        accepted = true;
    }
    return accepted;
}

bool GlobalCommandsImpl::cmd_deselect(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = false;
    DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();
    if (active_page == DisplayId::display_page::LOG_VIEWER)
    {
        dsp_comp_hub.dsp_shared->clear_log_entry_selection(*(dsp_comp_hub.dsp_shared->selected_log_entries));
        dsp_comp_hub.dsp_selector->refresh_display();
        accepted = true;
    }
    return accepted;
}

// Dummy method for display local commands
bool GlobalCommandsImpl::local_command(const std::string& command, StringTokenizer& tokenizer)
{
    // Not accepting a global command results in the command being sent to the active display instead
    return false;
}
