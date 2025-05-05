/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Ian Hangartner <icrashstuff at outlook dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "lang.h"

#include "shared/misc.h"
#include "tetra/util/physfs/physfs.h"

static bool read_line(PHYSFS_File* const fd, std::string& result)
{
    if (!fd)
        return false;

    result.clear();

    char buf;
    int read;
    while ((read = PHYSFS_readBytes(fd, &buf, 1)))
    {
        switch (buf)
        {
        case '\r':
            continue;
        case '\n':
            return true;
        default:
            result += buf;
            break;
        }
    }

    return false;
}

static bool read_entry(PHYSFS_File* const fd, std::string& id, std::string& string)
{
    if (!fd)
        return false;

    std::string line;
    if (!read_line(fd, line))
        return false;

    size_t equals_pos = line.find_first_of("=");
    if (!equals_pos)
        return false;

    id = line.substr(0, equals_pos);
    string = line.substr(equals_pos + 1);

    return true;
}

translation_map_t::translation_map_t() { }

translation_map_t::translation_map_t(const std::string& path)
{
    PHYSFS_File* fd = PHYSFS_openRead(path.c_str());
    if (!fd)
    {
        dc_log_error("Unable to open \"%s\" for reading!", path.c_str());
        return;
    }

    while (!PHYSFS_eof(fd))
    {
        std::string id, string;
        if (read_entry(fd, id, string))
            _map[id] = string;
    }

    PHYSFS_close(fd);
}

void translation_map_t::import_keys(const translation_map_t& m, const bool overwrite)
{
    for (auto it : m._map)
        if (overwrite || _map.find(it.first) == _map.end())
            _map[it.first] = it.second;
}

void translation_map_t::add_key(const std::string& id, const std::string& string, const bool overwrite)
{
    auto it = _map.find(id);

    if (!overwrite && it != _map.end())
        return;

    _map[id] = string;
}
