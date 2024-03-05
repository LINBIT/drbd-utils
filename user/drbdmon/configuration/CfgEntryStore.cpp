#include <configuration/CfgEntryStore.h>
#include <configuration/CfgEntryBoolean.h>
#include <configuration/CfgEntryIntegerTypes.h>
#include <configuration/IoException.h>
#include <comparators.h>
#include <new>
#include <memory>
#include <cstring>

// Configuration Entry Store UUID
// Variant 1 Version 4 (random): 6A454A5D-347A-498D-8E0A-6AD5F5014133
const std::string   CfgEntryStore::CES_UUID("\x6A\x45\x4A\x5D\x34\x7A\x49\x8D\x8E\x0A\x6A\xD5\xF5\x01\x41\x33");

// Configuration Entry Store Version: Major version (16 bits), minor version (16 bits)
// Minor versions are supposed to be backwards compatible
const uint32_t      CfgEntryStore::CES_VERSION  = 0x00010000UL;

CfgEntryStore::CfgEntryStore()
{
    config_mgr = std::unique_ptr<CfgMap> (new CfgMap(&comparators::compare_string));
    config = config_mgr.get();
}

CfgEntryStore::~CfgEntryStore() noexcept
{
    clear_impl();
}

void CfgEntryStore::clear_impl() noexcept
{
    CfgMap::NodesIterator iter(*config);
    while (iter.has_next())
    {
        CfgMap::Node* const cur_node = iter.next();
        // The key is contained in the entry, which is stored as the value
        CfgEntry* entry = cur_node->get_value();
        delete entry;
    }
    config->clear();
}

void CfgEntryStore::set_entry(CfgEntry* const entry)
{
    std::unique_ptr<CfgMap::Node> existing_node_mgr;
    const std::string& entry_key = entry->get_key();
    CfgMap::Node* existing_node = config->get_node(&entry_key);
    if (existing_node != nullptr)
    {
        existing_node_mgr = std::unique_ptr<CfgMap::Node>(existing_node);
        config->unlink_node(existing_node);
    }
    try
    {
        config->insert(&entry_key, entry);
    }
    catch (std::exception&)
    {
        if (existing_node != nullptr)
        {
            config->insert_node(existing_node);
        }
        existing_node_mgr.release();
        throw;
    }
    if (existing_node_mgr != nullptr)
    {
        CfgEntry* const existing_entry = existing_node->get_value();
        delete existing_entry;
    }
}

const CfgEntry* CfgEntryStore::get_entry(const std::string& entry_key) const
{
    const CfgEntry* const entry = config->get(&entry_key);
    return entry;
}

void CfgEntryStore::delete_entry(const std::string& entry_key)
{
    CfgMap::Node* const cur_node = config->get_node(&entry_key);
    if (cur_node != nullptr)
    {
        CfgEntry* const entry = cur_node->get_value();
        delete entry;
        config->remove_node(cur_node);
    }
}

bool CfgEntryStore::get_boolean(const std::string& entry_key, const bool default_value) const
{
    return get_numeric_entry<bool, CfgEntryBoolean>(entry_key, default_value, CfgEntry::TYPE_BOOLEAN);
}

uint8_t CfgEntryStore::get_unsgint8(const std::string& entry_key, const uint8_t default_value) const
{
    return get_numeric_entry<uint8_t, CfgEntryUnsgInt8>(entry_key, default_value, CfgEntry::TYPE_UNSGINT8);
}

uint16_t CfgEntryStore::get_unsgint16(const std::string& entry_key, const uint16_t default_value) const
{
    return get_numeric_entry<uint16_t, CfgEntryUnsgInt16>(entry_key, default_value, CfgEntry::TYPE_UNSGINT16);
}

uint32_t CfgEntryStore::get_unsgint32(const std::string& entry_key, const uint32_t default_value) const
{
    return get_numeric_entry<uint32_t, CfgEntryUnsgInt32>(entry_key, default_value, CfgEntry::TYPE_UNSGINT32);
}

uint64_t CfgEntryStore::get_unsgint64(const std::string& entry_key, const uint64_t default_value) const
{
    return get_numeric_entry<uint64_t, CfgEntryUnsgInt64>(entry_key, default_value, CfgEntry::TYPE_UNSGINT64);
}

int8_t CfgEntryStore::get_int8(const std::string& entry_key, const int8_t default_value) const
{
    return get_numeric_entry<int8_t, CfgEntryInt8>(entry_key, default_value, CfgEntry::TYPE_INT8);
}

int16_t CfgEntryStore::get_int16(const std::string& entry_key, const int16_t default_value) const
{
    return get_numeric_entry<int16_t, CfgEntryInt16>(entry_key, default_value, CfgEntry::TYPE_INT16);
}

int32_t CfgEntryStore::get_int32(const std::string& entry_key, const int32_t default_value) const
{
    return get_numeric_entry<int32_t, CfgEntryInt32>(entry_key, default_value, CfgEntry::TYPE_INT32);
}

int64_t CfgEntryStore::get_int64(const std::string& entry_key, const int64_t default_value) const
{
    return get_numeric_entry<int64_t, CfgEntryInt64>(entry_key, default_value, CfgEntry::TYPE_INT64);
}

CfgEntryStore::CfgMap::ValuesIterator CfgEntryStore::iterator()
{
    return CfgMap::ValuesIterator(*config);
}

