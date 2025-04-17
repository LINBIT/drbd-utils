#include <configuration/CfgEntryIntegerTypes.h>
#include <platform/IoException.h>

CfgEntryUnsgInt8::CfgEntryUnsgInt8()
{
}

CfgEntryUnsgInt8::CfgEntryUnsgInt8(const std::string& entry_key):
    CfgEntry::CfgEntry(entry_key)
{
}

CfgEntryUnsgInt8::CfgEntryUnsgInt8(const std::string& entry_key, const uint8_t value):
    CfgEntry::CfgEntry(entry_key)
{
    number = value;
}

CfgEntryUnsgInt8::~CfgEntryUnsgInt8() noexcept
{
}

uint32_t CfgEntryUnsgInt8::get_type() const
{
    return CfgEntry::TYPE_UNSGINT8;
}

uint8_t CfgEntryUnsgInt8::get_value() const
{
    return number;
}

void CfgEntryUnsgInt8::serialize(std::ostream& data_out) const
{
    CfgEntry::serialize_key(data_out);

    char char_out = static_cast<char> (number);
    data_out.write(&char_out, 1);
    if (!data_out.good())
    {
        throw IoException();
    }
}

void CfgEntryUnsgInt8::deserialize(std::istream& data_in)
{
    CfgEntry::deserialize_key(data_in);

    char char_in = 0;
    data_in.read(&char_in, 1);
    if (data_in.fail() || data_in.bad())
    {
        throw IoException();
    }

    number = static_cast<uint8_t> (char_in);
}

uint32_t CfgEntryUnsgInt16::get_type() const
{
    return CfgEntry::TYPE_UNSGINT16;
}

uint32_t CfgEntryUnsgInt32::get_type() const
{
    return CfgEntry::TYPE_UNSGINT32;
}

uint32_t CfgEntryUnsgInt64::get_type() const
{
    return CfgEntry::TYPE_UNSGINT64;
}

CfgEntryInt8::CfgEntryInt8()
{
}

CfgEntryInt8::CfgEntryInt8(const std::string& entry_key):
    CfgEntry::CfgEntry(entry_key)
{
}

CfgEntryInt8::CfgEntryInt8(const std::string& entry_key, const uint8_t value):
    CfgEntry::CfgEntry(entry_key)
{
    number = value;
}

CfgEntryInt8::~CfgEntryInt8() noexcept
{
}

uint32_t CfgEntryInt8::get_type() const
{
    return CfgEntry::TYPE_INT8;
}

int8_t CfgEntryInt8::get_value() const
{
    return number;
}

void CfgEntryInt8::serialize(std::ostream& data_out) const
{
    CfgEntry::serialize_key(data_out);

    char char_out = static_cast<char> (number);
    data_out.write(&char_out, 1);
    if (!data_out.good())
    {
        throw IoException();
    }
}

void CfgEntryInt8::deserialize(std::istream& data_in)
{
    CfgEntry::deserialize_key(data_in);

    char char_in = 0;
    data_in.read(&char_in, 1);
    if (data_in.fail() || data_in.bad())
    {
        throw IoException();
    }

    number = static_cast<int8_t> (char_in);
}

uint32_t CfgEntryInt16::get_type() const
{
    return CfgEntry::TYPE_INT16;
}

uint32_t CfgEntryInt32::get_type() const
{
    return CfgEntry::TYPE_INT32;
}

uint32_t CfgEntryInt64::get_type() const
{
    return CfgEntry::TYPE_INT64;
}
