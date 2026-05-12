#include <terminal/MDspSelectionFilter.h>
#include <terminal/DisplayConsts.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/HelpText.h>
#include <terminal/KeyCodes.h>
#include <comparators.h>
#include <string>
#include <string_transformations.h>
#include <cppdsaext/src/integerparse.h>

const ComponentsHub* MDspSelectionFilter::ToggleBoolFunctor::dsp_comp_hub_ptr {nullptr};

MDspSelectionFilter::MDspSelectionFilter(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub),
    rsc_op_normal(false),
    rsc_op_degraded(true),
    rsc_with_quorum(true),
    rsc_without_quorum(false),
    rsc_primary(DrbdRole::resource_role::PRIMARY),
    rsc_secondary(DrbdRole::resource_role::SECONDARY),
    vlm_op_normal(false),
    vlm_op_degraded(true),
    vlm_with_quorum(true),
    vlm_without_quorum(false),
    vlm_diskless_client(true),
    vlm_diskless_failed(false),
    vlm_uptodate(DrbdVolume::disk_state::UP_TO_DATE),
    vlm_consistent(DrbdVolume::disk_state::CONSISTENT),
    vlm_inconsistent(DrbdVolume::disk_state::INCONSISTENT),
    vlm_outdated(DrbdVolume::disk_state::OUTDATED),
    vlm_attaching(DrbdVolume::disk_state::ATTACHING),
    vlm_detaching(DrbdVolume::disk_state::DETACHING),
    vlm_failed(DrbdVolume::disk_state::FAILED),
    vlm_negotiating(DrbdVolume::disk_state::NEGOTIATING),
    vlm_unknown(DrbdVolume::disk_state::UNKNOWN),
    con_op_normal(false),
    con_op_degraded(true),
    con_primary(DrbdRole::resource_role::PRIMARY),
    con_secondary(DrbdRole::resource_role::SECONDARY),
    con_unknown_role(DrbdRole::resource_role::UNKNOWN),
    con_standalone(DrbdConnection::state::STANDALONE),
    con_disconnecting(DrbdConnection::state::DISCONNECTING),
    con_unconnected(DrbdConnection::state::UNCONNECTED),
    con_timeout(DrbdConnection::state::TIMEOUT),
    con_broken_pipe(DrbdConnection::state::BROKEN_PIPE),
    con_network_failure(DrbdConnection::state::NETWORK_FAILURE),
    con_protocol_error(DrbdConnection::state::PROTOCOL_ERROR),
    con_tear_down(DrbdConnection::state::TEAR_DOWN),
    con_connecting(DrbdConnection::state::CONNECTING),
    con_connected(DrbdConnection::state::CONNECTED),
    con_unknown_conn(DrbdConnection::state::UNKNOWN),
    peer_vlm_op_normal(false),
    peer_vlm_op_degraded(true),
    peer_vlm_with_quorum(true),
    peer_vlm_without_quorum(false),
    peer_vlm_diskless_client(true),
    peer_vlm_diskless_failed(false),
    peer_vlm_uptodate(DrbdVolume::disk_state::UP_TO_DATE),
    peer_vlm_consistent(DrbdVolume::disk_state::CONSISTENT),
    peer_vlm_inconsistent(DrbdVolume::disk_state::INCONSISTENT),
    peer_vlm_outdated(DrbdVolume::disk_state::OUTDATED),
    peer_vlm_attaching(DrbdVolume::disk_state::ATTACHING),
    peer_vlm_detaching(DrbdVolume::disk_state::DETACHING),
    peer_vlm_failed(DrbdVolume::disk_state::FAILED),
    peer_vlm_negotiating(DrbdVolume::disk_state::NEGOTIATING),
    peer_vlm_unknown_disk(DrbdVolume::disk_state::UNKNOWN),
    peer_vlm_off(DrbdVolume::repl_state::OFF),
    peer_vlm_established(DrbdVolume::repl_state::ESTABLISHED),
    peer_vlm_str_sync_src(DrbdVolume::repl_state::STARTING_SYNC_SOURCE),
    peer_vlm_str_sync_tgt(DrbdVolume::repl_state::STARTING_SYNC_TARGET),
    peer_vlm_wf_bm_src(DrbdVolume::repl_state::WF_BITMAP_SOURCE),
    peer_vlm_wf_bm_tgt(DrbdVolume::repl_state::WF_BITMAP_TARGET),
    peer_vlm_wf_sync_uuid(DrbdVolume::repl_state::WF_SYNC_UUID),
    peer_vlm_sync_src(DrbdVolume::repl_state::SYNC_SOURCE),
    peer_vlm_sync_tgt(DrbdVolume::repl_state::SYNC_TARGET),
    peer_vlm_psd_sync_src(DrbdVolume::repl_state::PAUSED_SYNC_SOURCE),
    peer_vlm_psd_sync_tgt(DrbdVolume::repl_state::PAUSED_SYNC_TARGET),
    peer_vlm_vfy_src(DrbdVolume::repl_state::VERIFY_SOURCE),
    peer_vlm_vfy_tgt(DrbdVolume::repl_state::VERIFY_TARGET),
    peer_vlm_ahead(DrbdVolume::repl_state::AHEAD),
    peer_vlm_behind(DrbdVolume::repl_state::BEHIND),
    peer_vlm_unknown_repl(DrbdVolume::repl_state::UNKNOWN)
{
    generate_filter_options_collection();

    ToggleBoolFunctor::dsp_comp_hub_ptr = &dsp_comp_hub;

    setup_cmd_functions();

    setup_pages();
}

MDspSelectionFilter::~MDspSelectionFilter() noexcept
{
}

void MDspSelectionFilter::generate_filter_options_collection()
{
    filter_options.resource_op_state.append(&rsc_op_normal);
    filter_options.resource_op_state.append(&rsc_op_degraded);
    filter_options.resource_quorum.append(&rsc_with_quorum);
    filter_options.resource_quorum.append(&rsc_without_quorum);
    filter_options.resource_role.append(&rsc_primary);
    filter_options.resource_role.append(&rsc_secondary);

    filter_options.volume_op_state.append(&vlm_op_normal);
    filter_options.volume_op_state.append(&vlm_op_degraded);
    filter_options.volume_quorum.append(&vlm_with_quorum);
    filter_options.volume_quorum.append(&vlm_without_quorum);
    filter_options.volume_diskless_state.append(&vlm_diskless_client);
    filter_options.volume_diskless_state.append(&vlm_diskless_failed);
    filter_options.volume_disk_state.append(&vlm_uptodate);
    filter_options.volume_disk_state.append(&vlm_consistent);
    filter_options.volume_disk_state.append(&vlm_inconsistent);
    filter_options.volume_disk_state.append(&vlm_outdated);
    filter_options.volume_disk_state.append(&vlm_attaching);
    filter_options.volume_disk_state.append(&vlm_detaching);
    filter_options.volume_disk_state.append(&vlm_failed);
    filter_options.volume_disk_state.append(&vlm_negotiating);
    filter_options.volume_disk_state.append(&vlm_unknown);

    filter_options.connection_op_state.append(&con_op_normal);
    filter_options.connection_op_state.append(&con_op_degraded);
    filter_options.connection_role.append(&con_primary);
    filter_options.connection_role.append(&con_secondary);
    filter_options.connection_role.append(&con_unknown_role);
    filter_options.connection_state.append(&con_standalone);
    filter_options.connection_state.append(&con_disconnecting);
    filter_options.connection_state.append(&con_unconnected);
    filter_options.connection_state.append(&con_timeout);
    filter_options.connection_state.append(&con_broken_pipe);
    filter_options.connection_state.append(&con_network_failure);
    filter_options.connection_state.append(&con_protocol_error);
    filter_options.connection_state.append(&con_tear_down);
    filter_options.connection_state.append(&con_connecting);
    filter_options.connection_state.append(&con_connected);
    filter_options.connection_state.append(&con_unknown_conn);

    filter_options.peer_volume_op_state.append(&peer_vlm_op_normal);
    filter_options.peer_volume_op_state.append(&peer_vlm_op_degraded);
    filter_options.peer_volume_quorum.append(&peer_vlm_with_quorum);
    filter_options.peer_volume_quorum.append(&peer_vlm_without_quorum);
    filter_options.peer_volume_diskless_state.append(&peer_vlm_diskless_client);
    filter_options.peer_volume_diskless_state.append(&peer_vlm_diskless_failed);
    filter_options.peer_volume_disk_state.append(&peer_vlm_uptodate);
    filter_options.peer_volume_disk_state.append(&peer_vlm_consistent);
    filter_options.peer_volume_disk_state.append(&peer_vlm_inconsistent);
    filter_options.peer_volume_disk_state.append(&peer_vlm_outdated);
    filter_options.peer_volume_disk_state.append(&peer_vlm_attaching);
    filter_options.peer_volume_disk_state.append(&peer_vlm_detaching);
    filter_options.peer_volume_disk_state.append(&peer_vlm_failed);
    filter_options.peer_volume_disk_state.append(&peer_vlm_negotiating);
    filter_options.peer_volume_disk_state.append(&peer_vlm_unknown_disk);
    filter_options.peer_volume_repl_state.append(&peer_vlm_off);
    filter_options.peer_volume_repl_state.append(&peer_vlm_established);
    filter_options.peer_volume_repl_state.append(&peer_vlm_str_sync_src);
    filter_options.peer_volume_repl_state.append(&peer_vlm_str_sync_tgt);
    filter_options.peer_volume_repl_state.append(&peer_vlm_wf_bm_src);
    filter_options.peer_volume_repl_state.append(&peer_vlm_wf_bm_tgt);
    filter_options.peer_volume_repl_state.append(&peer_vlm_wf_sync_uuid);
    filter_options.peer_volume_repl_state.append(&peer_vlm_sync_src);
    filter_options.peer_volume_repl_state.append(&peer_vlm_sync_tgt);
    filter_options.peer_volume_repl_state.append(&peer_vlm_psd_sync_src);
    filter_options.peer_volume_repl_state.append(&peer_vlm_psd_sync_tgt);
    filter_options.peer_volume_repl_state.append(&peer_vlm_vfy_src);
    filter_options.peer_volume_repl_state.append(&peer_vlm_vfy_tgt);
    filter_options.peer_volume_repl_state.append(&peer_vlm_ahead);
    filter_options.peer_volume_repl_state.append(&peer_vlm_behind);
    filter_options.peer_volume_repl_state.append(&peer_vlm_unknown_repl);
}

