#ifndef RESYNC_PROGRESS_H
#define RESYNC_PROGRESS_H

#include <objects/DrbdResource.h>
#include <objects/DrbdVolume.h>
#include <objects/DrbdConnection.h>

namespace resync_progress
{
    uint16_t percentage_for_volume(DrbdResource& rsc, const uint16_t vlm_nr);
}

#endif // RESYNC_PROGRESS_H
