#include <terminal/selection_filter.h>

namespace selection_filter
{
    FilterNode<DrbdResource>& make_resource_quorum_selector(
        FilterNode<DrbdResource>& chain_end,
        bool quorum_flag
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdResource>>(
            new GenericFilterNode<DrbdResource, bool>(
                &chain_end,
                [](const DrbdResource& resource, const bool& state_ref)
                {
                    return resource.has_quorum_alert() != state_ref;
                },
                quorum_flag
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdResource>& make_resource_role_selector(
        FilterNode<DrbdResource>& chain_end,
        DrbdRole::resource_role role
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdResource>>(
            new GenericFilterNode<DrbdResource, DrbdRole::resource_role>(
                &chain_end,
                [](const DrbdResource& resource, const DrbdRole::resource_role& role_ref)
                {
                    return resource.get_role() == role_ref;
                },
                role
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdVolume>& make_volume_quorum_selector(
        FilterNode<DrbdVolume>& chain_end,
        bool quorum_flag
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdVolume>>(
            new GenericFilterNode<DrbdVolume, bool>(
                &chain_end,
                [](const DrbdVolume& volume, const bool& state_ref)
                {
                    return volume.has_quorum_alert() != state_ref;
                },
                quorum_flag
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdVolume>& make_volume_disk_state_selector(
        FilterNode<DrbdVolume>& chain_end,
        DrbdVolume::disk_state state
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdVolume>>(
            new GenericFilterNode<DrbdVolume, DrbdVolume::disk_state>(
                &chain_end,
                [](const DrbdVolume& volume, const DrbdVolume::disk_state& state_ref)
                {
                    return volume.get_disk_state() == state_ref;
                },
                state
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdVolume>& make_volume_repl_state_selector(
        FilterNode<DrbdVolume>& chain_end,
        DrbdVolume::repl_state state
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdVolume>>(
            new GenericFilterNode<DrbdVolume, DrbdVolume::repl_state>(
                &chain_end,
                [](const DrbdVolume& volume, const DrbdVolume::repl_state& state_ref)
                {
                    return volume.get_replication_state() == state_ref;
                },
                state
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdVolume>& make_volume_client_state_selector(
        FilterNode<DrbdVolume>& chain_end,
        bool state
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdVolume>>(
            new GenericFilterNode<DrbdVolume, bool>(
                &chain_end,
                [](const DrbdVolume& volume, const bool& state_ref)
                {
                    return state_ref ?
                        (volume.get_disk_state() == DrbdVolume::disk_state::DISKLESS &&
                         volume.get_client_state() == DrbdVolume::client_state::ENABLED) :
                        (volume.get_disk_state() == DrbdVolume::disk_state::DISKLESS &&
                         volume.get_client_state() != DrbdVolume::client_state::ENABLED);
                },
                state
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdConnection>& make_connection_role_selector(
        FilterNode<DrbdConnection>& chain_end,
        DrbdRole::resource_role role
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdConnection>>(
            new GenericFilterNode<DrbdConnection, DrbdRole::resource_role>(
                &chain_end,
                [](const DrbdConnection& connection, const DrbdRole::resource_role& role_ref)
                {
                    return connection.get_role() == role_ref;
                },
                role
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdConnection>& make_connection_state_selector(
        FilterNode<DrbdConnection>& chain_end,
        DrbdConnection::state state
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdConnection>>(
            new GenericFilterNode<DrbdConnection, DrbdConnection::state>(
                &chain_end,
                [](const DrbdConnection& connection, const DrbdConnection::state& state_ref)
                {
                    return connection.get_connection_state() == state_ref;
                },
                state
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdConnection>& make_connection_sync_state_selector(
        FilterNode<DrbdConnection>& chain_end,
        DrbdConnection::sync_state_type state
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdConnection>>(
            new GenericFilterNode<DrbdConnection, DrbdConnection::sync_state_type>(
                &chain_end,
                [](const DrbdConnection& connection, const DrbdConnection::sync_state_type& state_ref)
                {
                    return connection.get_sync_state() == state_ref;
                },
                state
            )
        );
        return *(chain_end.next_node);
    }
}