void MDspSelectionFilter::reset_filter_options_selection()
{
    reset_filter_options_list(filter_options.resource_op_state);
    reset_filter_options_list(filter_options.resource_quorum);
    reset_filter_options_list(filter_options.resource_role);
    reset_filter_options_list(filter_options.volume_op_state);
    reset_filter_options_list(filter_options.volume_quorum);
    reset_filter_options_list(filter_options.volume_disk_state);
    reset_filter_options_list(filter_options.volume_diskless_state);
    reset_filter_options_list(filter_options.connection_op_state);
    reset_filter_options_list(filter_options.connection_role);
    reset_filter_options_list(filter_options.connection_state);
    reset_filter_options_list(filter_options.peer_volume_op_state);
    reset_filter_options_list(filter_options.peer_volume_quorum);
    reset_filter_options_list(filter_options.peer_volume_diskless_state);
    reset_filter_options_list(filter_options.peer_volume_disk_state);
    reset_filter_options_list(filter_options.peer_volume_repl_state);

    op_slct_rsc = false;
    op_slct_vlm = false;
    op_slct_con = false;
    op_slct_peer_vlm = false;

    rstr_to_slct_rsc = false;
    rstr_to_slct_vlm = false;
    rstr_to_slct_con = false;
    rstr_to_slct_peer_vlm = false;

    inv_rsc_name = false;
    inv_vlm_number = false;
    inv_vlm_state = false;
    inv_con_name = false;
    inv_con_state = false;
    inv_peer_vlm_disk_state = false;
    inv_peer_vlm_repl_state = false;

    rsc_name_pattern_input->clear_text();
    con_name_pattern_input->clear_text();
    vlm_number_input->clear_text();

    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspSelectionFilter::setup_cmd_functions()
{
    cmd_fn_rsc_primary                  = ToggleBoolFunctor(rsc_primary.selected);
    cmd_fn_rsc_secondary                = ToggleBoolFunctor(rsc_secondary.selected);
    cmd_fn_rsc_op_normal                = ToggleBoolFunctor(rsc_op_normal.selected);
    cmd_fn_rsc_op_degraded              = ToggleBoolFunctor(rsc_op_degraded.selected);
    cmd_fn_rsc_with_quorum              = ToggleBoolFunctor(rsc_with_quorum.selected);
    cmd_fn_rsc_without_quorum           = ToggleBoolFunctor(rsc_without_quorum.selected);

    cmd_fn_inv_rsc_name                 = ToggleBoolFunctor(inv_rsc_name);

    cmd_fn_vlm_op_normal                = ToggleBoolFunctor(vlm_op_normal.selected);
    cmd_fn_vlm_op_degraded              = ToggleBoolFunctor(vlm_op_degraded.selected);
    cmd_fn_vlm_with_quorum              = ToggleBoolFunctor(vlm_with_quorum.selected);
    cmd_fn_vlm_without_quorum           = ToggleBoolFunctor(vlm_without_quorum.selected);
    cmd_fn_vlm_diskless_client          = ToggleBoolFunctor(vlm_diskless_client.selected);
    cmd_fn_vlm_diskless_failed          = ToggleBoolFunctor(vlm_diskless_failed.selected);
    cmd_fn_vlm_uptodate                 = ToggleBoolFunctor(vlm_uptodate.selected);
    cmd_fn_vlm_consistent               = ToggleBoolFunctor(vlm_consistent.selected);
    cmd_fn_vlm_inconsistent             = ToggleBoolFunctor(vlm_inconsistent.selected);
    cmd_fn_vlm_outdated                 = ToggleBoolFunctor(vlm_outdated.selected);
    cmd_fn_vlm_attaching                = ToggleBoolFunctor(vlm_attaching.selected);
    cmd_fn_vlm_detaching                = ToggleBoolFunctor(vlm_detaching.selected);
    cmd_fn_vlm_failed                   = ToggleBoolFunctor(vlm_failed.selected);
    cmd_fn_vlm_negotiating              = ToggleBoolFunctor(vlm_negotiating.selected);
    cmd_fn_vlm_unknown                  = ToggleBoolFunctor(vlm_unknown.selected);

    cmd_fn_inv_vlm_number               = ToggleBoolFunctor(inv_vlm_number);
    cmd_fn_inv_vlm_state                = ToggleBoolFunctor(inv_vlm_state);

    cmd_fn_con_op_normal                = ToggleBoolFunctor(con_op_normal.selected);
    cmd_fn_con_op_degraded              = ToggleBoolFunctor(con_op_degraded.selected);
    cmd_fn_con_primary                  = ToggleBoolFunctor(con_primary.selected);
    cmd_fn_con_secondary                = ToggleBoolFunctor(con_secondary.selected);
    cmd_fn_con_unknown_role             = ToggleBoolFunctor(con_unknown_role.selected);
    cmd_fn_con_standalone               = ToggleBoolFunctor(con_standalone.selected);
    cmd_fn_con_disconnecting            = ToggleBoolFunctor(con_disconnecting.selected);
    cmd_fn_con_unconnected              = ToggleBoolFunctor(con_unconnected.selected);
    cmd_fn_con_timeout                  = ToggleBoolFunctor(con_timeout.selected);
    cmd_fn_con_broken_pipe              = ToggleBoolFunctor(con_broken_pipe.selected);
    cmd_fn_con_network_failure          = ToggleBoolFunctor(con_network_failure.selected);
    cmd_fn_con_protocol_error           = ToggleBoolFunctor(con_protocol_error.selected);
    cmd_fn_con_tear_down                = ToggleBoolFunctor(con_tear_down.selected);
    cmd_fn_con_connecting               = ToggleBoolFunctor(con_connecting.selected);
    cmd_fn_con_connected                = ToggleBoolFunctor(con_connected.selected);
    cmd_fn_con_unknown_conn             = ToggleBoolFunctor(con_unknown_conn.selected);

    cmd_fn_inv_con_name                 = ToggleBoolFunctor(inv_con_name);
    cmd_fn_inv_con_state                = ToggleBoolFunctor(inv_con_state);

    cmd_fn_peer_vlm_op_normal           = ToggleBoolFunctor(peer_vlm_op_normal.selected);
    cmd_fn_peer_vlm_op_degraded         = ToggleBoolFunctor(peer_vlm_op_degraded.selected);
    cmd_fn_peer_vlm_with_quorum         = ToggleBoolFunctor(peer_vlm_with_quorum.selected);
    cmd_fn_peer_vlm_without_quorum      = ToggleBoolFunctor(peer_vlm_without_quorum.selected);
    cmd_fn_peer_vlm_diskless_client     = ToggleBoolFunctor(peer_vlm_diskless_client.selected);
    cmd_fn_peer_vlm_diskless_failed     = ToggleBoolFunctor(peer_vlm_diskless_failed.selected);
    cmd_fn_peer_vlm_uptodate            = ToggleBoolFunctor(peer_vlm_uptodate.selected);
    cmd_fn_peer_vlm_consistent          = ToggleBoolFunctor(peer_vlm_consistent.selected);
    cmd_fn_peer_vlm_inconsistent        = ToggleBoolFunctor(peer_vlm_inconsistent.selected);
    cmd_fn_peer_vlm_outdated            = ToggleBoolFunctor(peer_vlm_outdated.selected);
    cmd_fn_peer_vlm_attaching           = ToggleBoolFunctor(peer_vlm_attaching.selected);
    cmd_fn_peer_vlm_detaching           = ToggleBoolFunctor(peer_vlm_detaching.selected);
    cmd_fn_peer_vlm_failed              = ToggleBoolFunctor(peer_vlm_failed.selected);
    cmd_fn_peer_vlm_negotiating         = ToggleBoolFunctor(peer_vlm_negotiating.selected);
    cmd_fn_peer_vlm_unknown_disk        = ToggleBoolFunctor(peer_vlm_unknown_disk.selected);

    cmd_fn_inv_peer_vlm_disk_state      = ToggleBoolFunctor(inv_peer_vlm_disk_state);

    cmd_fn_peer_vlm_off                 = ToggleBoolFunctor(peer_vlm_off.selected);
    cmd_fn_peer_vlm_established         = ToggleBoolFunctor(peer_vlm_established.selected);
    cmd_fn_peer_vlm_str_sync_src        = ToggleBoolFunctor(peer_vlm_str_sync_src.selected);
    cmd_fn_peer_vlm_str_sync_tgt        = ToggleBoolFunctor(peer_vlm_str_sync_tgt.selected);
    cmd_fn_peer_vlm_wf_bm_src           = ToggleBoolFunctor(peer_vlm_wf_bm_src.selected);
    cmd_fn_peer_vlm_wf_bm_tgt           = ToggleBoolFunctor(peer_vlm_wf_bm_tgt.selected);
    cmd_fn_peer_vlm_wf_sync_uuid        = ToggleBoolFunctor(peer_vlm_wf_sync_uuid.selected);
    cmd_fn_peer_vlm_sync_src            = ToggleBoolFunctor(peer_vlm_sync_src.selected);
    cmd_fn_peer_vlm_sync_tgt            = ToggleBoolFunctor(peer_vlm_sync_tgt.selected);
    cmd_fn_peer_vlm_psd_sync_src        = ToggleBoolFunctor(peer_vlm_psd_sync_src.selected);
    cmd_fn_peer_vlm_psd_sync_tgt        = ToggleBoolFunctor(peer_vlm_psd_sync_tgt.selected);
    cmd_fn_peer_vlm_vfy_src             = ToggleBoolFunctor(peer_vlm_vfy_src.selected);
    cmd_fn_peer_vlm_vfy_tgt             = ToggleBoolFunctor(peer_vlm_vfy_tgt.selected);
    cmd_fn_peer_vlm_ahead               = ToggleBoolFunctor(peer_vlm_ahead.selected);
    cmd_fn_peer_vlm_behind              = ToggleBoolFunctor(peer_vlm_behind.selected);
    cmd_fn_peer_vlm_unknown_repl        = ToggleBoolFunctor(peer_vlm_unknown_repl.selected);

    cmd_fn_inv_peer_vlm_repl_state      = ToggleBoolFunctor(inv_peer_vlm_repl_state);

    cmd_fn_rstr_to_slct_rsc             = ToggleBoolFunctor(rstr_to_slct_rsc);
    cmd_fn_rstr_to_slct_vlm             = ToggleBoolFunctor(rstr_to_slct_vlm);
    cmd_fn_rstr_to_slct_con             = ToggleBoolFunctor(rstr_to_slct_con);
    cmd_fn_rstr_to_slct_peer_vlm        = ToggleBoolFunctor(rstr_to_slct_peer_vlm);

    cmd_fn_op_slct_rsc                  = ToggleBoolFunctor(op_slct_rsc);
    cmd_fn_op_slct_vlm                  = ToggleBoolFunctor(op_slct_vlm);
    cmd_fn_op_slct_con                  = ToggleBoolFunctor(op_slct_con);
    cmd_fn_op_slct_peer_vlm             = ToggleBoolFunctor(op_slct_peer_vlm);

    cmd_fn_exec_select =
        [this]() -> void
        {
            execute_select();
        };
    cmd_fn_exec_deselect =
        [this]() -> void
        {
            execute_deselect();
        };
    cmd_fn_discard_inactive_obj =
        [this]() -> void
        {
            discard_inactive_objects_selection();
        };

    cmd_fn_reset_flt_opt_slct =
        [this]() -> void
        {
            reset_filter_options_selection();
        };
}

void MDspSelectionFilter::setup_pages()
{
    InputField& option_input = get_option_field();
    option_input.set_max_length(6);
    option_input.set_field_length(6);

    ClickableCommand::Builder cmd_bld;

    const uint16_t col_1_start  = 5;
    const uint16_t col_1_end    = 45;
    const uint16_t col_2_start  = 50;
    const uint16_t col_2_end    = 90;
    const uint16_t row_title    = 4;
    const uint16_t row_start    = 5;

    cmd_bld.coords.page         = 1;
    cmd_bld.coords.row          = row_start;
    cmd_bld.coords.start_col    = col_1_start;
    cmd_bld.coords.end_col      = col_1_end;

    // ===
    // Restrict selection - Name matching, volume numbers
    // ===

    // TODO: The resource/connection name input fields should actually have their maximum length set to
    //       DRBD's maximum resource name length and the maximum host name length, but those are currently
    //       only implicitly limited by DRBDmon's 4kiB limit on DRBD event lines
    rsc_name_pattern_input = std::unique_ptr<InputField>(new InputField(dsp_comp_hub, DisplayConsts::MAX_CMD_LENGTH));
    con_name_pattern_input = std::unique_ptr<InputField>(new InputField(dsp_comp_hub, DisplayConsts::MAX_CMD_LENGTH));
    vlm_number_input = std::unique_ptr<InputField>(new InputField(dsp_comp_hub, 5));

    rsc_name_pattern_input->set_field_length(90);
    rsc_name_pattern_input->set_position(5, 7);
    cmd_bld.coords.row = 8;
    cmd_inv_rsc_name = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_inv_rsc_name)
    );
    add_option(*cmd_inv_rsc_name);
    con_name_pattern_input->set_field_length(90);
    con_name_pattern_input->set_position(5, 10);
    cmd_bld.coords.row = 11;
    cmd_inv_con_name = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_inv_con_name)
    );
    add_option(*cmd_inv_con_name);
    vlm_number_input->set_field_length(5);
    vlm_number_input->set_position(5, 13);
    cmd_bld.coords.row = 14;
    cmd_inv_vlm_number = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_inv_vlm_number)
    );
    add_option(*cmd_inv_vlm_number);

    ++cmd_bld.coords.page;
    cmd_bld.coords.row  = row_start;
    cmd_bld.auto_nr     = 1;

    // ===
    // Restrict selection -- Existing selection
    // ===

    cmd_rstr_to_slct_rsc = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_rstr_to_slct_rsc)
    );
    add_option(*cmd_rstr_to_slct_rsc);
    cmd_rstr_to_slct_vlm = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_rstr_to_slct_vlm)
    );
    add_option(*cmd_rstr_to_slct_vlm);
    cmd_rstr_to_slct_con = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_rstr_to_slct_con)
    );
    add_option(*cmd_rstr_to_slct_con);
    cmd_rstr_to_slct_peer_vlm = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_rstr_to_slct_peer_vlm)
    );
    add_option(*cmd_rstr_to_slct_peer_vlm);

    // ===
    // Resource filter criteria
    // ===

    ++cmd_bld.coords.page;
    cmd_bld.auto_nr     = 1;

    cmd_bld.coords.row          = row_title;
    cmd_bld.coords.start_col    = col_2_start;
    cmd_bld.coords.end_col      = col_2_end;

    cmd_op_slct_rsc = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_op_slct_rsc)
    );
    add_option(*cmd_op_slct_rsc);

    cmd_bld.coords.row          = row_start;
    cmd_bld.coords.start_col    = col_1_start;
    cmd_bld.coords.end_col      = col_1_end;

    cmd_rsc_primary = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_rsc_primary)
    );
    add_option(*cmd_rsc_primary);
    cmd_rsc_secondary = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_rsc_secondary)
    );
    add_option(*cmd_rsc_secondary);
    cmd_rsc_op_normal = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_rsc_op_normal)
    );
    add_option(*cmd_rsc_op_normal);
    cmd_rsc_op_degraded = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_rsc_op_degraded)
    );
    add_option(*cmd_rsc_op_degraded);
    cmd_rsc_with_quorum = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_rsc_with_quorum)
    );
    add_option(*cmd_rsc_with_quorum);
    cmd_rsc_without_quorum = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_rsc_without_quorum)
    );
    add_option(*cmd_rsc_without_quorum);

    // ===
    // Volume filter criteria
    // ===

    ++cmd_bld.coords.page;
    cmd_bld.auto_nr     = 1;

    cmd_bld.coords.row          = row_title;
    cmd_bld.coords.start_col    = col_2_start;
    cmd_bld.coords.end_col      = col_2_end;

    cmd_op_slct_vlm = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_op_slct_vlm)
    );
    add_option(*cmd_op_slct_vlm);

    cmd_bld.coords.row          = row_start;
    cmd_bld.coords.start_col    = col_1_start;
    cmd_bld.coords.end_col      = col_1_end;

    cmd_vlm_op_normal = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_op_normal)
    );
    add_option(*cmd_vlm_op_normal);
    cmd_vlm_op_degraded = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_op_degraded)
    );
    add_option(*cmd_vlm_op_degraded);

    cmd_bld.coords.row          = row_start;
    cmd_bld.coords.start_col    = col_2_start;
    cmd_bld.coords.end_col      = col_2_end;

    cmd_vlm_with_quorum = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_with_quorum)
    );
    add_option(*cmd_vlm_with_quorum);
    cmd_vlm_without_quorum = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_without_quorum)
    );
    add_option(*cmd_vlm_without_quorum);

    cmd_bld.coords.row          = row_start + 3;
    cmd_bld.coords.start_col    = col_1_start;
    cmd_bld.coords.end_col      = col_1_end;

    cmd_vlm_diskless_client = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_diskless_client)
    );
    add_option(*cmd_vlm_diskless_client);
    cmd_vlm_diskless_failed = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_diskless_failed)
    );
    add_option(*cmd_vlm_diskless_failed);
    cmd_vlm_uptodate = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_uptodate)
    );
    add_option(*cmd_vlm_uptodate);
    cmd_vlm_consistent = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_consistent)
    );
    add_option(*cmd_vlm_consistent);
    cmd_vlm_inconsistent = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_inconsistent)
    );
    add_option(*cmd_vlm_inconsistent);
    cmd_vlm_outdated = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_outdated)
    );
    add_option(*cmd_vlm_outdated);

    cmd_bld.coords.row          = row_start + 3;
    cmd_bld.coords.start_col    = col_2_start;
    cmd_bld.coords.end_col      = col_2_end;

    cmd_vlm_attaching = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_attaching)
    );
    add_option(*cmd_vlm_attaching);
    cmd_vlm_detaching = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_detaching)
    );
    add_option(*cmd_vlm_detaching);
    cmd_vlm_failed = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_failed)
    );
    add_option(*cmd_vlm_failed);
    cmd_vlm_negotiating = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_negotiating)
    );
    add_option(*cmd_vlm_negotiating);
    cmd_vlm_unknown = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_vlm_unknown)
    );
    add_option(*cmd_vlm_unknown);

    cmd_inv_vlm_state = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_inv_vlm_state)
    );
    add_option(*cmd_inv_vlm_state);

    // ===
    // Connection filter criteria
    // ===

    ++cmd_bld.coords.page;
    cmd_bld.auto_nr             = 1;

    cmd_bld.coords.row          = row_title;
    cmd_bld.coords.start_col    = col_2_start;
    cmd_bld.coords.end_col      = col_2_end;

    cmd_op_slct_con = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_op_slct_con)
    );
    add_option(*cmd_op_slct_con);

    cmd_bld.coords.row          = row_start;
    cmd_bld.coords.start_col    = col_1_start;
    cmd_bld.coords.end_col      = col_1_end;

    cmd_con_op_normal = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_op_normal)
    );
    add_option(*cmd_con_op_normal);
    cmd_con_op_degraded = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_op_degraded)
    );
    add_option(*cmd_con_op_degraded);

    cmd_bld.coords.row          = row_start;
    cmd_bld.coords.start_col    = col_2_start;
    cmd_bld.coords.end_col      = col_2_end;

    cmd_con_primary = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_primary)
    );
    add_option(*cmd_con_primary);
    cmd_con_secondary = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_secondary)
    );
    add_option(*cmd_con_secondary);
    cmd_con_unknown_role = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_unknown_role)
    );
    add_option(*cmd_con_unknown_role);

    cmd_bld.coords.row          = row_start + 4;
    cmd_bld.coords.start_col    = col_1_start;
    cmd_bld.coords.end_col      = col_1_end;

    cmd_con_standalone = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_standalone)
    );
    add_option(*cmd_con_standalone);
    cmd_con_disconnecting = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_disconnecting)
    );
    add_option(*cmd_con_disconnecting);
    cmd_con_unconnected = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_unconnected)
    );
    add_option(*cmd_con_unconnected);
    cmd_con_timeout = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_timeout)
    );
    add_option(*cmd_con_timeout);

    cmd_bld.coords.row          = row_start + 4;
    cmd_bld.coords.start_col    = col_2_start;
    cmd_bld.coords.end_col      = col_2_end;

    cmd_con_broken_pipe = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_broken_pipe)
    );
    add_option(*cmd_con_broken_pipe);
    cmd_con_network_failure = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_network_failure)
    );
    add_option(*cmd_con_network_failure);
    cmd_con_protocol_error = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_protocol_error)
    );
    add_option(*cmd_con_protocol_error);
    cmd_con_tear_down = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_tear_down)
    );
    add_option(*cmd_con_tear_down);
    cmd_con_connecting = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_connecting)
    );
    add_option(*cmd_con_connecting);
    cmd_con_connected = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_connected)
    );
    add_option(*cmd_con_connected);
    cmd_con_unknown_conn = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_con_unknown_conn)
    );
    add_option(*cmd_con_unknown_conn);

    cmd_inv_con_state = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_inv_con_state)
    );
    add_option(*cmd_inv_con_state);

    // ===
    // Peer volume filter criteria - first page
    // ===

    ++cmd_bld.coords.page;
    cmd_bld.auto_nr             = 1;

    cmd_bld.coords.row          = row_title;
    cmd_bld.coords.start_col    = col_2_start;
    cmd_bld.coords.end_col      = col_2_end;

    cmd_op_slct_peer_vlm = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_op_slct_peer_vlm)
    );
    add_option(*cmd_op_slct_peer_vlm);

    cmd_bld.coords.row          = row_start;
    cmd_bld.coords.start_col    = col_1_start;
    cmd_bld.coords.end_col      = col_1_end;

    cmd_peer_vlm_op_normal = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_op_normal)
    );
    add_option(*cmd_peer_vlm_op_normal);
    cmd_peer_vlm_op_degraded = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_op_degraded)
    );
    add_option(*cmd_peer_vlm_op_degraded);

    cmd_bld.coords.row          = row_start;
    cmd_bld.coords.start_col    = col_2_start;
    cmd_bld.coords.end_col      = col_2_end;

    cmd_peer_vlm_with_quorum = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_with_quorum)
    );
    add_option(*cmd_peer_vlm_with_quorum);
    cmd_peer_vlm_without_quorum = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_without_quorum)
    );
    add_option(*cmd_peer_vlm_without_quorum);

    cmd_bld.coords.row          = row_start + 3;
    cmd_bld.coords.start_col    = col_1_start;
    cmd_bld.coords.end_col      = col_1_end;

    cmd_peer_vlm_diskless_client = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_diskless_client)
    );
    add_option(*cmd_peer_vlm_diskless_client);
    cmd_peer_vlm_diskless_failed = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_diskless_failed)
    );
    add_option(*cmd_peer_vlm_diskless_failed);
    cmd_peer_vlm_uptodate = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_uptodate)
    );
    add_option(*cmd_peer_vlm_uptodate);
    cmd_peer_vlm_consistent = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_consistent)
    );
    add_option(*cmd_peer_vlm_consistent);
    cmd_peer_vlm_inconsistent = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_inconsistent)
    );
    add_option(*cmd_peer_vlm_inconsistent);
    cmd_peer_vlm_outdated = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_outdated)
    );

    cmd_bld.coords.row          = row_start + 3;
    cmd_bld.coords.start_col    = col_2_start;
    cmd_bld.coords.end_col      = col_2_end;

    add_option(*cmd_peer_vlm_outdated);
    cmd_peer_vlm_attaching = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_attaching)
    );
    add_option(*cmd_peer_vlm_attaching);
    cmd_peer_vlm_detaching = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_detaching)
    );
    add_option(*cmd_peer_vlm_detaching);
    cmd_peer_vlm_failed = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_failed)
    );
    add_option(*cmd_peer_vlm_failed);
    cmd_peer_vlm_negotiating = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_negotiating)
    );
    add_option(*cmd_peer_vlm_negotiating);
    cmd_peer_vlm_unknown_disk = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_unknown_disk)
    );
    add_option(*cmd_peer_vlm_unknown_disk);

    cmd_inv_peer_vlm_disk_state = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_inv_peer_vlm_disk_state)
    );
    add_option(*cmd_inv_peer_vlm_disk_state);

    // ===
    // Peer volume filter criteria - second page
    // ===

    ++cmd_bld.coords.page;
    cmd_bld.coords.row          = row_start;
    cmd_bld.coords.start_col    = col_1_start;
    cmd_bld.coords.end_col      = col_1_end;
    cmd_bld.auto_nr             = 1;

    cmd_peer_vlm_off = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_off)
    );
    add_option(*cmd_peer_vlm_off);
    cmd_peer_vlm_established = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_established)
    );
    add_option(*cmd_peer_vlm_established);
    cmd_peer_vlm_str_sync_src = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_str_sync_src)
    );
    add_option(*cmd_peer_vlm_str_sync_src);
    cmd_peer_vlm_str_sync_tgt = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_str_sync_tgt)
    );
    add_option(*cmd_peer_vlm_str_sync_tgt);
    cmd_peer_vlm_wf_bm_src = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_wf_bm_src)
    );
    add_option(*cmd_peer_vlm_wf_bm_src);
    cmd_peer_vlm_wf_bm_tgt = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_wf_bm_tgt)
    );
    add_option(*cmd_peer_vlm_wf_bm_tgt);
    cmd_peer_vlm_wf_sync_uuid = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_wf_sync_uuid)
    );
    add_option(*cmd_peer_vlm_wf_sync_uuid);
    cmd_peer_vlm_sync_src = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_sync_src)
    );
    add_option(*cmd_peer_vlm_sync_src);
    cmd_peer_vlm_sync_tgt = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_sync_tgt)
    );
    add_option(*cmd_peer_vlm_sync_tgt);

    cmd_bld.coords.row          = row_start;
    cmd_bld.coords.start_col    = col_2_start;
    cmd_bld.coords.end_col      = col_2_end;

    cmd_peer_vlm_psd_sync_src = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_psd_sync_src)
    );
    add_option(*cmd_peer_vlm_psd_sync_src);
    cmd_peer_vlm_psd_sync_tgt = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_psd_sync_tgt)
    );
    add_option(*cmd_peer_vlm_psd_sync_tgt);
    cmd_peer_vlm_vfy_src = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_vfy_src)
    );
    add_option(*cmd_peer_vlm_vfy_src);
    cmd_peer_vlm_vfy_tgt = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_vfy_tgt)
    );
    add_option(*cmd_peer_vlm_vfy_tgt);
    cmd_peer_vlm_ahead = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_ahead)
    );
    add_option(*cmd_peer_vlm_ahead);
    cmd_peer_vlm_behind = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_behind)
    );
    add_option(*cmd_peer_vlm_behind);
    cmd_peer_vlm_unknown_repl = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_unknown_repl)
    );
    add_option(*cmd_peer_vlm_unknown_repl);

    cmd_inv_peer_vlm_repl_state = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_inv_peer_vlm_repl_state)
    );
    add_option(*cmd_inv_peer_vlm_repl_state);

    // ===
    // Execute page
    // ===

    ++cmd_bld.coords.page;
    cmd_bld.coords.row          = row_start;
    cmd_bld.coords.start_col    = col_1_start;
    cmd_bld.coords.end_col      = col_1_end;
    cmd_bld.auto_nr             = 1;

    cmd_exec_select = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_id("S", cmd_fn_exec_select)
    );
    add_option(*cmd_exec_select);
    cmd_exec_deselect = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_id("D", cmd_fn_exec_deselect)
    );
    add_option(*cmd_exec_deselect);
    ++cmd_bld.coords.row;
    cmd_discard_inactive_obj = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_page_dot_auto_nr(cmd_fn_discard_inactive_obj)
    );
    add_option(*cmd_discard_inactive_obj);
    ++cmd_bld.coords.row;
    cmd_reset_flt_opt_slct = std::unique_ptr<ClickableCommand>(
        cmd_bld.create_with_id("R", cmd_fn_reset_flt_opt_slct)
    );
    add_option(*cmd_reset_flt_opt_slct);

    // Statistics page
    ++cmd_bld.coords.page;
    stats_page = cmd_bld.coords.page;

    set_page_count(cmd_bld.coords.page);
}

