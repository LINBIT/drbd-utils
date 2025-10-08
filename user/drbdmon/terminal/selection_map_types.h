#ifndef SELECTION_MAP_TYPES_H
#define SELECTION_MAP_TYPES_H

#include <default_types.h>
#include <string>

// https://github.com/raltnoeder/cppdsaext
#include <QTree.h>

using VolumeSelectionMap        = QTree<uint16_t, void>;
using ConnectionSelectionMap    = QTree<std::string, VolumeSelectionMap>;

class ResourceSubSelections
{
  public:
    std::unique_ptr<VolumeSelectionMap>         volume_selection;
    std::unique_ptr<ConnectionSelectionMap>     connection_selection;
};

using ResourceSelectionMap      = QTree<std::string, ResourceSubSelections>;

#endif /* SELECTION_MAP_TYPES_H */
