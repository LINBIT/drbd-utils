#include <terminal/MDspConfiguration.h>
#include <terminal/KeyCodes.h>
#include <terminal/HelpText.h>
#include <persistent_configuration.h>
#include <configuration/CfgEntryStore.h>
#include <configuration/IoException.h>
#include <integerparse.h>
#include <dsaext.h>

const uint16_t  MDspConfiguration::DSP_INTERVAL_FIELD_ROW   = 7;

MDspConfiguration::MDspConfiguration(ComponentsHub& comp_hub, Configuration& config_ref):
    MDspMenuBase::MDspMenuBase(comp_hub),
    dsp_comp_hub_mutable(&comp_hub),
    config(&config_ref),
    input_display_interval(comp_hub, 5)
{
    saved_config = std::unique_ptr<Configuration>(new Configuration());

    cmd_fn_mouse_nav =
        [this]() -> void
        {
            opt_mouse_nav();
        };

    cmd_fn_discard_ok_tasks =
        [this]() -> void
        {
            opt_discard_ok_tasks();
        };
    cmd_fn_discard_failed_tasks =
        [this]() -> void
        {
            opt_discard_failed_tasks();
        };
    cmd_fn_suspend_new_tasks =
        [this]() -> void
        {
            opt_suspend_new_tasks();
        };

    cmd_fn_colors_dflt =
        [this]() -> void
        {
            opt_colors(DisplayStyleCollection::ColorStyle::DEFAULT);
        };
    cmd_fn_colors_dark256 =
        [this]() -> void
        {
            opt_colors(DisplayStyleCollection::ColorStyle::DARK_BG_256_COLORS);
        };
    cmd_fn_colors_dark16 =
        [this]() -> void
        {
            opt_colors(DisplayStyleCollection::ColorStyle::DARK_BG_16_COLORS);
        };
    cmd_fn_colors_light256 =
        [this]() -> void
        {
            opt_colors(DisplayStyleCollection::ColorStyle::LIGHT_BG_256_COLORS);
        };
    cmd_fn_colors_light16 =
        [this]() -> void
        {
            opt_colors(DisplayStyleCollection::ColorStyle::LIGHT_BG_16_COLORS);
        };

    cmd_fn_charset_dflt =
        [this]() -> void
        {
            opt_charset(DisplayStyleCollection::CharacterStyle::DEFAULT);
        };
    cmd_fn_charset_unicode =
        [this]() -> void
        {
            opt_charset(DisplayStyleCollection::CharacterStyle::UNICODE);
        };
    cmd_fn_charset_ascii =
        [this]() -> void
        {
            opt_charset(DisplayStyleCollection::CharacterStyle::ASCII);
        };

    cmd_fn_save_config =
        [this]() -> void
        {
            opt_save_config();
        };
    cmd_fn_load_config =
        [this]() -> void
        {
            opt_load_config();
        };
    cmd_fn_default_config =
        [this]() -> void
        {
            opt_default_config();
        };

    // Page 1
    cmd_mouse_nav = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "1", 1, 6, 5, 45,
            cmd_fn_mouse_nav
        )
    );
    input_display_interval.set_field_length(5);
    input_display_interval.set_position(30, DSP_INTERVAL_FIELD_ROW);

    // Page 2
    cmd_discard_ok_tasks = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "2", 2, 6, 5, 45,
            cmd_fn_discard_ok_tasks
        )
    );
    cmd_discard_failed_tasks = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "3", 2, 7, 5, 45,
            cmd_fn_discard_failed_tasks
        )
    );
    cmd_suspend_new_tasks = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "4", 2, 8, 5, 45,
            cmd_fn_suspend_new_tasks
        )
    );

    // Page 3
    cmd_colors_dflt = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "5", 3, 8, 5, 45,
            cmd_fn_colors_dflt
        )
    );
    cmd_colors_dark256 = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "6", 3, 9, 5, 45,
            cmd_fn_colors_dark256
        )
    );
    cmd_colors_dark16 = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "7", 3, 10, 5, 45,
            cmd_fn_colors_dark16
        )
    );
    cmd_colors_light256 = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "8", 3, 11, 5, 45,
            cmd_fn_colors_light256
        )
    );
    cmd_colors_light16 = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "9", 3, 12, 5, 45,
            cmd_fn_colors_light16
        )
    );

    // Page 4
    cmd_charset_dflt = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "10", 4, 8, 5, 45,
            cmd_fn_charset_dflt
        )
    );
    cmd_charset_unicode = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "11", 4, 9, 5, 45,
            cmd_fn_charset_unicode
        )
    );
    cmd_charset_ascii = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "12", 4, 10, 5, 45,
            cmd_fn_charset_ascii
        )
    );

    cmd_save_config = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "S", 1, 14, 5, 45,
            cmd_fn_save_config
        )
    );
    cmd_load_config = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "L", 1, 14, 45, 90,
            cmd_fn_load_config
        )
    );
    cmd_default_config = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "R", 1, 15, 5, 45,
            cmd_fn_default_config
        )
    );


    add_option(*cmd_mouse_nav);
    add_option(*cmd_discard_ok_tasks);
    add_option(*cmd_discard_failed_tasks);
    add_option(*cmd_suspend_new_tasks);
    add_option(*cmd_colors_dflt);
    add_option(*cmd_colors_dark256);
    add_option(*cmd_colors_dark16);
    add_option(*cmd_colors_light256);
    add_option(*cmd_colors_light16);
    add_option(*cmd_charset_dflt);
    add_option(*cmd_charset_unicode);
    add_option(*cmd_charset_ascii);
    add_option(*cmd_save_config);
    add_option(*cmd_load_config);
    add_option(*cmd_default_config);

    InputField& option_field = get_option_field();
    option_field.set_position(15, 17);

    set_page_count(4);
}

