#ifndef MDSPSELECTIONFILTER_H
#define MDSPSELECTIONFILTER_H

#include <default_types.h>
#include <string>
#include <terminal/MDspMenuBase.h>
#include <terminal/InputField.h>
#include <terminal/selection_filter.h>
#include <objects/DrbdResource.h>
#include <objects/DrbdConnection.h>
#include <objects/DrbdVolume.h>
#include <cppdsaext/src/VList.h>

class MDspSelectionFilter : public MDspMenuBase
{
  public:
    template<class T>
    class StateSelector
    {
      public:
        StateSelector(T state):
            value(state)
        {
        }
        virtual ~StateSelector() noexcept
        {
        }

        bool    selected   {false};
        const T value;
    };

  private:
    class ToggleBoolFunctor
    {
      public:
        ToggleBoolFunctor(bool& state);
        virtual ~ToggleBoolFunctor() noexcept;
        // Initialize pointer to the ComponentsHub instance before calling operator()
        // to enable automatic display updates
        void operator()() noexcept;
        bool& state_ref;

        static const ComponentsHub *dsp_comp_hub_ptr;
    };

    using ResourceRoleSelector          = StateSelector<DrbdRole::resource_role>;
    using ConnectionStateSelector       = StateSelector<DrbdConnection::state>;
    using VolumeDiskStateSelector       = StateSelector<DrbdVolume::disk_state>;
    using VolumeReplStateSelector       = StateSelector<DrbdVolume::repl_state>;

    class FilterOptionsCollection
    {
      public:
        VList<StateSelector<bool>>          resource_op_state;
        VList<ResourceRoleSelector>         resource_role;
        VList<StateSelector<bool>>          resource_quorum;

        VList<StateSelector<bool>>          volume_op_state;
        VList<StateSelector<bool>>          volume_quorum;
        VList<StateSelector<bool>>          volume_diskless_state;
        VList<VolumeDiskStateSelector>      volume_disk_state;

        VList<StateSelector<bool>>          connection_op_state;
        VList<ResourceRoleSelector>         connection_role;
        VList<ConnectionStateSelector>      connection_state;

        VList<StateSelector<bool>>          peer_volume_op_state;
        VList<StateSelector<bool>>          peer_volume_quorum;
        VList<StateSelector<bool>>          peer_volume_diskless_state;
        VList<VolumeDiskStateSelector>      peer_volume_disk_state;
        VList<VolumeReplStateSelector>      peer_volume_repl_state;

        using BoolSlctIter = VList<StateSelector<bool>>::ValuesIterator;
        using RoleSlctIter = VList<ResourceRoleSelector>::ValuesIterator;
        using DiskStateSlctIter = VList<VolumeDiskStateSelector>::ValuesIterator;
        using ConnStateSlctIter = VList<ConnectionStateSelector>::ValuesIterator;
        using ReplStateSlctIter = VList<VolumeReplStateSelector>::ValuesIterator;

        FilterOptionsCollection();
        virtual ~FilterOptionsCollection() noexcept;
    };

  public:
    MDspSelectionFilter(const ComponentsHub& comp_hub);
    virtual ~MDspSelectionFilter() noexcept;

    virtual void display_content() override;

    virtual uint64_t get_update_mask() noexcept override;
    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;

    virtual void text_cursor_ops() override;

    virtual void display_activated() override;
    virtual void display_deactivated() override;
    virtual void display_closed() override;

    virtual void cursor_to_previous_item() override;
    virtual void cursor_to_next_item() override;

  private:
    StateSelector<bool>         rsc_op_normal;
    StateSelector<bool>         rsc_op_degraded;
    StateSelector<bool>         rsc_with_quorum;
    StateSelector<bool>         rsc_without_quorum;
    ResourceRoleSelector        rsc_primary;
    ResourceRoleSelector        rsc_secondary;

    StateSelector<bool>         vlm_op_normal;
    StateSelector<bool>         vlm_op_degraded;
    StateSelector<bool>         vlm_with_quorum;
    StateSelector<bool>         vlm_without_quorum;
    StateSelector<bool>         vlm_diskless_client;
    StateSelector<bool>         vlm_diskless_failed;
    VolumeDiskStateSelector     vlm_uptodate;
    VolumeDiskStateSelector     vlm_consistent;
    VolumeDiskStateSelector     vlm_inconsistent;
    VolumeDiskStateSelector     vlm_outdated;
    VolumeDiskStateSelector     vlm_attaching;
    VolumeDiskStateSelector     vlm_detaching;
    VolumeDiskStateSelector     vlm_failed;
    VolumeDiskStateSelector     vlm_negotiating;
    VolumeDiskStateSelector     vlm_unknown;

