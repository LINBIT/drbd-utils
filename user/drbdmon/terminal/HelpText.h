#ifndef HELPTEXT_H
#define HELPTEXT_H

#include <terminal/ComponentsHub.h>

namespace helptext
{
    extern const size_t DFLT_HELP_TEXT_SPACE;

    enum class id_type : uint16_t
    {
        RSC_LIST    = 0,
        RSC_DETAIL,
        RSC_ACTIONS,
        VLM_LIST,
        VLM_DETAIL,
        VLM_ACTIONS,
        CON_LIST,
        CON_DETAIL,
        CON_ACTIONS,
        PVLM_LIST,
        MAIN_MENU,
        MESSAGE_LOG,
        MESSAGE_DETAIL,
        TASK_QUEUE,
        TASK_DETAIL,
        DRBDMON_COMMANDS,
        DRBD_COMMANDS,
        GENERAL_HELP,
        CONF_HELP
    };

    extern const char* const RSC_LIST_HELP_1;
    extern const char* const RSC_LIST_HELP_2;
    extern const char* const RSC_LIST_HELP_3;
    extern const char* const RSC_LINE_HELP;
    extern const char* const VLM_LIST_HELP_1;
    extern const char* const VLM_LIST_HELP_2;
    extern const char* const VLM_LINE_HELP;
    extern const char* const PEER_VLM_LIST_HELP_1;
    extern const char* const PEER_VLM_LIST_HELP_2;
    extern const char* const PEER_VLM_LIST_HELP_3;
    extern const char* const PEER_VLM_LIST_HELP_4;
    extern const char* const CON_LIST_HELP_1;
    extern const char* const CON_LIST_HELP_2;
    extern const char* const CON_LIST_HELP_3;
    extern const char* const CON_LINE_HELP;
    extern const char* const TASKQ_HELP_1;
    extern const char* const TASKQ_HELP_2;
    extern const char* const TASK_DETAIL_HELP_1;
    extern const char* const TASK_DETAIL_HELP_2;
    extern const char* const RSC_ACT_HELP_1;
    extern const char* const VLM_ACT_HELP_1;
    extern const char* const CON_ACT_HELP_1;
    extern const char* const RSC_DETAIL_HELP_1;
    extern const char* const VLM_DETAIL_HELP_1;
    extern const char* const CON_DETAIL_HELP_1;
    extern const char* const MSG_LOG_HELP_1;
    extern const char* const MSG_LOG_HELP_2;
    extern const char* const MSG_DETAIL_HELP_1;
    extern const char* const GENERAL_HELP_1;
    extern const char* const CONF_HELP_1;
    extern const char* const DRBDMON_CMD_HELP_1;
    extern const char* const DRBD_CMD_HELP_1;
    extern const char* const MAIN_MENU_HELP_1;

    extern const char* const INSERT_NAV_HELP_1;
    extern const char* const INSERT_NAV_LIST_HELP_1;
    extern const char* const INSERT_RSC_CMD_HELP_1;
    extern const char* const INSERT_VLM_CMD_HELP_1;
    extern const char* const INSERT_CON_CMD_HELP_1;

    extern void open_help_page(const id_type help_id, const ComponentsHub& dsp_comp_hub);
}

#endif /* HELPTEXT_H */