MDspConfiguration::~MDspConfiguration() noexcept
{
}

void MDspConfiguration::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_CONFIGURATION);

    const uint32_t page_nr = get_page_nr();
    if (saved_page_nr != page_nr)
    {
        MDspMenuBase::delegate_focus(false);
    }

    if (page_nr == 1)
    {
        display_page_01();
    }
    else
    if (page_nr == 2)
    {
        display_page_02();
    }
    else
    if (page_nr == 3)
    {
        display_page_03();
    }
    else
    if (page_nr == 4)
    {
        display_page_04();
    }
    else
    {
        set_page_nr(1);
        display_page_01();
    }

    switch (action_message)
    {
        case action_message_type::MSG_CONFIG_CHANGED:
            dsp_comp_hub.dsp_io->cursor_xy(5, 16);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
            dsp_comp_hub.dsp_io->write_text(" Unsaved changes");
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
            break;
        case action_message_type::MSG_CONFIG_SAVED:
            dsp_comp_hub.dsp_io->cursor_xy(5, 16);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
            dsp_comp_hub.dsp_io->write_text("Configuration saved");
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
            break;
        case action_message_type::MSG_CONFIG_LOADED:
            dsp_comp_hub.dsp_io->cursor_xy(5, 16);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
            dsp_comp_hub.dsp_io->write_text("Configuration loaded");
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
            break;
        case action_message_type::MSG_CONFIG_RESET:
            dsp_comp_hub.dsp_io->cursor_xy(5, 16);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
            dsp_comp_hub.dsp_io->write_text(" Unsaved changes");
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
            dsp_comp_hub.dsp_io->write_text(" - Configuration reset to defaults");
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
            action_message = action_message_type::MSG_CONFIG_CHANGED;
            break;
        case action_message_type::MSG_NONE:
            // fall-through
        default:
            // no-op
            break;
    }

    display_option_query(5, 17);

    saved_page_nr = page_nr;
}

