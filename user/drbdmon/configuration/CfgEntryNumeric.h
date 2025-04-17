#ifndef CFGENTRYNUMERIC_H
#define CFGENTRYNUMERIC_H

#include <default_types.h>
#include <configuration/CfgEntry.h>
#include <platform/IoException.h>
#include <istream>
#include <ostream>

template<typename T>
class CfgEntryNumeric : public CfgEntry
{
  public:
    CfgEntryNumeric()
    {
    }

    CfgEntryNumeric(const std::string entry_key, const T value):
        CfgEntry::CfgEntry(entry_key)
    {
        number = value;
    }

    virtual ~CfgEntryNumeric() noexcept
    {
    }

    const T get_value() const
    {
        return number;
    }

    void serialize(std::ostream& data_out) const override
    {
        CfgEntry::serialize_key(data_out);

        char buffer[sizeof (number)];

        for (size_t idx = 0; idx < sizeof (number); ++idx)
        {
            const size_t shift = (sizeof (number) - idx - 1) << 3;
            buffer[idx] = static_cast<unsigned char> ((number >> shift) & 0xFF);
        }

        data_out.write(buffer, sizeof (number));
        if (!data_out.good())
        {
            throw IoException("I/O error while serializing a numeric value");
        }
    }

    void deserialize(std::istream& data_in) override
    {
        CfgEntry::deserialize_key(data_in);

        char buffer[sizeof (number)];

        data_in.read(buffer, sizeof (number));
        if (data_in.fail() || data_in.bad())
        {
            throw IoException("I/O error while deserializing a numeric value");
        }

        number = 0;
        for (size_t idx = 0; idx < sizeof (number); ++idx)
        {
            number <<= 8;
            number |= static_cast<T> (buffer[idx]) & 0xFF;
        }
    }

  private:
    T   number  {0};

    static_assert(sizeof (T) >= 1, "Invalid data type, stored size < 1 byte");
};

#endif /* CFGENTRYNUMERIC_H */
