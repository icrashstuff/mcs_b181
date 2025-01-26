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
#include "lightmap.h"
#include "texture_terrain.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <vector>

struct level_t
{
    level_t(texture_terrain_t* terrain = NULL);

    ~level_t();

    std::vector<chunk_cubic_t*> chunks;

    /** Items should probably be removed from terrain **/

    lightmap_t lightmap;

    /* TODO: Use this for sky calculations */
    int world_height = 0;

    GLuint ebo = 0;

    /**
     * Builds all dirt meshes
     * TODO: Throttle rendering based on position
     * TODO: Wait for either a timeout to pass or all surrounding chunks to be loaded before building
     * TODO: Rebuild clean meshes that surround dirty ones
     */
    void build_dirty_meshes();

    /**
     * Clears chunks meshes without freeing GL resources
     */
    void rebuild_meshes();

    /**
     * Clears all meshes
     *
     * @param free_gl Optionally free all OpenGL resources
     */
    void clear_mesh(bool free_gl);

    /**
     * Switchs terrain texture for level
     *
     * NOTE: This will clear all the meshes because the atlas might be structurally different
     */
    void set_terrain(texture_terrain_t* _terrain)
    {
        terrain = _terrain;
        clear_mesh(false);
    }

    inline texture_terrain_t* get_terrain() { return terrain; }

    /**
     * Sets the block at the provided position and if necessary marks adjacent chunks for rebuilding/relighting
     */
    void set_block(glm::ivec3 pos, block_id_t type, Uint8 metadata);

    /**
     * Gets the block data at the provided position
     *
     * @returns false on block not found, true on block found
     */
    bool get_block(glm::ivec3 pos, block_id_t& type, Uint8& metadata);

    /**
     * Clears all chunks
     */
    void clear();

    // TODO
    void render();

    glm::vec3 camera_pos = { 0, 0, 0 };

private:
    struct ivec3_comparator_t
    {
        bool operator()(const glm::ivec3& a, const glm::ivec3& b)
        {
            if (a.x < b.x)
                return true;
            if (a.x > b.x)
                return false;

            if (a.y < b.y)
                return true;
            if (a.y > b.y)
                return false;

            return a.z < b.z;
        }
    };

    /** For quick retrieval of chunks */
    std::map<glm::ivec3, chunk_cubic_t*, ivec3_comparator_t> cmap;

    /**
     * Build and or replace the mesh for the corresponding chunk
     */
    void build_mesh(int chunk_x, int chunk_y, int chunk_z);

    /**
     * Conduct a lighting pass on corresponding chunk
     *
     * 3 Light passes are necessary for light to correctly jump chunks
     *
     * TODO: Handle light transmission in water/other translucent blocks
     */
    void light_pass(int chunk_x, int chunk_y, int chunk_z, bool local_only);

    texture_terrain_t* terrain;
};

#endif
