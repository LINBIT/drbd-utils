#include <terminal/MDspTaskDetail.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/KeyCodes.h>
#include <terminal/HelpText.h>
#include <string_transformations.h>

MDspTaskDetail::MDspTaskDetail(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub),
    format_text(comp_hub)
{
}

MDspTaskDetail::~MDspTaskDetail() noexcept
{
}

void MDspTaskDetail::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_TASK_DETAIL);

    const uint32_t lines_per_page = get_lines_per_page();
    uint32_t current_line = DisplayConsts::PAGE_NAV_Y + 1;
    bool text_changed = false;

    const uint64_t task_id = dsp_comp_hub.dsp_shared->task_id;
    if (task_id != SubProcessQueue::TASKQ_NONE)
    {
        std::unique_ptr<std::string> proc_stdout;
        std::unique_ptr<std::string> proc_stderr;

        std::mutex& queue_lock = dsp_comp_hub.sub_proc_queue->get_queue_lock();
        std::unique_lock<std::mutex> lock(queue_lock);

        SubProcessQueue::Entry* const task_entry = dsp_comp_hub.sub_proc_queue->get_entry(task_id);
        if (task_entry != nullptr)
        {
            if (task_id != saved_task_id)
            {
                if (saved_task_id != SubProcessQueue::TASKQ_NONE)
                {
                    reset();
                }
                saved_task_id = task_id;
            }

            SubProcessQueue::entry_state_type local_task_state =
                dsp_comp_hub.sub_proc_queue->get_entry_state(task_entry);

            if (local_task_state != task_state)
            {
                if (local_task_state == SubProcessQueue::entry_state_type::ACTIVE)
                {
                    SubProcess* const task_proc = task_entry->get_process();
                    if (task_proc != nullptr)
                    {
                        proc_id = task_proc->get_pid();
                    }
                }
                else
                if (local_task_state == SubProcessQueue::entry_state_type::ENDED && !fin_initialized)
                {
                    SubProcess* const task_proc = task_entry->get_process();
                    if (task_proc != nullptr)
                    {
                        exit_code = task_proc->get_exit_status();

                        proc_stdout = std::unique_ptr<std::string>(new std::string());
                        proc_stderr = std::unique_ptr<std::string>(new std::string());

                        *proc_stdout = task_proc->get_stdout_output();
                        *proc_stderr = task_proc->get_stderr_output();
                    }
                }

                task_state = local_task_state;
            }

            if (!info_initialized)
            {
                const CmdLine* const command = task_entry->get_command();
                if (command != nullptr)
                {
                    proc_info.reserve(0x400);
                    proc_info.append("\x1B\x02" "Command line:" "\x1B\xFF" "\n");
                    VList<std::string>::ValuesIterator arg_iter = command->get_argument_iterator();
                    bool space_flag = false;
                    while (arg_iter.has_next())
                    {
                        const std::string* const cmd_arg = arg_iter.next();
                        if (cmd_arg != nullptr)
                        {
                            if (space_flag)
                            {
                                proc_info.append(" ");
                            }
                            proc_info.append(*cmd_arg);

                            space_flag = true;
                        }
                    }
                }

                format_text.set_text(&proc_info);
                text_changed = true;

                info_initialized = true;
            }

            lock.unlock();

            if (!fin_initialized && proc_stdout != nullptr && proc_stderr != nullptr)
            {
                std::string filtered_stdout;
                std::string filtered_stderr;

                string_transformations::replace_escape(*proc_stdout, filtered_stdout);
                string_transformations::replace_escape(*proc_stderr, filtered_stderr);

                const size_t proc_stdout_length = filtered_stdout.length();
                const size_t proc_stderr_length = filtered_stderr.length();

                const bool have_proc_stdout = proc_stdout_length > 0;
                const bool have_proc_stderr = proc_stderr_length > 0;

                proc_info.append("\n\n");
                if (have_proc_stdout || have_proc_stderr)
                {
                    proc_info.reserve(proc_info.length() + proc_stdout_length + proc_stderr_length + 0x400);
                    if (have_proc_stdout)
                    {
                        proc_info.append("\x1B\x02" "Process stdout output:" "\x1B\xFF" "\n");
                        proc_info.append(filtered_stdout);
                    }
                    else
                    {
                        proc_info.append("\x1B\x02" "No process stdout output to display" "\x1B\xFF" "\n");
                    }

                    if (have_proc_stderr)
                    {
                        proc_info.append("\n\n");
                        proc_info.append("\x1B\x02" "Process stderr output:" "\x1B\xFF" "\n");
                        proc_info.append(filtered_stderr);
                    }
                    else
                    {
                        proc_info.append("\n\n");
                        proc_info.append("\x1B\x02" "No process stderr output to display" "\x1B\xFF" "\n");
                    }
                }
                else
                {
                    proc_info.append("\x1B\x02" "No process output to display" "\x1B\xFF" "\n");
                }

                format_text.set_text(&proc_info);
                text_changed = true;

                fin_initialized = true;
            }

            {
                const uint32_t page_nr = get_page_nr();
                if (page_nr == 1)
                {
                    // Task state
                    dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
                    dsp_comp_hub.dsp_io->write_text("Task state:");
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
                    if (task_state == SubProcessQueue::entry_state_type::ACTIVE)
                    {
                        dsp_comp_hub.dsp_io->write_text("Active");
                    }
                    else
                    if (task_state == SubProcessQueue::entry_state_type::READY)
                    {
                        dsp_comp_hub.dsp_io->write_text("Pending");
                    }
                    else
                    if (task_state == SubProcessQueue::entry_state_type::WAITING)
                    {
                        dsp_comp_hub.dsp_io->write_text("Suspended");
                    }
                    else
                    if (task_state == SubProcessQueue::entry_state_type::ENDED)
                    {
                        dsp_comp_hub.dsp_io->write_text("Finished");
                    }
                    else
                    {
                        dsp_comp_hub.dsp_io->write_text("Unknown/Invalid");
                    }
                    ++current_line;

                    // Process ID
                    dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
                    dsp_comp_hub.dsp_io->write_text("Process ID:");
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
                    if (task_state == SubProcessQueue::entry_state_type::ACTIVE)
                    {
                        dsp_comp_hub.dsp_io->write_fmt("%llu", static_cast<unsigned long long> (proc_id));
                    }
                    else
                    {
                        dsp_comp_hub.dsp_io->write_text("N/A");
                    }
                    ++current_line;

                    // Exit code
                    dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
                    dsp_comp_hub.dsp_io->write_text("Exit code:");
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
                    if (task_state == SubProcessQueue::entry_state_type::ENDED)
                    {
                        dsp_comp_hub.dsp_io->write_fmt("%d", exit_code);
                    }
                    else
                    {
                        dsp_comp_hub.dsp_io->write_text("N/A");
                    }
                    current_line += 2;
                }
            }

            // Number of lines outside of the TextColumn instance on the first page
            const uint32_t first_page_lines = 4;
            const uint32_t line_offset = current_line - DisplayConsts::PAGE_NAV_Y - 1;

            // Adjust TextColumn
            uint32_t page_count = get_page_count();
            if (text_changed || dsp_comp_hub.term_cols != saved_term_cols || dsp_comp_hub.term_rows != saved_term_rows)
            {
                format_text.restart();
                format_text.set_line_length(dsp_comp_hub.term_cols);
                uint32_t line_ctr = 0;
                while (format_text.skip_line())
                {
                    ++line_ctr;
                }
                page_count = dsp_comp_hub.dsp_common->calculate_page_count(
                    line_ctr + first_page_lines, lines_per_page
                );
                set_page_count(page_count);
            }

            uint32_t page_nr = get_page_nr();
            if (page_nr > page_count)
            {
                page_nr = page_count;
                set_page_nr(page_nr);
            }

            // Skip to the selected page
            uint32_t page_ctr = 1;
            uint32_t page_line_ctr = first_page_lines;
            format_text.restart();
            uint32_t skip_ctr = 0;
            while (page_ctr < page_nr && format_text.skip_line())
            {
                ++skip_ctr;
                ++page_line_ctr;
                if (page_line_ctr >= lines_per_page)
                {
                    page_line_ctr = 0;
                    ++page_ctr;
                }
            }

            std::string line;
            uint32_t line_ctr = page_nr == 1 ? line_offset : 0;
            uint32_t print_ctr = 0;
            while (format_text.next_line(line, dsp_comp_hub.active_color_table->rst) && line_ctr < lines_per_page)
            {
                ++print_ctr;
                dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
                dsp_comp_hub.dsp_io->write_text(line.c_str());
                ++current_line;
                ++line_ctr;
            }
        }
        else
        {
            dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
            dsp_comp_hub.dsp_io->write_text("The selected task entry has been discarded.");
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
            ++current_line;

            if (info_initialized)
            {
                reset();
            }
        }
    }
    else
    {
        dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 1);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("No selected task");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
        ++current_line;

        if (info_initialized)
        {
            reset();
        }
    }
}

