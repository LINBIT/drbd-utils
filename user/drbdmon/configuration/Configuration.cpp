#include <configuration/Configuration.h>
#include <configuration/CfgEntryBoolean.h>
#include <configuration/CfgEntryIntegerTypes.h>

const bool          Configuration::DFLT_DISCARD_SUCC_TASKS  {true};
const bool          Configuration::DFLT_DISCARD_FAIL_TASKS  {false};
const bool          Configuration::DFLT_SUSPEND_NEW_TASKS   {false};
const bool          Configuration::DFLT_ENABLE_MOUSE_NAV    {true};
const uint16_t      Configuration::DFLT_DSP_INTERVAL        {200};
const uint16_t      Configuration::DFLT_COLOR_SCHEME        {0};
const uint16_t      Configuration::DFLT_CHARACTER_SET       {0};

const std::string   Configuration::KEY_DISCARD_SUCC_TASKS   = "DiscardOkTasks";
const std::string   Configuration::KEY_DISCARD_FAIL_TASKS   = "DiscardFailedTasks";
const std::string   Configuration::KEY_SUSPEND_NEW_TASKS    = "SuspendNewTasks";
const std::string   Configuration::KEY_ENABLE_MOUSE_NAV     = "EnableMouseNav";
const std::string   Configuration::KEY_DSP_INTERVAL         = "DisplayInterval";
const std::string   Configuration::KEY_COLOR_SCHEME         = "ColorScheme";
const std::string   Configuration::KEY_CHARACTER_SET        = "CharacterSet";

Configuration::Configuration()
{
}

Configuration::~Configuration() noexcept
{
}

void Configuration::reset()
{
    discard_succ_tasks  = DFLT_DISCARD_SUCC_TASKS;
    discard_fail_tasks  = DFLT_DISCARD_FAIL_TASKS;
    suspend_new_tasks   = DFLT_SUSPEND_NEW_TASKS;
    enable_mouse_nav    = DFLT_ENABLE_MOUSE_NAV;
    dsp_interval        = DFLT_DSP_INTERVAL;
    color_scheme        = DFLT_COLOR_SCHEME;
    character_set       = DFLT_CHARACTER_SET;
}

void Configuration::save_to(CfgEntryStore& config)
{
    set_entry(config, new CfgEntryBoolean(KEY_DISCARD_SUCC_TASKS, discard_succ_tasks));
    set_entry(config, new CfgEntryBoolean(KEY_DISCARD_FAIL_TASKS, discard_fail_tasks));
    set_entry(config, new CfgEntryBoolean(KEY_SUSPEND_NEW_TASKS, suspend_new_tasks));
    set_entry(config, new CfgEntryBoolean(KEY_ENABLE_MOUSE_NAV, enable_mouse_nav));
    set_entry(config, new CfgEntryUnsgInt16(KEY_DSP_INTERVAL, dsp_interval));
    set_entry(config, new CfgEntryUnsgInt16(KEY_COLOR_SCHEME, color_scheme));
    set_entry(config, new CfgEntryUnsgInt16(KEY_CHARACTER_SET, character_set));
}

void Configuration::load_from(const CfgEntryStore& config)
{
    discard_succ_tasks  = config.get_boolean(KEY_DISCARD_SUCC_TASKS, DFLT_DISCARD_SUCC_TASKS);
    discard_fail_tasks  = config.get_boolean(KEY_DISCARD_FAIL_TASKS, DFLT_DISCARD_FAIL_TASKS);
    suspend_new_tasks   = config.get_boolean(KEY_SUSPEND_NEW_TASKS, DFLT_SUSPEND_NEW_TASKS);
    enable_mouse_nav    = config.get_boolean(KEY_ENABLE_MOUSE_NAV, DFLT_ENABLE_MOUSE_NAV);
    dsp_interval        = config.get_unsgint16(KEY_DSP_INTERVAL, DFLT_DSP_INTERVAL);
    color_scheme        = config.get_unsgint16(KEY_COLOR_SCHEME, DFLT_COLOR_SCHEME);
    character_set       = config.get_unsgint16(KEY_CHARACTER_SET, DFLT_CHARACTER_SET);
}
