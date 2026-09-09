#ifndef MDSPEXPORTSELECTION_H
#define MDSPEXPORTSELECTION_H

#include <default_types.h>
#include <memory>
#include <string>
#include <functional>
#include <terminal/MDspMenuBase.h>
#include <terminal/InputField.h>

class MDspExportSelection : public MDspMenuBase
{
  public:
    MDspExportSelection(const ComponentsHub& comp_hub);
    virtual ~MDspExportSelection() noexcept;

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

  private:
    std::unique_ptr<InputField>     path_input;
    std::string                     message;

    std::function<void()>   cmd_fn_toggle_exp_rsc;
    std::function<void()>   cmd_fn_toggle_exp_vlm;
    std::function<void()>   cmd_fn_toggle_exp_con;
    std::function<void()>   cmd_fn_toggle_exp_peer_vlm;
    std::function<void()>   cmd_fn_export;

    std::unique_ptr<ClickableCommand>   cmd_toggle_exp_rsc;
    std::unique_ptr<ClickableCommand>   cmd_toggle_exp_vlm;
    std::unique_ptr<ClickableCommand>   cmd_toggle_exp_con;
    std::unique_ptr<ClickableCommand>   cmd_toggle_exp_peer_vlm;
    std::unique_ptr<ClickableCommand>   cmd_export;

    bool exp_rsc        {true};
    bool exp_vlm        {false};
    bool exp_con        {false};
    bool exp_peer_vlm   {false};

    void export_selection();
};

#endif /* MDSPEXPORTSELECTION_H */
