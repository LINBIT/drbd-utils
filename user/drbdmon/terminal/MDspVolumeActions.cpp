#include <terminal/MDspVolumeActions.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/DisplayConsts.h>
#include <terminal/KeyCodes.h>
#include <terminal/HelpText.h>
#include <MessageLog.h>
#include <objects/DrbdResource.h>
#include <subprocess/SubProcessQueue.h>
#include <subprocess/CmdLine.h>
#include <subprocess/DrbdCmdConsts.h>
#include <memory>

const uint8_t   MDspVolumeActions::OPT_LIST_Y = 6;

MDspVolumeActions::MDspVolumeActions(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    cmd_fn_attach =
        [this]() -> void
        {
            selection_action(&MDspVolumeActions::action_attach);
        };
    cmd_fn_detach =
        [this]() -> void
        {
            selection_action(&MDspVolumeActions::action_detach);
        };
    cmd_fn_resize =
        [this]() -> void
        {
            selection_action(&MDspVolumeActions::action_resize);
        };
    cmd_fn_invalidate =
        [this]() -> void
        {
            selection_action(&MDspVolumeActions::action_invalidate);
        };

    uint16_t opt_line   = OPT_LIST_Y;
    uint16_t start_col  = 5;
    uint16_t end_col    = 45;

    cmd_attach = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "1", 1, opt_line, start_col, end_col,
            cmd_fn_attach
        )
    );
    ++opt_line;

    cmd_detach = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "2", 1, opt_line, start_col, end_col,
            cmd_fn_detach
        )
    );
    ++opt_line;

    cmd_resize = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "3", 1, opt_line, start_col, end_col,
            cmd_fn_resize
        )
    );
    ++opt_line;

    opt_line = OPT_LIST_Y;
    start_col = 50;
    end_col = 90;

    cmd_invalidate = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "4", 1, opt_line, start_col, end_col,
            cmd_fn_invalidate
        )
    );
    ++opt_line;

    add_option(*cmd_attach);
    add_option(*cmd_detach);
    add_option(*cmd_resize);
    add_option(*cmd_invalidate);
}

MDspVolumeActions::~MDspVolumeActions() noexcept
{
}

void MDspVolumeActions::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_VLM_ACT);

    dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 1);
    dsp_comp_hub.dsp_io->write_text("Volume actions: ");

    if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty())
    {
        DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
        if (rsc != nullptr)
        {
            dsp_comp_hub.dsp_io->write_text("Resource ");
            dsp_comp_hub.dsp_io->write_string_field(
                dsp_comp_hub.dsp_shared->monitor_rsc,
                dsp_comp_hub.term_cols - 26,
                false
            );

            dsp_comp_hub.dsp_io->cursor_xy(17, DisplayConsts::PAGE_NAV_Y + 2);
            if (!dsp_comp_hub.dsp_shared->ovrd_volume_selection &&
                dsp_comp_hub.dsp_shared->have_volumes_selection())
            {
                VolumesMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_volumes_map();
                const size_t count = selection_map.get_size();
                if (count > 1)
                {
                    dsp_comp_hub.dsp_io->write_fmt("%lu selected volumes", static_cast<unsigned long> (count));
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_fmt("%lu selected volume", static_cast<unsigned long> (count));
                }
            }
            else
            if (dsp_comp_hub.dsp_shared->monitor_vlm != DisplayConsts::VLM_NONE)
            {
                dsp_comp_hub.dsp_io->write_text("Volume ");
                dsp_comp_hub.dsp_io->write_fmt("%u", static_cast<unsigned int> (dsp_comp_hub.dsp_shared->monitor_vlm));
            }
        }
        else
        {
            dsp_comp_hub.dsp_io->write_text("Resource ");
            dsp_comp_hub.dsp_io->write_string_field(
                dsp_comp_hub.dsp_shared->monitor_rsc,
                dsp_comp_hub.term_cols - 40,
                false
            );
            dsp_comp_hub.dsp_io->write_text(" not active");
        }
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text("No selected resource");
    }

    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;
    const std::string& caution_color = dsp_comp_hub.active_color_table->caution_text;

    display_option(" 1   ", "Attach", *cmd_attach, std_color);
    display_option(" 2   ", "Detach", *cmd_detach, std_color);
    display_option(" 3   ", "Resize", *cmd_resize, std_color);
    display_option(" 4   ", "Invalidate", *cmd_invalidate, caution_color);

    display_option_query(5, 10);
}

