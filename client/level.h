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

#ifndef TETRA_CLIENT_LEVEL_H_INCLUDED
#define TETRA_CLIENT_LEVEL_H_INCLUDED

#include "chunk_cubic.h"
#include "texture_terrain.h"
#include <vector>

struct level_t
{
    level_t() { }

    ~level_t()
    {
        for (size_t i = 0; i < chunks.size(); i++)
            delete chunks[i];
        delete terrain;
    }

    std::vector<chunk_cubic_t*> chunks;

    /** Items should probably be removed from terrain **/
    texture_terrain_t* terrain;

    /**
     * Builds all dirt meshes
     * TODO: Rebuild clean meshes that surround dirty ones
     */
    void build_dirty_meshes();

    /**
     * Clears chunks (if any), optionally rebuilds the terrain atlas, and then builds all the chunks
     */
    void rebuild_meshes(bool rebuild_terrain);

    /**
     * Clears all meshes
     */
    void clear_mesh();

    /**
     * Clears all chunks
     */
    void clear();

    // TODO
    void render();

    void build_mesh(int chunk_x, int chunk_y, int chunk_z);
};

#endif
