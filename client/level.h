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
#include "entity/entity.h"
#include "lightmap.h"
#include "shaders/shaders.h"
#include "shared/ids.h"
#include "shared/inventory.h"
#include "sound/sound_world.h"
#include "texture_terrain.h"
#include "time_blended_modifer.h"
#include <GL/glew.h>
#include <entt/entt.hpp>
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
        bool operator()(const glm::ivec3& a, const glm::ivec3& b) const
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
    inline const std::vector<chunk_cubic_t*>& get_chunk_vec() { return chunks_render_order; }

    /**
     * Returns the chunk vector for iteration
     */
    inline const std::map<glm::ivec3, chunk_cubic_t*, ivec3_comparator_t>* get_chunk_map() const { return &cmap; }

    /**
     * Removes chunk from chunk vector and map, and then deletes it
     */
    void remove_chunk(const glm::ivec3 pos);

    /**
     * Adds chunk to chunk vector and map
     */
    void add_chunk(chunk_cubic_t* const c);

    /**
     * Get chunk
     */
    inline chunk_cubic_t* get_chunk(const glm::ivec3 pos)
    {
        decltype(cmap)::iterator it = cmap.find(pos);
        return (it == cmap.end()) ? NULL : it->second;
    }

    entt::registry ecs;

    typedef decltype(ecs)::entity_type ent_id_t;

    ent_id_t player_eid;

    lightmap_t lightmap;

    /* TODO: Use this for sky calculations */
    int world_height = 0;

    inline mc_id::gamemode_t gamemode_get() { return gamemode; }

    /**
     * Set gamemode
     *
     * @returns 1 on valid gamemode, 0 on failure
     */
    bool gamemode_set(int x);

    inline mc_id::dimension_t dimension_get() const { return dimension; }

    enum dimension_switch_result
    {
        DIM_SWITCH_SUCCESSFUL = 0,
        DIM_SWITCH_INVALID_DIM = 1,
        DIM_SWITCH_ALREADY_IN_USE = 2,
    };

    /**
     * Switch dimension
     *
     * If x != dimension, then this will clear all entities and chunks, and change the light map
     *
     * @param dim Dimension to switch to
     *
     * @returns 0 on success, 1 on invalid dimension, or 2 on dimension is already used
     */
    dimension_switch_result dimension_switch(const int dim);

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
     *
     * @param pos Position of block in block coordinates
     * @param type Type for the block to set
     * @param metadata Metadata for the block to set
     */
    inline void set_block(const glm::ivec3 pos, const block_id_t type, Uint8 metadata) { set_block(pos, { .id = type, .damage = metadata }); }

    /**
     * Sets the block at the provided position and if necessary marks adjacent chunks for rebuilding/relighting
     *
     * @param pos Position of block in block coordinates
     * @param block Block data
     */
    inline void set_block(const glm::ivec3 pos, const itemstack_t block)
    {
        chunk_cubic_t* c = NULL;
        set_block(pos, block, c);
    }

    /**
     * Sets the block at the provided position and if necessary marks adjacent chunks for rebuilding/relighting,
     * while caching the origin chunk for nearby future calls
     *
     * @param pos Position of block in block coordinates
     * @param block Block data
     * @param cache Pointer to last used chunk (NOTE: Pointer validity is only guaranteed in the absence of level_t::remove_chunk() calls)
     */
    void set_block(const glm::ivec3 pos, const itemstack_t block, chunk_cubic_t*& cache);

    /**
     * Gets the block data at the provided position
     *
     * @param pos Position of block in block coordinates
     * @param type Output for block id
     * @param metadata Output for block metadata
     *
     * @returns false on block not found, true on block found
     */
    bool get_block(const glm::ivec3 pos, block_id_t& type, Uint8& metadata);

    /**
     * Gets the block data at the provided position
     *
     * @param pos Position of block in block coordinates
     * @param item Output for block data
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
     * Gets the block data at the provided position, while caching the origin chunk for nearby future calls
     *
     * @param pos Position of block in block coordinates
     * @param item Output for block data
     * @param cache Pointer to last searched chunk (NOTE: Pointer validity is only guaranteed in the absence of level_t::remove_chunk() calls)
     *
     * @returns false if containing chunk not found, true otherwise
     */
    bool get_block(const glm::ivec3 pos, itemstack_t& item, chunk_cubic_t*& cache);

    /**
     * Get biome id at position
     *
     * @param pos Position in block coordinates
     */
    mc_id::biome_t get_biome_at(const glm::ivec3 pos);

    /**
     * Clears all chunks and entities
     */
    void clear();

    /**
     * Ticks all entities as many times as need
     */
    void tick();

    inline mc_tick_t get_last_tick() { return last_tick; };

    /**
     * Renders the world and entities
     *
     * @param win_size Window size (used for projection matrix)
     * @param delta_time Time since last frame in seconds
     */
    void render(const glm::ivec2 win_size, const float delta_time);

    inline glm::vec3 get_camera_pos() const { return foot_pos + glm::vec3(0, camera_offset_height, 0) + camera_direction * camera_offset_radius; }

    glm::vec3 foot_pos = { 0, 0, 0 };
    glm::vec3 camera_direction = { 1, 0, 0 };
    glm::vec3 camera_right = { 1, 0, 0 };
    glm::vec3 camera_up = { 0, 1, 0 };
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fov = 70.0f;
    float camera_offset_height = 1.62;
    float camera_offset_radius = 0.05;

    bool enable_timer_log_light = 0;
    bool enable_timer_log_mesh = 0;

    /* FOV increase while flying is 10%, Source: https://minecraft.wiki/w/Flying */
    time_blended_modifer_t modifier_fly = time_blended_modifer_t(75, 1.0f, 1.1f, false);
    /* I couldn't find anything definitive on the FOV increase while sprinting, so we'll just reuse the percentage from flying - Ian */
    time_blended_modifer_t modifier_sprint = time_blended_modifer_t(75, 1.0f, 1.1f, false);

    /**
     * If the value is greater than zero, then it will be used instead of r_render_distance
     */
    int render_distance_override = 0;

    struct performance_timer_t
    {
        Uint64 duration = 0;
        size_t built = 0;

        void operator+=(const performance_timer_t& rh) { duration += rh.duration, built += rh.built; }
    };

    performance_timer_t last_perf_light_pass1;
    performance_timer_t last_perf_light_pass2;
    performance_timer_t last_perf_light_pass3;
    performance_timer_t last_perf_light_pass4;
    performance_timer_t last_perf_mesh_pass;

    inventory_player_t inventory;

    Sint64 mc_seed = 0;
    long mc_time = 0;

    /** Used for the ambience sounds (https://minecraft.wiki/w/Ambience#Mood_algorithm) */
    float mood = 0;

    /** When music >= 1.0f, music will begin playing */
    float music = 0.6f;

    float damage_tilt = 0.0f;

    sound_world_t sound_engine;

    sound_resources_t* sound_resources = nullptr;