void MDspVolumeActions::selection_action(const action_func_type action_func)
{
    try
    {
        dsp_comp_hub.dsp_common->application_working();
        if (!dsp_comp_hub.dsp_shared->ovrd_volume_selection &&
            dsp_comp_hub.dsp_shared->have_volumes_selection())
        {
            VolumesMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_volumes_map();
            VolumesMap::KeysIterator iter(selection_map);

            const std::string& rsc_name = dsp_comp_hub.dsp_shared->monitor_rsc;
            if (!rsc_name.empty())
            {
                while (iter.has_next())
                {
                    const uint16_t vlm_nr = *(iter.next());
                    (this->*action_func)(rsc_name, vlm_nr);
                }
                dsp_comp_hub.dsp_selector->leave_display();
            }
        }
        else
        {
            const std::string& rsc_name = dsp_comp_hub.dsp_shared->monitor_rsc;
            const uint16_t vlm_nr = dsp_comp_hub.dsp_shared->monitor_vlm;

            if (!rsc_name.empty() && vlm_nr != DisplayConsts::VLM_NONE)
            {
                (this->*action_func)(rsc_name, vlm_nr);
                dsp_comp_hub.dsp_selector->leave_display();
            }
        }
    }
    catch (SubProcessQueue::QueueCapacityException&)
    {
        dsp_comp_hub.log->add_entry(
            MessageLog::log_level::ALERT,
            "Volume actions: Cannot execute command, insufficient queue capacity"
        );
    }
    catch (SubProcess::Exception&)
    {
        dsp_comp_hub.log->add_entry(
            MessageLog::log_level::ALERT,
            "Volume actions: Command failed: Sub-process execution error"
        );
    }
    dsp_comp_hub.dsp_common->application_idle();
    reposition_text_cursor();
}

void MDspVolumeActions::action_attach(const std::string& rsc_name, const uint16_t vlm_nr)
{
    std::string cmd_target(rsc_name);
    cmd_target.append("/");
    cmd_target.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_ATTACH);
    command->add_argument(cmd_target);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Attach disk, resource ");
    text.append(rsc_name);
    text.append(", volume ");
    text.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspVolumeActions::action_detach(const std::string& rsc_name, const uint16_t vlm_nr)
{
    std::string cmd_target(rsc_name);
    cmd_target.append("/");
    cmd_target.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_DETACH);
    command->add_argument(cmd_target);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Detach disk, resource ");
    text.append(rsc_name);
    text.append(", volume ");
    text.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspVolumeActions::action_resize(const std::string& rsc_name, const uint16_t vlm_nr)
{
    std::string cmd_target(rsc_name);
    cmd_target.append("/");
    cmd_target.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_RESIZE);
    command->add_argument(cmd_target);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Resize disk, resource ");
    text.append(rsc_name);
    text.append(", volume ");
    text.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspVolumeActions::action_invalidate(const std::string& rsc_name, const uint16_t vlm_nr)
{
    std::string cmd_target(rsc_name);
    cmd_target.append("/");
    cmd_target.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_INVALIDATE);
    command->add_argument(cmd_target);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Invalidate local data, resource ");
    text.append(rsc_name);
    text.append(", volume ");
    text.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

uint64_t MDspVolumeActions::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_DRBD;
}

bool MDspVolumeActions::key_pressed(const uint32_t key)
{
    bool intercepted = MDspMenuBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::VLM_ACTIONS, dsp_comp_hub);
            intercepted = true;
        }
    }
    return intercepted;
}

