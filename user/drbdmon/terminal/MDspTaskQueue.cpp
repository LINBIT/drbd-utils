#include <terminal/MDspTaskQueue.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/navigation.h>
#include <terminal/KeyCodes.h>
#include <terminal/HelpText.h>
#include <terminal/GlobalCommandConsts.h>
#include <subprocess/SubProcessQueue.h>
#include <new>
#include <memory>
#include <string>
#include <mutex>

const uint32_t  MDspTaskQueue::TASK_HEADER_Y    = 4;
const uint32_t  MDspTaskQueue::TASK_LIST_Y      = 5;

MDspTaskQueue::MDspTaskQueue(
        const ComponentsHub&            comp_hub,
        SubProcessQueue&                subproc_queue_ref,
        DisplayConsts::task_queue_type  task_queue_ref,
        TaskEntryMap&                   selection_map_ref,
        iterator_func_type              iterator_func_ref,
        offset_iterator_func_type       offset_iterator_func_ref,
        get_cursor_func_type            get_cursor_func_ref,
        set_cursor_func_type            set_cursor_func_ref
):
    MDspStdListBase(comp_hub),
    subproc_queue(subproc_queue_ref),
    task_queue(task_queue_ref),
    selection_map(selection_map_ref),
    iterator_func(iterator_func_ref),
    offset_iterator_func(offset_iterator_func_ref),
    get_cursor_func(get_cursor_func_ref),
    set_cursor_func(set_cursor_func_ref)
{
    task_id_func =
        [](SubProcessQueue::Entry* task_entry) -> const uint64_t&
        {
            return task_entry->get_id_ref();
        };

    prepared_data = std::unique_ptr<PrepTaskList>(new PrepTaskList(&MDspTaskQueue::compare_task_data));
}

MDspTaskQueue::~MDspTaskQueue() noexcept
{
    clear_task_data();
}

void MDspTaskQueue::display_task_header()
{
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->list_header.c_str());
    dsp_comp_hub.dsp_io->cursor_xy(1, TASK_HEADER_Y);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_LINE.c_str());

    dsp_comp_hub.dsp_io->cursor_xy(1, TASK_HEADER_Y);
    dsp_comp_hub.dsp_io->write_text("Task description");

    // Right aligned
    if (task_queue == DisplayConsts::task_queue_type::ACTIVE_QUEUE)
    {
        dsp_comp_hub.dsp_io->cursor_xy(dsp_comp_hub.term_cols - 10, TASK_HEADER_Y);
        dsp_comp_hub.dsp_io->write_text("Process ID");
    }

    // Right aligned
    if (task_queue == DisplayConsts::task_queue_type::FINISHED_QUEUE)
    {
        dsp_comp_hub.dsp_io->cursor_xy(dsp_comp_hub.term_cols - 3, TASK_HEADER_Y);
        dsp_comp_hub.dsp_io->write_fmt("ExC");
    }

    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
}