void CfgEntryStore::load_from(std::istream& data_in)
{
    if (data_in.good())
    {
        {
            char buffer[16];
            data_in.read(buffer, 16);
            if (data_in.fail() || data_in.bad())
            {
                throw IoException("I/O error while deserializing the configuration entry store UUID");
            }
            if (std::memcmp(CES_UUID.c_str(), buffer, 16) != 0)
            {
                // UUID mismatch
                throw IoException();
            }
        }

        {
            char buffer[4];
            data_in.read(buffer, 4);
            if (data_in.fail() || data_in.bad())
            {
                throw IoException("I/O error while deserializing the configuration entry store version field");
            }
            uint32_t stored_version = 0;
            for (size_t idx = 0; idx < 4; ++idx)
            {
                stored_version <<= 8;
                stored_version |= static_cast<uint32_t> (buffer[idx]) & 0xFF;
            }

            if ((stored_version & 0xFFFF0000UL) != (CES_VERSION & 0xFFFF0000UL))
            {
                throw IoException("Incompatible configuration entry store version");
            }
        }
    }
    while (data_in.good())
    {
        uint32_t entry_type = 0;
        char buffer[ENTRY_TYPE_SIZE];
        data_in.read(buffer, ENTRY_TYPE_SIZE);
        if (!data_in.eof())
        {
            if (data_in.fail() || data_in.bad())
            {
                throw IoException("I/O error while deserializing an entry type value");
            }

            for (size_t idx = 0; idx < ENTRY_TYPE_SIZE; ++idx)
            {
                entry_type <<= 8;
                entry_type = static_cast<uint32_t> (buffer[idx]) & 0xFF;
            }

            std::unique_ptr<CfgEntry> entry_mgr;
            if (entry_type == CfgEntry::TYPE_BOOLEAN)
            {
                entry_mgr = std::unique_ptr<CfgEntry>(dynamic_cast<CfgEntry*> (new CfgEntryBoolean()));
            }
            else
            if (entry_type == CfgEntry::TYPE_UNSGINT8)
            {
                entry_mgr = std::unique_ptr<CfgEntry>(dynamic_cast<CfgEntry*> (new CfgEntryUnsgInt8()));
            }
            else
            if (entry_type == CfgEntry::TYPE_UNSGINT16)
            {
                entry_mgr = std::unique_ptr<CfgEntry>(dynamic_cast<CfgEntry*> (new CfgEntryUnsgInt16()));
            }
            else
            if (entry_type == CfgEntry::TYPE_UNSGINT32)
            {
                entry_mgr = std::unique_ptr<CfgEntry>(dynamic_cast<CfgEntry*> (new CfgEntryUnsgInt32()));
            }
            else
            if (entry_type == CfgEntry::TYPE_UNSGINT64)
            {
                entry_mgr = std::unique_ptr<CfgEntry>(dynamic_cast<CfgEntry*> (new CfgEntryUnsgInt64()));
            }
            else
            if (entry_type == CfgEntry::TYPE_INT8)
            {
                entry_mgr = std::unique_ptr<CfgEntry>(dynamic_cast<CfgEntry*> (new CfgEntryInt8()));
            }
            else
            if (entry_type == CfgEntry::TYPE_INT16)
            {
                entry_mgr = std::unique_ptr<CfgEntry>(dynamic_cast<CfgEntry*> (new CfgEntryInt16()));
            }
            else
            if (entry_type == CfgEntry::TYPE_INT32)
            {
                entry_mgr = std::unique_ptr<CfgEntry>(dynamic_cast<CfgEntry*> (new CfgEntryInt32()));
            }
            else
            if (entry_type == CfgEntry::TYPE_INT64)
            {
                entry_mgr = std::unique_ptr<CfgEntry>(dynamic_cast<CfgEntry*> (new CfgEntryInt64()));
            }
            else
            {
                throw IoException();
            }

            CfgEntry* const entry = entry_mgr.get();
            entry->deserialize(data_in);

            const std::string& entry_key = entry->get_key();
            CfgEntry* const existing_entry = config->get(&entry_key);
            if (existing_entry != nullptr)
            {
                set_entry(entry);
                entry_mgr.release();
            }
        }
    }

    if ((data_in.fail() && !data_in.eof()) || data_in.bad())
    {
        throw IoException();
    }
}

void CfgEntryStore::save_to(std::ostream& data_out) const
{
    data_out.write(CES_UUID.c_str(), 16);
    if (!data_out.good())
    {
        throw IoException("I/O error while serializing the configuration entry store UUID");
    }
    {
        char buffer[4];
        for (size_t idx = 0; idx < 4; ++idx)
        {
            const size_t shift = (3 - idx) << 3;
            buffer[idx] = static_cast<unsigned char> ((CES_VERSION >> shift) & 0xFF);
        }
        data_out.write(buffer, 4);
        if (!data_out.good())
        {
            throw IoException("I/O error while serializing the configuration entry store version field");
        }
    }
    CfgMap::ValuesIterator entry_iter(*config);
    while (entry_iter.has_next())
    {
        CfgEntry* const entry = entry_iter.next();

        // Serialize the entry type
        const uint32_t entry_type = entry->get_type();
        char buffer[ENTRY_TYPE_SIZE];
        for (size_t idx = 0; idx < ENTRY_TYPE_SIZE; ++idx)
        {
            const size_t shift = (ENTRY_TYPE_SIZE - idx - 1) * 8;
            buffer[idx] = static_cast<char> ((entry_type >> shift) & 0xFF);
        }
        data_out.write(buffer, ENTRY_TYPE_SIZE);
        if (!data_out.good())
        {
            throw IoException("I/O error while serializing an entry type value");
        }

        entry->serialize(data_out);
    }
}