void MDspSelectionFilter::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_SLCT_FILTER);

    const uint32_t page = get_page_nr();

    // Clear selection update statistics when navigating away from the statistics page
    if (page != stats_page)
    {
        reset_stats_page();
    }

    if (page != 1)
    {
        active_input = nullptr;
        delegate_focus(false);
    }

    if (page >= 1 && page <= 2)
    {
        display_restrict_selection();
    }
    else
    if (page == 3)
    {
        display_resource_criteria();
    }
    else
    if (page == 4)
    {
        display_volume_criteria();
    }
    else
    if (page == 5)
    {
        display_connection_criteria();
    }
    else
    if (page >= 6 && page <= 7)
    {
        display_peer_volume_criteria();
    }
    else
    if (page == 8)
    {
        display_execute();
    }
    else
    if (page == stats_page)
    {
        display_statistics();
    }
}

uint64_t MDspSelectionFilter::get_update_mask() noexcept
{
    return 0;
}

bool MDspSelectionFilter::key_pressed(const uint32_t key)
{
    bool intercepted = false;
    // Option pages
    if (key == KeyCodes::FUNC_01)
    {
        helptext::open_help_page(helptext::id_type::SFLT_HELP, dsp_comp_hub);
        intercepted = true;
    }
    else
    {
        const uint32_t page = get_page_nr();
        if (page < stats_page)
        {
            intercepted = MDspMenuBase::key_pressed(key);
            if (!intercepted && is_focus_delegated() && active_input != nullptr)
            {
                active_input->key_pressed(key);
                intercepted = true;
            }
        }
        else
        {
            // No option entry on the statistics page
            intercepted = MDspBase::key_pressed(key);
        }
    }
    return intercepted;
}