void MDspTaskQueue::display_list()
{
    const std::string* page_id_ptr = &DisplayId::MDSP_TASKQ_ACT;
    switch (task_queue)
    {
        case DisplayConsts::task_queue_type::PENDING_QUEUE:
            page_id_ptr = &DisplayId::MDSP_TASKQ_PND;
            break;
        case DisplayConsts::task_queue_type::SUSPENDED_QUEUE:
            page_id_ptr = &DisplayId::MDSP_TASKQ_SSP;
            break;
        case DisplayConsts::task_queue_type::FINISHED_QUEUE:
            page_id_ptr = &DisplayId::MDSP_TASKQ_FIN;
            break;
        case DisplayConsts::task_queue_type::ACTIVE_QUEUE:
            // no-op
            break;
        default:
            // no-op
            break;
    }
    dsp_comp_hub.dsp_common->display_page_id(*page_id_ptr);

    display_task_header();

    uint64_t cursor_id = SubProcessQueue::TASKQ_NONE;
    const uint32_t lines_per_page = get_lines_per_page();

    preallocate_task_data(static_cast<uint16_t> (lines_per_page));

    uint32_t line_count = 0;
    std::mutex& queue_lock = subproc_queue.get_queue_lock();
    {
        std::unique_lock<std::mutex> lock(queue_lock);

        cursor_id = (subproc_queue.*get_cursor_func)();
        cursor_nav = cursor_id != SubProcessQueue::TASKQ_NONE;

        if (cursor_nav)
        {
            // Cursor navigation
            uint32_t dsp_task_idx = 0;

            SubProcessQueue::Iterator task_iter = (subproc_queue.*iterator_func)();
            const uint32_t task_count = static_cast<uint32_t> (task_iter.get_size());

            SubProcessQueue::Entry* page_first_task = find_first_entry_on_cursor_page(
                cursor_id, task_iter, lines_per_page, dsp_task_idx
            );

            set_page_nr((dsp_task_idx / lines_per_page) + 1);
            set_page_count(dsp_comp_hub.dsp_common->calculate_page_count(task_count, lines_per_page));

            if (page_first_task != nullptr)
            {
                SubProcessQueue::Iterator dsp_task_iter = (subproc_queue.*offset_iterator_func)(page_first_task);
                PrepTaskList::ValuesIterator prep_iter(*prepared_data);
                while (dsp_task_iter.has_next() && prep_iter.has_next() && line_count < lines_per_page)
                {
                    TaskData* const prep_data = prep_iter.next();
                    SubProcessQueue::Entry* const task_entry = dsp_task_iter.next();
                    prepare_task_line(task_entry, prep_data);
                    ++line_count;
                }
            }
        }
        else
        {
            // Page navigation
            SubProcessQueue::Iterator task_iter = (subproc_queue.*iterator_func)();
            set_page_count(
                dsp_comp_hub.dsp_common->calculate_page_count(
                    static_cast<uint32_t> (task_iter.get_size()),
                    lines_per_page
                )
            );

            navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, task_iter);
            PrepTaskList::ValuesIterator prep_iter(*prepared_data);
            while (task_iter.has_next() && prep_iter.has_next() && line_count < lines_per_page)
            {
                SubProcessQueue::Entry* const task_entry = task_iter.next();
                TaskData* const prep_data = prep_iter.next();
                prepare_task_line(task_entry, prep_data);
                ++line_count;
            }
        }

    }

    if (line_count == 0)
    {
        write_no_tasks_line();
    }
    else
    {
        const bool selecting = is_selecting();
        const bool show_proc_id = task_queue == DisplayConsts::task_queue_type::ACTIVE_QUEUE;
        const bool show_exit_status = task_queue == DisplayConsts::task_queue_type::FINISHED_QUEUE;
        uint32_t current_line = TASK_LIST_Y;
        uint32_t line_nr = 0;
        PrepTaskList::ValuesIterator prep_iter(*prepared_data);
        while (prep_iter.has_next() && line_nr < line_count)
        {
            TaskData* const prep_data = prep_iter.next();
            write_task_line(prep_data, selecting, show_proc_id, show_exit_status, cursor_id, current_line);
            ++line_nr;
        }
    }

    clear_task_data();
}

