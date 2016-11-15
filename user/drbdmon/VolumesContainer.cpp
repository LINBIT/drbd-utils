#include <VolumesContainer.h>
#include <comparators.h>

// @throws std::bad_alloc
VolumesContainer::VolumesContainer()
{
    volume_list = new VolumesMap(&comparators::compare_uint16);
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
    delete volume_list;
}

// @throws std::bad_alloc, dsaext::DuplicateInsertException
void VolumesContainer::add_volume(DrbdVolume* volume)
{
    uint16_t* volume_key {nullptr};
    try
    {
        volume_key = new uint16_t;
        *volume_key = volume->get_volume_nr();
        volume_list->insert(volume_key, volume);
    }
    catch (dsaext::DuplicateInsertException& dup_exc)
    {
        delete volume_key;
        throw dup_exc;
    }
    catch (std::bad_alloc& out_of_memory_exc)
    {
        delete volume_key;
        throw out_of_memory_exc;
    }
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
