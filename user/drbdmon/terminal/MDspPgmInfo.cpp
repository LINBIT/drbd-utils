#include <terminal/MDspPgmInfo.h>
#include <DrbdMonConsts.h>

const char* const MDspPgmInfo::PGM_INFO_TEXT =
    "Monitoring and administration utility for LINBIT(R) DRBD(R)\n"
    "\n"
    "This program is free open source software licensed under the GNU Public License, version 2 (GPLv2).\n"
    "\n"
    "Copyright (C) 2022, 2023 LINBIT HA-Solutions GmbH\n"
    "Copyright (C) 2022, 2023 LINBIT USA LLC\n";

const size_t MDspPgmInfo::RESERVE_CAPACITY  = 2000;

MDspPgmInfo::MDspPgmInfo(const ComponentsHub& comp_hub):
    MDspBase::MDspBase(comp_hub),
    format_text(comp_hub)
{
}

MDspPgmInfo::~MDspPgmInfo() noexcept
{
}

void MDspPgmInfo::display_content()
{
    format_text.set_line_length(dsp_comp_hub.term_cols - 2);

    std::string info_text;
    info_text.reserve(RESERVE_CAPACITY);

    info_text.append("\n");
    info_text.append("\x1B\x04");
    info_text.append(DrbdMonConsts::PROGRAM_NAME);
    info_text.append(" ");
    info_text.append(DrbdMonConsts::PROJECT_VERSION);
    info_text.append("\n");
    info_text.append("\x1B\xFF");
    info_text.append("\x1B\x05");
    info_text.append("drbd-utils ");
    info_text.append(DrbdMonConsts::UTILS_VERSION);
    info_text.append("\n");
    info_text.append("Build source: ");
    info_text.append(DrbdMonConsts::BUILD_HASH);
    info_text.append("\x1B\xFF");
    info_text.append("\n\n");

    info_text.append(PGM_INFO_TEXT);
    format_text.set_text(&info_text);

    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_PGM_INFO);

    uint32_t current_line = DisplayConsts::PAGE_NAV_Y + 1;

    std::string line;
    const std::string& color = dsp_comp_hub.active_color_table->help_text;
    for (bool have_line = format_text.next_line(line, color);
         have_line;
         have_line = format_text.next_line(line, color))
    {
        dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
        dsp_comp_hub.dsp_io->write_text(line.c_str());
        ++current_line;
    }
}

void MDspPgmInfo::enter_command_line_mode()
{
}

void MDspPgmInfo::leave_command_line_mode()
{
}

void MDspPgmInfo::enter_page_nav_mode()
{
}

void MDspPgmInfo::leave_page_nav_mode(const page_change_type change)
{
}

void MDspPgmInfo::cursor_to_next_item()
{
}

void MDspPgmInfo::cursor_to_previous_item()
{
}

void MDspPgmInfo::display_deactivated()
{
    MDspBase::display_deactivated();
    format_text.reset();
}

void MDspPgmInfo::reset_display()
{
    MDspBase::reset_display();
}

void MDspPgmInfo::synchronize_data()
{
}

uint64_t MDspPgmInfo::get_update_mask() noexcept
{
    return 0;
}
