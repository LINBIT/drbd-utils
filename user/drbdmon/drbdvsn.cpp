#include <drbdvsn.h>
#include <string>
#include <iostream>
#include <fstream>
#include <ios>
#include <new>
#include <memory>
#include <dsaext.h>
#include <integerparse.h>

static const std::string PROC_DRBD_PATH("/proc/drbd");
static const std::string DRBD_VSN_PREFIX("version: ");
static constexpr const char DOT = '.';
static constexpr const size_t BUFFER_SIZE = 256;

static DrbdVersion identify_version(uint16_t major_version, uint16_t minor_version);

// @throws std::bad_alloc
DrbdVersion probe_drbd_version()
{
    DrbdVersion version = DrbdVersion::UNDETERMINED;
    std::ifstream kmod_info(PROC_DRBD_PATH.c_str(), std::ios::in);
    if (kmod_info.good())
    {
        std::unique_ptr<char[]> buffer_mgr(new char[BUFFER_SIZE]);
        char* const buffer = buffer_mgr.get();
        buffer[0] = '\0';

        kmod_info.getline(buffer, BUFFER_SIZE);
        if (!kmod_info.fail())
        {
            version = DrbdVersion::UNRECOGNIZED;
            std::string version_line(buffer);
            if (version_line.find(DRBD_VSN_PREFIX) != std::string::npos)
            {
                version_line = version_line.substr(DRBD_VSN_PREFIX.length());
                size_t dot_pos = version_line.find(DOT);
                if (dot_pos != std::string::npos)
                {
                    std::string major_version_str(version_line.substr(0, dot_pos));
                    std::string minor_version_str(version_line.substr(dot_pos + 1));
                    dot_pos = minor_version_str.find(DOT);
                    if (dot_pos != std::string::npos)
                    {
                        minor_version_str = minor_version_str.substr(0, dot_pos);
                    }
                    try
                    {
                        const uint16_t major_version = dsaext::parse_unsigned_int16(major_version_str);
                        const uint16_t minor_version = dsaext::parse_unsigned_int16(minor_version_str);

                        version = identify_version(major_version, minor_version);
                    }
                    catch (dsaext::NumberFormatException&)
                    {
                        // Unparseable output -> Unrecognized DRBD version
                    }
                }
            }
        }
    }
    return version;
}

static DrbdVersion identify_version(const uint16_t major_version, const uint16_t minor_version)
{
    DrbdVersion version = DrbdVersion::UNRECOGNIZED;
    if (major_version == 9 && minor_version == 0)
    {
        version = DrbdVersion::DRBD_9_0;
    }
    else
    if (major_version == 8 && minor_version == 4)
    {
        version = DrbdVersion::DRBD_8_4;
    }
    return version;
}