    StateSelector<bool>         con_op_normal;
    StateSelector<bool>         con_op_degraded;
    ResourceRoleSelector        con_primary;
    ResourceRoleSelector        con_secondary;
    ResourceRoleSelector        con_unknown_role;
    ConnectionStateSelector     con_standalone;
    ConnectionStateSelector     con_disconnecting;
    ConnectionStateSelector     con_unconnected;
    ConnectionStateSelector     con_timeout;
    ConnectionStateSelector     con_broken_pipe;
    ConnectionStateSelector     con_network_failure;
    ConnectionStateSelector     con_protocol_error;
    ConnectionStateSelector     con_tear_down;
    ConnectionStateSelector     con_connecting;
    ConnectionStateSelector     con_connected;
    ConnectionStateSelector     con_unknown_conn;

    StateSelector<bool>         peer_vlm_op_normal;
    StateSelector<bool>         peer_vlm_op_degraded;
    StateSelector<bool>         peer_vlm_with_quorum;
    StateSelector<bool>         peer_vlm_without_quorum;
    StateSelector<bool>         peer_vlm_diskless_client;
    StateSelector<bool>         peer_vlm_diskless_failed;
    VolumeDiskStateSelector     peer_vlm_uptodate;
    VolumeDiskStateSelector     peer_vlm_consistent;
    VolumeDiskStateSelector     peer_vlm_inconsistent;
    VolumeDiskStateSelector     peer_vlm_outdated;
    VolumeDiskStateSelector     peer_vlm_attaching;
    VolumeDiskStateSelector     peer_vlm_detaching;
    VolumeDiskStateSelector     peer_vlm_failed;
    VolumeDiskStateSelector     peer_vlm_negotiating;
    VolumeDiskStateSelector     peer_vlm_unknown_disk;

    VolumeReplStateSelector     peer_vlm_off;
    VolumeReplStateSelector     peer_vlm_established;
    VolumeReplStateSelector     peer_vlm_str_sync_src;
    VolumeReplStateSelector     peer_vlm_str_sync_tgt;
    VolumeReplStateSelector     peer_vlm_wf_bm_src;
    VolumeReplStateSelector     peer_vlm_wf_bm_tgt;
    VolumeReplStateSelector     peer_vlm_wf_sync_uuid;
    VolumeReplStateSelector     peer_vlm_sync_src;
    VolumeReplStateSelector     peer_vlm_sync_tgt;
    VolumeReplStateSelector     peer_vlm_psd_sync_src;
    VolumeReplStateSelector     peer_vlm_psd_sync_tgt;
    VolumeReplStateSelector     peer_vlm_vfy_src;
    VolumeReplStateSelector     peer_vlm_vfy_tgt;
    VolumeReplStateSelector     peer_vlm_ahead;
    VolumeReplStateSelector     peer_vlm_behind;
    VolumeReplStateSelector     peer_vlm_unknown_repl;

    std::function<void()>   cmd_fn_rsc_op_normal;
    std::function<void()>   cmd_fn_rsc_op_degraded;
    std::function<void()>   cmd_fn_rsc_with_quorum;
    std::function<void()>   cmd_fn_rsc_without_quorum;
    std::function<void()>   cmd_fn_rsc_primary;
    std::function<void()>   cmd_fn_rsc_secondary;

    std::function<void()>   cmd_fn_inv_rsc_name;

    std::function<void()>   cmd_fn_vlm_op_normal;
    std::function<void()>   cmd_fn_vlm_op_degraded;
    std::function<void()>   cmd_fn_vlm_with_quorum;
    std::function<void()>   cmd_fn_vlm_without_quorum;
    std::function<void()>   cmd_fn_vlm_diskless_client;
    std::function<void()>   cmd_fn_vlm_diskless_failed;
    std::function<void()>   cmd_fn_vlm_uptodate;
    std::function<void()>   cmd_fn_vlm_consistent;
    std::function<void()>   cmd_fn_vlm_inconsistent;
    std::function<void()>   cmd_fn_vlm_outdated;
    std::function<void()>   cmd_fn_vlm_attaching;
    std::function<void()>   cmd_fn_vlm_detaching;
    std::function<void()>   cmd_fn_vlm_failed;
    std::function<void()>   cmd_fn_vlm_negotiating;
    std::function<void()>   cmd_fn_vlm_unknown;

    std::function<void()>   cmd_fn_inv_vlm_number;
    std::function<void()>   cmd_fn_inv_vlm_state;

