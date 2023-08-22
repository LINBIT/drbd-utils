#ifndef MDSPCONNECTIONACTIONS_H
#define MDSPCONNECTIONACTIONS_H

#include <default_types.h>
#include <terminal/MDspMenuBase.h>

class MDspConnectionActions : public MDspMenuBase
{
  public:
    typedef void (MDspConnectionActions::*action_func_type)(const std::string& rsc_name, const std::string& con_name);

    static const uint8_t    OPT_LIST_Y;

    MDspConnectionActions(const ComponentsHub& comp_hub);
    virtual ~MDspConnectionActions() noexcept;

    virtual void display_content() override;
    virtual bool key_pressed(const uint32_t key) override;

    virtual uint64_t get_update_mask() noexcept override;

  private:
    std::function<void()>   cmd_fn_connect;
    std::function<void()>   cmd_fn_disconnect;
    std::function<void()>   cmd_fn_discard;

    std::unique_ptr<ClickableCommand>   cmd_connect;
    std::unique_ptr<ClickableCommand>   cmd_disconnect;
    std::unique_ptr<ClickableCommand>   cmd_discard;

    void selection_action(const action_func_type action_func);

    void action_connect(const std::string& rsc_name, const std::string& con_name);
    void action_disconnect(const std::string& rsc_name, const std::string& con_name);
    void action_connect_discard(const std::string& rsc_name, const std::string& con_name);
};

#endif /* MDSPCONNECTIONACTIONS_H */
