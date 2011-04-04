/* Copyright 2011 ZAO "Begun".
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __POOL_STACK_
#define __POOL_STACK_

namespace pool_ns {

template <typename _DATA>
class stack
{
private:
    _DATA * buffer;
    size_t reserved;
    size_t current;

public:
    stack(size_t reserve = 1024) : buffer(new _DATA[reserve]), reserved(reserve), current(-1){}
    ~stack(){delete[] buffer;}

    void push(_DATA l)
    {
        if(current + 1 >= reserved)
        {
            reserve(2 * reserved);
        }

        buffer[++current] = l;
    }

    void reserve(size_t sz)
    {
        _DATA * _t = new _DATA[sz];

        size_t to_wr = size() < sz ? size() : sz;

        if(to_wr)
        {
            memcpy(_t, buffer, to_wr * sizeof(_DATA));
        }

        reserved  = sz;

        delete[] buffer;

        buffer = _t;
    }

    _DATA pop()
    {
        // FIXME warning current is always >= 0 because unsigned
        // return current >= 0 ? buffer[current--] : buffer[0];
        return buffer[current--];
    }

    size_t size()const
    {
        return current + 1;
    }

    size_t capacity()const
    {
        return reserved;
    }

    bool empty()const
    {
        return size() == 0;
    }

    void erase()
    {
        current = 0;
    }

    _DATA * data()
    {
        return buffer;
    }
};

}

#endif

