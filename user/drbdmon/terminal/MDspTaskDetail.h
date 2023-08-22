#ifndef MDSPTASKDETAIL_H
#define MDSPTASKDETAIL_H

#include <default_types.h>
#include <terminal/MDspMenuBase.h>
#include <terminal/TextColumn.h>
#include <subprocess/SubProcessQueue.h>
#include <string>

class MDspTaskDetail : public MDspMenuBase
{
  public:
    MDspTaskDetail(const ComponentsHub& comp_hub);
    virtual ~MDspTaskDetail() noexcept;

    virtual void display_content() override;
    virtual uint64_t get_update_mask() noexcept override;
    virtual bool key_pressed(const uint32_t key) override;

    virtual void text_cursor_ops() override;

    virtual void display_activated() override;
    virtual void display_deactivated() override;
    virtual void display_closed() override;

  private:
    TextColumn  format_text;
    uint16_t    saved_term_cols     {0};
    uint16_t    saved_term_rows     {0};
    bool        fin_initialized     {false};
    bool        info_initialized    {false};
    uint64_t    proc_id             {0};
    int         exit_code           {0};
    std::string proc_info;
    SubProcessQueue::entry_state_type task_state {SubProcessQueue::entry_state_type::INVALID_ID};

    std::function<void()>   cmd_fn_suspend;
    std::function<void()>   cmd_fn_make_pending;
    std::function<void()>   cmd_fn_terminate;
    std::function<void()>   cmd_fn_remove;

    std::unique_ptr<ClickableCommand>   cmd_suspend;
    std::unique_ptr<ClickableCommand>   cmd_make_pending;
    std::unique_ptr<ClickableCommand>   cmd_terminate;
    std::unique_ptr<ClickableCommand>   cmd_remove;

    void opt_actions();
    void opt_make_pending();
    void opt_terminate();
    void opt_remove();

    void reset();
    bool is_task_state(const uint64_t task_id, const SubProcessQueue::entry_state_type task_state);

    uint32_t get_lines_per_page() noexcept;
};

#endif /* MDSPTASKDETAIL_H */
