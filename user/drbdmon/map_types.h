#ifndef MAP_TYPES_H
#define	MAP_TYPES_H

#include <string>
#include <cstdint>

// https://github.com/raltnoeder/cppdsaext
#include <QTree.h>
#include <VMap.h>

// Forward declarations required due to circular includes
// Do not remove
class DrbdResource;
class DrbdConnection;
class DrbdVolume;

// Map of resource names => DrbdResource objects
using ResourcesMap   = QTree<std::string, DrbdResource>;

// Map of connection names => DrbdConnection objects
using ConnectionsMap = QTree<std::string, DrbdConnection>;

// Map of volume number => DrbdVolume objects
using VolumesMap     = QTree<uint16_t, DrbdVolume>;

// Map of strings, key => value
// Used for the parameters listed in 'drbdsetup events2' lines
using PropsMap       = QTree<std::string, std::string>;

// Map of characters => function description
using HotkeysMap     = VMap<const char, const std::string>;

#endif	/* MAP_TYPES_H */
