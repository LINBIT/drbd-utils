#ifndef MDSPRESOURCES_H
#define MDSPRESOURCES_H

#include <default_types.h>
#include <terminal/ComponentsHub.h>
#include <terminal/MDspStdListBase.h>
#include <terminal/MouseEvent.h>
#include <objects/DrbdResource.h>
#include <map_types.h>
#include <string>
#include <memory>
#include <functional>

class MDspResources : public MDspStdListBase
{
  public:
    static const uint8_t    RSC_HEADER_Y;
    static const uint8_t    RSC_LIST_Y;

    MDspResources(const ComponentsHub& comp_hub);
    virtual ~MDspResources() noexcept;

    virtual void display_list() override;
    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;

    virtual bool execute_command(const std::string& command, StringTokenizer& tokenizer);
    virtual void clear_selection() override;

    virtual void cursor_to_next_item() override;
    virtual void cursor_to_previous_item() override;

    virtual bool is_cursor_nav() override;
    virtual void reset_cursor_position() override;
    virtual void clear_cursor() override;
    virtual bool is_selecting() override;
    virtual void toggle_select_cursor_item() override;

    virtual void display_activated() override;
    virtual void display_deactivated() override;
    virtual void reset_display() override;
    virtual void synchronize_data() override;

    virtual uint64_t get_update_mask() noexcept override;

  private:
    std::string cursor_rsc;

    std::function<bool(DrbdResource*)>  problem_filter;

    void display_rsc_header();
    void display_at_cursor();
    void display_at_page();
    void display_common_unfiltered_stats();
    void list_item_clicked(MouseEvent& mouse);
    void write_resource_line(DrbdResource* const rsc, uint32_t& current_line, const bool selecting);
    void write_no_resources_line(const bool problem_mode_flag);
    bool is_problem_mode(DrbdResource* const rsc);

    uint32_t get_lines_per_page();
    DrbdResource* find_resource_near_cursor(ResourcesMap& selected_map);
    ResourcesMap& select_resources_map() const;
};

#endif /* MDSPRESOURCES_H */
