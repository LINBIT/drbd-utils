#ifndef SELECTION_FILTER_H
#define SELECTION_FILTER_H

#include <memory>
#include <functional>
#include <objects/DrbdResource.h>
#include <objects/DrbdConnection.h>
#include <objects/DrbdVolume.h>
#include <terminal/SharedData.h>
#include <MessageLog.h>
#include <string_matching.h>
#include <map_types.h>

namespace selection_filter
{
    template<class T>
    class FilterNode
    {
      public:
        FilterNode(FilterNode<T>* const chain_end)
        {
            prev_node = chain_end;
        }

        virtual ~FilterNode() noexcept
        {
        }

        virtual bool match(const T& object) const = 0;

        std::unique_ptr<FilterNode<T>>  next_node;
        FilterNode<T>*                  prev_node;
    };

    template<class T>
    class FilterChain : public FilterNode<T>
    {
      public:
        FilterChain():
            FilterNode<T>::FilterNode(nullptr)
        {
        }

        virtual ~FilterChain() noexcept
        {
            FilterNode<T>* iter_node = this;
            while (iter_node->next_node)
            {
                iter_node = iter_node->next_node.get();
            }
            while (iter_node->prev_node != nullptr)
            {
                iter_node = iter_node->prev_node;
                iter_node->next_node = nullptr;
            }
        }

        virtual bool match(const T& object) const
        {
            const FilterNode<T>* iter_node = this;
            // Empty chain always matches
            bool match_flag = !iter_node->next_node;
            while (!match_flag && iter_node->next_node)
            {
                iter_node = iter_node->next_node.get();
                match_flag = iter_node->match(object);
            }
            return match_flag;
        }

        virtual bool is_empty() const
        {
            return !this->next_node;
        }
    };

    template<class O, class M>
    class GenericFilterNode : public FilterNode<O>
    {
      public:
        GenericFilterNode(
            FilterNode<O>* const chain_end,
            std::function<bool(const O&, const M&)> match_function_ref,
            M& value
        ):
            FilterNode<O>::FilterNode(chain_end),
            match_function(match_function_ref),
            match_value(value)
        {
        }

        virtual ~GenericFilterNode() noexcept
        {
        }

        virtual bool match(const O& object) const override
        {
            return match_function(object, match_value);
        }

        std::function<bool(const O& object, const M& match_value)> match_function;
        M match_value;
    };

    template<class T>
    class ObjectNameFilterNode : public FilterNode<T>
    {
      public:
        // @throws std::bad_alloc, PatternLimitException
        ObjectNameFilterNode(FilterNode<T>* const chain_end, const std::string& name_pattern):
            FilterNode<T>::FilterNode(chain_end)
        {
            string_matching::process_pattern(name_pattern, pattern_list);
        }

        virtual ~ObjectNameFilterNode() noexcept
        {
        }

        virtual bool match(const T& object) const override
        {
            const std::string& object_name = object.get_name();
            return string_matching::match_text(object_name, pattern_list.get());
        }

      private:
        std::unique_ptr<string_matching::PatternItem> pattern_list;
    };

    template<class T>
    FilterNode<T>& make_object_degraded_selector(
        FilterNode<T>& chain_end,
        bool state
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<T>>(
            new GenericFilterNode<T, bool>(
                &chain_end,
                [](const T& object, const bool& state_ref)
                {
                    return state_ref ?
                        (object.has_alert_state() || object.has_warn_state() || object.has_mark_state()) :
                        (!object.has_alert_state() && !object.has_warn_state() && !object.has_mark_state());
                },
                state
            )
        );
        return *(chain_end.next_node);
    }

    // @throws std::bad_alloc, PatternLimitException
    template<class T>
    FilterNode<T>& make_object_name_selector(
        FilterNode<T>& chain_end,
        const std::string& name_pattern
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<T>>(
            new ObjectNameFilterNode<T>(&chain_end, name_pattern)
        );
        return *(chain_end.next_node);
    }

    class SelectionTargets
    {
      public:
        bool    select_resources        {false};
        bool    select_volumes          {false};
        bool    select_connections      {false};
        bool    select_peer_volumes     {false};
    };

    class FilterRestrictions
    {
      public:
        bool    selected_resources      {false};
        bool    selected_volumes        {false};
        bool    selected_connections    {false};
        bool    selected_peer_volumes   {false};
    };

    class MatchInversions
    {
      public:
        bool    resource_name           {false};
        bool    volume_number           {false};
        bool    connection_name         {false};
        bool    connection_state        {false};
        bool    volume_state            {false};
        bool    peer_volume_disk_state  {false};
        bool    peer_volume_repl_state  {false};
    };

    class FilterSettings
    {
      public:
        std::string                     rsc_name_pattern;
        std::string                     con_name_pattern;
        uint16_t                        vlm_number          {0};
        bool                            filter_vlm_number   {false};

        SelectionTargets                targets;
        FilterRestrictions              restrictions;
        MatchInversions                 invert;

