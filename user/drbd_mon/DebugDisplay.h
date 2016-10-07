#ifndef DEBUGDISPLAY_H
#define	DEBUGDISPLAY_H

#include <string>
#include <cstdint>

#include <GenericDisplay.h>
#include <map_types.h>
#include <DrbdResource.h>
#include <DrbdConnection.h>
#include <DrbdVolume.h>
#include <MessageLog.h>

class DebugDisplay : public GenericDisplay
{
  public:
    DebugDisplay(
        ResourcesMap& resources_map_ref,
        MessageLog&   log_ref,
        HotkeysMap&   hotkeys_info_ref
    );
    DebugDisplay(const DebugDisplay& orig) = delete;
    DebugDisplay& operator=(const DebugDisplay& orig) = delete;
    DebugDisplay(DebugDisplay&& orig) = default;
    DebugDisplay& operator=(DebugDisplay&& orig) = default;
    virtual ~DebugDisplay() noexcept;

    virtual void status_display() override;
    virtual void display_header() const override;
    virtual void set_terminal_size(uint16_t size_x, uint16_t size_y) override;

  private:
    ResourcesMap& resources_map;
    MessageLog& log;
    HotkeysMap& hotkeys_info;

    virtual void display_hotkeys_info() const;
};

#endif	/* DEBUGDISPLAY_H */
