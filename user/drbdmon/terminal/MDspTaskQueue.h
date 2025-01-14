#ifndef TERMINAL_MDSPTASKQUEUE_H_
#define TERMINAL_MDSPTASKQUEUE_H_

#include <default_types.h>
#include <terminal/MDspStdListBase.h>
#include <terminal/DisplayConsts.h>
#include <terminal/SharedData.h>
#include <subprocess/SubProcessQueue.h>
#include <VList.h>

extern "C"
{
    #include <unistd.h>
}

class MDspTaskQueue : public MDspStdListBase
{
  public:
    static const uint32_t   TASK_HEADER_Y;
    static const uint32_t   TASK_LIST_Y;

    typedef SubProcessQueue::Iterator (SubProcessQueue::*iterator_func_type)();
    typedef SubProcessQueue::Iterator (SubProcessQueue::*offset_iterator_func_type)(SubProcessQueue::Entry* const);
    typedef uint64_t (SubProcessQueue::*get_cursor_func_type)();
    typedef void (SubProcessQueue::*set_cursor_func_type)(const uint64_t id);

    std::function<const uint64_t&(SubProcessQueue::Entry*)> task_id_func;

    MDspTaskQueue(
        const ComponentsHub&            comp_hub,
        SubProcessQueue&                subproc_queue_ref,
        DisplayConsts::task_queue_type  task_queue_ref,
        TaskEntryMap&                   selection_map_ref,
        iterator_func_type              iterator_func_ref,
        offset_iterator_func_type       offset_iterator_func_ref,
        get_cursor_func_type            get_cursor_func_ref,
        set_cursor_func_type            set_cursor_func_ref
    );
    virtual ~MDspTaskQueue() noexcept;

    virtual void display_task_header();
    virtual void display_list() override;

    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;
    virtual bool execute_custom_command(const std::string& command, StringTokenizer& tokenizer) override;

    virtual void cursor_to_next_item() override;
    virtual void cursor_to_previous_item() override;

    virtual bool is_cursor_nav() override;
    virtual void reset_cursor_position() override;
    virtual void clear_cursor() override;
    virtual bool is_selecting() override;
    virtual void toggle_select_cursor_item() override;
    virtual void clear_selection() override;

    virtual void display_activated() override;
    virtual void display_deactivated() override;
    virtual void reset_display() override;
    virtual void synchronize_data() override;
    virtual uint64_t get_update_mask() noexcept override;

  private:
    SubProcessQueue&                subproc_queue;
    DisplayConsts::task_queue_type  task_queue;
    TaskEntryMap&                   selection_map;
    iterator_func_type              iterator_func;
    offset_iterator_func_type       offset_iterator_func;
    get_cursor_func_type            get_cursor_func;
    set_cursor_func_type            set_cursor_func;

    class TaskData
    {
      public:
        uint64_t    task_id         {SubProcessQueue::TASKQ_NONE};
        std::string description;
        uint64_t    process_id      {0};
        int         exit_status     {-1};
    };

    using PrepTaskList = VList<TaskData>;

    std::unique_ptr<PrepTaskList>   prepared_data;

    bool cursor_nav     {false};

    static int compare_task_data(const TaskData* const key, const TaskData* const other);

    uint32_t get_lines_per_page();

    void list_item_clicked(MouseEvent& mouse);
    void preallocate_task_data(const uint16_t line_count);
    void prepare_task_line(SubProcessQueue::Entry* const task_entry, TaskData* const data);
    void write_no_tasks_line();
    void write_task_line(
        TaskData* const data,
        const bool selecting,
        const bool show_proc_id,
        const bool show_exit_status,
        uint64_t cursor_id,
        uint32_t& current_line
    );
    void clear_task_data() noexcept;
    SubProcessQueue::Entry* find_first_entry_on_cursor_page(
        const uint64_t              cursor_id,
        SubProcessQueue::Iterator&  item_iter,
        const uint32_t              lines_per_page,
        uint32_t&                   item_idx
    );
};

#endif /* TERMINAL_MDSPTASKQUEUE_H_ */