        // Resource operational state (normal/degraded) chain
        FilterChain<DrbdResource>       rsc_op_chain;
        // Resource quorum state chain
        FilterChain<DrbdResource>       rsc_quorum_chain;
        // Resource role chain
        FilterChain<DrbdResource>       rsc_role_chain;

        // Volume operational state (normal/degraded) chain
        FilterChain<DrbdVolume>         vlm_op_chain;
        FilterChain<DrbdVolume>         vlm_quorum_chain;
        FilterChain<DrbdVolume>         vlm_state_chain;

        // Connection operational state (normal/degraded) chain
        FilterChain<DrbdConnection>     con_op_chain;
        FilterChain<DrbdConnection>     con_role_chain;
        FilterChain<DrbdConnection>     con_state_chain;

        // Peer volume operational state (normal/degraded) chain
        FilterChain<DrbdVolume>         peer_vlm_op_chain;
        FilterChain<DrbdVolume>         peer_vlm_quorum_chain;
        FilterChain<DrbdVolume>         peer_vlm_state_chain;
        FilterChain<DrbdVolume>         peer_vlm_repl_state_chain;
    };

    FilterNode<DrbdResource>& make_resource_quorum_selector(
        FilterNode<DrbdResource>& chain_end,
        bool state
    );

    FilterNode<DrbdResource>& make_resource_role_selector(
        FilterNode<DrbdResource>& chain_end,
        DrbdRole::resource_role role
    );

    FilterNode<DrbdVolume>& make_volume_quorum_selector(
        FilterNode<DrbdVolume>& chain_end,
        bool state
    );

    FilterNode<DrbdVolume>& make_volume_disk_state_selector(
        FilterNode<DrbdVolume>& chain_end,
        DrbdVolume::disk_state state
    );

    FilterNode<DrbdVolume>& make_volume_repl_state_selector(
        FilterNode<DrbdVolume>& chain_end,
        DrbdVolume::repl_state state
    );

    FilterNode<DrbdVolume>& make_volume_client_state_selector(
        FilterNode<DrbdVolume>& chain_end,
        bool state
    );

    FilterNode<DrbdConnection>& make_connection_role_selector(
        FilterNode<DrbdConnection>& chain_end,
        DrbdRole::resource_role role
    );

    FilterNode<DrbdConnection>& make_connection_state_selector(
        FilterNode<DrbdConnection>& chain_end,
        DrbdConnection::state state
    );

    FilterNode<DrbdConnection>& make_connection_sync_state_selector(
        FilterNode<DrbdConnection>& chain_end,
        DrbdConnection::sync_state_type state
    );

    void filter_select(
        FilterSettings&     settings,
        ResourcesMap&       rsc_map,
        SharedData&         dsp_shared,
        MessageLog&         debug_log
    );

    void filter_deselect(
        FilterSettings&     settings,
        ResourcesMap&       rsc_map,
        SharedData&         dsp_shared,
        MessageLog&         debug_log
    );

    DrbdResource* next_resource(
        ResourcesMap&                                               rsc_map,
        const std::unique_ptr<ResourcesMap::ValuesIterator>&        rsc_iter,
        const std::unique_ptr<ResourceSelectionMap::KeysIterator>&  slct_rsc_iter
    );

    DrbdVolume* next_volume(
        DrbdResource* const rsc,
        const std::unique_ptr<VolumesMap::ValuesIterator>&          vlm_iter,
        const std::unique_ptr<VolumeSelectionMap::KeysIterator>&    slct_vlm_iter
    );

    DrbdConnection* next_connection(
        DrbdResource* const rsc,
        const std::unique_ptr<ConnectionsMap::ValuesIterator>&          con_iter,
        const std::unique_ptr<ConnectionSelectionMap::KeysIterator>&    slct_con_iter
    );

    DrbdVolume* next_peer_volume(
        DrbdConnection* const con,
        const std::unique_ptr<VolumesMap::ValuesIterator>&          peer_vlm_iter,
        const std::unique_ptr<VolumeSelectionMap::KeysIterator>&    slct_peer_vlm_iter
    );

    void next_volume_for_deselect(
        DrbdResource* const rsc,
        const std::unique_ptr<VolumesMap::ValuesIterator>&          vlm_iter,
        const std::unique_ptr<VolumeSelectionMap::KeysIterator>&    slct_vlm_iter,
        const uint16_t*&                                            vlm_nr,
        DrbdVolume*&                                                vlm
    );

    void next_connection_for_deselect(
        DrbdResource* const rsc,
        const std::unique_ptr<ConnectionsMap::ValuesIterator>&          con_iter,
        const std::unique_ptr<ConnectionSelectionMap::KeysIterator>&    slct_con_iter,
        const std::string*&                                             con_name,
        DrbdConnection*&                                                con
    );

    void next_peer_volume_for_deselect(
        DrbdConnection* const con,
        const std::unique_ptr<VolumesMap::ValuesIterator>&          peer_vlm_iter,
        const std::unique_ptr<VolumeSelectionMap::KeysIterator>&    slct_peer_vlm_iter,
        const uint16_t*&                                            peer_vlm_nr,
        DrbdVolume*&                                                peer_vlm
    );
}

#endif /* SELECTION_FILTER_H */
