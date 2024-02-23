#include <terminal/DisplayId.h>
#include <terminal/DisplayIdMap.h>

#include <string>

namespace dspid
{
    const DisplayId ID_RSC_LIST(&DisplayId::MDSP_RSC_LIST, DisplayId::display_page::RSC_LIST);
    const DisplayId ID_RSC_DETAIL(&DisplayId::MDSP_RSC_DETAIL, DisplayId::display_page::RSC_DETAIL);
    const DisplayId ID_VLM_LIST(&DisplayId::MDSP_VLM_LIST, DisplayId::display_page::VLM_LIST);
    const DisplayId ID_VLM_DETAIL(&DisplayId::MDSP_VLM_DETAIL, DisplayId::display_page::VLM_DETAIL);
    const DisplayId ID_CON_LIST(&DisplayId::MDSP_CON_LIST, DisplayId::display_page::CON_LIST);
    const DisplayId ID_CON_DETAIL(&DisplayId::MDSP_CON_DETAIL, DisplayId::display_page::CON_DETAIL);
    const DisplayId ID_PEER_VLM_LIST(&DisplayId::MDSP_PEER_VLM_LIST, DisplayId::display_page::PEER_VLM_LIST);
    const DisplayId ID_PEER_VLM_DETAIL(&DisplayId::MDSP_PEER_VLM_DETAIL, DisplayId::display_page::PEER_VLM_DETAIL);
    const DisplayId ID_MAIN_MENU(&DisplayId::MDSP_MAIN_MENU, DisplayId::display_page::MAIN_MENU);
    const DisplayId ID_TASKQ_ACT(&DisplayId::MDSP_TASKQ_ACT, DisplayId::display_page::TASKQ_ACT);
    const DisplayId ID_TASKQ_SSP(&DisplayId::MDSP_TASKQ_SSP, DisplayId::display_page::TASKQ_SSP);
    const DisplayId ID_TASKQ_PND(&DisplayId::MDSP_TASKQ_PND, DisplayId::display_page::TASKQ_PND);
    const DisplayId ID_TASKQ_FIN(&DisplayId::MDSP_TASKQ_FIN, DisplayId::display_page::TASKQ_FIN);
    const DisplayId ID_HELP_IDX(&DisplayId::MDSP_HELP_IDX, DisplayId::display_page::HELP_IDX);
    const DisplayId ID_SYNC_LIST(&DisplayId::MDSP_SYNC_LIST, DisplayId::display_page::SYNC_LIST);
    const DisplayId ID_LOG_VIEW(&DisplayId::MDSP_LOG_VIEW, DisplayId::display_page::LOG_VIEWER);
    const DisplayId ID_DEBUG_LOG_VIEW(&DisplayId::MDSP_DEBUG_LOG_VIEW, DisplayId::display_page::DEBUG_LOG_VIEWER);
    const DisplayId ID_MSG_VIEW(&DisplayId::MDSP_MSG_VIEW, DisplayId::display_page::MSG_VIEWER);
    const DisplayId ID_DEBUG_MSG_VIEW(&DisplayId::MDSP_DEBUG_MSG_VIEW, DisplayId::display_page::DEBUG_MSG_VIEWER);
    const DisplayId ID_RSC_ACT(&DisplayId::MDSP_RSC_ACT, DisplayId::display_page::RSC_ACTIONS);
    const DisplayId ID_VLM_ACT(&DisplayId::MDSP_VLM_ACT, DisplayId::display_page::VLM_ACTIONS);
    const DisplayId ID_CON_ACT(&DisplayId::MDSP_CON_ACT, DisplayId::display_page::CON_ACTIONS);
    const DisplayId ID_PEER_VLM_ACT(&DisplayId::MDSP_PEER_VLM_ACT, DisplayId::display_page::PEER_VLM_ACTIONS);
    const DisplayId ID_PGM_INFO(&DisplayId::MDSP_PGM_INFO, DisplayId::display_page::PGM_INFO);
    const DisplayId ID_TASK_DETAIL(&DisplayId::MDSP_TASK_DETAIL, DisplayId::display_page::TASK_DETAIL);
    const DisplayId ID_CONFIGURATION(&DisplayId::MDSP_CONFIGURATION, DisplayId::display_page::CONFIGURATION);

    // @throws std::bad_alloc
    void initialize_display_ids(DisplayMap& map)
    {
        try
        {
            map.insert(&DisplayId::MDSP_RSC_LIST,           &ID_RSC_LIST);
            map.insert(&DisplayId::MDSP_RSC_DETAIL,         &ID_RSC_DETAIL);
            map.insert(&DisplayId::MDSP_VLM_LIST,           &ID_VLM_LIST);
            map.insert(&DisplayId::MDSP_VLM_DETAIL,         &ID_VLM_DETAIL);
            map.insert(&DisplayId::MDSP_CON_LIST,           &ID_CON_LIST);
            map.insert(&DisplayId::MDSP_CON_DETAIL,         &ID_CON_DETAIL);
            map.insert(&DisplayId::MDSP_PEER_VLM_LIST,      &ID_PEER_VLM_LIST);
            map.insert(&DisplayId::MDSP_PEER_VLM_DETAIL,    &ID_PEER_VLM_DETAIL);
            map.insert(&DisplayId::MDSP_MAIN_MENU,          &ID_MAIN_MENU);
            map.insert(&DisplayId::MDSP_TASKQ_ACT,          &ID_TASKQ_ACT);
            map.insert(&DisplayId::MDSP_TASKQ_SSP,          &ID_TASKQ_SSP);
            map.insert(&DisplayId::MDSP_TASKQ_PND,          &ID_TASKQ_PND);
            map.insert(&DisplayId::MDSP_TASKQ_FIN,          &ID_TASKQ_FIN);
            map.insert(&DisplayId::MDSP_HELP_IDX,           &ID_HELP_IDX);
            map.insert(&DisplayId::MDSP_SYNC_LIST,          &ID_SYNC_LIST);
            map.insert(&DisplayId::MDSP_LOG_VIEW,           &ID_LOG_VIEW);
            map.insert(&DisplayId::MDSP_DEBUG_LOG_VIEW,     &ID_DEBUG_LOG_VIEW);
            map.insert(&DisplayId::MDSP_MSG_VIEW,           &ID_MSG_VIEW);
            map.insert(&DisplayId::MDSP_DEBUG_MSG_VIEW,     &ID_DEBUG_MSG_VIEW);
            map.insert(&DisplayId::MDSP_RSC_ACT,            &ID_RSC_ACT);
            map.insert(&DisplayId::MDSP_VLM_ACT,            &ID_VLM_ACT);
            map.insert(&DisplayId::MDSP_CON_ACT,            &ID_CON_ACT);
            map.insert(&DisplayId::MDSP_PEER_VLM_ACT,       &ID_PEER_VLM_ACT);
            map.insert(&DisplayId::MDSP_PGM_INFO,           &ID_PGM_INFO);
            map.insert(&DisplayId::MDSP_TASK_DETAIL,        &ID_TASK_DETAIL);
            map.insert(&DisplayId::MDSP_CONFIGURATION,      &ID_CONFIGURATION);
        }
        catch (dsaext::DuplicateInsertException&)
        {
            throw std::logic_error("Implementation error: Duplicate display ID insertion into a map");
        }
    }
}
