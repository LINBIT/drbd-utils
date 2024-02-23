#ifndef DISPLAYID_H
#define DISPLAYID_H

#include <default_types.h>
#include <string>

class DisplayId
{
  public:
    enum class display_page : uint16_t
    {
        RSC_LIST        = 0,
        VLM_LIST,
        CON_LIST,
        PEER_VLM_LIST,
        RSC_DETAIL,
        VLM_DETAIL,
        CON_DETAIL,
        PEER_VLM_DETAIL,
        MAIN_MENU,
        TASKQ_ACT,
        TASKQ_SSP,
        TASKQ_PND,
        TASKQ_FIN,
        HELP_IDX,
        HELP_VIEWER,
        SYNC_LIST,
        LOG_VIEWER,
        DEBUG_LOG_VIEWER,
        MSG_VIEWER,
        DEBUG_MSG_VIEWER,
        RSC_ACTIONS,
        VLM_ACTIONS,
        CON_ACTIONS,
        PEER_VLM_ACTIONS,
        PGM_INFO,
        TASK_DETAIL,
        CONFIGURATION
    };

    static const std::string    MDSP_RSC_LIST;
    static const std::string    MDSP_RSC_DETAIL;
    static const std::string    MDSP_VLM_LIST;
    static const std::string    MDSP_VLM_DETAIL;
    static const std::string    MDSP_CON_LIST;
    static const std::string    MDSP_CON_DETAIL;
    static const std::string    MDSP_PEER_VLM_LIST;
    static const std::string    MDSP_PEER_VLM_DETAIL;
    static const std::string    MDSP_MAIN_MENU;
    static const std::string    MDSP_TASKQ_ACT;
    static const std::string    MDSP_TASKQ_SSP;
    static const std::string    MDSP_TASKQ_PND;
    static const std::string    MDSP_TASKQ_FIN;
    static const std::string    MDSP_HELP_IDX;
    static const std::string    MDSP_SYNC_LIST;
    static const std::string    MDSP_LOG_VIEW;
    static const std::string    MDSP_DEBUG_LOG_VIEW;
    static const std::string    MDSP_MSG_VIEW;
    static const std::string    MDSP_DEBUG_MSG_VIEW;
    static const std::string    MDSP_RSC_ACT;
    static const std::string    MDSP_VLM_ACT;
    static const std::string    MDSP_CON_ACT;
    static const std::string    MDSP_PEER_VLM_ACT;
    static const std::string    MDSP_PGM_INFO;
    static const std::string    MDSP_TASK_DETAIL;
    static const std::string    MDSP_CONFIGURATION;

    const std::string* const    name;
    const display_page          page_id;

    DisplayId(const std::string* const name_ptr, DisplayId::display_page id);
    virtual ~DisplayId() noexcept;
};

#endif /* DISPLAYID_H */
