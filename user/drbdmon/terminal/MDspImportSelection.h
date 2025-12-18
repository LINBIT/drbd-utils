#ifndef MDSPIMPORTSELECTION_H
#define MDSPIMPORTSELECTION_H

#include <default_types.h>
#include <memory>
#include <string>
#include <functional>
#include <ios>
#include <terminal/MDspMenuBase.h>
#include <terminal/InputField.h>

class MDspImportSelection : public MDspMenuBase
{
  public:
    MDspImportSelection(const ComponentsHub& comp_hub);
    virtual ~MDspImportSelection() noexcept;

    virtual void display_content() override;

    virtual uint64_t get_update_mask() noexcept override;
    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;

    virtual void text_cursor_ops() override;

    virtual void display_activated() override;
    virtual void display_deactivated() override;
    virtual void display_closed() override;

    virtual void cursor_to_previous_item() override;
    virtual void cursor_to_next_item() override;

    static const std::streamsize    IN_BUFFER_SIZE;
    static const size_t             MAX_LINE_LENGTH;

  private:
    class ImportException : public std::exception
    {
    };

    static const std::string    IMPORT_VALID_CHARS;
    static const char* const    IMPORT_ERROR_MSG;

    std::unique_ptr<InputField>     path_input;
    std::string                     message;

    bool    imp_rsc         {true};
    bool    imp_vlm         {false};
    bool    imp_con         {false};
    bool    imp_peer_vlm    {false};

    std::function<void()>   cmd_fn_toggle_imp_rsc;
    std::function<void()>   cmd_fn_toggle_imp_vlm;
    std::function<void()>   cmd_fn_toggle_imp_con;
    std::function<void()>   cmd_fn_toggle_imp_peer_vlm;
    std::function<void()>   cmd_fn_import;

    std::unique_ptr<ClickableCommand>   cmd_toggle_imp_rsc;
    std::unique_ptr<ClickableCommand>   cmd_toggle_imp_vlm;
    std::unique_ptr<ClickableCommand>   cmd_toggle_imp_con;
    std::unique_ptr<ClickableCommand>   cmd_toggle_imp_peer_vlm;
    std::unique_ptr<ClickableCommand>   cmd_import;

    void import_selection();
    void process_line(const std::string& in_line, const size_t line_nr);
    // @throws ImportException
    void check_valid(const std::string& text);
};

#endif /* MDSPIMPORTSELECTION_H */
