#ifndef ENVIRONMENTVARIABLES_H
#define ENVIRONMENTVARIABLES_H

#include <string>

namespace env_var
{
    extern const std::string drbd_resource;
    extern const std::string drbd_resource_role;
    extern const std::string drbd_resource_quorum;

    extern const std::string drbd_volume_nr;
    extern const std::string drbd_volume_count;
    extern const std::string drbd_volume_is_client;
    extern const std::string drbd_volume_quorum;
    extern const std::string drbd_volume_minor_nr;
    extern const std::string drbd_volume_disk_state;
    extern const std::string drbd_volume_repl_state;
    extern const std::string drbd_volume_sync_perc;

    extern const std::string drbd_connection;
    extern const std::string drbd_connection_count;
    extern const std::string drbd_connection_role;
    extern const std::string drbd_connection_state;
    extern const std::string drbd_connection_sync_state;
    extern const std::string drbd_peer_volume_count;
}

#endif /* ENVIRONMENTVARIABLES_H */
