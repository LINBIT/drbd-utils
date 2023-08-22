#ifndef MDSPVOLUMEACTIONS_H
#define MDSPVOLUMEACTIONS_H

#include <default_types.h>
#include <terminal/MDspMenuBase.h>

class MDspVolumeActions : public MDspMenuBase
{
  public:
    typedef void (MDspVolumeActions::*action_func_type)(const std::string& rsc_name, const uint16_t vlm_nr);

    static const uint8_t    OPT_LIST_Y;

    MDspVolumeActions(const ComponentsHub& comp_hub);
    virtual ~MDspVolumeActions() noexcept;

    virtual void display_content() override;
    virtual bool key_pressed(const uint32_t key) override;

    virtual uint64_t get_update_mask() noexcept override;

  private:
    std::function<void()>   cmd_fn_attach;
    std::function<void()>   cmd_fn_detach;
    std::function<void()>   cmd_fn_resize;
    std::function<void()>   cmd_fn_invalidate;

    std::unique_ptr<ClickableCommand>   cmd_attach;
    std::unique_ptr<ClickableCommand>   cmd_detach;
    std::unique_ptr<ClickableCommand>   cmd_resize;
    std::unique_ptr<ClickableCommand>   cmd_invalidate;

    void selection_action(const action_func_type action_func);

    void action_attach(const std::string& rsc_name, const uint16_t vlm_nr);
    void action_detach(const std::string& rsc_name, const uint16_t vlm_nr);
    void action_resize(const std::string& rsc_name, const uint16_t vlm_nr);
    void action_invalidate(const std::string& rsc_name, const uint16_t vlm_nr);
};

#endif /* MDSPVOLUMEACTIONS_H */
