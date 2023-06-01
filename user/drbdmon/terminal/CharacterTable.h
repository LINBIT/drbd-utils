#ifndef CHARACTERTABLE_H
#define CHARACTERTABLE_H

#include <string>

class CharacterTable
{
  public:
    // Marked element
    std::string sym_marked;

    std::string sym_info;
    std::string sym_warn;
    std::string sym_alert;

    std::string sym_con;
    std::string sym_not_con;
    std::string sym_discon;

    std::string sym_disk;
    std::string sym_diskless;
    std::string sym_disk_failed;

    std::string sym_pri;
    std::string sym_sec;

    std::string sym_unknown;

    std::string sym_cmd_arrow;

    std::string sym_last_page;

    std::string sym_close;

    std::string sym_working;

    std::string sync_blk_fin;
    std::string sync_blk_rmn;

    std::string checked_box;
    std::string unchecked_box;

    // @throws std::bad_alloc
    CharacterTable();
    virtual ~CharacterTable() noexcept;
};

#endif /* CHARACTERTABLE_H */
