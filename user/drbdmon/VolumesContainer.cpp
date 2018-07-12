#include <VolumesContainer.h>

// @throws std::bad_alloc
VolumesContainer::VolumesContainer():
    volume_list(new VolumesMap(&dsaext::generic_compare<uint16_t>))
{
}

VolumesContainer::~VolumesContainer() noexcept
{
    VolumesMap::NodesIterator dtor_iter(*volume_list);
    while (dtor_iter.has_next())
    {
        VolumesMap::Node* node = dtor_iter.next();
        delete node->get_key();
        delete node->get_value();
    }
    volume_list->clear();
}

// @throws std::bad_alloc, dsaext::DuplicateInsertException
void VolumesContainer::add_volume(DrbdVolume* volume)
{
    std::unique_ptr<uint16_t> volume_key(new uint16_t);
    *(volume_key.get()) = volume->get_volume_nr();
    volume_list->insert(volume_key.get(), volume);
    static_cast<void> (volume_key.release());
}

void VolumesContainer::remove_volume(uint16_t volume_nr)
{
    VolumesMap::Node* node = volume_list->get_node(&volume_nr);
    if (node != nullptr)
    {
        delete node->get_key();
        delete node->get_value();
        volume_list->remove_node(node);
    }
}

DrbdVolume* VolumesContainer::get_volume(uint16_t volume_nr)
{
    DrbdVolume* volume = volume_list->get(&volume_nr);
    return volume;
}

VolumesContainer::VolumesIterator VolumesContainer::volumes_iterator()
{
    return VolumesIterator(*this);
}
