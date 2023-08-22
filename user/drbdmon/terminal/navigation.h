#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <default_types.h>
#include <functional>

namespace navigation
{
    // Advances the iterator to the first item on the selected page, or if that page is empty, instead to
    // the first item on the last page that contains any items
    template<typename I>
    void find_first_item_on_page(
        const uint32_t  page_nr,
        const uint32_t  line_offset,
        const uint32_t  lines_per_page,
        I&              item_iter
    )
    {
        const size_t item_count = item_iter.get_size();
        uint32_t search_item_idx = ((std::max(page_nr, static_cast<uint32_t> (1)) - 1) * lines_per_page) +
            line_offset;
        if (search_item_idx >= item_count)
        {
            // Selected page is past the last page that shows the item
            // Select the first item on the last page that shows any items instead
            search_item_idx = static_cast<uint32_t> (
                ((std::max(item_count, static_cast<size_t> (1)) - 1) / lines_per_page) * lines_per_page
            );
        }
        uint32_t item_idx = 0;
        while (item_iter.has_next() && item_idx < search_item_idx)
        {
            item_iter.next();
            ++item_idx;
        }
    }

    template<typename I, typename K>
    I* find_first_item_on_cursor_page(
        const K&                        cursor_key,
        std::function<const K&(I*)>&    key_func,
        dsaext::QIterator<I>&           item_iter,
        const uint32_t                  lines_per_page,
        uint32_t&                       item_idx
    )
    {
        uint32_t idx = 0;

        I* page_first_item = nullptr;
        uint32_t page_line_ctr = 0;
        while (item_iter.has_next())
        {
            I* const item = item_iter.next();
            const K& item_key = key_func(item);
            if (page_line_ctr == 0)
            {
                page_first_item = item;
            }
            if (item_key >= cursor_key)
            {
                break;
            }
            ++idx;
            ++page_line_ctr;
            if (page_line_ctr >= lines_per_page)
            {
                page_line_ctr = 0;
            }
        }

        item_idx = idx;

        return page_first_item;
    }

    // Returns a pointer to the first item, selected by the filter function, on the selected page,
    // or if that page is empty, instead to the first item on the last page that contains any items.
    // Returns nullptr if there are no items.
    template<typename T>
    T* find_first_filtered_item_on_page(
        const uint32_t              page_nr,
        const uint32_t              line_offset,
        const uint32_t              lines_per_page,
        dsaext::QIterator<T>&       item_iter,
        std::function<bool(T*)>&    filter_func,
        uint32_t&                   filtered_item_count
    )
    {
        T* search_item = nullptr;
        T* last_page_first_item = nullptr;
        const uint32_t search_item_idx = ((std::max(page_nr, static_cast<uint32_t> (1)) - 1) * lines_per_page) +
            line_offset;
        uint32_t item_idx = 0;
        uint32_t item_counter = 0;
        uint32_t page_line = 0;
        while (item_iter.has_next())
        {
            T* cur_item = item_iter.next();
            if (filter_func(cur_item))
            {
                ++item_counter;
                if (page_line == 0)
                {
                    last_page_first_item = cur_item;
                }

                if (item_idx == search_item_idx)
                {
                    search_item = cur_item;
                    break;
                }

                ++item_idx;
                ++page_line;
                if (page_line >= lines_per_page)
                {
                    page_line = 0;
                }
            }
        }
        while (item_iter.has_next())
        {
            T* cur_item = item_iter.next();
            if (filter_func(cur_item))
            {
                ++item_counter;
            }
        }

        if (item_idx < search_item_idx)
        {
            search_item = last_page_first_item;
        }

        filtered_item_count = item_counter;

        return search_item;
    }

    template<typename I, typename K>
    I* find_first_filtered_item_on_cursor_page(
        const K&                    cursor_key,
        std::function<const K&(I*)> key_func,
        dsaext::QIterator<I>&       item_iter,
        std::function<bool(I*)>&    filter_func,
        const uint32_t              lines_per_page,
        uint32_t&                   item_idx,
        uint32_t&                   item_count
    )
    {
        uint32_t idx = 0;
        uint32_t count = 0;

        I* page_first_item = nullptr;
        uint32_t page_line_ctr = 0;
        while (item_iter.has_next())
        {
            I* const item = item_iter.next();
            if (filter_func(item))
            {
                ++count;
                ++idx;
                const K& item_key = key_func(item);
                if (page_line_ctr == 0)
                {
                    page_first_item = item;
                }
                if (item_key >= cursor_key)
                {
                    break;
                }
                ++page_line_ctr;
                if (page_line_ctr >= lines_per_page)
                {
                    page_line_ctr = 0;
                }
            }
        }
        while (item_iter.has_next())
        {
            I* const item = item_iter.next();
            if (filter_func(item))
            {
                ++count;
            }
        }

        item_idx = idx;
        item_count = count;

        return page_first_item;
    }

    // Selects the item closest to the cursor if it exists.
    // The selection oder is:
    // 1. the item selected by the cursor
    // 2. its successor item
    // 3. its predecessor item
    // 4. nullptr, since there are zero items
    template<template<typename, typename> class M, typename K, typename V>
    V* find_item_near_cursor(M<K, V>& map, const K* const key)
    {
        V* search_item = map.get_ceiling_value(key);
        if (search_item == nullptr)
        {
            // Item name under cursor not in map and no successor item
            // Select predecessor item
            search_item = map.get_floor_value(key);
        }
        return search_item;
    }
}

#endif /* NAVIGATION_H */
