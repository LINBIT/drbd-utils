#ifndef MDSPCONFIGURATION_H
#define MDSPCONFIGURATION_H

#include <terminal/MDspMenuBase.h>
#include <configuration/Configuration.h>

class MDspConfiguration : public MDspMenuBase
{
  public:
    static const uint16_t   DSP_INTERVAL_FIELD_ROW;

    MDspConfiguration(ComponentsHub& comp_hub, Configuration& config_ref);
    virtual ~MDspConfiguration() noexcept;

    virtual void display_content() override;
    virtual uint64_t get_update_mask() noexcept override;
    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;

    virtual void text_cursor_ops() override;

    virtual void display_activated() override;
    virtual void display_closed() override;

    virtual void cursor_to_next_item() override;
    virtual void cursor_to_previous_item() override;

  private:
    enum class action_message_type : uint8_t
    {
        MSG_NONE,
        MSG_CONFIG_SAVED,
        MSG_CONFIG_LOADED,
        MSG_CONFIG_RESET,
        MSG_CONFIG_CHANGED
    };

    ComponentsHub*  dsp_comp_hub_mutable    {nullptr};
    Configuration*  config                  {nullptr};
    InputField      input_display_interval;

    action_message_type     action_message  {action_message_type::MSG_NONE};

    std::unique_ptr<Configuration> saved_config;

    std::function<void()>   cmd_fn_mouse_nav;

    std::function<void()>   cmd_fn_discard_ok_tasks;
    std::function<void()>   cmd_fn_discard_failed_tasks;
    std::function<void()>   cmd_fn_suspend_new_tasks;

    std::function<void()>   cmd_fn_colors_dflt;
    std::function<void()>   cmd_fn_colors_dark256;
    std::function<void()>   cmd_fn_colors_dark16;
    std::function<void()>   cmd_fn_colors_light256;
    std::function<void()>   cmd_fn_colors_light16;

    std::function<void()>   cmd_fn_charset_dflt;
    std::function<void()>   cmd_fn_charset_unicode;
    std::function<void()>   cmd_fn_charset_ascii;

    std::function<void()>   cmd_fn_save_config;
    std::function<void()>   cmd_fn_load_config;
    std::function<void()>   cmd_fn_default_config;

    std::unique_ptr<ClickableCommand>   cmd_mouse_nav;

    std::unique_ptr<ClickableCommand>   cmd_discard_ok_tasks;
    std::unique_ptr<ClickableCommand>   cmd_discard_failed_tasks;
    std::unique_ptr<ClickableCommand>   cmd_suspend_new_tasks;

    std::unique_ptr<ClickableCommand>   cmd_colors_dflt;
    std::unique_ptr<ClickableCommand>   cmd_colors_dark256;
    std::unique_ptr<ClickableCommand>   cmd_colors_dark16;
    std::unique_ptr<ClickableCommand>   cmd_colors_light256;
    std::unique_ptr<ClickableCommand>   cmd_colors_light16;

    std::unique_ptr<ClickableCommand>   cmd_charset_dflt;
    std::unique_ptr<ClickableCommand>   cmd_charset_unicode;
    std::unique_ptr<ClickableCommand>   cmd_charset_ascii;

    std::unique_ptr<ClickableCommand>   cmd_save_config;
    std::unique_ptr<ClickableCommand>   cmd_load_config;
    std::unique_ptr<ClickableCommand>   cmd_default_config;

    uint32_t    saved_page_nr           {0};

    void display_page_01();
    void display_page_02();
    void display_page_03();
    void display_page_04();

    void toggle_focus_delegation();

    void opt_mouse_nav();
    void opt_discard_ok_tasks();
    void opt_discard_failed_tasks();
    void opt_suspend_new_tasks();
    void opt_colors(const DisplayStyleCollection::ColorStyle style);
    void opt_charset(const DisplayStyleCollection::CharacterStyle style);
    void opt_display_interval();
    void opt_save_config();
    void opt_load_config();
    void opt_default_config();

    void apply_config();

    void option_change_performed();
};

#endif /* MDSPCONFIGURATION_H */