    std::function<void()>   cmd_fn_con_op_normal;
    std::function<void()>   cmd_fn_con_op_degraded;
    std::function<void()>   cmd_fn_con_primary;
    std::function<void()>   cmd_fn_con_secondary;
    std::function<void()>   cmd_fn_con_unknown_role;
    std::function<void()>   cmd_fn_con_standalone;
    std::function<void()>   cmd_fn_con_disconnecting;
    std::function<void()>   cmd_fn_con_unconnected;
    std::function<void()>   cmd_fn_con_timeout;
    std::function<void()>   cmd_fn_con_broken_pipe;
    std::function<void()>   cmd_fn_con_network_failure;
    std::function<void()>   cmd_fn_con_protocol_error;
    std::function<void()>   cmd_fn_con_tear_down;
    std::function<void()>   cmd_fn_con_connecting;
    std::function<void()>   cmd_fn_con_connected;
    std::function<void()>   cmd_fn_con_unknown_conn;

    std::function<void()>   cmd_fn_inv_con_name;
    std::function<void()>   cmd_fn_inv_con_state;

    std::function<void()>   cmd_fn_peer_vlm_op_normal;
    std::function<void()>   cmd_fn_peer_vlm_op_degraded;
    std::function<void()>   cmd_fn_peer_vlm_with_quorum;
    std::function<void()>   cmd_fn_peer_vlm_without_quorum;
    std::function<void()>   cmd_fn_peer_vlm_diskless_client;
    std::function<void()>   cmd_fn_peer_vlm_diskless_failed;
    std::function<void()>   cmd_fn_peer_vlm_uptodate;
    std::function<void()>   cmd_fn_peer_vlm_consistent;
    std::function<void()>   cmd_fn_peer_vlm_inconsistent;
    std::function<void()>   cmd_fn_peer_vlm_outdated;
    std::function<void()>   cmd_fn_peer_vlm_attaching;
    std::function<void()>   cmd_fn_peer_vlm_detaching;
    std::function<void()>   cmd_fn_peer_vlm_failed;
    std::function<void()>   cmd_fn_peer_vlm_negotiating;
    std::function<void()>   cmd_fn_peer_vlm_unknown_disk;

    std::function<void()>   cmd_fn_inv_peer_vlm_disk_state;

    std::function<void()>   cmd_fn_peer_vlm_off;
    std::function<void()>   cmd_fn_peer_vlm_established;
    std::function<void()>   cmd_fn_peer_vlm_str_sync_src;
    std::function<void()>   cmd_fn_peer_vlm_str_sync_tgt;
    std::function<void()>   cmd_fn_peer_vlm_wf_bm_src;
    std::function<void()>   cmd_fn_peer_vlm_wf_bm_tgt;
    std::function<void()>   cmd_fn_peer_vlm_wf_sync_uuid;
    std::function<void()>   cmd_fn_peer_vlm_sync_src;
    std::function<void()>   cmd_fn_peer_vlm_sync_tgt;
    std::function<void()>   cmd_fn_peer_vlm_psd_sync_src;
    std::function<void()>   cmd_fn_peer_vlm_psd_sync_tgt;
    std::function<void()>   cmd_fn_peer_vlm_vfy_src;
    std::function<void()>   cmd_fn_peer_vlm_vfy_tgt;
    std::function<void()>   cmd_fn_peer_vlm_ahead;
    std::function<void()>   cmd_fn_peer_vlm_behind;
    std::function<void()>   cmd_fn_peer_vlm_unknown_repl;

    std::function<void()>   cmd_fn_inv_peer_vlm_repl_state;

    std::function<void()>   cmd_fn_exec_select;
    std::function<void()>   cmd_fn_exec_deselect;
    std::function<void()>   cmd_fn_discard_inactive_obj;

    std::function<void()>   cmd_fn_rstr_to_slct_rsc;
    std::function<void()>   cmd_fn_rstr_to_slct_vlm;
    std::function<void()>   cmd_fn_rstr_to_slct_con;
    std::function<void()>   cmd_fn_rstr_to_slct_peer_vlm;

    std::function<void()>   cmd_fn_op_slct_rsc;
    std::function<void()>   cmd_fn_op_slct_vlm;
    std::function<void()>   cmd_fn_op_slct_con;
    std::function<void()>   cmd_fn_op_slct_peer_vlm;

    std::function<void()>   cmd_fn_reset_flt_opt_slct;