bool MDspSelectionFilter::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspMenuBase::mouse_action(mouse);
    if (!intercepted)
    {
        const uint32_t page = get_page_nr();
        if (page == 1)
        {
            if (rsc_name_pattern_input->mouse_action(mouse))
            {
                active_input = rsc_name_pattern_input.get();
                intercepted = true;
            }
            else
            if (con_name_pattern_input->mouse_action(mouse))
            {
                active_input = con_name_pattern_input.get();
                intercepted = true;
            }
            else
            if (vlm_number_input->mouse_action(mouse))
            {
                active_input = vlm_number_input.get();
                intercepted = true;
            }

            if (intercepted)
            {
                delegate_focus(true);
            }
            else
            {
                InputField& option_field = get_option_field();
                if (option_field.mouse_action(mouse))
                {
                    active_input = nullptr;
                    delegate_focus(false);
                    intercepted = true;
                }
            }

            if (intercepted)
            {
                dsp_comp_hub.dsp_selector->refresh_display();
            }
        }
    }
    return intercepted;
}

void MDspSelectionFilter::text_cursor_ops()
{
    bool have_cursor = false;
    const uint32_t page = get_page_nr();
    if (is_focus_delegated())
    {
        if (page == 1 && active_input != nullptr)
        {
            active_input->cursor();
            have_cursor = true;
        }
        else
        {
            delegate_focus(false);
        }
    }
    if (!have_cursor && page < stats_page)
    {
        MDspMenuBase::text_cursor_ops();
    }
}

