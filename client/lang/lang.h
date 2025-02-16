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

#include <map>
#include <string>

class translation_map_t
{
public:
    /**
     * Construct an empty translation map
     */
    translation_map_t();

    /**
     * Load a translation map from a PHYSFS path
     */
    translation_map_t(const std::string path);

    inline const char* get_translation(const char* translation_id)
    {
        auto it = _map.find(translation_id);
        return (it == _map.end()) ? translation_id : it->second.c_str();
    }

    const inline std::string get_name() { return get_translation("language.name"); }
    const inline std::string get_region() { return get_translation("language.region"); }
    const inline std::string get_code() { return get_translation("language.code"); }

    /**
     * Import keys from another map
     *
     * @param m Map to import from
     * @param overwrite Overwrite pre-existing keys
     */
    void import_keys(const translation_map_t& m, const bool overwrite = true);

    /**
     * Add key to map
     *
     * @param id Translation id of key
     * @param string Value of key
     * @param overwrite Overwrite pre-existing key
     */
    void add_key(const std::string id, const std::string string, const bool overwrite = true);

    std::map<std::string, std::string> _map;
};
