#ifndef COLORTABLE_H
#define COLORTABLE_H

#include <string>

class ColorTable
{
  public:
    static constexpr size_t EMP_CLM_TEXT_SIZE   = 5;

    std::string rst;
    std::string rst_fg;
    std::string rst_bg;

    std::string norm;
    std::string warn;
    std::string alert;

    std::string rsc_name;

    std::string vlm_nr;
    std::string vlm_count;

    std::string con_name;
    std::string con_count;

    std::string bg_cursor;
    std::string bg_marked;

    std::string bg_text_field;

    // Command line background
    std::string bg_cmd;
    // Command line label
    std::string cmd_label;
    // Command line input
    std::string cmd_input;

    // Application title bar
    std::string title_bar;

    // Header for lists
    std::string list_header;

    // Hot key field
    std::string hotkey_field;

    // Hot key label (description)
    std::string hotkey_label;

    // Status label (for selection mode, problem mode, cursor mode, page mode, etc.)
    std::string status_label;

    // Alert label (typically with a red background)
    std::string alert_label;

    // Help text color;
    std::string help_text;

    // Emphasis 1 - 5 for text column text
    std::string emp_clm_text[EMP_CLM_TEXT_SIZE];

    // Color for emphasized text, e.g. highlighted items or field labels
    std::string emphasis_text;

    // Color for highlighting text with caution notices, warnings, error messages, etc.
    std::string caution_text;

    // Color for menu options key
    std::string option_key;

    // Color for menu options text
    std::string option_text;

    // Color for the close-display symbol
    std::string sym_close;

    // @throws std::bad_alloc
    ColorTable();
    virtual ~ColorTable() noexcept;
};

#endif /* COLORTABLE_H */