void MDspSelectionFilter::display_activated()
{
    MDspMenuBase::display_activated();
    // Commands entered on the command line affect all selected objects while displaying the selection filters
    dsp_comp_hub.dsp_shared->ovrd_resource_selection = false;
    dsp_comp_hub.dsp_shared->ovrd_connection_selection = false;
    dsp_comp_hub.dsp_shared->ovrd_volume_selection = false;
    dsp_comp_hub.dsp_shared->ovrd_peer_volume_selection = false;
}

void MDspSelectionFilter::display_deactivated()
{
    MDspMenuBase::display_deactivated();
    reset_stats_page();
}

void MDspSelectionFilter::display_closed()
{
    reset_filter_options_selection();
    MDspMenuBase::display_closed();
    set_page_nr(1);
}

MDspSelectionFilter::ToggleBoolFunctor::ToggleBoolFunctor(bool& state):
    state_ref(state)
{
}

MDspSelectionFilter::ToggleBoolFunctor::~ToggleBoolFunctor() noexcept
{
}

// Initialize pointer to the ComponentsHub instance before calling operator()
// to enable automatic display updates
void MDspSelectionFilter::ToggleBoolFunctor::operator()() noexcept
{
    state_ref = !state_ref;
    if (MDspSelectionFilter::ToggleBoolFunctor::dsp_comp_hub_ptr != nullptr)
    {
        MDspSelectionFilter::ToggleBoolFunctor::dsp_comp_hub_ptr->dsp_selector->refresh_display();
    }
}

MDspSelectionFilter::FilterOptionsCollection::FilterOptionsCollection():
    resource_op_state(&compare_state_selector<bool>),
    resource_role(&compare_state_selector<DrbdRole::resource_role>),
    resource_quorum(&compare_state_selector<bool>),
    volume_op_state(&compare_state_selector<bool>),
    volume_quorum(&compare_state_selector<bool>),
    volume_diskless_state(&compare_state_selector<bool>),
    volume_disk_state(&compare_state_selector<DrbdVolume::disk_state>),
    connection_op_state(&compare_state_selector<bool>),
    connection_role(&compare_state_selector<DrbdRole::resource_role>),
    connection_state(&compare_state_selector<DrbdConnection::state>),
    peer_volume_op_state(&compare_state_selector<bool>),
    peer_volume_quorum(&compare_state_selector<bool>),
    peer_volume_diskless_state(&compare_state_selector<bool>),
    peer_volume_disk_state(&compare_state_selector<DrbdVolume::disk_state>),
    peer_volume_repl_state(&compare_state_selector<DrbdVolume::repl_state>)
{
}

MDspSelectionFilter::FilterOptionsCollection::~FilterOptionsCollection() noexcept
{
    // VList nodes are released automatically on destruction,
    // and the contained values are owned by the display
}

void MDspSelectionFilter::display_toggle(
    const char* const   text,
    ClickableCommand&   cmd,
    const bool&         selected
)
{
    MDspMenuBase::display_selectable(5, text, cmd, selected);
}

void MDspSelectionFilter::display_restrict_selection()
{
    const uint32_t page = get_page_nr();
    if (page == 1)
    {
        dsp_comp_hub.dsp_io->cursor_xy(3, 4);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("Restrict selection:");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        dsp_comp_hub.dsp_io->cursor_xy(3, 6);
        dsp_comp_hub.dsp_io->write_text("Only resources matching the name:");
        rsc_name_pattern_input->display();
        display_toggle("Invert resource name match", *cmd_inv_rsc_name, inv_rsc_name);

        dsp_comp_hub.dsp_io->cursor_xy(3, 9);
        dsp_comp_hub.dsp_io->write_text("Only connections matching the name:");
        con_name_pattern_input->display();
        display_toggle("Invert connection name match", *cmd_inv_con_name, inv_con_name);

        dsp_comp_hub.dsp_io->cursor_xy(3, 12);
        dsp_comp_hub.dsp_io->write_text("Only volume number:");
        vlm_number_input->display();
        display_toggle("Invert volume number match", *cmd_inv_vlm_number, inv_vlm_number);
    }
    else
    if (page == 2)
    {
        dsp_comp_hub.dsp_io->cursor_xy(3, 4);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("Restrict filter matching:");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        display_toggle("Selected resources", *cmd_rstr_to_slct_rsc, rstr_to_slct_rsc);
        display_toggle("Selected volumes", *cmd_rstr_to_slct_vlm, rstr_to_slct_vlm);
        display_toggle("Selected connections", *cmd_rstr_to_slct_con, rstr_to_slct_con);
        display_toggle("Selected peer volumes", *cmd_rstr_to_slct_peer_vlm, rstr_to_slct_peer_vlm);
    }

    display_option_query(5, 17);
}

void MDspSelectionFilter::display_resource_criteria()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Resource filter criteria:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    display_toggle("Deselect resources", *cmd_op_slct_rsc, op_slct_rsc);
    display_toggle("Fully operational resources", *cmd_rsc_op_normal, rsc_op_normal.selected);
    display_toggle("Degraded resources", *cmd_rsc_op_degraded, rsc_op_degraded.selected);
    display_toggle("Resources with quorum", *cmd_rsc_with_quorum, rsc_with_quorum.selected);
    display_toggle("Resources without quorum", *cmd_rsc_without_quorum, rsc_without_quorum.selected);
    display_toggle("Primary resources", *cmd_rsc_primary, rsc_primary.selected);
    display_toggle("Secondary resources", *cmd_rsc_secondary, rsc_secondary.selected);

    display_option_query(5, 17);
}

