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
#include "entity.h"
#include "lightmap.h"
#include "shaders.h"
#include "shared/inventory.h"
#include "texture_terrain.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <vector>

/**
 * Handles both level data and the rendering of said level data
 */
struct level_t
{
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

    level_t(texture_terrain_t* const terrain = NULL);

    ~level_t();

    /**
     * Returns the chunk vector for iteration
     */
    inline const std::vector<chunk_cubic_t*>& get_chunk_vec() { return chunks; }

    /**
     * Returns the chunk vector for iteration
     */
    inline const std::map<glm::ivec3, chunk_cubic_t*, ivec3_comparator_t>& get_chunk_map() { return cmap; }

    /**
     * Removes chunk from chunk vector and map, and then deletes it
     */
    void remove_chunk(const glm::ivec3 pos);

    /**
     * Adds chunk to chunk vector and map
     */
    void add_chunk(chunk_cubic_t* const c);

    std::map<eid_t, entity_base_t*> entities;

    /**
     * Get the entity with the corresponding EID
     *
     * @param eid Id of entity to retreive
     * @returns The entity with the corresponding EID or NULL if it wasn't found
     */
    inline entity_base_t* get_ent(const eid_t eid)
    {
        const std::map<eid_t, entity_base_t*>::iterator it = entities.find(eid);
        return (it == entities.end()) ? NULL : it->second;
    }

    /**
     * Get or create the entity with the corresponding EID
     *
     * @param eid Id of entity to retrieve/create
     * @returns The entity found or created with the corresponding EID
     */
    inline entity_base_t* get_or_create_ent(const eid_t eid)
    {
        std::map<eid_t, entity_base_t*>::iterator it = entities.find(eid);
        if (it == entities.end())
            it = entities.insert(it, std::make_pair(eid, new entity_base_t()));
        return it->second;
    }

    lightmap_t lightmap;

    /* TODO: Use this for sky calculations */
    int world_height = 0;

    GLuint ebo = 0;

    GLuint ent_missing_vao = 0;
    GLuint ent_missing_vbo = 0;
    size_t ent_missing_vert_count = 0;

    shader_t* shader_terrain = NULL;

    /**
     * Clears all meshes
     *
     * @param free_gl Optionally free all OpenGL resources
     */
    void clear_mesh(const bool free_gl);

    /**
     * Switchs terrain texture for level
     *
     * NOTE: This will clear all the meshes because the atlas might be structurally different
     */
    void set_terrain(texture_terrain_t* const _terrain);

    inline texture_terrain_t* get_terrain() { return terrain; }

    /**
     * Sets the block at the provided position and if necessary marks adjacent chunks for rebuilding/relighting
     */
    void set_block(const glm::ivec3 pos, const block_id_t type, Uint8 metadata);

    /**
     * Sets the block at the provided position and if necessary marks adjacent chunks for rebuilding/relighting
     */
    inline void set_block(const glm::ivec3 pos, const itemstack_t item) { set_block(pos, item.id, item.damage); }

    /**
     * Gets the block data at the provided position
     *
     * @returns false on block not found, true on block found
     */
    bool get_block(const glm::ivec3 pos, block_id_t& type, Uint8& metadata);

    /**
     * Gets the block data at the provided position
     *
     * @returns false on block not found, true on block found
     */
    inline bool get_block(const glm::ivec3 pos, itemstack_t& item)
    {
        block_id_t type;
        Uint8 metadata;
        if (!get_block(pos, type, metadata))
            return false;
        item.id = type;
        item.damage = metadata;
        return true;
    }

    /**
     * Clears all chunks
     */
    void clear();

    /**
     * Renders the world and entities
     *
     * @param win_size Window size (used for projection matrix)
     */
    void render(const glm::ivec2 win_size);

    glm::vec3 camera_pos = { 0, 0, 0 };
    glm::vec3 camera_direction = { 1, 0, 0 };
    glm::vec3 camera_right = { 1, 0, 0 };
    glm::vec3 camera_up = { 0, 1, 0 };
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fov = 70.0f;

    inventory_player_t inventory;

private:
    /**
     * Renders all entities
     *
     * This should be placed in-between the solid and translucent rendering passes
     */
    void render_entities();

    /** For quick retrieval of chunks */
    std::map<glm::ivec3, chunk_cubic_t*, ivec3_comparator_t> cmap;

    /* For sorted chunks */
    std::vector<chunk_cubic_t*> chunks;

    /**
     * Runs the culling pass and builds all visible/near visible dirty meshes
     *
     * TODO: Honor visibility for lighting?
     * TODO: Throttle light?
     * TODO: Wait for either a timeout to pass or all surrounding chunks to be loaded before building
     * TODO: Rebuild clean meshes that surround dirty ones
     */
    void build_dirty_meshes();

    /**
     * Culls all chunks that are behind the camera or outside of the render distance
     *
     * TODO: An equivalent for entities
     */
    void cull_chunks(const glm::ivec2 win_size, const int render_distance);

    /**
     * Build and or replace the mesh for the corresponding chunk
     */
    void build_mesh(const int chunk_x, const int chunk_y, const int chunk_z);

    /**
     * Conduct a lighting pass on corresponding chunk
     *
     * 3 Light passes are necessary for light to correctly jump chunks
     *
     * TODO: Handle light transmission in water/other translucent blocks
     */
    void light_pass(const int chunk_x, const int chunk_y, const int chunk_z, const bool local_only);

    /** Items should probably be removed from terrain **/
    texture_terrain_t* terrain;
};

#endif
