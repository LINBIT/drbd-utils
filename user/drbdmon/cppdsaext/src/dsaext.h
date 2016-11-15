/**
 * Data structures and algorithms extensions
 *
 * @version 2016-03-21_001
 * @author  Robert Altnoeder (r.altnoeder@gmx.net)
 *
 * Copyright (C) 2012 - 2016 Robert ALTNOEDER
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided that
 * the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
            K* key;
            V* value;
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
        virtual void insert(K* key, V* value) = 0;
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
        DuplicateInsertException();
        DuplicateInsertException(const DuplicateInsertException& orig) = default;
        DuplicateInsertException& operator=(const DuplicateInsertException& orig) = default;
        DuplicateInsertException(DuplicateInsertException&& orig) = default;
        DuplicateInsertException& operator=(DuplicateInsertException&& orig) = default;
        virtual ~DuplicateInsertException() noexcept;
    };
}

#endif	/* DSAEXT_H */
