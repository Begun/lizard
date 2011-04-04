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

#ifndef __LIZARD_MEM_CHUNK_HPP___
#define __LIZARD_MEM_CHUNK_HPP___

#include <errno.h>
#include <fcntl.h>
#include <lizard/config.hpp>
#include <lizard/utils.hpp>
#include <string.h>
#include <utils/logger.hpp>

namespace lizard
{

//---------------------------------------------------------------------------------------

template<int data_size>
class mem_chunk
{
    static Logger& slogger;

    uint8_t page[data_size + 1];

    size_t  sz;
    size_t  current;
    bool    can_expand;

    mem_chunk<data_size> * next;

    void insert_page();

public:

    mem_chunk();
    ~mem_chunk();

    size_t page_size()const;
    const void * get_data()const;
    void * get_data();
    size_t get_data_size()const;
    size_t& marker();
    size_t get_total_data_size()const;
    mem_chunk<data_size> * get_next()const;

    bool set_expand(bool exp);

    void reset();

    size_t append_data(const void * data, size_t data_sz);

    bool write_to_fd(int fd, bool& can_write, bool& want_write, bool& wreof);
    /**
     * Читает данные из сокета. Дополняет текущую страницу до конца, и выходит.
     * Если в текущей странице нет места для записи, расширяет ее.
     * \param [in] fd сокет для чтения данных
     * \param [out] can_read выставляется в \b false, если выполняется одно из условий:
     *  - произошла ошибка чтения и это не EINTR
     *  - прочитаны не все запрошенные данные (а запрашивают до заполнения страницы)
     *  - если соединение закрыто (read вернул 0)
     * \param [out] want_read выставляется в \b false, если выполняется одно из условий:
     *  - произошла ошибка чтения EINTR
     *  - если нет места для записи, и нельзя его расширить
     *  - если прочитали столько, что места больше нет
     * \param [out] rdeof выставляется в true, если соединение закрыто (read вернул 0)
     * \return
     *  - \b false, если нет места в текущей странице, и нельзя ее расширить.
     *  - \b true во всех остальных случаях.
     */
    bool read_from_fd(int fd, bool& can_read, bool& want_read, bool& rdeof);

    void print();
};

template <int data_size>
Logger& mem_chunk<data_size>::slogger = getLog("lizard");

class mem_block
{
    static Logger& slogger;

    uint8_t *     page;
    size_t        page_capacity;
    size_t        page_sz;
    size_t        current;

public:

    explicit mem_block(size_t max_sz = 0);
    ~mem_block();

    size_t size()const;
    size_t capacity()const;
    const void * get_data()const;
    void * get_data();
    size_t& marker();

    void resize(size_t max_sz = 0);

    void reset();

    size_t append_data(const void * data, size_t data_sz);

    bool write_to_fd(int fd, bool& can_write, bool& want_write, bool& wreof);
    bool read_from_fd(int fd, bool& can_read, bool& want_read, bool& rdeof);

    void print();
};
//---------------------------------------------------------------------------------------
}

#include "mem_chunk.tcc"
//---------------------------------------------------------------------------------------

#endif
