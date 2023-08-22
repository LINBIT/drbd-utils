#ifndef VOLUMESCONTAINER_H
#define	VOLUMESCONTAINER_H

#include <default_types.h>
#include <new>
#include <memory>
#include <string>

#include <map_types.h>
// https://github.com/raltnoeder/cppdsaext
#include <dsaext.h>
#include <objects/DrbdVolume.h>

class VolumesContainer
{
  public:
    class VolumesIterator : public VolumesMap::ValuesIterator
    {
      public:
        VolumesIterator(VolumesContainer& con_ref) :
            VolumesMap::ValuesIterator::ValuesIterator(*(con_ref.volume_list))
        {
        }

        VolumesIterator(VolumesContainer& con_ref, VolumesMap::Node& start_node) :
            VolumesMap::ValuesIterator::ValuesIterator(*(con_ref.volume_list), start_node)
        {
        }

        VolumesIterator(const VolumesIterator& orig) = default;
        VolumesIterator& operator=(const VolumesIterator& orig) = default;
        VolumesIterator(VolumesIterator&& orig) = default;
        VolumesIterator& operator=(VolumesIterator&& orig) = default;

        virtual ~VolumesIterator() noexcept override
        {
        }
    };

    // @throws std::bad_alloc
    VolumesContainer();
    virtual ~VolumesContainer() noexcept;
    VolumesContainer(const VolumesContainer& orig) = delete;
    VolumesContainer& operator=(const VolumesContainer& orig) = delete;
    VolumesContainer(VolumesContainer&& orig) = delete;
    VolumesContainer& operator=(VolumesContainer&& orig) = delete;
    // @throws std::bad_alloc, dsaext::DuplicateInsertException
    virtual void add_volume(DrbdVolume* vol);
    virtual void remove_volume(uint16_t volume_nr);
    virtual DrbdVolume* get_volume(uint16_t volume_nr);
    virtual uint16_t get_volume_count() const;
    virtual VolumesIterator volumes_iterator();
    virtual VolumesIterator volumes_iterator(uint16_t volume_nr);

  private:
    const std::unique_ptr<VolumesMap> volume_list;
};

#endif	/* VOLUMESCONTAINER_H */
