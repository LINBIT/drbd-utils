#ifndef MDSPOVERVIEW_H
#define MDSPOVERVIEW_H

#include <default_types.h>
#include <terminal/MDspMenuBase.h>
#include <terminal/ComponentsHub.h>
#include <terminal/MouseEvent.h>
#include <objects/DrbdResource.h>
#include <objects/DrbdVolume.h>
#include <objects/DrbdConnection.h>
#include <comparators.h>
#include <map_types.h>
#include <string>
#include <memory>
#include <functional>

class MDspOverview : public MDspMenuBase
{
  private:
    using RscRoleMap = QTree<const DrbdRole::resource_role, uint32_t>;
    using VlmDiskStateMap = QTree<const DrbdVolume::disk_state, uint32_t>;
    using VlmReplStateMap = QTree<const DrbdVolume::repl_state, uint32_t>;
    using ConStateMap = QTree<const DrbdConnection::state, uint32_t>;
    using ConSyncStateMap = QTree<const DrbdConnection::sync_state_type, uint32_t>;
    using QuorumMap = QTree<const bool, uint32_t>;

  public:
    MDspOverview(const ComponentsHub& comp_hub);
    virtual ~MDspOverview() noexcept;

    virtual void display_activated() override;
    virtual void display_content() override;
    virtual bool key_pressed(const uint32_t key) override;
    virtual bool mouse_action(MouseEvent& mouse) override;

    virtual uint64_t get_update_mask() noexcept override;
    virtual void display_closed() override;

    virtual void synchronize_data() override;
    virtual void cursor_to_next_item() override;
    virtual void cursor_to_previous_item() override;
    virtual void text_cursor_ops() override;

  private:
    class ClusterStats
    {
      public:
        ClusterStats();
        virtual ~ClusterStats() noexcept;
        ClusterStats(const ClusterStats& other) = default;
        ClusterStats(ClusterStats&& orig) = default;
        ClusterStats& operator=(const ClusterStats& other) = default;
        ClusterStats& operator=(ClusterStats&& orig) = default;

        uint32_t    rsc_count               {0};
        uint32_t    vlm_disk_count          {0};
        uint32_t    vlm_client_count        {0};
        uint32_t    con_count               {0};
        uint32_t    peer_vlm_client_count   {0};
    };

    // Total number of each state
    std::unique_ptr<RscRoleMap>         rsc_roles;
    std::unique_ptr<VlmDiskStateMap>    vlm_disk_states;
    std::unique_ptr<VlmReplStateMap>    vlm_repl_states;
    std::unique_ptr<ConStateMap>        con_states;
    std::unique_ptr<ConSyncStateMap>    con_sync_states;
    std::unique_ptr<VlmDiskStateMap>    peer_vlm_disk_states;
    std::unique_ptr<VlmReplStateMap>    peer_vlm_repl_states;
    std::unique_ptr<QuorumMap>          vlm_quorum_map;

    // Number of resources where each state occurs
    std::unique_ptr<VlmDiskStateMap>    vlm_disk_states_per_rsc;
    std::unique_ptr<VlmReplStateMap>    vlm_repl_states_per_rsc;
    std::unique_ptr<ConStateMap>        con_states_per_rsc;
    std::unique_ptr<ConSyncStateMap>    con_sync_states_per_rsc;
    std::unique_ptr<VlmDiskStateMap>    peer_vlm_disk_states_per_rsc;
    std::unique_ptr<VlmReplStateMap>    peer_vlm_repl_states_per_rsc;
    std::unique_ptr<QuorumMap>          rsc_quorum_map;

    bool have_maps {false};

    std::function<void(const DrbdVolume::disk_state, const uint32_t, uint16_t&)> fn_dsp_vlm_disk;
    std::function<void(const DrbdConnection::state, const uint32_t, uint16_t&)> fn_dsp_con_state;
    std::function<void(const DrbdConnection::sync_state_type, const uint32_t, uint16_t&)> fn_dsp_con_sync_state;
    std::function<void(const DrbdVolume::disk_state, const uint32_t, uint16_t&)> fn_dsp_peer_vlm_disk;
    std::function<void(const DrbdVolume::repl_state, const uint32_t, uint16_t&)> fn_dsp_peer_vlm_repl;