private:
    mc_id::gamemode_t gamemode = mc_id::GAMEMODE_SPECTATOR;

    mc_id::dimension_t dimension = mc_id::DIMENSION_OVERWORLD;

    /**
     * Actual tick call
     */
    void tick_real();

    /**
     * Last tick processed, incremented *after* level_t::tick_real() is called
     */
    mc_tick_t last_tick;

    /**
     * Renders all entities
     *
     * This should be placed in-between the solid and translucent rendering passes
     */
    void render_entities();

    /** For quick retrieval of chunks */
    std::map<glm::ivec3, chunk_cubic_t*, ivec3_comparator_t> cmap;

    /* Render order chunks (sorted by distance to camera) */
    std::vector<chunk_cubic_t*> chunks_render_order;

    /**
     * Light order chunks (Arranged in descending strips of chunks with the same XY coordinates)
     */
    std::vector<chunk_cubic_t*> chunks_light_order;

    /** If set to true then level_t::chunks_render_order will be sorted, and then this bool will be cleared */
    bool request_render_order_sort = 1;

    /** If set to true then level_t::chunks_light_order will be sorted, and then this bool will be cleared */
    bool request_light_order_sort = 1;

    Uint64 last_render_order_sort_time = 0;
    Uint64 last_light_order_sort_time = 0;

    glm::i64vec3 last_render_order_cpos = { 0, 0, 0 };

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
     * 4 Light passes are necessary for light to correctly jump chunks
     */
    void light_pass(const int chunk_x, const int chunk_y, const int chunk_z, const bool local_only);

    /**
     * Conduct a lighting pass on corresponding chunk
     *
     * 4 Light passes are necessary for light to correctly jump chunks
     */
    void light_pass_sky(const int chunk_x, const int chunk_y, const int chunk_z, const bool local_only);

    /** Items should probably be removed from terrain **/
    texture_terrain_t* terrain;

    /** Cubiomes generator */
    void* generator = NULL;
    void generator_create();
    void generator_destroy();

    /**
     * Generates biome colors/climate values for a chunk with side strips for blending purposes
     *
     * @param chunk_pos Chunk position in chunk coordinates
     * @param colors Output for color values, index: [x+1][y+1]
     * @param temperature Output for temperature values, index: [x+1][y+1]
     * @param humidity Output for humidity values, index: [x+1][y+1]
     */
    void generate_climate_colors(const glm::ivec3 chunk_pos, glm::vec3 colors[18][18], float temperature[18][18], float humidity[18][18]);

    /**
     * Generates temperature and humidity values for a chunk with side strips for blending purposes
     *
     * @param chunk_pos Chunk position in chunk coordinates
     * @param temperature Output for temperature values, index: [x+1][y+1]
     * @param humidity Output for humidity values, index: [x+1][y+1]
     */
    void generate_climate_parameters(const glm::ivec3 chunk_pos, float temperature[18][18], float humidity[18][18]);

    /**
     * Generate biome ids with a certain amount of oversample
     *
     * @param level Level to pull information from
     * @param chunk_pos Chunk position in chunk coordinates
     * @param biome_ids Output vector for biome ids, index: (Z * (oversample * 2 + SUBCHUNK_SIZE_X) + X)
     * @param oversample Amount of blocks to oversample the ids by
     */
    void generate_biome_ids(const glm::ivec3 chunk_pos, std::vector<mc_id::biome_t>& biome_ids, const int oversample);
};

#endif
