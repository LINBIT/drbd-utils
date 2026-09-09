#include <terminal/MDspImportSelection.h>
#include <terminal/HelpText.h>
#include <terminal/KeyCodes.h>
#include <iostream>
#include <fstream>
#include <bounds.h>
#include <cppdsaext/src/dsaext.h>
#include <cppdsaext/src/integerparse.h>

const std::streamsize   MDspImportSelection::IN_BUFFER_SIZE     = 0x4000;
const size_t            MDspImportSelection::MAX_LINE_LENGTH    = 400;
const std::string       MDspImportSelection::IMPORT_VALID_CHARS(".-_+=@");
const char* const       MDspImportSelection::IMPORT_ERROR_MSG = "Import failed (See message log for details)";

MDspImportSelection::MDspImportSelection(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    path_input = std::unique_ptr<InputField>(
        new InputField(comp_hub, 5, 13, 400, 80)
    );

    cmd_fn_toggle_imp_rsc =
        [this]() -> void
        {
            imp_rsc = !imp_rsc;
            dsp_comp_hub.dsp_selector->refresh_display();
        };
    cmd_fn_toggle_imp_vlm =
        [this]() -> void
        {
            imp_vlm = !imp_vlm;
            dsp_comp_hub.dsp_selector->refresh_display();
        };
    cmd_fn_toggle_imp_con =
        [this]() -> void
        {
            imp_con = !imp_con;
            dsp_comp_hub.dsp_selector->refresh_display();
        };
    cmd_fn_toggle_imp_peer_vlm =
        [this]() -> void
        {
            imp_peer_vlm = !imp_peer_vlm;
            dsp_comp_hub.dsp_selector->refresh_display();
        };
    cmd_fn_import =
        [this]() -> void
        {
            import_selection();
        };

    ClickableCommand::Builder bld;

    bld.coords.start_col = 5;
    bld.coords.end_col = 45;
    bld.coords.row = 5;
    bld.auto_nr = 1;

    cmd_toggle_imp_rsc = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_toggle_imp_rsc)
    );
    add_option(*cmd_toggle_imp_rsc);
    cmd_toggle_imp_vlm = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_toggle_imp_vlm)
    );
    add_option(*cmd_toggle_imp_vlm);
    cmd_toggle_imp_con = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_toggle_imp_con)
    );
    add_option(*cmd_toggle_imp_con);
    cmd_toggle_imp_peer_vlm = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_toggle_imp_peer_vlm)
    );
    add_option(*cmd_toggle_imp_peer_vlm);

    ++bld.coords.row;
    cmd_import = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_import)
    );
    add_option(*cmd_import);
}

MDspImportSelection::~MDspImportSelection() noexcept
{
}

void MDspImportSelection::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_IMPORT_SLCT);

    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Import selection:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    display_selectable(5, "Resources", *cmd_toggle_imp_rsc, imp_rsc);
    display_selectable(5, "Volumes", *cmd_toggle_imp_vlm, imp_vlm);
    display_selectable(5, "Connections", *cmd_toggle_imp_con, imp_con);
    display_selectable(5, "Peer volumes", *cmd_toggle_imp_peer_vlm, imp_peer_vlm);

    display_option(5, "Import from file", *cmd_import, dsp_comp_hub.active_color_table->option_text);

    dsp_comp_hub.dsp_io->cursor_xy(3, 12);
    dsp_comp_hub.dsp_io->write_text("Import from file:");
    path_input->display();

    if (!message.empty())
    {
        dsp_comp_hub.dsp_io->cursor_xy(5, 15);
        dsp_comp_hub.dsp_io->write_text(message.c_str());
    }

    display_option_query(5, 17);
}

uint64_t MDspImportSelection::get_update_mask() noexcept
{
    return 0;
}

bool MDspImportSelection::key_pressed(const uint32_t key)
{
    bool intercepted = false;
    if (key == KeyCodes::FUNC_01)
    {
        helptext::open_help_page(helptext::id_type::IMPS_HELP, dsp_comp_hub);
        intercepted = true;
    }
    else
    {
        const bool delegated = is_focus_delegated();
        if (delegated && key == static_cast<uint32_t> ('/'))
        {
            path_input->key_pressed(key);
            intercepted = true;
        }
        else
        {
            intercepted = MDspMenuBase::key_pressed(key);
        }
        if (delegated && !intercepted)
        {
            path_input->key_pressed(key);
            intercepted = true;
        }
    }
    return intercepted;
}

