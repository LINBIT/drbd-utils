#ifndef DISPLAYIDMAP_H
#define DISPLAYIDMAP_H

#include <default_types.h>
#include <cppdsaext/src/QTree.h>
#include <terminal/DisplayId.h>

using DisplayMap = QTree<std::string, DisplayId>;

namespace dspid
{
    extern const DisplayId ID_RSC_LIST;
    extern const DisplayId ID_RSC_DETAIL;
    extern const DisplayId ID_VLM_LIST;
    extern const DisplayId ID_VLM_DETAIL;
    extern const DisplayId ID_CON_LIST;
    extern const DisplayId ID_CON_DETAIL;
    extern const DisplayId ID_PEER_VLM_LIST;
    extern const DisplayId ID_PEER_VLM_DETAIL;
    extern const DisplayId ID_MAIN_MENU;
    extern const DisplayId ID_TASKQ_ACT;
    extern const DisplayId ID_TASKQ_SSP;
    extern const DisplayId ID_TASKQ_PND;
    extern const DisplayId ID_TASKQ_FIN;
    extern const DisplayId ID_SYNC_LIST;
    extern const DisplayId ID_HELP_IDX;
    extern const DisplayId ID_LOG_VIEW;
    extern const DisplayId ID_DEBUG_LOG_VIEW;
    extern const DisplayId ID_MSG_VIEW;
    extern const DisplayId ID_DEBUG_MSG_VIEW;
    extern const DisplayId ID_RSC_ACT;
    extern const DisplayId ID_VLM_ACT;
    extern const DisplayId ID_CON_ACT;
    extern const DisplayId ID_PEER_VLM_ACT;
    extern const DisplayId ID_TASK_DETAIL;
    extern const DisplayId ID_CONFIGURATION;

    // @throws std::bad_alloc
    void initialize_display_ids(DisplayMap& map);
}

#endif /* DISPLAYIDCOLLECTION_H */
