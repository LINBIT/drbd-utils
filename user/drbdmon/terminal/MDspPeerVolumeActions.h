#ifndef MDSPPEERVOLUMEACTIONS_H
#define MDSPPEERVOLUMEACTIONS_H

#include <default_types.h>
#include <terminal/MDspMenuBase.h>

class MDspPeerVolumeActions : public MDspMenuBase
{
  public:
    typedef void (MDspPeerVolumeActions::*action_func_type)(
        const std::string&  rsc_name,
        const std::string&  con_name,
        const uint16_t      vlm_nr
    );

    static const uint8_t    OPT_LIST_Y;

    MDspPeerVolumeActions(const ComponentsHub& comp_hub);
    virtual ~MDspPeerVolumeActions() noexcept;

    virtual void display_content() override;
    virtual bool key_pressed(const uint32_t key) override;

    virtual uint64_t get_update_mask() noexcept override;

  private:
    std::function<void()>   cmd_fn_pause_sync;
    std::function<void()>   cmd_fn_resume_sync;
    std::function<void()>   cmd_fn_verify;
    std::function<void()>   cmd_fn_invalidate_remote;

    std::unique_ptr<ClickableCommand>   cmd_pause_sync;
    std::unique_ptr<ClickableCommand>   cmd_resume_sync;
    std::unique_ptr<ClickableCommand>   cmd_verify;
    std::unique_ptr<ClickableCommand>   cmd_invalidate_remote;

    void selection_action(const action_func_type action_func);

    void action_pause_sync(const std::string& rsc_name, const std::string& con_name, const uint16_t vlm_nr);
    void action_resume_sync(const std::string& rsc_name, const std::string& con_name, const uint16_t vlm_nr);
    void action_verify(const std::string& rsc_name, const std::string& con_name, const uint16_t vlm_nr);
    void action_invalidate_remote(const std::string& rsc_name, const std::string& con_name, const uint16_t vlm_nr);
};

#endif /* MDSPPEERVOLUMEACTIONS_H */