bool MDspTaskQueue::key_pressed(const uint32_t key)
{
    bool intercepted = MDspStdListBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::ENTER)
        {
            if (cursor_nav)
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::MDSP_TASK_DETAIL);
            }
            intercepted = true;
        }
        else
        if (key == KeyCodes::DELETE)
        {
            if (task_queue == DisplayConsts::task_queue_type::ACTIVE_QUEUE)
            {
                if (is_selecting())
                {
                    TaskEntryMap::KeysIterator task_iter(selection_map);
                    while (task_iter.has_next())
                    {
                        const uint64_t* const entry_id_ptr = task_iter.next();
                        subproc_queue.terminate_task(*entry_id_ptr, true);
                    }
                }
                else
                if (is_cursor_nav())
                {
                    const uint64_t entry_id = (subproc_queue.*get_cursor_func)();
                    subproc_queue.terminate_task(entry_id, true);
                }
            }
            else
            {
                if (is_selecting())
                {
                    TaskEntryMap::KeysIterator task_iter(selection_map);
                    while (task_iter.has_next())
                    {
                        const uint64_t* const entry_id_ptr = task_iter.next();
                        subproc_queue.remove_entry(*entry_id_ptr);
                    }
                    dsp_comp_hub.dsp_shared->clear_task_selection(selection_map);
                }
                else
                if (is_cursor_nav())
                {
                    const uint64_t entry_id = (subproc_queue.*get_cursor_func)();
                    subproc_queue.remove_entry(entry_id);
                }
            }
            intercepted = true;
        }
        else
        if (key == KeyCodes::BACKSPACE || key == static_cast<uint32_t> ('s') || key == static_cast<uint32_t> ('S'))
        {
            if (task_queue == DisplayConsts::task_queue_type::PENDING_QUEUE)
            {
                if (selection_map.get_size() >= 1)
                {
                    TaskEntryMap::KeysIterator task_iter(selection_map);
                    while (task_iter.has_next())
                    {
                        const uint64_t* const entry_id_ptr = task_iter.next();
                        subproc_queue.inactivate_entry(*entry_id_ptr);
                    }
                    dsp_comp_hub.dsp_shared->clear_task_selection(selection_map);
                }
                else
                if (is_cursor_nav())
                {
                    const uint64_t entry_id = (subproc_queue.*get_cursor_func)();
                    subproc_queue.inactivate_entry(entry_id);
                }
            }
            intercepted = true;
        }
        else
        if (key == KeyCodes::INSERT || key == static_cast<uint32_t> ('p') || key == static_cast<uint32_t> ('P'))
        {
            if (task_queue == DisplayConsts::task_queue_type::SUSPENDED_QUEUE)
            {
                if (selection_map.get_size() >= 1)
                {
                    TaskEntryMap::KeysIterator task_iter(selection_map);
                    while (task_iter.has_next())
                    {
                        const uint64_t* const entry_id_ptr = task_iter.next();
                        subproc_queue.activate_entry(*entry_id_ptr);
                    }
                    dsp_comp_hub.dsp_shared->clear_task_selection(selection_map);
                }
                else
                if (is_cursor_nav())
                {
                    const uint64_t entry_id = (subproc_queue.*get_cursor_func)();
                    subproc_queue.activate_entry(entry_id);
                }
            }
            intercepted = true;
        }
        else
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::TASK_QUEUE, dsp_comp_hub);
            intercepted = true;
        }
    }
    return intercepted;
}

bool MDspTaskQueue::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspStdListBase::mouse_action(mouse);
    if (!intercepted)
    {
        if ((mouse.button == MouseEvent::button_id::BUTTON_01 || mouse.button == MouseEvent::button_id::BUTTON_03) &&
            mouse.event == MouseEvent::event_id::MOUSE_RELEASE &&
            mouse.coord_row >= TASK_LIST_Y)
        {
            const uint32_t lines_per_page = get_lines_per_page();
            if (mouse.coord_row < TASK_LIST_Y + lines_per_page)
            {
                list_item_clicked(mouse);
                if (mouse.button == MouseEvent::button_id::BUTTON_03 && is_cursor_nav())
                {
                    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::MDSP_TASK_DETAIL);
                }
                intercepted = true;
            }
        }
    }
    return intercepted;
}

