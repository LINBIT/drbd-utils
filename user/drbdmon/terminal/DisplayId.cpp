#include <terminal/DisplayId.h>

const std::string   DisplayId::MDSP_RSC_LIST        = {"RSCL"};
const std::string   DisplayId::MDSP_RSC_DETAIL      = {"RSC"};
const std::string   DisplayId::MDSP_VLM_LIST        = {"VLML"};
const std::string   DisplayId::MDSP_VLM_DETAIL      = {"VLM"};
const std::string   DisplayId::MDSP_CON_LIST        = {"CONL"};
const std::string   DisplayId::MDSP_CON_DETAIL      = {"CON"};
const std::string   DisplayId::MDSP_PEER_VLM_LIST   = {"PVLML"};
const std::string   DisplayId::MDSP_PEER_VLM_DETAIL = {"PVLM"};
const std::string   DisplayId::MDSP_MAIN_MENU       = {"MAIN"};
const std::string   DisplayId::MDSP_TASKQ_ACT       = {"ACTQ"};
const std::string   DisplayId::MDSP_TASKQ_SSP       = {"SSPQ"};
const std::string   DisplayId::MDSP_TASKQ_PND       = {"PNDQ"};
const std::string   DisplayId::MDSP_TASKQ_FIN       = {"FINQ"};
const std::string   DisplayId::MDSP_SYNC_LIST       = {"SYNC"};
const std::string   DisplayId::MDSP_HELP_IDX        = {"HELP"};
const std::string   DisplayId::MDSP_LOG_VIEW        = {"LOG"};
const std::string   DisplayId::MDSP_DEBUG_LOG_VIEW  = {"DBGLOG"};
const std::string   DisplayId::MDSP_MSG_VIEW        = {"MSG"};
const std::string   DisplayId::MDSP_DEBUG_MSG_VIEW  = {"DBGMSG"};
const std::string   DisplayId::MDSP_RSC_ACT         = {"RSCA"};
const std::string   DisplayId::MDSP_VLM_ACT         = {"VLMA"};
const std::string   DisplayId::MDSP_CON_ACT         = {"CONA"};
const std::string   DisplayId::MDSP_PEER_VLM_ACT    = {"PVLMA"};
const std::string   DisplayId::MDSP_PGM_INFO        = {"INFO"};
const std::string   DisplayId::MDSP_TASK_DETAIL     = {"TASK"};
const std::string   DisplayId::MDSP_CONFIGURATION   = {"CONF"};

DisplayId::DisplayId(const std::string* const name_ptr, const display_page id):
    name(name_ptr),
    page_id(id)
{
}

DisplayId::~DisplayId() noexcept
{
}
