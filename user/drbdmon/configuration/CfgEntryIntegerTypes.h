#ifndef CFGENTRYINTEGERTYPES_H
#define CFGENTRYINTEGERTYPES_H

#include <default_types.h>
#include <configuration/CfgEntry.h>
#include <configuration/CfgEntryNumeric.h>
#include <string>

class CfgEntryUnsgInt8 : public CfgEntry
{
  public:
    CfgEntryUnsgInt8();
    CfgEntryUnsgInt8(const std::string& entry_key);
    CfgEntryUnsgInt8(const std::string& entry_key, const uint8_t value);
    virtual ~CfgEntryUnsgInt8() noexcept;
    virtual uint32_t get_type() const override;
    virtual uint8_t get_value() const;
    virtual void serialize(std::ostream& data_out) const override;
    virtual void deserialize(std::istream& data_in) override;
  private:
    uint8_t number {0};
};

class CfgEntryUnsgInt16 : public CfgEntryNumeric<uint16_t>
{
  public:
    using CfgEntryNumeric<uint16_t>::CfgEntryNumeric;
    virtual uint32_t get_type() const override;
    using CfgEntryNumeric<uint16_t>::get_value;
};

class CfgEntryUnsgInt32 : public CfgEntryNumeric<uint32_t>
{
  public:
    using CfgEntryNumeric<uint32_t>::CfgEntryNumeric;
    virtual uint32_t get_type() const override;
    using CfgEntryNumeric<uint32_t>::get_value;
};

class CfgEntryUnsgInt64 : public CfgEntryNumeric<uint64_t>
{
  public:
    using CfgEntryNumeric<uint64_t>::CfgEntryNumeric;
    virtual uint32_t get_type() const override;
    using CfgEntryNumeric<uint64_t>::get_value;
};

class CfgEntryInt8 : public CfgEntry
{
  public:
    CfgEntryInt8();
    CfgEntryInt8(const std::string& entry_key);
    CfgEntryInt8(const std::string& entry_key, const uint8_t value);
    virtual ~CfgEntryInt8() noexcept;
    virtual uint32_t get_type() const override;
    virtual int8_t get_value() const;
    virtual void serialize(std::ostream& data_out) const override;
    virtual void deserialize(std::istream& data_in) override;
  private:
    int8_t number {0};
};

class CfgEntryInt16 : public CfgEntryNumeric<int16_t>
{
  public:
    using CfgEntryNumeric<int16_t>::CfgEntryNumeric;
    virtual uint32_t get_type() const override;
    using CfgEntryNumeric<int16_t>::get_value;
};

class CfgEntryInt32 : public CfgEntryNumeric<int32_t>
{
  public:
    using CfgEntryNumeric<int32_t>::CfgEntryNumeric;
    virtual uint32_t get_type() const override;
    using CfgEntryNumeric<int32_t>::get_value;
};

class CfgEntryInt64 : public CfgEntryNumeric<int64_t>
{
  public:
    using CfgEntryNumeric<int64_t>::CfgEntryNumeric;
    virtual uint32_t get_type() const override;
    using CfgEntryNumeric<int64_t>::get_value;
};

#endif /* CFGENTRYINTEGERTYPES_H */
