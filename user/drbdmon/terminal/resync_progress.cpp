#include <terminal/resync_progress.h>

namespace resync_progress
{
    uint16_t percentage_for_volume(DrbdResource& rsc, const uint16_t vlm_nr)
    {
        uint16_t sync_perc = 10000;
        DrbdResource::ConnectionsIterator con_iter = rsc.connections_iterator();
        while (con_iter.has_next())
        {
            DrbdConnection* const con = con_iter.next();
            DrbdVolume* const peer_vlm = con->get_volume(vlm_nr);
            if (peer_vlm != nullptr)
            {
                const DrbdVolume::repl_state state = peer_vlm->get_replication_state();
                if (state == DrbdVolume::repl_state::SYNC_TARGET)
                {
                    sync_perc = peer_vlm->get_sync_perc();
                    break;
                }
            }
        }
        return sync_perc;
    }
}
