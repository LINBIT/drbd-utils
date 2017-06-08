/**
 * Binary search in a sorted array
 *
 * @version 2016-04-11_001
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
#ifndef BSEARCH_H
#define	BSEARCH_H

#include <cstddef>

template<typename T>
class BSearch
{
  public:
    typedef int (*compare_func)(const T* key, const T* other);

    static const size_t NPOS = ~(static_cast<size_t> (0));

    BSearch(compare_func compare):
        compare_ptr(compare)
    {
    }

    BSearch(const BSearch& orig) = default;
    BSearch& operator=(const BSearch& orig) = default;
    BSearch(BSearch&& orig) = default;
    BSearch& operator=(BSearch&& orig) = default;

    virtual ~BSearch() noexcept
    {
    }

    size_t find(
        T*           array,
        size_t       array_length,
        T*           value
    )
    {
        return BSearch::find(array, array_length, value, compare_ptr);
    }

    static size_t find(
        T*           array,
        size_t       array_length,
        T*           value,
        compare_func compare
    )
    {
        size_t result      {NPOS};
        size_t start_index {0};
        size_t end_index   {array_length};

        size_t width {array_length};
        while (width > 0)
        {
            size_t mid_index = start_index + (width / 2);
            int direction = compare(&(array[mid_index]), value);
            if (direction < 0)
            {
                start_index = mid_index + 1;
            }
            else
            if (direction > 0)
            {
                end_index = mid_index;
            }
            else
            {
                result = mid_index;
                break;
            }
            width = end_index - start_index;
        }

        return result;
    }

  private:
    compare_func compare_ptr;
};

#endif	/* BSEARCH_H */