bool MDspTaskQueue::execute_custom_command(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = false;
    if (command == cmd_names::KEY_CMD_SELECT_ALL)
    {
        std::mutex& queue_lock = subproc_queue.get_queue_lock();
        std::unique_lock<std::mutex> lock(queue_lock);

        SubProcessQueue::Iterator task_iter = (subproc_queue.*iterator_func)();
        while (task_iter.has_next())
        {
            SubProcessQueue::Entry* const task_entry = task_iter.next();
            const uint64_t entry_id = task_entry->get_id();
            dsp_comp_hub.dsp_shared->select_task(selection_map, entry_id);
        }
        dsp_comp_hub.dsp_selector->refresh_display();
        accepted = true;
    }
    else
    if (command == cmd_names::KEY_CMD_DESELECT_ALL || command == cmd_names::KEY_CMD_CLEAR_SELECTION)
    {
        dsp_comp_hub.dsp_shared->clear_task_selection(selection_map);
        dsp_comp_hub.dsp_selector->refresh_display();
        accepted = true;
    }
    return accepted;
}

void MDspTaskQueue::list_item_clicked(MouseEvent& mouse)
{
    const uint32_t lines_per_page = get_lines_per_page();
    const uint32_t selected_line = mouse.coord_row - TASK_LIST_Y;

    uint64_t cursor_id = SubProcessQueue::TASKQ_NONE;
    std::mutex& queue_lock = subproc_queue.get_queue_lock();
    {
        std::unique_lock<std::mutex> lock(queue_lock);

        SubProcessQueue::Iterator task_iter = (subproc_queue.*iterator_func)();
        navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, task_iter);

        uint32_t line_ctr = 0;
        while (task_iter.has_next() && line_ctr < selected_line)
        {
            task_iter.next();
            ++line_ctr;
        }
        if (task_iter.has_next())
        {
            const SubProcessQueue::Entry* const entry = task_iter.next();
            cursor_id = entry->get_id();
            (subproc_queue.*set_cursor_func)(cursor_id);
        }
    }

    if (cursor_id != SubProcessQueue::TASKQ_NONE)
    {
        if (mouse.coord_column <= DisplayConsts::MAX_SELECT_X)
        {
            toggle_select_cursor_item();
        }
        dsp_comp_hub.dsp_selector->refresh_display();
    }
}

