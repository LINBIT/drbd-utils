#include <terminal/MDspHelpIndex.h>
#include <terminal/HelpText.h>

MDspHelpIndex::MDspHelpIndex(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    cmd_fn_general_help =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::GENERAL_HELP, dsp_comp_hub);
        };
    cmd_fn_rsc_list =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::RSC_LIST, dsp_comp_hub);
        };
    cmd_fn_rsc_detail =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::RSC_DETAIL, dsp_comp_hub);
        };
    cmd_fn_rsc_actions =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::RSC_ACTIONS, dsp_comp_hub);
        };
    cmd_fn_vlm_list =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::VLM_LIST, dsp_comp_hub);
        };
    cmd_fn_vlm_detail =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::VLM_DETAIL, dsp_comp_hub);
        };
    cmd_fn_vlm_actions =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::VLM_ACTIONS, dsp_comp_hub);
        };
    cmd_fn_con_list =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::CON_LIST, dsp_comp_hub);
        };
    cmd_fn_con_detail =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::CON_DETAIL, dsp_comp_hub);
        };
    cmd_fn_con_actions =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::CON_ACTIONS, dsp_comp_hub);
        };
    cmd_fn_pvlm_list =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::PVLM_LIST, dsp_comp_hub);
        };
    cmd_fn_msg_log =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::MESSAGE_LOG, dsp_comp_hub);
        };
    cmd_fn_msg_detail =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::MESSAGE_DETAIL, dsp_comp_hub);
        };
    cmd_fn_taskq =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::TASK_QUEUE, dsp_comp_hub);
        };
    cmd_fn_task_detail =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::TASK_DETAIL, dsp_comp_hub);
        };
    cmd_fn_global_cmd =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::DRBDMON_COMMANDS, dsp_comp_hub);
        };
    cmd_fn_drbd_cmd =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::DRBD_COMMANDS, dsp_comp_hub);
        };
    cmd_fn_conf =
        [this]() -> void
        {
            helptext::open_help_page(helptext::id_type::CONF_HELP, dsp_comp_hub);
        };

    ClickableCommand::Builder cmd_builder;
    cmd_builder.coords.page = 1;
    cmd_builder.coords.row = 6;
    cmd_builder.coords.start_col = 5;
    cmd_builder.coords.end_col = 45;
    cmd_builder.auto_nr = 1;

    cmd_general_help = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_general_help)
    );
    cmd_rsc_list = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_rsc_list)
    );
    cmd_rsc_detail = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_rsc_detail)
    );
    cmd_rsc_actions = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_rsc_actions)
    );
    cmd_vlm_list = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_vlm_list)
    );
    cmd_vlm_detail  = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_vlm_detail)
    );
    cmd_vlm_actions = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_vlm_actions)
    );
    cmd_con_list = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_con_list)
    );
    cmd_con_detail = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_con_detail)
    );

    cmd_builder.coords.row = 6;
    cmd_builder.coords.start_col = 50;
    cmd_builder.coords.end_col = 95;

    cmd_con_actions = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_con_actions)
    );
    cmd_pvlm_list = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_pvlm_list)
    );
    cmd_msg_log = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_msg_log)
    );
    cmd_msg_detail = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_msg_detail)
    );
    cmd_taskq = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_taskq)
    );
    cmd_task_detail = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_task_detail)
    );
    cmd_global_cmd = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_global_cmd)
    );
    cmd_drbd_cmd = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_drbd_cmd)
    );
    cmd_conf = std::unique_ptr<ClickableCommand>(
        cmd_builder.create_with_auto_nr(cmd_fn_conf)
    );

    add_option(*cmd_general_help);
    add_option(*cmd_rsc_list);
    add_option(*cmd_rsc_detail);
    add_option(*cmd_rsc_actions);
    add_option(*cmd_vlm_list);
    add_option(*cmd_vlm_detail);
    add_option(*cmd_vlm_actions);
    add_option(*cmd_con_list);
    add_option(*cmd_con_detail);
    add_option(*cmd_con_actions);
    add_option(*cmd_pvlm_list);
    add_option(*cmd_msg_log);
    add_option(*cmd_msg_detail);
    add_option(*cmd_taskq);
    add_option(*cmd_task_detail);
    add_option(*cmd_global_cmd);
    add_option(*cmd_drbd_cmd);
    add_option(*cmd_conf);

    InputField& option_field = get_option_field();
    option_field.set_position(15, 16);
}

MDspHelpIndex::~MDspHelpIndex() noexcept
{
    clear_options();
}

void MDspHelpIndex::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_HELP_IDX);

    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text("Help index - Select help topic:");

    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;

    display_option(" 1   ", "General help", *cmd_general_help, std_color);
    display_option(" 2   ", "Resource list", *cmd_rsc_list, std_color);
    display_option(" 3   ", "Resource details", *cmd_rsc_detail, std_color);
    display_option(" 4   ", "Resource actions", *cmd_rsc_actions, std_color);
    display_option(" 5   ", "Volume list", *cmd_vlm_list, std_color);
    display_option(" 6   ", "Volume details", *cmd_vlm_detail, std_color);
    display_option(" 7   ", "Volume actions", *cmd_vlm_actions, std_color);
    display_option(" 8   ", "Connection list", *cmd_con_list, std_color);
    display_option(" 9   ", "Connection details", *cmd_con_detail, std_color);

    display_option("10   ", "Connection actions", *cmd_con_actions, std_color);
    display_option("11   ", "Peer volume list", *cmd_pvlm_list, std_color);
    display_option("12   ", "Message log", *cmd_msg_log, std_color);
    display_option("13   ", "Message details", *cmd_msg_detail, std_color);
    display_option("14   ", "Task queues", *cmd_taskq, std_color);
    display_option("15   ", "Task details", *cmd_task_detail, std_color);
    display_option("16   ", "DRBDmon commands", *cmd_global_cmd, std_color);
    display_option("17   ", "DRBD commands", *cmd_drbd_cmd, std_color);
    display_option("18   ", "DRBDmon configuration", *cmd_conf, std_color);

    display_option_query(5, 16);
}

uint64_t MDspHelpIndex::get_update_mask() noexcept
{
    return 0;
}