void MDspSelectionFilter::display_volume_criteria()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Volume filter criteria:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    dsp_comp_hub.dsp_io->cursor_xy(3, 7);
    dsp_comp_hub.dsp_io->write_text("and any of the selected volume states:");
    display_toggle("(De)select volumes", *cmd_op_slct_vlm, op_slct_vlm);
    display_toggle("Fully operational volumes", *cmd_vlm_op_normal, vlm_op_normal.selected);
    display_toggle("Degraded volumes", *cmd_vlm_op_degraded, vlm_op_degraded.selected);
    display_toggle("Volumes with quorum", *cmd_vlm_with_quorum, vlm_with_quorum.selected);
    display_toggle("Volumes without quorum", *cmd_vlm_without_quorum, vlm_without_quorum.selected);
    display_toggle("Diskless (client)", *cmd_vlm_diskless_client, vlm_diskless_client.selected);
    display_toggle("Diskless (failed)", *cmd_vlm_diskless_failed, vlm_diskless_failed.selected);
    display_toggle(DrbdVolume::DS_LABEL_UP_TO_DATE, *cmd_vlm_uptodate, vlm_uptodate.selected);
    display_toggle(DrbdVolume::DS_LABEL_CONSISTENT, *cmd_vlm_consistent, vlm_consistent.selected);
    display_toggle(DrbdVolume::DS_LABEL_INCONSISTENT, *cmd_vlm_inconsistent, vlm_inconsistent.selected);
    display_toggle(DrbdVolume::DS_LABEL_OUTDATED, *cmd_vlm_outdated, vlm_outdated.selected);
    display_toggle(DrbdVolume::DS_LABEL_ATTACHING, *cmd_vlm_attaching, vlm_attaching.selected);
    display_toggle(DrbdVolume::DS_LABEL_DETACHING, *cmd_vlm_detaching, vlm_detaching.selected);
    display_toggle(DrbdVolume::DS_LABEL_FAILED, *cmd_vlm_failed, vlm_failed.selected);
    display_toggle(DrbdVolume::DS_LABEL_NEGOTIATING, *cmd_vlm_negotiating, vlm_negotiating.selected);
    display_toggle(DrbdVolume::DS_LABEL_UNKNOWN, *cmd_vlm_unknown, vlm_unknown.selected);
    display_toggle("Invert disk state match", *cmd_inv_vlm_state, inv_vlm_state);

    display_option_query(5, 17);
}

void MDspSelectionFilter::display_connection_criteria()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Connection filter criteria:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    display_toggle("(De)select connections", *cmd_op_slct_con, op_slct_con);
    display_toggle("Fully operational connections", *cmd_con_op_normal, con_op_normal.selected);
    display_toggle("Degraded connections", *cmd_con_op_degraded, con_op_degraded.selected);
    display_toggle("Primary peers", *cmd_con_primary, con_primary.selected);
    display_toggle("Secondary peers", *cmd_con_secondary, con_secondary.selected);
    display_toggle(DrbdConnection::ROLE_LABEL_UNKNOWN, *cmd_con_unknown_role, con_unknown_role.selected);

    dsp_comp_hub.dsp_io->cursor_xy(3, 8);
    dsp_comp_hub.dsp_io->write_text("and any of the selected connection states:");

    display_toggle(DrbdConnection::CS_LABEL_STANDALONE, *cmd_con_standalone, con_standalone.selected);
    display_toggle(DrbdConnection::CS_LABEL_DISCONNECTING, *cmd_con_disconnecting, con_disconnecting.selected);
    display_toggle(DrbdConnection::CS_LABEL_UNCONNECTED, *cmd_con_unconnected, con_unconnected.selected);
    display_toggle(DrbdConnection::CS_LABEL_TIMEOUT, *cmd_con_timeout, con_timeout.selected);
    display_toggle(DrbdConnection::CS_LABEL_BROKEN_PIPE, *cmd_con_broken_pipe, con_broken_pipe.selected);
    display_toggle(
        DrbdConnection::CS_LABEL_NETWORK_FAILURE,
        *cmd_con_network_failure, con_network_failure.selected
    );
    display_toggle(DrbdConnection::CS_LABEL_PROTOCOL_ERROR, *cmd_con_protocol_error, con_protocol_error.selected);
    display_toggle(DrbdConnection::CS_LABEL_TEAR_DOWN, *cmd_con_tear_down, con_tear_down.selected);
    display_toggle(DrbdConnection::CS_LABEL_CONNECTING, *cmd_con_connecting, con_connecting.selected);
    display_toggle(DrbdConnection::CS_LABEL_CONNECTED, *cmd_con_connected, con_connected.selected);
    display_toggle(DrbdConnection::CS_LABEL_UNKNOWN, *cmd_con_unknown_conn, con_unknown_conn.selected);
    display_toggle("Invert connection state match", *cmd_inv_con_state, inv_con_state);

    display_option_query(5, 17);
}

void MDspSelectionFilter::display_peer_volume_criteria()
{
    const uint32_t page = get_page_nr();
    if (page == 6)
    {
        dsp_comp_hub.dsp_io->cursor_xy(3, 4);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("Peer volume filter criteria");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        display_toggle("(De)select peer volumes", *cmd_op_slct_peer_vlm, op_slct_peer_vlm);
        display_toggle("Fully operational volumes", *cmd_peer_vlm_op_normal, peer_vlm_op_normal.selected);
        display_toggle("Degraded volumes", *cmd_peer_vlm_op_degraded, peer_vlm_op_degraded.selected);
        display_toggle("Volumes with quorum", *cmd_peer_vlm_with_quorum, peer_vlm_with_quorum.selected);
        display_toggle("Volumes without quorum", *cmd_peer_vlm_without_quorum, peer_vlm_without_quorum.selected);

        dsp_comp_hub.dsp_io->cursor_xy(3, 7);
        dsp_comp_hub.dsp_io->write_text("and any of the selected peer volume disk states:");

        display_toggle("Diskless (client)", *cmd_peer_vlm_diskless_client, peer_vlm_diskless_client.selected);
        display_toggle("Diskless (failed)", *cmd_peer_vlm_diskless_failed, peer_vlm_diskless_failed.selected);
        display_toggle(DrbdVolume::DS_LABEL_UP_TO_DATE, *cmd_peer_vlm_uptodate, peer_vlm_uptodate.selected);
        display_toggle(DrbdVolume::DS_LABEL_CONSISTENT, *cmd_peer_vlm_consistent, peer_vlm_consistent.selected);
        display_toggle(
            DrbdVolume::DS_LABEL_INCONSISTENT,
            *cmd_peer_vlm_inconsistent, peer_vlm_inconsistent.selected
        );
        display_toggle(DrbdVolume::DS_LABEL_OUTDATED, *cmd_peer_vlm_outdated, peer_vlm_outdated.selected);
        display_toggle(DrbdVolume::DS_LABEL_ATTACHING, *cmd_peer_vlm_attaching, peer_vlm_attaching.selected);
        display_toggle(DrbdVolume::DS_LABEL_DETACHING, *cmd_peer_vlm_detaching, peer_vlm_detaching.selected);
        display_toggle(DrbdVolume::DS_LABEL_FAILED, *cmd_peer_vlm_failed, peer_vlm_failed.selected);
        display_toggle(DrbdVolume::DS_LABEL_NEGOTIATING, *cmd_peer_vlm_negotiating, peer_vlm_negotiating.selected);
        display_toggle(DrbdVolume::DS_LABEL_UNKNOWN, *cmd_peer_vlm_unknown_disk, peer_vlm_unknown_disk.selected);
        display_toggle("Invert disk state match", *cmd_inv_peer_vlm_disk_state, inv_peer_vlm_disk_state);
    }
    else
    if (page == 7)
    {
        dsp_comp_hub.dsp_io->cursor_xy(3, 4);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("Peer volume filter criteria - any of the selected replication states:");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        display_toggle(
            DrbdVolume::RS_LABEL_OFF,
            *cmd_peer_vlm_off,          peer_vlm_off.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_ESTABLISHED,
            *cmd_peer_vlm_established,  peer_vlm_established.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_STARTING_SYNC_SOURCE,
            *cmd_peer_vlm_str_sync_src, peer_vlm_str_sync_src.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_STARTING_SYNC_TARGET,
            *cmd_peer_vlm_str_sync_tgt, peer_vlm_str_sync_tgt.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_WF_BITMAP_SOURCE,
            *cmd_peer_vlm_wf_bm_src,    peer_vlm_wf_bm_src.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_WF_BITMAP_TARGET,
            *cmd_peer_vlm_wf_bm_tgt,    peer_vlm_wf_bm_tgt.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_WF_SYNC_UUID,
            *cmd_peer_vlm_wf_sync_uuid, peer_vlm_wf_sync_uuid.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_SYNC_SOURCE,
            *cmd_peer_vlm_sync_src,     peer_vlm_sync_src.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_SYNC_TARGET,
            *cmd_peer_vlm_sync_tgt,     peer_vlm_sync_tgt.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_PAUSED_SYNC_SOURCE,
            *cmd_peer_vlm_psd_sync_src, peer_vlm_psd_sync_src.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_PAUSED_SYNC_TARGET,
            *cmd_peer_vlm_psd_sync_tgt, peer_vlm_psd_sync_tgt.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_VERIFY_SOURCE,
            *cmd_peer_vlm_vfy_src,      peer_vlm_vfy_src.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_VERIFY_TARGET,
            *cmd_peer_vlm_vfy_tgt,      peer_vlm_vfy_tgt.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_AHEAD,
            *cmd_peer_vlm_ahead,        peer_vlm_ahead.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_BEHIND,
            *cmd_peer_vlm_behind,       peer_vlm_behind.selected
        );
        display_toggle(
            DrbdVolume::RS_LABEL_UNKNOWN,
            *cmd_peer_vlm_unknown_repl, peer_vlm_unknown_repl.selected
        );
        display_toggle("Invert replication state match", *cmd_inv_peer_vlm_repl_state, inv_peer_vlm_repl_state);
    }

    display_option_query(5, 17);
}

