#ifndef COMPONENTSHUB_H
#define COMPONENTSHUB_H

#include <default_types.h>
#include <DrbdMonCore.h>
#include <platform/SystemApi.h>
#include <terminal/DisplaySelector.h>
#include <terminal/DisplayIo.h>
#include <terminal/DisplayCommon.h>
#include <terminal/SharedData.h>
#include <terminal/DisplayStyleCollection.h>
#include <terminal/AnsiControl.h>
#include <terminal/ColorTable.h>
#include <terminal/CharacterTable.h>
#include <terminal/TermSize.h>
#include <terminal/GlobalCommands.h>
#include <terminal/DrbdCommands.h>
#include <subprocess/SubProcessQueue.h>
#include <configuration/Configuration.h>
#include <MessageLog.h>
#include <map_types.h>
#include <string>

class InputField;

class ComponentsHub
{
  public:
    DrbdMonCore*            core_instance   {nullptr};
    SystemApi*              sys_api         {nullptr};
    DisplaySelector*        dsp_selector    {nullptr};
    DisplayIo*              dsp_io          {nullptr};
    DisplayCommon*          dsp_common      {nullptr};
    SharedData*             dsp_shared      {nullptr};
    InputField*             command_line    {nullptr};
    TermSize*               term_size       {nullptr};
    ResourcesMap*           rsc_map         {nullptr};
    ResourcesMap*           prb_rsc_map     {nullptr};
    MessageLog*             log             {nullptr};
    MessageLog*             debug_log       {nullptr};
    GlobalCommands*         global_cmd_exec {nullptr};
    DrbdCommands*           drbd_cmd_exec   {nullptr};
    const Configuration*    config          {nullptr};
    const std::string*      node_name       {nullptr};
    const std::string*      events_file     {nullptr};

    AnsiControl*            ansi_ctl    {nullptr};
    DisplayStyleCollection* style_coll  {nullptr};

    const ColorTable*       active_color_table      {nullptr};
    const CharacterTable*   active_character_table  {nullptr};

    SubProcessQueue*    sub_proc_queue  {nullptr};

    bool        have_term_size  {false};
    uint16_t    term_cols       {100};
    uint16_t    term_rows       {30};

    ComponentsHub();
    virtual ~ComponentsHub() noexcept;

    virtual DrbdResource* get_monitor_resource() const;
    virtual DrbdConnection* get_monitor_connection() const;

    virtual void verify() const;
};

#endif /* COMPONENTSHUB_H */
