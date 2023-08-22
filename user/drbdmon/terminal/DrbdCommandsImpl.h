#ifndef DRBDCOMMANDSIMPL_H
#define DRBDCOMMANDSIMPL_H

#include <default_types.h>
#include <terminal/CommandsBase.h>
#include <terminal/DrbdCommands.h>
#include <terminal/ComponentsHub.h>
#include <subprocess/CmdLine.h>
#include <QTree.h>
#include <StringTokenizer.h>
#include <new>
#include <memory>
#include <string>

class DrbdCommandsImpl : public DrbdCommands, public CommandsBase<DrbdCommandsImpl>
{
  public:
    static const std::string    KEY_CMD_START;
    static const std::string    KEY_CMD_STOP;
    static const std::string    KEY_CMD_UP;
    static const std::string    KEY_CMD_DOWN;
    static const std::string    KEY_CMD_ADJUST;
    static const std::string    KEY_CMD_PRIMARY;
    static const std::string    KEY_CMD_FORCE_PRIMARY;
    static const std::string    KEY_CMD_SECONDARY;
    static const std::string    KEY_CMD_CONNECT;
    static const std::string    KEY_CMD_DISCONNECT;
    static const std::string    KEY_CMD_ATTACH;
    static const std::string    KEY_CMD_DETACH;
    static const std::string    KEY_CMD_DISCARD_CONNECT;
    static const std::string    KEY_CMD_VERIFY;
    static const std::string    KEY_CMD_INVALIDATE;

    static const size_t         STRING_PREALLOC_LENGTH;

    DrbdCommandsImpl(const ComponentsHub& comp_hub);
    virtual ~DrbdCommandsImpl() noexcept;

    virtual bool execute_command(const std::string& command, StringTokenizer& tokenizer) override;
    virtual bool complete_command(const std::string& prefix, std::string& completion) override;

  private:
    typedef void (DrbdCommandsImpl::*exec_rsc_type)(const std::string& rsc_name);
    typedef void (DrbdCommandsImpl::*exec_con_type)(const std::string& rsc_name, const std::string& con_name);
    typedef void (DrbdCommandsImpl::*exec_vlm_type)(const std::string& rsc_name, const uint16_t vlm_nr);

    const ComponentsHub& dsp_comp_hub;

    Entry entry_start;
    Entry entry_stop;
    Entry entry_up;
    Entry entry_down;
    Entry entry_adjust;
    Entry entry_primary;
    Entry entry_force_primary;
    Entry entry_secondary;
    Entry entry_connect;
    Entry entry_disconnect;
    Entry entry_attach;
    Entry entry_detach;
    Entry entry_discard_connect;
    Entry entry_verify;
    Entry entry_invalidate;

    bool cmd_start(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_stop(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_adjust(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_primary(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_force_primary(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_secondary(const std::string& command, StringTokenizer& tokenizer);

    bool cmd_connect(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_disconnect(const std::string& command, StringTokenizer& tokenizer);

    bool cmd_attach(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_detach(const std::string& command, StringTokenizer& tokenizer);

    bool cmd_discard_connect(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_verify(const std::string& command, StringTokenizer& tokenizer);
    bool cmd_invalidate(const std::string& command, StringTokenizer& tokenizer);

    bool exec_for_resources(const std::string& command, StringTokenizer& tokenizer, exec_rsc_type exec_func);
    bool exec_for_connections(const std::string& command, StringTokenizer& tokenizer, exec_con_type exec_func);
    bool exec_for_volumes(const std::string& command, StringTokenizer& tokenizer, exec_vlm_type exec_func);

    void exec_start(const std::string& rsc_name);
    void exec_stop(const std::string& rsc_name);
    void exec_adjust(const std::string& rsc_name);
    void exec_primary(const std::string& rsc_name);
    void exec_force_primary(const std::string& rsc_name);
    void exec_secondary(const std::string& rsc_name);

    void exec_connect(const std::string& rsc_name, const std::string& con_name);
    void exec_disconnect(const std::string& rsc_name, const std::string& con_name);

    void exec_attach(const std::string& rsc_name, const uint16_t vlm_nr);
    void exec_detach(const std::string& rsc_name, const uint16_t vlm_nr);

    void exec_discard_connect(const std::string& rsc_name, const std::string& con_name);
    void exec_verify(const std::string& rsc_name, const std::string& con_name, const uint16_t vlm_nr);
    void exec_invalidate(const std::string& rsc_name, const uint16_t vlm_nr);

    void get_resource_name(const std::string& argument, std::string& rsc_name);
    void get_connection_name(const std::string& argument, std::string& con_name);
    void get_volume_number(const std::string& argument, uint16_t& vlm_nr);

    bool can_run_resource_cmd();
    bool can_run_connection_cmd();
    bool can_run_volume_cmd();

    void queue_command(std::unique_ptr<CmdLine>& command);
};

#endif /* DRBDCOMMANDSIMPL_H_ */