void MDspSelectionFilter::display_execute()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Execute:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;

    dsp_comp_hub.dsp_io->cursor_xy(3, 6);
    display_option(5, "Select matching objects", *cmd_exec_select, std_color);
    display_option(5, "Deselect matching objects", *cmd_exec_deselect, std_color);
    display_option(5, "Deselect inactive objects", *cmd_discard_inactive_obj, std_color);
    display_option(5, "Reset filter settings", *cmd_reset_flt_opt_slct, std_color);

    display_option_query(5, 17);
}

void MDspSelectionFilter::display_statistics()
{

    current_stats = dsp_comp_hub.dsp_shared->get_selection_statistics();

    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Selection statistics:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    dsp_comp_hub.dsp_io->cursor_xy(5, 6);
    if (current_stats.rsc_count > 0)
    {
        dsp_comp_hub.dsp_io->write_text("Current selection:");

        std::string number_str;
        uint16_t row = 7;

        dsp_comp_hub.dsp_io->cursor_xy(9, row);
        print_stats_line(current_stats.rsc_count, number_str, "resource", "resources",
                         dsp_comp_hub.active_color_table->rsc_name);
        ++row;

        if (current_stats.vlm_count > 0)
        {
            dsp_comp_hub.dsp_io->cursor_xy(9, row);
            print_stats_line(current_stats.vlm_count, number_str, "volume", "volumes",
                             dsp_comp_hub.active_color_table->vlm_count);
            ++row;
        }

        if (current_stats.con_count > 0)
        {
            dsp_comp_hub.dsp_io->cursor_xy(9, row);
            print_stats_line(current_stats.con_count, number_str, "connection", "connections",
                             dsp_comp_hub.active_color_table->con_count);
            ++row;

            if (current_stats.peer_vlm_count > 0)
            {
                dsp_comp_hub.dsp_io->cursor_xy(9, row);
                print_stats_line(current_stats.peer_vlm_count, number_str, "peer volume", "peer volumes",
                                 dsp_comp_hub.active_color_table->vlm_count);
                ++row;
            }
        }

        if (display_diff_stats)
        {
            const uint64_t rsc_diff = diff(current_stats.rsc_count, previous_stats.rsc_count);
            const uint64_t vlm_diff = diff(current_stats.vlm_count, previous_stats.vlm_count);
            const uint64_t con_diff = diff(current_stats.con_count, previous_stats.con_count);
            const uint64_t peer_vlm_diff = diff(current_stats.peer_vlm_count, previous_stats.peer_vlm_count);

            const bool rsc_inc = comparators::compare(&current_stats.rsc_count, &previous_stats.rsc_count) >= 0;
            const bool vlm_inc = comparators::compare(&current_stats.vlm_count, &previous_stats.vlm_count) >= 0;
            const bool con_inc = comparators::compare(&current_stats.con_count, &previous_stats.con_count) >= 0;
            const bool peer_vlm_inc =
                comparators::compare(&current_stats.peer_vlm_count, &previous_stats.peer_vlm_count) >= 0;

            dsp_comp_hub.dsp_io->cursor_xy(5, row);
            dsp_comp_hub.dsp_io->write_text("Selection update:");
            ++row;
            if (rsc_diff > 0 || vlm_diff > 0 || con_diff > 0 || peer_vlm_diff > 0)
            {
                if (rsc_diff > 0)
                {
                    dsp_comp_hub.dsp_io->cursor_xy(9, row);
                    dsp_comp_hub.dsp_io->write_text(rsc_inc ? "+ " : "- ");
                    print_stats_line(rsc_diff, number_str, "resource", "resources",
                                     dsp_comp_hub.active_color_table->rsc_name);
                    ++row;
                }

                if (vlm_diff > 0)
                {
                    dsp_comp_hub.dsp_io->cursor_xy(9, row);
                    dsp_comp_hub.dsp_io->write_text(vlm_inc ? "+ " : "- ");
                    print_stats_line(vlm_diff, number_str, "volume", "volumes",
                                     dsp_comp_hub.active_color_table->vlm_count);
                    ++row;
                }

                if (con_diff > 0)
                {
                    dsp_comp_hub.dsp_io->cursor_xy(9, row);
                    dsp_comp_hub.dsp_io->write_text(con_inc ? "+ " : "- ");
                    print_stats_line(con_diff, number_str, "connection", "connections",
                                     dsp_comp_hub.active_color_table->con_count);
                    ++row;
                }

                if (peer_vlm_diff > 0)
                {
                    dsp_comp_hub.dsp_io->cursor_xy(9, row);
                    dsp_comp_hub.dsp_io->write_text(peer_vlm_inc ? "+ " : "- ");
                    print_stats_line(peer_vlm_diff, number_str, "peer volume", "peer volumes",
                                     dsp_comp_hub.active_color_table->vlm_count);
                    ++row;
                }
            }
            else
            {
                dsp_comp_hub.dsp_io->cursor_xy(9, row);
                dsp_comp_hub.dsp_io->write_text("No changes");
                ++row;
            }
        }

        if (!error_msg.empty())
        {
            ++row;
            dsp_comp_hub.dsp_io->cursor_xy(5, row);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
            dsp_comp_hub.dsp_io->write_text(error_msg.c_str());
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
        }
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text("No selection");
    }
}

uint64_t MDspSelectionFilter::diff(const uint64_t value, const uint64_t other)
{
    return value >= other ? value - other : other - value;
}

void MDspSelectionFilter::print_stats_line(
    const uint64_t      value,
    std::string&        render_str,
    const char* const   label_single,
    const char* const   label_multi,
    const std::string&  color
)
{
    string_transformations::format_uint64(value, render_str, true);
    // Shorten to tens of billions
    const size_t str_length = render_str.length();
    if (str_length >= 13 && render_str[12] == ' ')
    {
        render_str = render_str.substr(12, str_length);
    }
    dsp_comp_hub.dsp_io->write_text(color.c_str());
    dsp_comp_hub.dsp_io->write_text(render_str.c_str());
    dsp_comp_hub.dsp_io->write_text(" ");
    dsp_comp_hub.dsp_io->write_text(value == 1 ? label_single : label_multi);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
}

void MDspSelectionFilter::reset_stats_page()
{
    error_msg.clear();
    display_diff_stats = false;
}

void MDspSelectionFilter::cursor_to_previous_item()
{
    const uint32_t page = get_page_nr();
    if (page == 1)
    {
        if (active_input == con_name_pattern_input.get())
        {
            active_input = rsc_name_pattern_input.get();
        }
        else
        if (active_input == vlm_number_input.get())
        {
            active_input = con_name_pattern_input.get();
        }
        else
        if (is_focus_delegated())
        {
            active_input = nullptr;
            delegate_focus(false);
        }
        else
        {
            active_input = vlm_number_input.get();
            delegate_focus(true);
        }
        dsp_comp_hub.dsp_selector->refresh_display();
    }
}

void MDspSelectionFilter::cursor_to_next_item()
{
    const uint32_t page = get_page_nr();
    if (page == 1)
    {
        if (active_input == rsc_name_pattern_input.get())
        {
            active_input = con_name_pattern_input.get();
        }
        else
        if (active_input == con_name_pattern_input.get())
        {
            active_input = vlm_number_input.get();
        }
        else
        if (is_focus_delegated())
        {
            active_input = nullptr;
            delegate_focus(false);
        }
        else
        {
            active_input = rsc_name_pattern_input.get();
            delegate_focus(true);
        }
        dsp_comp_hub.dsp_selector->refresh_display();
    }
}

