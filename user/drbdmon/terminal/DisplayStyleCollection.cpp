#include <terminal/DisplayStyleCollection.h>

// @throws std::bad_alloc
DisplayStyleCollection::DisplayStyleCollection()
{
    unicode_tbl = std::unique_ptr<CharacterTable>(new CharacterTable());
    ascii_tbl   = std::unique_ptr<CharacterTable>(new CharacterTable());

    dark_bg_256clr  = std::unique_ptr<ColorTable>(new ColorTable());
    light_bg_256clr = std::unique_ptr<ColorTable>(new ColorTable());
    dark_bg_16clr   = std::unique_ptr<ColorTable>(new ColorTable());
    light_bg_16clr  = std::unique_ptr<ColorTable>(new ColorTable());

    {
        CharacterTable& tbl = *(unicode_tbl.get());

        tbl.sym_marked      = "\xE2\x96\xA0";

        tbl.sym_con         = "\xE2\x87\x84";
        tbl.sym_discon      = "\xE2\x96\xB2";
        tbl.sym_not_con     = "\xE2\x96\xB2";

        tbl.sym_diskless    = "\xE2\x97\x8B";
        tbl.sym_disk        = "\xE2\x97\x8F";
        tbl.sym_disk_failed = "\xE2\x96\xB2";

        tbl.sym_alert       = "\xE2\x96\xB2";
        tbl.sym_warn        = "\xE2\x96\xB3";
        tbl.sym_info        = "\xE2\x96\xAB";

        tbl.sym_pri         = "\xE2\x97\x89";
        tbl.sym_sec         = "\xE2\x97\x8B";

        tbl.sym_unknown     = "?";

        tbl.sym_cmd_arrow   = "\xE2\x87\x92";

        tbl.sym_last_page   = "\xE2\x96\xB6";

        tbl.sym_close       = "\xE2\x9C\x95";

        tbl.sym_working     = "\xE2\x8C\x9B";

        tbl.sync_blk_fin    = "\xE2\x96\x88";
        tbl.sync_blk_rmn    = "\xE2\x96\x91";

        tbl.checked_box     = "\xE2\x98\x92";
        tbl.unchecked_box   = "\xE2\x98\x90";
    }

    {
        CharacterTable& tbl = *(ascii_tbl.get());

        tbl.sym_marked      = "*";

        tbl.sym_con         = ">";
        tbl.sym_discon      = "!";
        tbl.sym_not_con     = "!";

        tbl.sym_diskless    = ">";
        tbl.sym_disk        = "o";
        tbl.sym_disk_failed = "!";

        tbl.sym_alert       = "!";
        tbl.sym_warn        = "?";
        tbl.sym_info        = "=";

        tbl.sym_pri         = "+";
        tbl.sym_sec         = "-";

        tbl.sym_unknown     = "?";

        tbl.sym_cmd_arrow   = ">";

        tbl.sym_last_page   = ">";

        tbl.sym_close       = "X";

        tbl.sym_working     = "X";

        tbl.sync_blk_fin    = "#";
        tbl.sync_blk_rmn    = ".";

        tbl.checked_box     = "Y";
        tbl.unchecked_box   = "N";
    }

    {
        ColorTable& clr = *(dark_bg_256clr.get());

        clr.rst             = "\x1B[0m";
        clr.rst_fg          = "\x1B[39m";
        clr.rst_bg          = "\x1B[49m";

        clr.norm            = "\x1B[38;5;120m";
        clr.warn            = "\x1B[38;5;226m";
        clr.alert           = "\x1B[38;5;196m";

        clr.rsc_name        = "\x1B[38;5;120m";

        clr.vlm_nr          = "\x1B[38;5;80m";
        clr.vlm_count       = "\x1B[38;5;80m";

        clr.con_name        = "\x1B[38;5;99m";
        clr.con_count       = "\x1B[38;5;99m";

        clr.bg_cursor       = "\x1B[48;5;18m";
        clr.bg_marked       = "\x1B[48;5;238m";

        clr.bg_text_field   = "\x1B[48;5;238m";

        clr.bg_cmd          = "\x1B[48;5;238m";
        clr.cmd_label       = "\x1B[48;5;20m\x1B[38;5;254m";
        clr.cmd_input       = "\x1B[48;5;18m\x1B[38;5;254m";

        clr.title_bar       = "\x1B[48;5;35m\x1B[38;5;22m";
        clr.list_header     = "\x1B[48;5;254m\x1B[38;5;0m";

        clr.hotkey_field    = "\x1B[48;5;254m\x1B[38;5;0m";
        clr.hotkey_label    = "\x1B[48;5;0m\x1B[38;5;254m";

        clr.status_label    = "\x1B[48;5;18m\x1B[38;5;254m";
        clr.alert_label     = "\x1B[48;5;196m\x1B[38;5;254m";

        clr.help_text       = "\x1B[38;5;35m";

        clr.emp_clm_text[0] = "\x1B[38;5;231m";
        clr.emp_clm_text[1] = "\x1B[38;5;226m";
        clr.emp_clm_text[2] = "\x1B[38;5;196m";
        clr.emp_clm_text[3] = "\x1B[38;5;46m";
        clr.emp_clm_text[4] = "\x1B[38;5;27m";

        clr.emphasis_text   = "\x1B[38;5;15m";
        clr.caution_text    = "\x1B[38;5;196m";

        clr.option_key      = "\x1B[38;5;226m";
        clr.option_text     = "\x1B[38;5;35m";

        clr.sym_close       = "\x1B[48;5;196m\x1B[38;5;15m";
    }

    {
        ColorTable& clr = *(light_bg_256clr.get());

        clr.rst             = "\x1B[0m";
        clr.rst_fg          = "\x1B[39m";
        clr.rst_bg          = "\x1B[49m";

        clr.norm            = "\x1B[38;5;65m";
        clr.warn            = "\x1B[38;5;208m";
        clr.alert           = "\x1B[38;5;196m";

        clr.rsc_name        = "\x1B[38;5;65m";

        clr.vlm_nr          = "\x1B[38;5;44m";
        clr.vlm_count       = "\x1B[38;5;44m";

        clr.con_name        = "\x1B[38;5;27m";
        clr.con_count       = "\x1B[38;5;27m";

        clr.bg_cursor       = "\x1B[48;5;51m";
        clr.bg_marked       = "\x1B[48;5;254m";

        clr.bg_text_field   = "\x1B[48;5;254m";

        clr.bg_cmd          = "\x1B[48;5;254m";
        clr.cmd_label       = "\x1B[48;5;39m\x1B[38;5;232m";
        clr.cmd_input       = "\x1B[48;5;51m\x1B[38;5;232m";

        clr.title_bar       = "\x1B[48;5;35m\x1B[38;5;22m";
        clr.list_header     = "\x1B[48;5;0m\x1B[38;5;254m";

        clr.hotkey_field    = "\x1B[48;5;235m\x1B[38;5;254m";
        clr.hotkey_label    = "\x1B[49m\x1B[38;5;235m";

        clr.status_label    = "\x1B[48;5;117m\x1B[38;5;254m";
        clr.alert_label     = "\x1B[48;5;196m\x1B[38;5;254m";

        clr.help_text       = "\x1B[38;5;35m";

        clr.emp_clm_text[0] = "\x1B[38;5;16m";
        clr.emp_clm_text[1] = "\x1B[38;5;214m";
        clr.emp_clm_text[2] = "\x1B[38;5;196m";
        clr.emp_clm_text[3] = "\x1B[38;5;40m";
        clr.emp_clm_text[4] = "\x1B[38;5;21m";

        clr.emphasis_text   = "\x1B[38;5;0m";
        clr.caution_text    = "\x1B[38;5;196m";

        clr.option_key      = "\x1B[38;5;39m";
        clr.option_text     = "\x1B[38;5;35m";

        clr.sym_close       = "\x1B[48;5;196m\x1B[38;5;0m";
    }

    {
        ColorTable& clr = *(dark_bg_16clr.get());

        clr.rst             = "\x1B[0m";
        clr.rst_fg          = "\x1B[39m";
        clr.rst_bg          = "\x1B[49m";

        clr.norm            = "\x1B[1;32m";
        clr.warn            = "\x1B[1;33m";
        clr.alert           = "\x1B[1;31m";

        clr.rsc_name        = "\x1B[1;32m";

        clr.vlm_nr          = "\x1B[36m";
        clr.vlm_count       = "\x1B[36m";

        clr.con_name        = "\x1B[1;34m";
        clr.con_count       = "\x1B[1;34m";

        clr.bg_cursor       = "\x1B[47m";
        clr.bg_marked       = "\x1B[44m";

        clr.bg_text_field   = "\x1B[44m";

        clr.bg_cmd          = "\x1B[44m";
        clr.cmd_label       = "\x1B[39m";   // Default color
        clr.cmd_input       = "\x1B[44m\x1B[37m";

        clr.title_bar       = "\x1B[42m\x1B[30m";
        clr.list_header     = "\x1B[47m\x1B[30m";

        clr.hotkey_field    = "\x1B[47m\x1B[30m";
        clr.hotkey_label    = "\x1B[40m\x1B[37m";

        clr.status_label    = "\x1B[44m\x1B[37m";
        clr.alert_label     = "\x1B[41m\x1B[30m";

        clr.help_text       = "\x1B[32m";

        clr.emp_clm_text[0] = "\x1B[1;37m";
        clr.emp_clm_text[1] = "\x1B[1;33m";
        clr.emp_clm_text[2] = "\x1B[1;31m";
        clr.emp_clm_text[3] = "\x1B[1;32m";
        clr.emp_clm_text[4] = "\x1B[1;34m";

        clr.emphasis_text   = "\x1B[1;37m";
        clr.caution_text    = "\x1B[1;31m";

        clr.option_key      = "\x1B[1;34m";
        clr.option_text     = "\x1B[0;32m";

        clr.sym_close       = "\x1B[41m\x1B[1;37m";
    }

    {
        ColorTable& clr = *(light_bg_16clr.get());

        clr.rst             = "\x1B[0m";
        clr.rst_fg          = "\x1B[39m";
        clr.rst_bg          = "\x1B[49m";

        clr.norm            = "\x1B[32m";
        clr.warn            = "\x1B[33m";
        clr.alert           = "\x1B[31m";

        clr.rsc_name        = "\x1B[32m";

        clr.vlm_nr          = "\x1B[36m";
        clr.vlm_count       = "\x1B[36m";

        clr.con_name        = "\x1B[1;34m";
        clr.con_count       = "\x1B[1;34m";

        clr.bg_cursor       = "\x1B[47m";
        clr.bg_marked       = "\x1B[44m";

        clr.bg_text_field   = "\x1B[44m";

        clr.bg_cmd          = "\x1B[44m";
        clr.cmd_label       = "\x1B[39m";   // Default color
        clr.cmd_input       = "\x1B[44m\x1B[37m";

        clr.title_bar       = "\x1B[42m\x1B[30m";
        clr.list_header     = "\x1B[40m\x1B[37m";

        clr.hotkey_field    = "\x1B[40m\x1B[37m";
        clr.hotkey_label    = "\x1B[47m\x1B[30m";

        clr.status_label    = "\x1B[44m\x1B[37m";
        clr.alert_label     = "\x1B[41m\x1B[30m";

        clr.help_text       = "\x1B[32m";

        clr.emp_clm_text[0] = "\x1B[1;39m";
        clr.emp_clm_text[1] = "\x1B[1;33m";
        clr.emp_clm_text[2] = "\x1B[1;31m";
        clr.emp_clm_text[3] = "\x1B[1;32m";
        clr.emp_clm_text[4] = "\x1B[1;34m";

        clr.emphasis_text   = "\x1B[1;34m";
        clr.caution_text    = "\x1B[1;31m";

        clr.option_key      = "\x1B[1;34m";
        clr.option_text     = "\x1B[0;32m";

        clr.sym_close       = "\x1B[41m\x1B[30m";
    }
}