uint64_t MDspTaskDetail::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_TASK_QUEUE;
}

bool MDspTaskDetail::key_pressed(const uint32_t key)
{
    bool intercepted = MDspBase::key_pressed(key);
    if (!intercepted)
    {
        const uint64_t task_id = dsp_comp_hub.dsp_shared->task_id;
        if (key == KeyCodes::DELETE)
        {
            if (task_id != SubProcessQueue::TASKQ_NONE)
            {
                if (is_task_state(task_id, SubProcessQueue::entry_state_type::ACTIVE))
                {
                    dsp_comp_hub.sub_proc_queue->terminate_task(task_id, true);
                }
            }
            intercepted = true;
        }
        else
        if (key == KeyCodes::BACKSPACE || key == static_cast<uint32_t> ('s') || key == static_cast<uint32_t> ('S'))
        {
            if (task_id != SubProcessQueue::TASKQ_NONE)
            {
                if (is_task_state(task_id, SubProcessQueue::entry_state_type::READY))
                {
                    dsp_comp_hub.sub_proc_queue->inactivate_entry(task_id);
                }
            }
            intercepted = true;
        }
        else
        if (key == KeyCodes::INSERT || key == static_cast<uint32_t> ('p') || key == static_cast<uint32_t> ('P'))
        {
            if (task_id != SubProcessQueue::TASKQ_NONE)
            {
                if (is_task_state(task_id, SubProcessQueue::entry_state_type::WAITING))
                {
                    dsp_comp_hub.sub_proc_queue->activate_entry(task_id);
                }
            }
            intercepted = true;
        }
        else
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::TASK_DETAIL, dsp_comp_hub);
            intercepted = true;
        }
    }
    return intercepted;
}