void MDspConfiguration::display_page_01()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text("Configuration - Display behavior");

    const std::string& checked = dsp_comp_hub.active_character_table->checked_box;
    const std::string& unchecked = dsp_comp_hub.active_character_table->unchecked_box;

    const std::string& option_color = dsp_comp_hub.active_color_table->option_text;

    std::string option_text;
    option_text = (config->enable_mouse_nav ? checked : unchecked);
    option_text += " ";
    option_text += "Mouse navigation";
    display_option(" 1   ", option_text.c_str(), *cmd_mouse_nav, option_color);

    display_option(" S   ", "Save configuration", *cmd_save_config, option_color);
    display_option(" L   ", "Load configuration", *cmd_load_config, option_color);
    display_option(" R   ", "Reset to defaults", *cmd_default_config, option_color);

    dsp_comp_hub.dsp_io->cursor_xy(6, DSP_INTERVAL_FIELD_ROW);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->option_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Display interval:");
    dsp_comp_hub.dsp_io->cursor_xy(36, DSP_INTERVAL_FIELD_ROW);
    dsp_comp_hub.dsp_io->write_text("msec");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    input_display_interval.display();
}

void MDspConfiguration::display_page_02()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text("Configuration - Task queue options");

    const std::string& checked = dsp_comp_hub.active_character_table->checked_box;
    const std::string& unchecked = dsp_comp_hub.active_character_table->unchecked_box;

    const std::string& option_color = dsp_comp_hub.active_color_table->option_text;

    std::string option_text;

    option_text = (config->discard_succ_tasks ? checked : unchecked);
    option_text += " ";
    option_text += "Discard successfully completed tasks";
    display_option(" 2   ", option_text.c_str(), *cmd_discard_ok_tasks, option_color);

    option_text = (config->discard_fail_tasks ? checked : unchecked);
    option_text += " ";
    option_text += "Discard all completed tasks";
    display_option(" 3   ", option_text.c_str(), *cmd_discard_failed_tasks, option_color);

    option_text = (config->suspend_new_tasks ? checked : unchecked);
    option_text += " ";
    option_text += "Suspend new tasks";
    display_option(" 4   ", option_text.c_str(), *cmd_suspend_new_tasks, option_color);
}

void MDspConfiguration::display_page_03()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text("Configuration - Color scheme");

    const std::string& option_color = dsp_comp_hub.active_color_table->option_text;

    dsp_comp_hub.dsp_io->cursor_xy(3, 6);
    dsp_comp_hub.dsp_io->write_text("Current color scheme: ");
    const DisplayStyleCollection::ColorStyle current_scheme =
        dsp_comp_hub.style_coll->get_color_style_by_numeric_id(config->color_scheme);

    switch (current_scheme)
    {
       case DisplayStyleCollection::ColorStyle::DARK_BG_256_COLORS:
           dsp_comp_hub.dsp_io->write_text("256 colors on dark background");
           break;
       case DisplayStyleCollection::ColorStyle::DARK_BG_16_COLORS:
           dsp_comp_hub.dsp_io->write_text("16 colors on dark background");
           break;
       case DisplayStyleCollection::ColorStyle::LIGHT_BG_256_COLORS:
           dsp_comp_hub.dsp_io->write_text("256 colors on light background");
           break;
       case DisplayStyleCollection::ColorStyle::LIGHT_BG_16_COLORS:
           dsp_comp_hub.dsp_io->write_text("16 colors on light background");
           break;
       case DisplayStyleCollection::ColorStyle::DEFAULT:
           // fall-through
       default:
           dsp_comp_hub.dsp_io->write_text("Default");
    }

    display_option(" 5   ", "Default", *cmd_colors_dflt, option_color);
    display_option(" 6   ", "256 colors on dark background", *cmd_colors_dark256, option_color);
    display_option(" 7   ", "16 colors on dark background", *cmd_colors_dark16, option_color);
    display_option(" 8   ", "256 colors on light background", *cmd_colors_light256, option_color);
    display_option(" 9   ", "16 colors on light background", *cmd_colors_light16, option_color);
}

