#ifndef MDSPCONNECTIONS_H
#define MDSPCONNECTIONS_H

#include <terminal/MDspStdListBase.h>
#include <terminal/ComponentsHub.h>
#include <terminal/MouseEvent.h>
#include <objects/DrbdResource.h>
#include <objects/DrbdConnection.h>
#include <map_types.h>
#include <cstdint>
#include <string>
#include <memory>
#include <functional>

class MDspConnections : public MDspStdListBase
{
  public:
    static const uint8_t    RSC_HEADER_Y;
    static const uint8_t    CON_HEADER_Y;
    static const uint8_t    CON_LIST_Y;

    MDspConnections(const ComponentsHub& comp_hub);
    virtual ~MDspConnections() noexcept;

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
    std::string displayed_rsc;

    std::string cursor_con;

    std::function<bool(DrbdConnection*)>                problem_filter;
    std::function<const std::string&(DrbdConnection*)>  con_key_func;

    void display_at_cursor();
    void display_at_page();
    void display_common_unfiltered_stats();
    void reset_cursor_connection();
    void list_item_clicked(MouseEvent& mouse);
    void write_connection_line(DrbdConnection* const con, uint32_t& current_line, const bool selecting);
    void write_no_connections_line(const bool problem_mode_flag);
    void monitor_cursor_common_keys(const uint32_t key);
    bool is_problem_mode(DrbdResource* const rsc);

    uint32_t get_lines_per_page();
};

#endif /* MDSPCONNECTIONS_H */