    ClusterStats statistics;

    uint32_t    rsc_primary_count       {0};
    uint32_t    rsc_no_primary_count    {0};

    std::function<void()>               cmd_fn_refresh_analysis;
    std::unique_ptr<ClickableCommand>   cmd_refresh_analysis;

    void analyze_drbd_state();

    // Format and display a counter in the range [0, 9999999] with tousands-separators and right-aligned
    // (field width 9 characters, e.g. "1,000,000" or "   10,000")
    void dsp_write_counter(const uint32_t counter);

    // @throws std::bad_alloc
    void allocate_all_maps();
    void deallocate_all_maps() noexcept;

    void init_all_map_counters();
    void init_vlm_disk_state_counters(VlmDiskStateMap& map);
    void init_vlm_repl_state_counters(VlmReplStateMap& map);
    void init_con_state_counters(ConStateMap& map);
    void init_con_sync_state_counters(ConSyncStateMap& map);
    void reset_all_map_counters();

    static int bool_comparator(const bool* const value, const bool* const other) noexcept;

    template<template<typename, typename> class M, typename K, typename V>
    void allocate_map_counter(M<const K, V>& map, K key)
    {
        std::unique_ptr<K> map_key(new K);
        K* const map_key_ptr = map_key.get();
        *map_key_ptr = key;

        std::unique_ptr<uint32_t> counter(new uint32_t);
        uint32_t* const counter_ptr = counter.get();
        *counter_ptr = 0;

        map.insert(map_key_ptr, counter_ptr);
        map_key.release();
        counter.release();
    }

    template<typename T>
    void deallocate_map(std::unique_ptr<T>& map_ptr)
    {
        if (map_ptr != nullptr)
        {
            deallocate_map_counters(*map_ptr);
            map_ptr = nullptr;
        }
    }

    template<template<typename, typename> class M, typename K, typename V>
    void deallocate_map_counters(M<K, V>& map)
    {
        typename M<K, V>::NodesIterator map_iter(map);
        while (map_iter.has_next())
        {
            typename M<K, V>::Node* const map_node = map_iter.next();
            K* const map_key = map_node->get_key();
            V* const counter = map_node->get_value();
            delete map_key;
            delete counter;
        }
    }

    template<template<typename, typename> class M, typename K>
    void reset_map_counters(M<K, uint32_t>& map)
    {
        typename M<K, uint32_t>::ValuesIterator map_iter(map);
        while (map_iter.has_next())
        {
            uint32_t* const counter = map_iter.next();
            *counter = 0;
        }
    }

    template<template<typename, typename> class M, typename K>
    void increase_counter(M<const K, uint32_t>& map, const K& key)
    {
        uint32_t* const counter = map.get(&key);
        if (counter != nullptr)
        {
            ++(*counter);
        }
    }

    template<template<typename, typename> class M, typename K>
    const uint32_t get_count(M<const K, uint32_t>& map, const K& key)
    {
        const uint32_t* const counter_ptr = map.get(&key);
        const uint32_t counter = counter_ptr == nullptr ? 0 : *counter_ptr;
        return counter;
    }

    template<template<typename, typename> class M, typename K>
    void iterate_map_entries(
        M<const K, uint32_t>&   map,
        uint16_t&               dsp_line,
        const uint16_t          max_line,
        std::function<void(const K, const uint32_t, uint16_t&)> display_fn
    )
    {
        typename M<const K, uint32_t>::NodesIterator entries_iter(map);
        while (entries_iter.has_next() && dsp_line <= max_line)
        {
            typename M<const K, uint32_t>::Node* const entry_node = entries_iter.next();
            const K state = *(entry_node->get_key());
            const uint32_t counter = *(entry_node->get_value());
            display_fn(state, counter, dsp_line);
        }
    }
};

#endif // MDSPOVERVIEW_H
