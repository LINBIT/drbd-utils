#include <DebugDisplay.h>
#include <DrbdMon.h>

DebugDisplay::DebugDisplay(
    ResourcesMap& resources_map_ref,
    MessageLog&   log_ref,
    HotkeysMap&   hotkeys_info_ref
):
    resources_map(resources_map_ref),
    log(log_ref),
    hotkeys_info(hotkeys_info_ref)
{
}

DebugDisplay::~DebugDisplay() noexcept
{
}

void DebugDisplay::initial_display()
{
    display_header();
    std::fputs("Reading initial DRBD status\n", stdout);
}

void DebugDisplay::status_display()
{
    display_header();

    ResourcesMap::ValuesIterator res_iter(resources_map);
    while (res_iter.has_next())
    {
        DrbdResource& resource = *(res_iter.next());
        const std::string& res_name = resource.get_name();
        std::fprintf(stdout, "Resource %s\n", res_name.c_str());

        std::fputs("  Volumes:\n", stdout);
        VolumesMap::ValuesIterator vol_iter = resource.volumes_iterator();
        while (vol_iter.has_next())
        {
            DrbdVolume& volume = *(vol_iter.next());
            uint16_t vol_id = volume.get_volume_nr();

            fprintf(stdout, "  * Volume #%u\n", (unsigned int) vol_id);
        }

        std::fputs("  Connections:\n", stdout);
        ConnectionsMap::ValuesIterator conn_iter = resource.connections_iterator();
        while (conn_iter.has_next())
        {
            DrbdConnection& connection = *(conn_iter.next());
            const std::string& conn_name = connection.get_name();
            std::fprintf(stdout, "  * Connection -> %s\n", conn_name.c_str());

            std::fputs("  Peer Volumes:\n", stdout);
            VolumesMap::ValuesIterator peer_vol_iter = connection.volumes_iterator();
            while (peer_vol_iter.has_next())
            {
                DrbdVolume& volume = *(peer_vol_iter.next());
                uint16_t vol_id = volume.get_volume_nr();

                std::fprintf(stdout, "    * Peer-Volume #%u\n", (unsigned int) vol_id);
            }
        }
    }

    display_hotkeys_info();

    std::fputc('\n', stdout);
    std::fflush(stdout);
}

void DebugDisplay::display_header() const
{
    std::fprintf(stdout, "%s DebugDisplay v%s\n\n", DrbdMon::PROGRAM_NAME.c_str(), DrbdMon::VERSION.c_str());
}

void DebugDisplay::set_terminal_size(uint16_t size_x, uint16_t size_y)
{
    // no-op
}

void DebugDisplay::display_hotkeys_info() const
{
    HotkeysMap::NodesIterator hotkeys_iter(hotkeys_info);
    size_t count = hotkeys_iter.get_size();
    for (size_t index = 0; index < count; ++index)
    {
        HotkeysMap::Node* node = hotkeys_iter.next();
        const char hotkey = *(node->get_key());
        const std::string& description = *(node->get_value());
        if (index > 0)
        {
            std::fputs(" / ", stdout);
        }
        std::fprintf(stdout, "%c => %s", hotkey, description.c_str());
    }
    std::fputc('\n', stdout);
}

void DebugDisplay::key_pressed(const char key)
{
}
