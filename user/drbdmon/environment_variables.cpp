#include <environment_variables.h>

namespace env_var
{
    const std::string drbd_resource("drbd_resource");
    const std::string drbd_resource_role("drbd_resource_role");
    const std::string drbd_resource_quorum("drbd_resource_quorum");

    const std::string drbd_volume_nr("drbd_volume_nr");
    const std::string drbd_volume_count("drbd_volume_count");
    const std::string drbd_volume_is_client("drbd_volume_is_client");
    const std::string drbd_volume_quorum("drbd_volume_quorum");
    const std::string drbd_volume_minor_nr("drbd_volume_minor_nr");
    const std::string drbd_volume_disk_state("drbd_volume_disk_state");
    const std::string drbd_volume_repl_state("drbd_volume_repl_state");
    const std::string drbd_volume_sync_perc("drbd_volume_sync_perc");

    const std::string drbd_connection("drbd_connection");
    const std::string drbd_connection_count("drbd_connection_count");
    const std::string drbd_connection_role("drbd_connection_role");
    const std::string drbd_connection_state("drbd_connection_state");
    const std::string drbd_connection_sync_state("drbd_connection_sync_state");
    const std::string drbd_peer_volume_count("drbd_peer_volume_count");
}
