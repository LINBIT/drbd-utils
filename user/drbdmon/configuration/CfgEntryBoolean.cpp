#include <configuration/CfgEntryBoolean.h>

CfgEntryBoolean::CfgEntryBoolean()
{
}

CfgEntryBoolean::CfgEntryBoolean(const std::string& entry_key, const bool value):
    CfgEntry::CfgEntry(entry_key)
{
    flag = value;
}

CfgEntryBoolean::~CfgEntryBoolean() noexcept
{
}

uint32_t CfgEntryBoolean::get_type() const
{
    return CfgEntry::TYPE_BOOLEAN;
}

const bool CfgEntryBoolean::get_value() const
{
    return flag;
}

void CfgEntryBoolean::serialize(std::ostream& data_out) const
{
    CfgEntry::serialize_key(data_out);

    const char char_out = static_cast<char> (flag ? 1 : 0);
    data_out.write(&char_out, 1);
    if (!data_out.good())
    {
        throw IoException("I/O error while serializing a boolean value");
    }
}

void CfgEntryBoolean::deserialize(std::istream& data_in)
{
    CfgEntry::deserialize_key(data_in);

    char char_in;
    data_in.read(&char_in, 1);
    if (!data_in.good())
    {
        throw IoException("I/O error while deserializing a boolean value");
    }

    if (char_in == 0)
    {
        flag = false;
    }
    else
    if (char_in == 1)
    {
        flag = true;
    }
    else
    {
        throw IoException("Invalid stored value for boolean configuration value");
    }
}
