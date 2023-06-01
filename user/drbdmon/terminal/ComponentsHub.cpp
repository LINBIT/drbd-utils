#include <terminal/ComponentsHub.h>
#include <objects/DrbdResource.h>
#include <stdexcept>

ComponentsHub::ComponentsHub()
{
}

ComponentsHub::~ComponentsHub() noexcept
{
}

DrbdResource* ComponentsHub::get_monitor_resource() const
{
    DrbdResource* rsc = nullptr;
    if (!dsp_shared->monitor_rsc.empty())
    {
        rsc = rsc_map->get(&(dsp_shared->monitor_rsc));
    }
    return rsc;
}

DrbdConnection* ComponentsHub::get_monitor_connection() const
{
    DrbdConnection* con = nullptr;
    if (!dsp_shared->monitor_con.empty())
    {
        DrbdResource* const rsc = get_monitor_resource();
        if (rsc != nullptr)
        {
            con = rsc->get_connection(dsp_shared->monitor_con);
        }
    }
    return con;
}

void ComponentsHub::verify() const
{
    if (core_instance == nullptr ||
        sys_api == nullptr ||
        dsp_selector == nullptr ||
        dsp_io == nullptr ||
        dsp_common == nullptr ||
        command_line == nullptr ||
        term_size == nullptr ||
        rsc_map == nullptr ||
        prb_rsc_map == nullptr ||
        log == nullptr ||
        node_name == nullptr ||
        ansi_ctl == nullptr ||
        style_coll == nullptr ||
        active_color_table == nullptr ||
        active_character_table == nullptr ||
        sub_proc_queue == nullptr ||
        drbd_cmd_exec == nullptr ||
        global_cmd_exec == nullptr ||
        config == nullptr)
    {
        throw std::logic_error("Uninitialized ComponentsHub field");
    }
}