void MDspConfiguration::display_page_04()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text("Configuration - Character set");

    const std::string& option_color = dsp_comp_hub.active_color_table->option_text;

    dsp_comp_hub.dsp_io->cursor_xy(3, 6);
    dsp_comp_hub.dsp_io->write_text("Current character set: ");
    const DisplayStyleCollection::CharacterStyle current_set =
        dsp_comp_hub.style_coll->get_character_style_by_numeric_id(config->character_set);

    switch (current_set)
    {
       case DisplayStyleCollection::CharacterStyle::UNICODE:
           dsp_comp_hub.dsp_io->write_text("Unicode (UTF-8)");
           break;
       case DisplayStyleCollection::CharacterStyle::ASCII:
           dsp_comp_hub.dsp_io->write_text("ASCII (extended)");
           break;
       case DisplayStyleCollection::CharacterStyle::DEFAULT:
           // fall-through
       default:
           dsp_comp_hub.dsp_io->write_text("Default");
    }

    display_option("10   ", "Default", *cmd_charset_dflt, option_color);
    display_option("11   ", "Unicode (UTF-8)", *cmd_charset_unicode, option_color);
    display_option("12   ", "ASCII (extended)", *cmd_charset_ascii, option_color);
}

uint64_t MDspConfiguration::get_update_mask() noexcept
{
    return 0;
}

void MDspConfiguration::text_cursor_ops()
{
    const uint32_t page_nr = get_page_nr();
    if (page_nr == 1 && MDspMenuBase::is_focus_delegated())
    {
        input_display_interval.cursor();
    }
    else
    {
        MDspMenuBase::text_cursor_ops();
    }
}

bool MDspConfiguration::key_pressed(const uint32_t key)
{
    bool intercepted = false;
    if (key == KeyCodes::TAB || key == KeyCodes::ENTER)
    {
        const uint32_t page_nr = get_page_nr();
        if (page_nr == 1)
        {
            if (MDspMenuBase::is_focus_delegated())
            {
                opt_display_interval();
                intercepted = true;
            }
            else
            if (key == KeyCodes::TAB)
            {
                toggle_focus_delegation();
                intercepted = true;
            }
        }
    }
    if (!intercepted)
    {
        intercepted = MDspMenuBase::key_pressed(key);
        if (!intercepted)
        {
            if (key == KeyCodes::FUNC_01)
            {
                helptext::open_help_page(helptext::id_type::CONF_HELP, dsp_comp_hub);
                intercepted = true;
            }
            else
            if (MDspMenuBase::is_focus_delegated())
            {
                input_display_interval.key_pressed(key);
                intercepted = true;
            }
        }
    }
    return intercepted;
}

bool MDspConfiguration::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspMenuBase::mouse_action(mouse);
    if (!intercepted)
    {
        if (mouse.button == MouseEvent::button_id::BUTTON_01 && mouse.event == MouseEvent::event_id::MOUSE_DOWN)
        {
            const uint32_t page_nr = get_page_nr();
            if (page_nr == 1)
            {
                if (mouse.coord_row == DSP_INTERVAL_FIELD_ROW)
                {
                    MDspMenuBase::delegate_focus(true);
                    input_display_interval.mouse_action(mouse);
                    dsp_comp_hub.dsp_selector->refresh_display();
                    intercepted = true;
                }
                else
                if (MDspMenuBase::is_focus_delegated())
                {
                    opt_display_interval();
                    dsp_comp_hub.dsp_selector->refresh_display();
                    intercepted = true;
                }
            }
        }
    }
    return intercepted;
}

void MDspConfiguration::display_activated()
{
    MDspMenuBase::display_activated();

    const std::string& dsp_interval_text = input_display_interval.get_text();
    if (dsp_interval_text.empty())
    {
        const std::string interval_str = std::to_string(static_cast<unsigned int> (config->dsp_interval));
        input_display_interval.set_text(interval_str);
    }

    saved_page_nr = 0;
}

void MDspConfiguration::display_closed()
{
    MDspMenuBase::display_closed();

    set_page_nr(1);
    if (action_message == action_message_type::MSG_CONFIG_SAVED ||
        action_message == action_message_type::MSG_CONFIG_LOADED)
    {
        action_message = action_message_type::MSG_NONE;
    }

    input_display_interval.clear_text();
}

void MDspConfiguration::cursor_to_previous_item()
{
    const uint32_t page_nr = get_page_nr();
    if (page_nr == 1)
    {
        toggle_focus_delegation();
    }
}

