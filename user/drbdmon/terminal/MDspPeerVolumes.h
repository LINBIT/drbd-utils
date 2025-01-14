#ifndef MDSPPEERVOLUMES_H
#define MDSPPEERVOLUMES_H

#include <default_types.h>
#include <terminal/MDspStdListBase.h>
#include <terminal/ComponentsHub.h>
#include <terminal/MouseEvent.h>
#include <objects/DrbdResource.h>
#include <objects/DrbdConnection.h>
#include <objects/DrbdVolume.h>
#include <terminal/DisplayConsts.h>
#include <map_types.h>
#include <string>
#include <memory>
#include <functional>

class MDspPeerVolumes : public MDspStdListBase
{
  public:
    static const uint8_t    RSC_HEADER_Y;
    static const uint8_t    CON_HEADER_Y;
    static const uint8_t    PEER_VLM_HEADER_Y;
    static const uint8_t    PEER_VLM_LIST_Y;

    MDspPeerVolumes(const ComponentsHub& comp_hub);
    virtual ~MDspPeerVolumes() noexcept;

    virtual void display_list() override;
    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;

    virtual bool execute_custom_command(const std::string& command, StringTokenizer& tokenizer) override;
    virtual void clear_selection();
    virtual bool is_selecting() override;
    virtual void toggle_select_cursor_item() override;
    virtual void cursor_to_next_item() override;
    virtual void cursor_to_previous_item() override;

    virtual bool is_cursor_nav() override;
    virtual void reset_cursor_position() override;
    virtual void clear_cursor() override;

    virtual void select_volume(const uint16_t vlm_nr);
    virtual void deselect_volume(const uint16_t vlm_nr);

    virtual void display_activated() override;
    virtual void display_deactivated() override;
    virtual void reset_display() override;
    virtual void synchronize_data() override;
    virtual void notify_data_updated() override;

    virtual uint64_t get_update_mask() noexcept override;

  private:
    std::string displayed_rsc;
    std::string displayed_con;

    uint16_t    cursor_vlm      {DisplayConsts::VLM_NONE};

    std::function<bool(DrbdVolume*)>                problem_filter;
    std::function<const uint16_t&(DrbdVolume*)>     vlm_key_func;

    void display_volume_header();
    void display_at_cursor();
    void display_at_page();
    void list_item_clicked(MouseEvent& mouse);
    void write_volume_line(DrbdVolume* const vlm, uint32_t& current_line, const bool selecting);
    bool is_problem_mode(DrbdResource* const rsc, DrbdConnection* const con);

    uint32_t get_lines_per_page();
};

#endif /* MDSPPEERVOLUMES_H */