bool MDspImportSelection::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspMenuBase::mouse_action(mouse);
    if (!intercepted)
    {
        intercepted = path_input->mouse_action(mouse);
        if (intercepted)
        {
            delegate_focus(true);
        }
        else
        {
            InputField& option_field = get_option_field();
            intercepted = option_field.mouse_action(mouse);
            if (intercepted)
            {
                delegate_focus(false);
            }
        }

        if (intercepted)
        {
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
    return intercepted;
}

void MDspImportSelection::text_cursor_ops()
{
    if (is_focus_delegated())
    {
        path_input->cursor();
    }
    else
    {
        MDspMenuBase::text_cursor_ops();
    }
}

void MDspImportSelection::display_activated()
{
    delegate_focus(false);
}

void MDspImportSelection::display_deactivated()
{
    message.clear();
}

void MDspImportSelection::display_closed()
{
    path_input->clear_text();
    message.clear();

    imp_rsc = true;
    imp_vlm = false;
    imp_con = false;
    imp_peer_vlm = false;
}

void MDspImportSelection::cursor_to_previous_item()
{
    const bool state = is_focus_delegated();
    delegate_focus(!state);
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspImportSelection::cursor_to_next_item()
{
    const bool state = is_focus_delegated();
    delegate_focus(!state);
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspImportSelection::import_selection()
{
    dsp_comp_hub.dsp_common->application_working();

    message.clear();

    const std::string& filename = path_input->get_text();
    if (!filename.empty() && (imp_rsc || imp_vlm || imp_con || imp_peer_vlm))
    {
        std::fstream file_in(filename.c_str(), std::ios_base::in);
        if (file_in.is_open())
        {
            std::unique_ptr<char[]> buffer_mgr(new char[IN_BUFFER_SIZE]);
            char* const buffer = buffer_mgr.get();

            size_t line_nr = 0;
            try
            {
                std::string in_line;
                in_line.reserve(MAX_LINE_LENGTH);
                while (file_in.good())
                {
                    file_in.read(buffer, IN_BUFFER_SIZE);
                    if (!file_in.bad())
                    {
                        const size_t read_count = bounds(
                            static_cast<size_t> (0),
                            static_cast<size_t> (file_in.gcount()),
                            static_cast<size_t> (IN_BUFFER_SIZE)
                        );
                        size_t line_start = 0;
                        size_t idx = 0;
                        while (idx < read_count)
                        {
                            const char in_byte = buffer[idx];
                            if (in_byte == '\r' || in_byte == '\n')
                            {
                                const size_t length = idx - line_start;
                                if (length > 0)
                                {
                                    if (MAX_LINE_LENGTH - in_line.length() >= length)
                                    {
                                        in_line.append(&buffer[line_start], length);
                                        process_line(in_line, line_nr);
                                        in_line.clear();
                                    }
                                    else
                                    {
                                        std::string error_msg("Import from \"");
                                        error_msg += filename;
                                        error_msg += "\" failed\n\n";
                                        error_msg += "Line #";
                                        error_msg += std::to_string(static_cast<unsigned long long> (line_nr));
                                        error_msg += " is too long";
                                        dsp_comp_hub.log->add_entry(MessageLog::log_level::ALERT, error_msg);
                                        throw ImportException();
                                    }
                                }
                                line_start = idx + 1;

                                if (in_byte == '\n')
                                {
                                    ++line_nr;
                                }
                            }
                            ++idx;
                        }
                        const size_t length = idx - line_start;
                        if (length > 0)
                        {
                            if (MAX_LINE_LENGTH - in_line.length() >= length)
                            {
                                in_line.append(&buffer[line_start], length);
                            }
                            else
                            {
                                std::string error_msg("Import from \"");
                                error_msg += filename;
                                error_msg += "\" failed\n\n";
                                error_msg += "Line #";
                                error_msg += std::to_string(static_cast<unsigned long long> (line_nr));
                                error_msg += " is too long";
                                dsp_comp_hub.log->add_entry(MessageLog::log_level::ALERT, error_msg);
                                throw ImportException();
                            }
                        }
                    }
                    else
                    {
                        message = "Import failed: I/O error";
                    }
                }
            }
            catch (dsaext::NumberFormatException&)
            {
                std::string error_msg("Import from \"");
                error_msg += filename;
                error_msg += "\" failed\n\n";
                error_msg += "Unparsable volume number on line #";
                error_msg += std::to_string(static_cast<unsigned long long> (line_nr));
                dsp_comp_hub.log->add_entry(MessageLog::log_level::ALERT, error_msg);

                message = IMPORT_ERROR_MSG;
            }
            catch (ImportException&)
            {
                message = IMPORT_ERROR_MSG;
            }

            file_in.close();

            if (message.empty())
            {
                message = "Import complete.";
            }
        }
        else
        {
            message = "Import failed: Cannot open input file";
        }
    }
    else
    {
        if (filename.empty())
        {
            message = "Enter import file path";
        }
        else
        if (!(imp_rsc || imp_vlm || imp_con || imp_peer_vlm))
        {
            message = "Select objects to import";
        }
    }

    dsp_comp_hub.dsp_selector->refresh_display();
}

// @throws dsaext::NumberFormatException
void MDspImportSelection::process_line(const std::string& in_line, const size_t line_nr)
{
    try
    {
        size_t split_idx = in_line.find(':');
        if (split_idx != std::string::npos)
        {
            // Connection or peer volume
            if (imp_con || imp_peer_vlm)
            {
                std::string rsc_name(in_line, 0, split_idx);
                std::string con_name(in_line, split_idx + 1);
                split_idx = con_name.find('/');
                if (split_idx == std::string::npos)
                {
                    // Connection
                    if (imp_con)
                    {
                        check_valid(rsc_name);
                        check_valid(con_name);
                        dsp_comp_hub.dsp_shared->select_connection(rsc_name, con_name);
                    }
                }
                else
                {
                    // Peer volume
                    if (imp_peer_vlm)
                    {
                        std::string vlm_nr_text(con_name, split_idx + 1);
                        con_name = con_name.substr(0, split_idx);
                        check_valid(con_name);
                        const uint16_t vlm_nr = dsaext::parse_unsigned_int16(vlm_nr_text);
                        dsp_comp_hub.dsp_shared->select_peer_volume(rsc_name, con_name, vlm_nr);
                    }
                }
            }
        }
        else
        {
            split_idx = in_line.find('/');
            if (split_idx != std::string::npos)
            {
                // Volume
                if (imp_vlm)
                {
                    std::string rsc_name(in_line, 0, split_idx);
                    std::string vlm_nr_text(in_line, split_idx + 1);
                    check_valid(rsc_name);
                    const uint16_t vlm_nr = dsaext::parse_unsigned_int16(vlm_nr_text);
                    dsp_comp_hub.dsp_shared->select_volume(rsc_name, vlm_nr);
                }
            }
            else
            {
                // Resource
                if (imp_rsc)
                {
                    check_valid(in_line);
                    dsp_comp_hub.dsp_shared->select_resource(in_line);
                }
            }
        }
    }
    catch (ImportException&)
    {
        const std::string& filename = path_input->get_text();
        std::string error_msg("Import from \"");
        error_msg += filename;
        error_msg += "\" failed\n\n";
        error_msg += "Invalid character on line #";
        error_msg += std::to_string(static_cast<unsigned long long> (line_nr));
        dsp_comp_hub.log->add_entry(MessageLog::log_level::ALERT, error_msg);
        throw;
    }
}

// @throws ImportException
void MDspImportSelection::check_valid(const std::string& text)
{
    const char* const text_chars = text.c_str();
    const size_t length = text.length();
    size_t idx = 0;
    while (idx < length)
    {
        char letter = text_chars[idx];
        if (!((letter >= 'a' && letter <= 'z') ||
              (letter >= 'A' && letter <= 'Z') ||
              (letter >= '0' && letter <= '9') ||
              IMPORT_VALID_CHARS.find(letter) != std::string::npos))
        {
            break;
        }
        ++idx;
    }
    if (idx != length)
    {
        throw ImportException();
    }
}