void MDspSelectionFilter::execute_select()
{
    bool config_good = true;
    // TODO: Add configuration consistency checks here
    if (config_good)
    {
        try
        {
            filter_select();
            display_diff_stats = true;
        }
        catch (dsaext::NumberFormatException&)
        {
            error_msg = "Unparsable volume number";
        }
        catch (string_matching::PatternLimitException&)
        {
            error_msg = "Text pattern exceeds maximum number of wildcard characters";
        }
    }

    // Switch to the statistics page
    set_page_nr(stats_page);

    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspSelectionFilter::execute_deselect()
{
    bool config_good = op_slct_rsc || op_slct_vlm || op_slct_con || op_slct_peer_vlm;
    if (!config_good)
    {
        error_msg = "Choose object types to deselect";
    }

    if (config_good)
    {
        try
        {
            filter_deselect();
            display_diff_stats = true;
        }
        catch (dsaext::NumberFormatException&)
        {
            error_msg = "Unparsable volume number";
        }
        catch (string_matching::PatternLimitException&)
        {
            error_msg = "Text pattern exceeds maximum number of wildcard characters";
        }
    }

    // Switch to the statistics page
    set_page_nr(stats_page);

    dsp_comp_hub.dsp_selector->refresh_display();
}

// @throws std::bad_alloc, string_matching::PatternLimitException, dsaext::NumberFormatException
void MDspSelectionFilter::filter_select()
{
    dsp_comp_hub.dsp_common->application_working();

    previous_stats = dsp_comp_hub.dsp_shared->get_selection_statistics();

    std::unique_ptr<selection_filter::FilterSettings> settings_mgr(new selection_filter::FilterSettings());
    selection_filter::FilterSettings& settings = *settings_mgr;
    configure_filter_settings(settings);

    SharedData& dsp_shared = *(dsp_comp_hub.dsp_shared);
    ResourcesMap& rsc_map = *(dsp_comp_hub.rsc_map);
    MessageLog& debug_log = *(dsp_comp_hub.debug_log);
    selection_filter::filter_select(settings, rsc_map, dsp_shared, debug_log);
}

// @throws std::bad_alloc, string_matching::PatternLimitException, dsaext::NumberFormatException
void MDspSelectionFilter::filter_deselect()
{
    dsp_comp_hub.dsp_common->application_working();

    previous_stats = dsp_comp_hub.dsp_shared->get_selection_statistics();

    std::unique_ptr<selection_filter::FilterSettings> settings_mgr(new selection_filter::FilterSettings());
    selection_filter::FilterSettings& settings = *settings_mgr;
    configure_filter_settings(settings);

    SharedData& dsp_shared = *(dsp_comp_hub.dsp_shared);
    ResourcesMap& rsc_map = *(dsp_comp_hub.rsc_map);
    MessageLog& debug_log = *(dsp_comp_hub.debug_log);
    selection_filter::filter_deselect(settings, rsc_map, dsp_shared, debug_log);
}

void MDspSelectionFilter::discard_inactive_objects_selection()
{
    dsp_comp_hub.dsp_common->application_working();

    ResourcesMap& rsc_map = *dsp_comp_hub.rsc_map;
    ResourceSelectionMap::NodesIterator slct_rsc_iter(*dsp_comp_hub.dsp_shared->selected_resources);
    while (slct_rsc_iter.has_next())
    {
        ResourceSelectionMap::Node* const slct_rsc_node = slct_rsc_iter.next();
        const std::string& rsc_name = *(slct_rsc_node->get_key());
        DrbdResource* const rsc = rsc_map.get(&rsc_name);
        if (rsc != nullptr)
        {
            ResourceSubSelections& sub_selections = *(slct_rsc_node->get_value());
            if (sub_selections.volume_selection)
            {
                VolumeSelectionMap::KeysIterator slct_vlm_iter(*sub_selections.volume_selection);
                while (slct_vlm_iter.has_next())
                {
                    const uint16_t& vlm_nr = *(slct_vlm_iter.next());
                    if (rsc->get_volume(vlm_nr) == nullptr)
                    {
                        dsp_comp_hub.dsp_shared->deselect_volume(sub_selections, vlm_nr);
                    }
                }
            }

            if (sub_selections.connection_selection)
            {
                ConnectionSelectionMap::NodesIterator slct_con_iter(*sub_selections.connection_selection);
                while (slct_con_iter.has_next())
                {
                    ConnectionSelectionMap::Node* const slct_con_node = slct_con_iter.next();
                    const std::string& con_name = *(slct_con_node->get_key());
                    DrbdConnection* const con = rsc->get_connection(con_name);
                    if (con != nullptr)
                    {
                        VolumeSelectionMap* const slct_peer_vlm_map = slct_con_node->get_value();
                        if (slct_peer_vlm_map != nullptr)
                        {
                            VolumeSelectionMap::KeysIterator slct_peer_vlm_iter(*slct_peer_vlm_map);
                            while (slct_peer_vlm_iter.has_next())
                            {
                                const uint16_t& peer_vlm_nr = *(slct_peer_vlm_iter.next());
                                if (con->get_volume(peer_vlm_nr) == nullptr)
                                {
                                    dsp_comp_hub.dsp_shared->deselect_peer_volume(*slct_con_node, peer_vlm_nr);
                                }
                            }
                        }
                    }
                    else
                    {
                        dsp_comp_hub.dsp_shared->deselect_connection(sub_selections, con_name);
                    }
                }
            }
        }
        else
        {
            dsp_comp_hub.dsp_shared->deselect_resource(rsc_name);
        }
    }
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspSelectionFilter::configure_filter_settings(selection_filter::FilterSettings& settings)
{
    if (!rsc_name_pattern_input->is_empty())
    {
        settings.rsc_name_pattern = rsc_name_pattern_input->get_text();
    }
    if (!con_name_pattern_input->is_empty())
    {
        settings.con_name_pattern = con_name_pattern_input->get_text();
    }
    if (!vlm_number_input->is_empty())
    {
        const std::string& vlm_nr_text = vlm_number_input->get_text();
        // throws dsaext::NumberFormatException
        settings.vlm_number = dsaext::parse_unsigned_int16(vlm_nr_text);
        settings.filter_vlm_number = true;
    }

    SelectorToChain<VList<StateSelector<bool>>, bool, DrbdResource>::transform(
        filter_options.resource_op_state,
        settings.rsc_op_chain,
        &selection_filter::make_object_degraded_selector
    );

    SelectorToChain<VList<StateSelector<bool>>, bool, DrbdResource>::transform(
        filter_options.resource_quorum,
        settings.rsc_quorum_chain,
        &selection_filter::make_resource_quorum_selector
    );

    SelectorToChain<VList<ResourceRoleSelector>, DrbdResource::resource_role, DrbdResource>::transform(
        filter_options.resource_role,
        settings.rsc_role_chain,
        &selection_filter::make_resource_role_selector
    );

    SelectorToChain<VList<StateSelector<bool>>, bool, DrbdVolume>::transform(
        filter_options.volume_op_state,
        settings.vlm_op_chain,
        &selection_filter::make_object_degraded_selector
    );

    SelectorToChain<VList<StateSelector<bool>>, bool, DrbdVolume>::transform(
        filter_options.volume_quorum,
        settings.vlm_quorum_chain,
        &selection_filter::make_volume_quorum_selector
    );

    // Remember end of the chain for appending to the same chain again
    selection_filter::FilterNode<DrbdVolume>* vlm_state_chain_end =
        SelectorToChain<VList<StateSelector<bool>>, bool, DrbdVolume>::transform(
            filter_options.volume_diskless_state,
            settings.vlm_state_chain,
            &selection_filter::make_volume_client_state_selector
        );

    // Append to the end of the existing chain
    SelectorToChain<VList<VolumeDiskStateSelector>, DrbdVolume::disk_state, DrbdVolume>::transform(
        filter_options.volume_disk_state,
        *vlm_state_chain_end,
        &selection_filter::make_volume_disk_state_selector
    );

    SelectorToChain<VList<StateSelector<bool>>, bool, DrbdConnection>::transform(
        filter_options.connection_op_state,
        settings.con_op_chain,
        &selection_filter::make_object_degraded_selector
    );

    SelectorToChain<VList<ResourceRoleSelector>, DrbdResource::resource_role, DrbdConnection>::transform(
        filter_options.connection_role,
        settings.con_role_chain,
        &selection_filter::make_connection_role_selector
    );

    SelectorToChain<VList<ConnectionStateSelector>, DrbdConnection::state, DrbdConnection>::transform(
        filter_options.connection_state,
        settings.con_state_chain,
        &selection_filter::make_connection_state_selector
    );

    SelectorToChain<VList<StateSelector<bool>>, bool, DrbdVolume>::transform(
        filter_options.peer_volume_op_state,
        settings.peer_vlm_op_chain,
        &selection_filter::make_object_degraded_selector
    );

    SelectorToChain<VList<StateSelector<bool>>, bool, DrbdVolume>::transform(
        filter_options.peer_volume_quorum,
        settings.peer_vlm_quorum_chain,
        &selection_filter::make_volume_quorum_selector
    );

    // Remember end of the chain for appending to the same chain again
    selection_filter::FilterNode<DrbdVolume>* peer_vlm_state_chain_end =
        SelectorToChain<VList<StateSelector<bool>>, bool, DrbdVolume>::transform(
            filter_options.peer_volume_diskless_state,
            settings.peer_vlm_state_chain,
            &selection_filter::make_volume_client_state_selector
        );

    // Append to the end of the existing chain
    SelectorToChain<VList<VolumeDiskStateSelector>, DrbdVolume::disk_state, DrbdVolume>::transform(
        filter_options.peer_volume_disk_state,
        *peer_vlm_state_chain_end,
        &selection_filter::make_volume_disk_state_selector
    );

    SelectorToChain<VList<VolumeReplStateSelector>, DrbdVolume::repl_state, DrbdVolume>::transform(
        filter_options.peer_volume_repl_state,
        settings.peer_vlm_repl_state_chain,
        &selection_filter::make_volume_repl_state_selector
    );

    settings.restrictions.selected_resources = rstr_to_slct_rsc;
    settings.restrictions.selected_volumes = rstr_to_slct_vlm;
    settings.restrictions.selected_connections = rstr_to_slct_con;
    settings.restrictions.selected_peer_volumes = rstr_to_slct_peer_vlm;

    settings.targets.select_resources = op_slct_rsc;
    settings.targets.select_volumes = op_slct_vlm;
    settings.targets.select_connections = op_slct_con;
    settings.targets.select_peer_volumes = op_slct_peer_vlm;

    settings.invert.resource_name = inv_rsc_name;
    settings.invert.connection_name = inv_con_name;
    settings.invert.volume_number = inv_vlm_number;
    settings.invert.volume_state = inv_vlm_state;
    settings.invert.connection_state = inv_con_state;
    settings.invert.peer_volume_disk_state = inv_peer_vlm_disk_state;
    settings.invert.peer_volume_repl_state = inv_peer_vlm_repl_state;
}