void MDspTaskDetail::text_cursor_ops()
{
    // no-op; prevents MDspMenuBase from positioning the cursor for the option field, which is not used
}

void MDspTaskDetail::display_activated()
{
    MDspBase::display_activated();
}

void MDspTaskDetail::display_deactivated()
{
    MDspBase::display_deactivated();
}

void MDspTaskDetail::display_closed()
{
    MDspBase::display_closed();
    reset();
}

void MDspTaskDetail::reset()
{
    format_text.reset();
    saved_term_cols     = 0;
    saved_term_rows     = 0;
    fin_initialized     = false;
    info_initialized    = false;
    proc_id             = 0;
    exit_code           = 0;
    proc_info.clear();
    task_state          = SubProcessQueue::entry_state_type::INVALID_ID;
    saved_task_id       = SubProcessQueue::TASKQ_NONE;
}

bool MDspTaskDetail::is_task_state(const uint64_t task_id, const SubProcessQueue::entry_state_type query_task_state)
{
    bool result = false;

    std::mutex& queue_lock = dsp_comp_hub.sub_proc_queue->get_queue_lock();
    std::unique_lock<std::mutex> lock(queue_lock);

    SubProcessQueue::Entry* const task_entry = dsp_comp_hub.sub_proc_queue->get_entry(task_id);
    if (task_entry != nullptr)
    {
        const SubProcessQueue::entry_state_type actual_task_state =
            dsp_comp_hub.sub_proc_queue->get_entry_state(task_entry);
        result = actual_task_state == query_task_state;
    }

    return result;
}

uint32_t MDspTaskDetail::get_lines_per_page() noexcept
{
    return dsp_comp_hub.term_rows - DisplayConsts::PAGE_NAV_Y - 2;
}