    std::unique_ptr<ClickableCommand>   cmd_rsc_op_normal;
    std::unique_ptr<ClickableCommand>   cmd_rsc_op_degraded;
    std::unique_ptr<ClickableCommand>   cmd_rsc_with_quorum;
    std::unique_ptr<ClickableCommand>   cmd_rsc_without_quorum;
    std::unique_ptr<ClickableCommand>   cmd_rsc_primary;
    std::unique_ptr<ClickableCommand>   cmd_rsc_secondary;

    std::unique_ptr<ClickableCommand>   cmd_inv_rsc_name;

    std::unique_ptr<ClickableCommand>   cmd_vlm_op_normal;
    std::unique_ptr<ClickableCommand>   cmd_vlm_op_degraded;
    std::unique_ptr<ClickableCommand>   cmd_vlm_with_quorum;
    std::unique_ptr<ClickableCommand>   cmd_vlm_without_quorum;
    std::unique_ptr<ClickableCommand>   cmd_vlm_diskless_client;
    std::unique_ptr<ClickableCommand>   cmd_vlm_diskless_failed;
    std::unique_ptr<ClickableCommand>   cmd_vlm_uptodate;
    std::unique_ptr<ClickableCommand>   cmd_vlm_consistent;
    std::unique_ptr<ClickableCommand>   cmd_vlm_inconsistent;
    std::unique_ptr<ClickableCommand>   cmd_vlm_outdated;
    std::unique_ptr<ClickableCommand>   cmd_vlm_attaching;
    std::unique_ptr<ClickableCommand>   cmd_vlm_detaching;
    std::unique_ptr<ClickableCommand>   cmd_vlm_failed;
    std::unique_ptr<ClickableCommand>   cmd_vlm_negotiating;
    std::unique_ptr<ClickableCommand>   cmd_vlm_unknown;

    std::unique_ptr<ClickableCommand>   cmd_inv_vlm_number;
    std::unique_ptr<ClickableCommand>   cmd_inv_vlm_state;

    std::unique_ptr<ClickableCommand>   cmd_con_op_normal;
    std::unique_ptr<ClickableCommand>   cmd_con_op_degraded;
    std::unique_ptr<ClickableCommand>   cmd_con_primary;
    std::unique_ptr<ClickableCommand>   cmd_con_secondary;
    std::unique_ptr<ClickableCommand>   cmd_con_unknown_role;
    std::unique_ptr<ClickableCommand>   cmd_con_standalone;
    std::unique_ptr<ClickableCommand>   cmd_con_disconnecting;
    std::unique_ptr<ClickableCommand>   cmd_con_unconnected;
    std::unique_ptr<ClickableCommand>   cmd_con_timeout;
    std::unique_ptr<ClickableCommand>   cmd_con_broken_pipe;
    std::unique_ptr<ClickableCommand>   cmd_con_network_failure;
    std::unique_ptr<ClickableCommand>   cmd_con_protocol_error;
    std::unique_ptr<ClickableCommand>   cmd_con_tear_down;
    std::unique_ptr<ClickableCommand>   cmd_con_connecting;
    std::unique_ptr<ClickableCommand>   cmd_con_connected;
    std::unique_ptr<ClickableCommand>   cmd_con_unknown_conn;

    std::unique_ptr<ClickableCommand>   cmd_inv_con_name;
    std::unique_ptr<ClickableCommand>   cmd_inv_con_state;

    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_op_normal;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_op_degraded;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_with_quorum;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_without_quorum;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_diskless_client;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_diskless_failed;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_uptodate;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_consistent;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_inconsistent;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_outdated;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_attaching;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_detaching;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_failed;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_negotiating;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_unknown_disk;

    std::unique_ptr<ClickableCommand>   cmd_inv_peer_vlm_disk_state;

    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_off;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_established;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_str_sync_src;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_str_sync_tgt;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_wf_bm_src;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_wf_bm_tgt;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_wf_sync_uuid;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_sync_src;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_sync_tgt;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_psd_sync_src;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_psd_sync_tgt;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_vfy_src;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_vfy_tgt;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_ahead;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_behind;
    std::unique_ptr<ClickableCommand>   cmd_peer_vlm_unknown_repl;

    std::unique_ptr<ClickableCommand>   cmd_inv_peer_vlm_repl_state;

    std::unique_ptr<ClickableCommand>   cmd_exec_select;
    std::unique_ptr<ClickableCommand>   cmd_exec_deselect;
    std::unique_ptr<ClickableCommand>   cmd_discard_inactive_obj;

    std::unique_ptr<ClickableCommand>   cmd_rstr_to_slct_rsc;
    std::unique_ptr<ClickableCommand>   cmd_rstr_to_slct_vlm;
    std::unique_ptr<ClickableCommand>   cmd_rstr_to_slct_con;
    std::unique_ptr<ClickableCommand>   cmd_rstr_to_slct_peer_vlm;

