#ifndef DRBDCOMMANDS_H
#define DRBDCOMMANDS_H

#include <default_types.h>
#include <StringTokenizer.h>
#include <string>
#include <objects/DrbdResource.h>
#include <objects/DrbdVolume.h>
#include <objects/DrbdConnection.h>

class DrbdCommands
{
  public:
    virtual ~DrbdCommands() noexcept
    {
    }

    typedef void (DrbdCommands::*resource_action_fn)(const std::string& rsc_name);
    typedef void (DrbdCommands::*volume_action_fn)(const std::string& rsc_name, const uint16_t vlm_nr);
    typedef void (DrbdCommands::*connection_action_fn)(const std::string& rsc_name, const std::string& con_name);
    typedef void (DrbdCommands::*peer_volume_action_fn)(const std::string& rsc_name,
                                                        const std::string& con_name,
                                                        const uint16_t peer_vlm_nr);

    virtual bool execute_command(const std::string& command, StringTokenizer& tokenizer) = 0;
    virtual bool complete_command(const std::string& prefix, std::string& completion) = 0;

    virtual void exec_start(const std::string& rsc_name) = 0;
    virtual void exec_stop(const std::string& rsc_name) = 0;
    virtual void exec_adjust(const std::string& rsc_name) = 0;
    virtual void exec_adjust_skip_disk(const std::string& rsc_name) = 0;
    virtual void exec_adjust_skip_net(const std::string& rsc_name) = 0;
    virtual void exec_adjust_skip_disk_net(const std::string& rsc_name) = 0;
    virtual void exec_primary(const std::string& rsc_name) = 0;
    virtual void exec_force_primary(const std::string& rsc_name) = 0;
    virtual void exec_secondary(const std::string& rsc_name) = 0;
    virtual void exec_force_secondary(const std::string& rsc_name) = 0;

    virtual void exec_connect(const std::string& rsc_name, const std::string& con_name) = 0;
    virtual void exec_disconnect(const std::string& rsc_name, const std::string& con_name) = 0;
    virtual void exec_force_disconnect(const std::string& rsc_name, const std::string& con_name) = 0;

    virtual void exec_attach(const std::string& rsc_name, const uint16_t vlm_nr) = 0;
    virtual void exec_detach(const std::string& rsc_name, const uint16_t vlm_nr) = 0;

    virtual void exec_discard_connect(const std::string& rsc_name, const std::string& con_name) = 0;
    virtual void exec_verify(const std::string& rsc_name, const std::string& con_name, const uint16_t vlm_nr) = 0;
    virtual void exec_invalidate(const std::string& rsc_name, const uint16_t vlm_nr) = 0;
    virtual void exec_invalidate_remote(
        const std::string&  rsc_name,
        const std::string&  con_name,
        const uint16_t      vlm_nr
    ) = 0;

    virtual void exec_pause_sync(const std::string& rsc_name, const std::string& con_name, const uint16_t vlm_nr) = 0;
    virtual void exec_resume_sync(const std::string& rsc_name, const std::string& con_name, const uint16_t vlm_nr) = 0;

    virtual void exec_resource_program(
        const std::string& program,
        const std::string& rsc_name,
        const DrbdResource* const rsc
    ) = 0;
    virtual void exec_volume_program(
        const std::string& program,
        const std::string& rsc_name,
        const DrbdResource* const rsc,
        const uint16_t vlm_nr,
        const DrbdVolume* const vlm
    ) = 0;
    virtual void exec_connection_program(
        const std::string& program,
        const std::string& rsc_name,
        const DrbdResource* const rsc,
        const std::string& con_name,
        const DrbdConnection* const con
    ) = 0;
    virtual void exec_peer_volume_program(
        const std::string& program,
        const std::string& rsc_name,
        const DrbdResource* const rsc,
        const std::string& con_name,
        const DrbdConnection* const con,
        const uint16_t vlm_nr,
        const DrbdVolume* const vlm
    ) = 0;
};

#endif /* DRBDCOMMANDS_H */
