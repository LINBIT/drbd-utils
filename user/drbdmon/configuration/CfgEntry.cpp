#include <configuration/CfgEntry.h>
#include <platform/IoException.h>

const size_t    CfgEntry::MAX_KEY_LENGTH    = 40;

const uint32_t  CfgEntry::TYPE_CFG_VERSION  = 0x80000000;
const uint32_t  CfgEntry::TYPE_BOOLEAN      = 0x0;
const uint32_t  CfgEntry::TYPE_UNSGINT8     = 0x1;
const uint32_t  CfgEntry::TYPE_UNSGINT16    = 0x2;
const uint32_t  CfgEntry::TYPE_UNSGINT32    = 0x3;
const uint32_t  CfgEntry::TYPE_UNSGINT64    = 0x4;
const uint32_t  CfgEntry::TYPE_INT8         = 0x5;
const uint32_t  CfgEntry::TYPE_INT16        = 0x6;
const uint32_t  CfgEntry::TYPE_INT32        = 0x7;
const uint32_t  CfgEntry::TYPE_INT64        = 0x8;
const uint32_t  CfgEntry::TYPE_BINARY       = 0x9;

CfgEntry::CfgEntry()
{
}

CfgEntry::CfgEntry(const std::string& entry_key):
    key(entry_key)
{
}

CfgEntry::~CfgEntry() noexcept
{
}

const std::string& CfgEntry::get_key() const
{
    return key;
}

void CfgEntry::serialize_key(std::ostream& data_out) const
{
    const size_t key_length = key.length();
    if (key_length < 1 || key_length > MAX_KEY_LENGTH)
    {
        throw IoException();
    }

    const char char_out = static_cast<char> (key_length);

    data_out.write(&char_out, 1);
    data_out.write(key.c_str(), key_length);
    if (!data_out.good())
    {
        throw IoException();
    }
}

void CfgEntry::deserialize_key(std::istream& data_in)
{
    char char_in = 0;
    size_t key_length = 0;

    data_in.read(&char_in, 1);
    if (!data_in.good())
    {
        throw IoException();
    }

    key_length = static_cast<size_t> (char_in) & 0xFF;
    if (key_length < 1 || key_length > MAX_KEY_LENGTH)
    {
        throw IoException();
    }

    std::unique_ptr<char[]> buffer_mgr(new char[key_length]);
    char* const buffer = buffer_mgr.get();
    data_in.read(buffer, key_length);
    if (!data_in.good())
    {
        throw IoException();
    }

    key.clear();
    key.append(buffer, key_length);
}
