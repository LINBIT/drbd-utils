#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <cstdint>
#include <configuration/CfgEntryStore.h>
#include <configuration/CfgEntry.h>
#include <string>

class Configuration
{
  public:
    static const bool           DFLT_DISCARD_SUCC_TASKS;
    static const bool           DFLT_DISCARD_FAIL_TASKS;
    static const bool           DFLT_SUSPEND_NEW_TASKS;
    static const bool           DFLT_ENABLE_MOUSE_NAV;
    static const uint16_t       DFLT_DSP_INTERVAL;
    static const uint16_t       DFLT_COLOR_SCHEME;
    static const uint16_t       DFLT_CHARACTER_SET;

    static const std::string    KEY_DISCARD_SUCC_TASKS;
    static const std::string    KEY_DISCARD_FAIL_TASKS;
    static const std::string    KEY_SUSPEND_NEW_TASKS;
    static const std::string    KEY_ENABLE_MOUSE_NAV;
    static const std::string    KEY_DSP_INTERVAL;
    static const std::string    KEY_COLOR_SCHEME;
    static const std::string    KEY_CHARACTER_SET;

    bool        discard_succ_tasks      {DFLT_DISCARD_SUCC_TASKS};
    bool        discard_fail_tasks      {DFLT_DISCARD_FAIL_TASKS};
    bool        suspend_new_tasks       {DFLT_SUSPEND_NEW_TASKS};

    bool        enable_mouse_nav        {DFLT_ENABLE_MOUSE_NAV};

    uint16_t    dsp_interval            {DFLT_DSP_INTERVAL};

    uint16_t    color_scheme            {DFLT_COLOR_SCHEME};
    uint16_t    character_set           {DFLT_CHARACTER_SET};

    Configuration();
    virtual ~Configuration() noexcept;

    virtual void save_to(CfgEntryStore& config);
    virtual void load_from(const CfgEntryStore& config);

    virtual void reset();

  private:
    template<typename T>
    void set_entry(CfgEntryStore& config, T* const entry)
    {
        std::unique_ptr<CfgEntry> entry_mgr(dynamic_cast<CfgEntry* const> (entry));
        if (entry_mgr != nullptr)
        {
            config.set_entry(entry);
            entry_mgr.release();
        }
        else
        {
            // Not a subclass of CfgEntry, or entry == nullptr
            delete entry;
        }
    }
};

#endif /* CONFIGURATION_H */