void MDspTaskQueue::cursor_to_next_item()
{
    std::mutex& queue_lock = subproc_queue.get_queue_lock();
    std::unique_lock<std::mutex> lock(queue_lock);

    const uint64_t cursor_id = (subproc_queue.*get_cursor_func)();
    if (cursor_id != SubProcessQueue::TASKQ_NONE)
    {
        SubProcessQueue::Iterator task_iter = (subproc_queue.*iterator_func)();
        while (task_iter.has_next())
        {
            const SubProcessQueue::Entry* const task_entry = task_iter.next();
            const uint64_t task_id = task_entry->get_id();
            if (task_id == cursor_id)
            {
                break;
            }
        }
        if (task_iter.has_next())
        {
            const SubProcessQueue::Entry* const next_task_entry = task_iter.next();
            const uint64_t next_task_id = next_task_entry->get_id();
            (subproc_queue.*set_cursor_func)(next_task_id);
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
}

void MDspTaskQueue::cursor_to_previous_item()
{
    std::mutex& queue_lock = subproc_queue.get_queue_lock();
    std::unique_lock<std::mutex> lock(queue_lock);

    const uint64_t cursor_id = (subproc_queue.*get_cursor_func)();
    if (cursor_id != SubProcessQueue::TASKQ_NONE)
    {
        uint64_t prev_task_id = SubProcessQueue::TASKQ_NONE;
        SubProcessQueue::Iterator task_iter = (subproc_queue.*iterator_func)();
        while (task_iter.has_next())
        {
            const SubProcessQueue::Entry* const task_entry = task_iter.next();
            const uint64_t task_id = task_entry->get_id();
            if (task_id == cursor_id)
            {
                break;
            }
            prev_task_id = task_id;
        }
        if (prev_task_id != SubProcessQueue::TASKQ_NONE)
        {
            (subproc_queue.*set_cursor_func)(prev_task_id);
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
}

bool MDspTaskQueue::is_cursor_nav()
{
    return cursor_nav;
}

void MDspTaskQueue::reset_cursor_position()
{
    uint64_t cursor_id = SubProcessQueue::TASKQ_NONE;
    const uint32_t lines_per_page = get_lines_per_page();

    std::mutex& queue_lock = subproc_queue.get_queue_lock();
    std::unique_lock<std::mutex> lock(queue_lock);

    SubProcessQueue::Iterator task_iter = (subproc_queue.*iterator_func)();
    navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, task_iter);
    if (task_iter.has_next())
    {
        const SubProcessQueue::Entry* const task_entry = task_iter.next();
        cursor_id = task_entry->get_id();
    }
    (subproc_queue.*set_cursor_func)(cursor_id);
}

void MDspTaskQueue::clear_cursor()
{
    (subproc_queue.*set_cursor_func)(SubProcessQueue::TASKQ_NONE);
}

bool MDspTaskQueue::is_selecting()
{
    return dsp_comp_hub.dsp_shared->have_task_selection(selection_map);
}

void MDspTaskQueue::toggle_select_cursor_item()
{
    uint64_t cursor_id = SubProcessQueue::TASKQ_NONE;
    {
        std::mutex& queue_lock = subproc_queue.get_queue_lock();
        std::unique_lock<std::mutex> lock(queue_lock);

        cursor_id = (subproc_queue.*get_cursor_func)();
    }
    if (cursor_id != SubProcessQueue::TASKQ_NONE)
    {
        dsp_comp_hub.dsp_shared->toggle_task_selection(selection_map, cursor_id);
    }
}

void MDspTaskQueue::clear_selection()
{
    dsp_comp_hub.dsp_shared->clear_task_selection(selection_map);
}

uint32_t MDspTaskQueue::get_lines_per_page()
{
    return dsp_comp_hub.term_rows - TASK_LIST_Y - 2;
}

int MDspTaskQueue::compare_task_data(const TaskData* const key, const TaskData* const other)
{
    int result = 0;
    if (key != nullptr && other != nullptr)
    {
        if (key->task_id < other->task_id)
        {
            result = -1;
        }
        else
        if (key->task_id > other->task_id)
        {
            result = 1;
        }
    }
    else
    if (key != nullptr || other != nullptr)
    {
        result = (key == nullptr ? -1 : 1);
    }
    return result;
}

void MDspTaskQueue::display_activated()
{
    MDspStdListBase::display_activated();
}

void MDspTaskQueue::display_deactivated()
{
    MDspStdListBase::display_deactivated();
    clear_task_data();
}

void MDspTaskQueue::reset_display()
{
    MDspStdListBase::reset_display();
}

void MDspTaskQueue::synchronize_data()
{
    dsp_comp_hub.dsp_shared->task_id = (subproc_queue.*get_cursor_func)();
}

uint64_t MDspTaskQueue::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_TASK_QUEUE;
}

void MDspTaskQueue::preallocate_task_data(const uint16_t line_count)
{
    std::unique_ptr<TaskData> entry;
    for (uint32_t idx = 0; idx < line_count; ++idx)
    {
        entry = std::unique_ptr<TaskData>(new TaskData());
        prepared_data->append(entry.get());
        entry.release();
    }
}

void MDspTaskQueue::prepare_task_line(SubProcessQueue::Entry* const task_entry, TaskData* const data)
{
    data->task_id = task_entry->get_id();
    const CmdLine* const command = task_entry->get_command();
    if (command != nullptr)
    {
        data->description = command->get_description();
    }
    SubProcess* const subproc = task_entry->get_process();
    if (subproc != nullptr)
    {
        data->process_id = subproc->get_pid();
        data->exit_status = subproc->get_exit_status();
    }
}

void MDspTaskQueue::write_no_tasks_line()
{
    dsp_comp_hub.dsp_io->cursor_xy(1, TASK_LIST_Y);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
    dsp_comp_hub.dsp_io->write_text("This task queue is empty");
}

void MDspTaskQueue::write_task_line(
    TaskData* const data,
    const bool selecting,
    const bool show_proc_id,
    const bool show_exit_status,
    uint64_t cursor_id,
    uint32_t& current_line
)
{
    const bool is_under_cursor = data->task_id == cursor_id;
    bool is_selected = false;
    if (selecting)
    {
        if (selection_map.get_node(&(data->task_id)) != nullptr)
        {
            is_selected = true;
        }
    }

    const std::string& rst_bg =
        (is_under_cursor ? dsp_comp_hub.active_color_table->bg_cursor :
        (is_selected ? dsp_comp_hub.active_color_table->bg_marked : dsp_comp_hub.active_color_table->rst_bg));

    dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
    dsp_comp_hub.dsp_io->write_text(rst_bg.c_str());
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_LINE.c_str());
    if (is_selected)
    {
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_marked.c_str());
    }
    else
    {
        dsp_comp_hub.dsp_io->write_char(' ');
    }

    dsp_comp_hub.dsp_io->write_char(' ');

    dsp_comp_hub.dsp_io->write_string_field(data->description, dsp_comp_hub.term_cols - 32, false);

    if (show_proc_id)
    {
        if (data->process_id != 0)
        {
            dsp_comp_hub.dsp_io->cursor_xy(dsp_comp_hub.term_cols - 24, current_line);
            dsp_comp_hub.dsp_io->write_fmt("%24llu", static_cast<unsigned long long> (data->process_id));
        }
        else
        {
            dsp_comp_hub.dsp_io->cursor_xy(dsp_comp_hub.term_cols - 3, current_line);
            dsp_comp_hub.dsp_io->write_text("N/A");
        }
    }

    if (show_exit_status)
    {
        if (data->exit_status == SubProcess::EXIT_STATUS_NONE)
        {
            dsp_comp_hub.dsp_io->cursor_xy(dsp_comp_hub.term_cols - 3, current_line);
            dsp_comp_hub.dsp_io->write_text("N/A");
        }
        else
        if (data->exit_status == SubProcess::EXIT_STATUS_FAILED)
        {
            dsp_comp_hub.dsp_io->cursor_xy(dsp_comp_hub.term_cols - 8, current_line);
            dsp_comp_hub.dsp_io->write_text("(FAILED)");
        }
        else
        {
            dsp_comp_hub.dsp_io->cursor_xy(dsp_comp_hub.term_cols - 3, current_line);
            dsp_comp_hub.dsp_io->write_fmt("%3d", static_cast<int> (data->exit_status & 0xFF));
        }
    }

    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    ++current_line;
}

void MDspTaskQueue::clear_task_data() noexcept
{
    PrepTaskList::ValuesIterator iter(*prepared_data);
    while (iter.has_next())
    {
        TaskData* const entry = iter.next();
        delete entry;
    }
    prepared_data->clear();
}

SubProcessQueue::Entry* MDspTaskQueue::find_first_entry_on_cursor_page(
    const uint64_t              cursor_id,
    SubProcessQueue::Iterator&  task_iter,
    const uint32_t              lines_per_page,
    uint32_t&                   entry_idx
)
{
    uint32_t idx = 0;

    SubProcessQueue::Entry* page_first_entry = nullptr;
    uint32_t page_line_ctr = 0;
    while (task_iter.has_next())
    {
        SubProcessQueue::Entry* const entry = task_iter.next();
        const uint64_t entry_id = entry->get_id();
        if (page_line_ctr == 0)
        {
            page_first_entry = entry;
        }
        if (entry_id == cursor_id)
        {
            break;
        }
        ++idx;
        ++page_line_ctr;
        if (page_line_ctr >= lines_per_page)
        {
            page_line_ctr = 0;
        }
    }

    entry_idx = idx;

    return page_first_entry;
}
