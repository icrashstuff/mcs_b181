/* SPDX-License-Identifier: 0BSD
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Ian Hangartner <icrashstuff at outlook dot com>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef SHARED_BUILD_INFO_H
#define SHARED_BUILD_INFO_H

#include <string>

namespace build_info
{

extern const char* build_mode;

namespace ver_string
{
    extern std::string client();
    extern std::string server();
    extern std::string bridge();
    extern std::string shared();
}

namespace git
{
    extern const char* refspec;

    namespace hash
    {
        /** Last commit, period */
        extern const char* root;

        /** Last commit touching "client/" */
        extern const char* client_only;

        /** Last commit touching "server/" */
        extern const char* server_only;

        /** Last commit touching "bridge/" */
        extern const char* bridge_only;

        /** Last commit touching "shared/" */
        extern const char* shared_only;
    }

    namespace dirty
    {
        /** Dirty flag for entire repository */
        extern const bool root;

        /** Dirty flag for "client/" */
        extern const bool client_only;

        /** Dirty flag for "server/" */
        extern const bool server_only;

        /** Dirty flag for "bridge/" */
        extern const bool bridge_only;

        /** Dirty flag for "shared/" */
        extern const bool shared_only;
    }
} /* namespace git */
}

#endif
