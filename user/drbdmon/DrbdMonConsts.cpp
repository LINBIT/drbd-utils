#include <DrbdMonConsts.h>

extern "C"
{
    #include <config.h>
    #include <drbd_buildtag.h>
}

const std::string DrbdMonConsts::PROGRAM_NAME       = "DRBDmon";
const std::string DrbdMonConsts::PROJECT_VERSION    = "V1R2M1";
const std::string DrbdMonConsts::UTILS_VERSION      = PACKAGE_VERSION;
const std::string DrbdMonConsts::BUILD_HASH         = GITHASH;
