#ifndef MDSPLOGVIEWER_H
#define MDSPLOGVIEWER_H

#include <default_types.h>
#include <MessageLog.h>
#include <map_types.h>
#include <terminal/MDspStdListBase.h>
#include <functional>

class MDspLogViewer : public MDspStdListBase
{
  public:
    std::function<const uint64_t&(MessageLog::Entry*)> log_key_func;

    static const uint8_t    LOG_HEADER_Y;
    static const uint8_t    LOG_LIST_Y;

    MDspLogViewer(const ComponentsHub& comp_hub, MessageLog& log_ref);
    virtual ~MDspLogViewer() noexcept;

    virtual void display_list() override;

    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;

    virtual void display_activated() override;
    virtual void display_deactivated() override;
    virtual void reset_display() override;
    virtual void synchronize_data() override;

    virtual void cursor_to_next_item() override;
    virtual void cursor_to_previous_item() override;

    virtual bool is_cursor_nav() override;
    virtual void reset_cursor_position() override;
    virtual void clear_cursor() override;
    virtual bool is_selecting() override;
    virtual void toggle_select_cursor_item() override;
    virtual void clear_selection() override;

    virtual uint64_t get_update_mask() noexcept override;

  private:
    MessageLog& log;

    uint32_t get_lines_per_page() noexcept;
    void list_item_clicked(MouseEvent& mouse);
    void display_log_header();
    void write_log_line(MessageLog::Entry* const msg_entry, const bool selecting, uint32_t& current_line);
    void cursor_to_nearest_item();
    // Returns a reference to the message ID that is shared between displays
    uint64_t& get_message_id() noexcept;
    // Returns a reference to the map of selected log entries
    MessageMap& get_selected_log_entries() noexcept;
};

#endif /* MDSPLOGVIEWER_H */