DisplayStyleCollection::~DisplayStyleCollection() noexcept
{
}

DisplayStyleCollection::ColorStyle DisplayStyleCollection::get_color_style_by_numeric_id(
    const uint16_t id
) const
{
    ColorStyle style = ColorStyle::DEFAULT;
    if (id == static_cast<uint16_t> (ColorStyle::DARK_BG_256_COLORS))
    {
        style = ColorStyle::DARK_BG_256_COLORS;
    }
    else
    if (id == static_cast<uint16_t> (ColorStyle::DARK_BG_16_COLORS))
    {
        style = ColorStyle::DARK_BG_16_COLORS;
    }
    if (id == static_cast<uint16_t> (ColorStyle::LIGHT_BG_256_COLORS))
    {
        style = ColorStyle::LIGHT_BG_256_COLORS;
    }
    else
    if (id == static_cast<uint16_t> (ColorStyle::LIGHT_BG_16_COLORS))
    {
        style = ColorStyle::LIGHT_BG_16_COLORS;
    }
    return style;
}

DisplayStyleCollection::CharacterStyle DisplayStyleCollection::get_character_style_by_numeric_id(
    const uint16_t id
) const
{
    CharacterStyle style = CharacterStyle::DEFAULT;
    if (id == static_cast<uint16_t> (CharacterStyle::UNICODE))
    {
        style = CharacterStyle::UNICODE;
    }
    else
    if (id == static_cast<uint16_t> (CharacterStyle::ASCII))
    {
        style = CharacterStyle::ASCII;
    }
    return style;
}

const ColorTable& DisplayStyleCollection::get_color_table(const ColorStyle style) const
{
    const ColorTable* tbl = dark_bg_256clr.get();
    switch (style)
    {
        case ColorStyle::LIGHT_BG_16_COLORS:
            tbl = light_bg_16clr.get();
            break;
        case ColorStyle::LIGHT_BG_256_COLORS:
            tbl = light_bg_256clr.get();
            break;
        case ColorStyle::DARK_BG_16_COLORS:
            tbl = dark_bg_16clr.get();
            break;
        case ColorStyle::DARK_BG_256_COLORS:
            // fall-through
        case ColorStyle::DEFAULT:
            // fall-through
        default:
            break;
    }
    return *tbl;
}

const CharacterTable& DisplayStyleCollection::get_character_table(const CharacterStyle style) const
{
    const CharacterTable* tbl = unicode_tbl.get();
    switch (style)
    {
        case CharacterStyle::ASCII:
            tbl = ascii_tbl.get();
            break;
        case CharacterStyle::UNICODE:
            // fall-through
        case CharacterStyle::DEFAULT:
            // fall-through
        default:
            break;
    }
    return *tbl;
}
