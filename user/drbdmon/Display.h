#ifndef DISPLAY_H
#define	DISPLAY_H

#include <new>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <string>
#include <cstdint>

#include <GenericDisplay.h>
#include <map_types.h>
#include <DrbdResource.h>
#include <DrbdConnection.h>
#include <DrbdVolume.h>
#include <DrbdRole.h>

class Display : public GenericDisplay
{
  public:
    static const char* ANSI_CLEAR;
    static const char* ANSI_CLEAR_LINE;

    static const char* FORMAT_HEADER;

    static const char* RES_LABEL;
    static const char* RES_FORMAT_NAME;

    static const char* ROLE_LABEL;
    static const char* ROLE_FORMAT_PRIMARY;
    static const char* ROLE_FORMAT_SECONDARY;

    static const char* CONN_LABEL;
    static const char* CONN_FORMAT_NAME;
    static const char* CONN_FORMAT_NODE_ID;
    static const char* CONN_FORMAT_STATE_NORM;

    static const char* VOL_LABEL;
    static const char* VOL_LABEL_MINOR;
    static const char* VOL_FORMAT_NR;
    static const char* VOL_FORMAT_MINOR;
    static const char* VOL_FORMAT_STATE_NORM;
    static const char* VOL_FORMAT_STATE_CLIENT;
    static const char* VOL_FORMAT_REPL_NORM;

    static const char* FORMAT_WARN;
    static const char* FORMAT_ALERT;
    static const char* FORMAT_RESET;

    static const uint16_t MIN_SIZE_X;
    static const uint16_t MAX_SIZE_X;
    static const uint16_t MIN_SIZE_Y;
    static const uint16_t MAX_SIZE_Y;

    Display(std::ostream& out_ref, ResourcesMap& resources_map_ref);
    Display(const Display& orig) = delete;
    Display& operator=(const Display& orig) = delete;
    Display(Display&& orig) = delete;
    Display& operator=(Display&& orig) = delete;
    virtual ~Display() noexcept override
    {
    }

    virtual void clear();
    virtual void initial_display() override;
    virtual void status_display() override;
    virtual void display_header() const override;

    virtual void list_resources();
    virtual void list_connections(DrbdResource& res);
    virtual void list_volumes(DrbdResource& res);
    virtual void list_peer_volumes(DrbdConnection& conn);

    virtual void set_terminal_size(uint16_t size_x, uint16_t size_y) override;
    virtual void key_pressed(const char key) override;

  private:
    ResourcesMap& resources_map;
    std::ostream& out;
    uint16_t term_x {80};
    uint16_t term_y {25};
    bool hide_norm_peer_volumes {true};
    bool show_header {true};
    bool enable_term_size {true};
};

#endif	/* DISPLAY_H */