    std::unique_ptr<ClickableCommand>   cmd_op_slct_rsc;
    std::unique_ptr<ClickableCommand>   cmd_op_slct_vlm;
    std::unique_ptr<ClickableCommand>   cmd_op_slct_con;
    std::unique_ptr<ClickableCommand>   cmd_op_slct_peer_vlm;

    std::unique_ptr<ClickableCommand>   cmd_reset_flt_opt_slct;

    FilterOptionsCollection filter_options;

    std::unique_ptr<InputField> rsc_name_pattern_input;
    std::unique_ptr<InputField> con_name_pattern_input;
    std::unique_ptr<InputField> vlm_number_input;

    // Restrict filtering to already selected objects
    bool rstr_to_slct_rsc       {false};
    bool rstr_to_slct_vlm       {false};
    bool rstr_to_slct_con       {false};
    bool rstr_to_slct_peer_vlm  {false};

    // What objects to operate on when selecting or deselecting
    bool op_slct_rsc                {false};
    bool op_slct_vlm                {false};
    bool op_slct_con                {false};
    bool op_slct_peer_vlm           {false};

    bool inv_rsc_name               {false};
    bool inv_con_name               {false};
    bool inv_vlm_number             {false};

    bool inv_con_state              {false};
    bool inv_vlm_state              {false};
    bool inv_peer_vlm_disk_state    {false};
    bool inv_peer_vlm_repl_state    {false};

    InputField* active_input        {nullptr};

    uint32_t                        stats_page  {0};

    SharedData::SelectionStatistics current_stats;
    SharedData::SelectionStatistics previous_stats;
    bool                            display_diff_stats;
    std::string                     error_msg;

    void display_toggle(const char* const text, ClickableCommand& cmd, const bool& selected);

    void display_restrict_selection();
    void display_resource_criteria();
    void display_volume_criteria();
    void display_connection_criteria();
    void display_peer_volume_criteria();
    void display_execute();
    void display_statistics();
    uint64_t diff(const uint64_t value, const uint64_t other);
    void print_stats_line(
        const uint64_t      value,
        std::string&        render_str,
        const char* const   label_single,
        const char* const   label_multi,
        const std::string&  color
    );
    void reset_stats_page();

    void execute_select();
    void execute_deselect();
    // @throws dsaext::NumberFormatException
    void filter_select();
    // @throws dsaext::NumberFormatException
    void filter_deselect();
    void discard_inactive_objects_selection();

    void configure_filter_settings(selection_filter::FilterSettings& settings);

    void generate_filter_options_collection();
    void reset_filter_options_selection();
    void setup_cmd_functions();
    void setup_pages();

    template<class T>
    void reset_filter_options_list(VList<T>& opt_list)
    {
        typename VList<T>::ValuesIterator iter(opt_list);
        while (iter.has_next())
        {
            T* const selector = iter.next();
            selector->selected = false;
        }
    }

    template<class LIST, class SLCT_TYPE, class OBJ>
    class SelectorToChain
    {
      public:
        typedef selection_filter::FilterNode<OBJ>& (*append_chain_node_fn)(
            selection_filter::FilterNode<OBJ>&,
            SLCT_TYPE value
        );

        static selection_filter::FilterNode<OBJ>* transform(
            LIST& selector_list,
            selection_filter::FilterNode<OBJ>& chain,
            append_chain_node_fn append_chain_node
        )
        {
            typename LIST::ValuesIterator list_iter(selector_list);
            selection_filter::FilterNode<OBJ>* chain_end_ptr = &chain;
            while (list_iter.has_next())
            {
                StateSelector<SLCT_TYPE>* const selector = list_iter.next();
                if (selector->selected)
                {
                    chain_end_ptr = &(append_chain_node(*chain_end_ptr, selector->value));
                }
            }
            return chain_end_ptr;
        }
    };
};

template<class T>
int compare_state_selector(
    const MDspSelectionFilter::StateSelector<T>* const key_ptr,
    const MDspSelectionFilter::StateSelector<T>* const other_ptr
)
{
    int result = 0;
    if (key_ptr->value < other_ptr->value)
    {
        result = -1;
    }
    else
    if (key_ptr->value > other_ptr->value)
    {
        result = 1;
    }

    if (result == 0)
    {
        if (key_ptr->selected < other_ptr->selected)
        {
            result = -1;
        }
        else
        if (key_ptr->selected > other_ptr->selected)
        {
            result = 1;
        }
    }

    return result;
}

#endif /* MDSPSELECTIONFILTER_H */