void MDspConfiguration::cursor_to_next_item()
{
    const uint32_t page_nr = get_page_nr();
    if (page_nr == 1)
    {
        toggle_focus_delegation();
    }
}

void MDspConfiguration::toggle_focus_delegation()
{
    MDspMenuBase::delegate_focus(!MDspMenuBase::is_focus_delegated());
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspConfiguration::opt_mouse_nav()
{
    config->enable_mouse_nav = !config->enable_mouse_nav;
    if (config->enable_mouse_nav)
    {
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_MOUSE_ON.c_str());
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_MOUSE_OFF.c_str());
    }

    option_change_performed();
}

void MDspConfiguration::opt_discard_ok_tasks()
{
    config->discard_succ_tasks = !config->discard_succ_tasks;
    dsp_comp_hub.sub_proc_queue->set_discard_succeeded_tasks(config->discard_succ_tasks);

    option_change_performed();
}

void MDspConfiguration::opt_discard_failed_tasks()
{
    config->discard_fail_tasks = !config->discard_fail_tasks;
    dsp_comp_hub.sub_proc_queue->set_discard_finished_tasks(config->discard_fail_tasks);

    option_change_performed();
}

void MDspConfiguration::opt_suspend_new_tasks()
{
    config->suspend_new_tasks = !config->suspend_new_tasks;
    dsp_comp_hub.dsp_shared->activate_tasks = !config->suspend_new_tasks;

    option_change_performed();
}

void MDspConfiguration::opt_colors(const DisplayStyleCollection::ColorStyle style)
{
    config->color_scheme = static_cast<uint16_t> (style);
    const ColorTable& color_tbl = dsp_comp_hub.style_coll->get_color_table(style);
    dsp_comp_hub_mutable->active_color_table = &color_tbl;

    option_change_performed();
}

void MDspConfiguration::opt_charset(const DisplayStyleCollection::CharacterStyle style)
{
    config->character_set = static_cast<uint16_t> (style);
    const CharacterTable& char_tbl = dsp_comp_hub.style_coll->get_character_table(style);
    dsp_comp_hub_mutable->active_character_table = &char_tbl;

    option_change_performed();
}

void MDspConfiguration::opt_display_interval()
{
    const uint16_t prev_dsp_interval = config->dsp_interval;
    try
    {
        const std::string dsp_interval_text = input_display_interval.get_text();
        const uint16_t dsp_interval = dsaext::parse_unsigned_int16(dsp_interval_text);
        config->dsp_interval = dsp_interval;
        // TODO: Set the new display interval
    }
    catch (dsaext::NumberFormatException&)
    {
        // Nonsensical value, reset input field contents
        // Implicitly also resets the cursor offset
        input_display_interval.clear_text();
        const std::string interval_str = std::to_string(static_cast<unsigned int> (config->dsp_interval));
        input_display_interval.set_text(interval_str);
    }
    MDspMenuBase::delegate_focus(false);

    if (prev_dsp_interval != config->dsp_interval)
    {
        option_change_performed();
        dsp_comp_hub.core_instance->notify_config_changed();
    }
    else
    {
        dsp_comp_hub.dsp_selector->refresh_display();
    }
}

void MDspConfiguration::opt_save_config()
{
    dsp_comp_hub.dsp_common->application_working();

    dsp_comp_hub.dsp_io->cursor_xy(1, 16);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_LINE.c_str());

    InputField& config_option_field = get_option_field();
    config_option_field.clear_text();

    const std::string config_file = dsp_comp_hub.sys_api->get_config_file_path();
    try
    {
        std::unique_ptr<CfgEntryStore> config_store(new CfgEntryStore());
        configuration::save_configuration(config_file, *config, *config_store, *(dsp_comp_hub.sys_api));

        action_message = action_message_type::MSG_CONFIG_SAVED;
        dsp_comp_hub.dsp_selector->refresh_display();
    }
    catch (IoException& io_exc)
    {
        action_message = action_message_type::MSG_NONE;

        dsp_comp_hub.dsp_io->cursor_xy(5, 16);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
        dsp_comp_hub.dsp_io->write_text("Failed to save the configuration, see message log entry for details.");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        std::string error_message("Failed to write to the configuration file\n");
        error_message += "    The configuration file or a part of the path "
            "to the configuration file may be inaccessible.\n";
        error_message += "Configuration file path: ";
        error_message += config_file;
        dsp_comp_hub.log->add_entry(MessageLog::log_level::ALERT, error_message);

        dsp_comp_hub.dsp_common->application_idle();

        config_option_field.display();
        MDspMenuBase::text_cursor_ops();
    }
}

