/**
 * From https://github.com/raltnoeder/cppdsaext
 * Included under the GPLv2 license. See source project for other licensing options.
 *
 * Data structures and algorithms extensions
 *
 * @version 2018-05-16_001
 * @author  Robert Altnoeder (r.altnoeder@gmx.net)
 *
 * Copyright (C) 2012 - 2018 Robert ALTNOEDER
 */
#ifndef DSAEXT_H
#define	DSAEXT_H

#include <cstddef>
#include <stdexcept>

namespace dsaext
{
    template<typename K, typename V>
    class Map
    {
      public:
        typedef struct entry_s
        {
            const K* key;
            const V* value;
        }
        entry;

        class Node
        {
          public:
            virtual K* get_key() const = 0;
            virtual V* get_value() const = 0;

            virtual ~Node()
            {
            }
        };

        virtual ~Map()
        {
        }

        virtual V* get(const K* key) const = 0;
        virtual void insert(const K* key, const V* value) = 0;
        virtual void remove(const K* key) = 0;
        virtual void clear() = 0;
        virtual size_t get_size() const = 0;
    };

    template<typename T>
    class QIterator
    {
      public:
        virtual ~QIterator()
        {
        }
        virtual T* next() = 0;
        virtual bool has_next() const = 0;
        virtual size_t get_size() const = 0;
    };

    class DuplicateInsertException : public std::exception
    {
      public:
        DuplicateInsertException() = default;
        DuplicateInsertException(const DuplicateInsertException& orig) = default;
        DuplicateInsertException& operator=(const DuplicateInsertException& orig) = default;
        DuplicateInsertException(DuplicateInsertException&& orig) = default;
        DuplicateInsertException& operator=(DuplicateInsertException&& orig) = default;
        virtual ~DuplicateInsertException() noexcept;
    };

    class NumberFormatException : public std::exception
    {
      public:
        NumberFormatException() = default;
        NumberFormatException(const NumberFormatException& orig) = default;
        NumberFormatException& operator=(const NumberFormatException& orig) = default;
        NumberFormatException(NumberFormatException&& orig) = default;
        NumberFormatException& operator=(NumberFormatException&& orig) = default;
        virtual ~NumberFormatException() noexcept;
    };

    template<typename T>
    int generic_compare(const T* const key_ptr, const T* const other_ptr)
    {
        int result = 0;
        if (*key_ptr < *other_ptr)
        {
            result = -1;
        }
        else
        if (*key_ptr > *other_ptr)
        {
            result = 1;
        }
        return result;
    }

    template<typename T>
    const T& generic_bounds(const T& min_value, const T& value, const T& max_value)
    {
        const T* result = &value;
        if (*result < min_value)
        {
            result = &min_value;
        }
        else
        if (*result > max_value)
        {
            result = &max_value;
        }
        return *result;
    }
}

#endif	/* DSAEXT_H */
