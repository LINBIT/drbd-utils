#ifndef GLOBALCOMMANDSIMPL_H
#define GLOBALCOMMANDSIMPL_H

#include <terminal/CommandsBase.h>
#include <terminal/GlobalCommands.h>
#include <terminal/ComponentsHub.h>
#include <configuration/Configuration.h>
#include <QTree.h>
#include <new>
#include <memory>

class GlobalCommandsImpl : public GlobalCommands, public CommandsBase<GlobalCommandsImpl>
{
  public:
    GlobalCommandsImpl(ComponentsHub& comp_hub, Configuration& config_ref);
    virtual ~GlobalCommandsImpl() noexcept;

    virtual bool execute_command(const std::string& command, StringTokenizer& tokenizer) override;
    virtual bool complete_command(const std::string& prefix, std::string& completion) override;

  private:
    ComponentsHub& dsp_comp_hub;
    Configuration* config {nullptr};

    Entry entry_exit;
    Entry entry_display;
    Entry entry_colors;
    Entry entry_charset;
    Entry entry_select_all;
    Entry entry_deselect;
    Entry entry_cursor;

    bool cmd_exit(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_display(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_colors(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_charset(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_select_all(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_deselect(const std::string& command, StringTokenizer& tokenizer);
    // Dummy method for display local commands
    bool local_command(const std::string& command, StringTokenizer& tokenizer);
};

#endif /* GLOBALCOMMANDSIMPL_H */