void MDspConfiguration::opt_load_config()
{
    dsp_comp_hub.dsp_common->application_working();

    dsp_comp_hub.dsp_io->cursor_xy(1, 16);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_LINE.c_str());

    InputField& config_option_field = get_option_field();
    config_option_field.clear_text();

    const std::string config_file = dsp_comp_hub.sys_api->get_config_file_path();
    try
    {
        std::unique_ptr<CfgEntryStore> config_store(new CfgEntryStore());
        configuration::initialize_cfg_entry_store(*config_store);
        configuration::load_configuration(config_file, *config, *config_store, *(dsp_comp_hub.sys_api));

        apply_config();

        const std::string interval_str = std::to_string(static_cast<unsigned int> (config->dsp_interval));
        input_display_interval.set_text(interval_str);

        action_message = action_message_type::MSG_CONFIG_LOADED;
        dsp_comp_hub.dsp_selector->refresh_display();
    }
    catch (IoException& io_exc)
    {
        action_message = action_message_type::MSG_NONE;

        dsp_comp_hub.dsp_io->cursor_xy(5, 16);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
        dsp_comp_hub.dsp_io->write_text("Failed to load the configuration, see message log entry for details.");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        std::string error_message("Failed to read the configuration file\n");
        error_message += "    The configuration file or a part of the path "
            "to the configuration file may be inaccessible, or no configuration file is present.\n";
        error_message += "Configuration file path: ";
        error_message += config_file;
        dsp_comp_hub.log->add_entry(MessageLog::log_level::ALERT, error_message);

        dsp_comp_hub.dsp_common->application_idle();

        config_option_field.display();
        MDspMenuBase::text_cursor_ops();
    }
}

void MDspConfiguration::opt_default_config()
{
    dsp_comp_hub.dsp_common->application_working();

    config->reset();

    apply_config();

    const std::string interval_str = std::to_string(static_cast<unsigned int> (config->dsp_interval));
    input_display_interval.set_text(interval_str);

    action_message = action_message_type::MSG_CONFIG_RESET;

    InputField& config_option_field = get_option_field();
    config_option_field.clear_text();

    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspConfiguration::apply_config()
{
    const DisplayStyleCollection::ColorStyle colors_id =
        dsp_comp_hub.style_coll->get_color_style_by_numeric_id(config->color_scheme);
    const ColorTable& color_tbl = dsp_comp_hub.style_coll->get_color_table(colors_id);
    dsp_comp_hub_mutable->active_color_table = &color_tbl;

    const DisplayStyleCollection::CharacterStyle charset_id =
        dsp_comp_hub.style_coll->get_character_style_by_numeric_id(config->character_set);
    const CharacterTable& char_tbl = dsp_comp_hub.style_coll->get_character_table(charset_id);
    dsp_comp_hub_mutable->active_character_table = &char_tbl;

    dsp_comp_hub.sub_proc_queue->set_discard_finished_tasks(config->discard_fail_tasks);
    dsp_comp_hub.sub_proc_queue->set_discard_succeeded_tasks(config->discard_succ_tasks);
    dsp_comp_hub.dsp_shared->activate_tasks = !config->suspend_new_tasks;

    if (config->enable_mouse_nav)
    {
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_MOUSE_ON.c_str());
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_MOUSE_OFF.c_str());
    }

    dsp_comp_hub.core_instance->notify_config_changed();
}

void MDspConfiguration::option_change_performed()
{
    action_message = action_message_type::MSG_CONFIG_CHANGED;

    InputField& config_option_field = get_option_field();
    config_option_field.clear_text();

    dsp_comp_hub.dsp_selector->refresh_display();
}
