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
#include "game.h"

#include "level.h"
#include "migration_gl.h"

#include <algorithm>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include "tetra/util/convar.h"
#include "tetra/util/stb_sprintf.h"
#include <SDL3/SDL.h>

#include "state.h"

#define PASS_TIMER_START()             \
    do                                 \
    {                                  \
        built = 0;                     \
        tick_start = SDL_GetTicksNS(); \
    } while (0)
#define PASS_TIMER_STOP(CONDITION, fmt, ...)     \
    do                                           \
    {                                            \
        elapsed = SDL_GetTicksNS() - tick_start; \
        if ((CONDITION) && built)                \
            dc_log(fmt, ##__VA_ARGS__);          \
    } while (0)

static convar_int_t r_mesh_throttle("r_mesh_throttle", 1, 1, 64, "Maximum number of chunks that can be meshed per frame", CONVAR_FLAG_SAVE);
static convar_int_t r_render_distance("r_render_distance", 8, 1, 64, "Maximum chunk distance that can be viewed at once", CONVAR_FLAG_SAVE);

void level_t::clear_mesh(const bool free_gl)
{
    for (chunk_cubic_t* c : chunks_render_order)
    {
        if (free_gl)
            c->free_renderer_resources();
        if (c->dirty_level < chunk_cubic_t::DIRTY_LEVEL_MESH)
            c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_MESH;
    }
}

/* Ideally this would use the Gribb/Hartmann method to extract the planes from a projection/camera matrix to get the plane normals */
void level_t::cull_chunks(const glm::ivec2 win_size, const int render_distance)
{
    update_chunk_renderer_hints();
    camera_direction.x = SDL_cosf(glm::radians(yaw)) * SDL_cosf(glm::radians(pitch));
    camera_direction.y = SDL_sinf(glm::radians(pitch));
    camera_direction.z = SDL_sinf(glm::radians(yaw)) * SDL_cosf(glm::radians(pitch));
    camera_direction = glm::normalize(camera_direction);

    const float half_v = tanf(glm::radians(fov) * .5f);
    const float half_h = half_v * float(win_size.x) / float(win_size.y);
    const glm::vec3 cam_right = glm::normalize(glm::cross(glm::vec3(0.0f, -1.0f, 0.0f), camera_direction));
    const glm::vec3 cam_up = glm::normalize(glm::cross(camera_direction, cam_right));

    glm::vec3 fov_normals[4];

    /* Horizontal Normals */
    fov_normals[0] = glm::normalize(glm::cross(-camera_direction + cam_right * half_h, cam_up));
    fov_normals[1] = glm::normalize(glm::cross(+camera_direction + cam_right * half_h, cam_up));

    /* Vertical Normals */
    fov_normals[2] = glm::normalize(glm::cross(cam_right, -camera_direction + cam_up * half_v));
    fov_normals[3] = glm::normalize(glm::cross(cam_right, +camera_direction + cam_up * half_v));

    /* Convert camera position to 1/2 chunk coords and translate it to center chunk positions */
    const glm::ivec3 camera_half_chunk_pos = (glm::ivec3(get_camera_pos()) >> 3) - glm::ivec3(1, 1, 1);
    const float render_distance_half_chunk = render_distance * 2.0f;
    const float min_dist = -4.0f;

#define CULL_REJECT_IF(condition) \
    if (condition)                \
    {                             \
        c->visible = 0;           \
        continue;                 \
    }
    for (chunk_cubic_t* c : chunks_render_order)
    {
        CULL_REJECT_IF(c->renderer_hints.uniform_air);

        const glm::vec3 chunk_center = glm::vec3((c->pos << 1) - camera_half_chunk_pos);

        /* Beyond render distance culling */
        CULL_REJECT_IF(glm::length(glm::vec2(chunk_center.x, chunk_center.z)) > render_distance_half_chunk);

        /* Behind the camera culling */
        CULL_REJECT_IF(glm::dot(camera_direction, chunk_center) < min_dist);

        /* FOV Culling */
        CULL_REJECT_IF(glm::dot(fov_normals[0], chunk_center) < min_dist);
        CULL_REJECT_IF(glm::dot(fov_normals[1], chunk_center) < min_dist);
        CULL_REJECT_IF(glm::dot(fov_normals[2], chunk_center) < min_dist);
        CULL_REJECT_IF(glm::dot(fov_normals[3], chunk_center) < min_dist);
        c->visible = 1;
    }
#undef CULL_REJECT_IF
}

void level_t::update_chunk_renderer_hints()
{
    for (chunk_cubic_t* c : chunks_light_order)
    {
        if (c->renderer_hints.hints_set)
            continue;
        c->update_renderer_hints();
    }
}

void level_t::build_dirty_meshes()
{
    update_chunk_renderer_hints();

    size_t built;
    Uint64 elapsed;
    Uint64 tick_start;
    const Uint64 tick_func_start_ms = SDL_GetTicks();

    if (request_light_order_sort || tick_func_start_ms - last_light_order_sort_time > 5000)
    {
        std::sort(chunks_light_order.begin(), chunks_light_order.end(), [](const chunk_cubic_t* const a, const chunk_cubic_t* const b) {
            if (a->pos.x > b->pos.x)
                return true;
            if (a->pos.x < b->pos.x)
                return false;
            if (a->pos.z > b->pos.z)
                return true;
            if (a->pos.z < b->pos.z)
                return false;
            return a->pos.y > b->pos.y;
        });
        last_light_order_sort_time = SDL_GetTicks();
        request_light_order_sort = 0;
    }

    /* Dirty level propagation pass (Backwards and forwards, twice just to be sure) */
    PASS_TIMER_START();
    for (int i = chunks_light_order.size() * 2 - 1, vec_size = chunks_light_order.size(), i_min = -vec_size * 2; i > i_min; i--)
    {
        chunk_cubic_t* c = chunks_light_order[abs(i) % vec_size];
        if (c->dirty_level <= chunk_cubic_t::DIRTY_LEVEL_MESH)
            continue;

        chunk_cubic_t::dirty_level_t adj_dirt_level = chunk_cubic_t::dirty_level_t(c->dirty_level - 1);

        assert(adj_dirt_level != chunk_cubic_t::DIRTY_LEVEL_NONE);

#define ASSIGN_DIRT_LVL_IF(WHO, LVL, COND)             \
    if ((WHO) && (COND) && (WHO)->dirty_level < (LVL)) \
    (WHO)->dirty_level = (LVL)
        ASSIGN_DIRT_LVL_IF(c->neighbors.pos_x, adj_dirt_level, !c->renderer_hints.opaque_face_pos_x);
        ASSIGN_DIRT_LVL_IF(c->neighbors.pos_y, adj_dirt_level, !c->renderer_hints.opaque_face_pos_y);
        ASSIGN_DIRT_LVL_IF(c->neighbors.pos_z, adj_dirt_level, !c->renderer_hints.opaque_face_pos_z);
        ASSIGN_DIRT_LVL_IF(c->neighbors.neg_x, adj_dirt_level, !c->renderer_hints.opaque_face_neg_x);
        ASSIGN_DIRT_LVL_IF(c->neighbors.neg_y, c->dirty_level, !c->renderer_hints.opaque_face_neg_y);
        ASSIGN_DIRT_LVL_IF(c->neighbors.neg_z, adj_dirt_level, !c->renderer_hints.opaque_face_neg_z);
#undef ASSIGN_DIRT_LVL_IF

        built++;
    }
    PASS_TIMER_STOP(0, "Propagated dirty level for %zu chunks in %.2f ms (%.2f ms per)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);

    /* Clear Light Pass */
    PASS_TIMER_START();
    for (auto it = chunks_light_order.begin(); it != chunks_light_order.end(); it++)
    {
        chunk_cubic_t* c = *it;
        if (c->dirty_level != chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL)
            continue;
        c->clear_light_block(0);
        c->light_pass_block_setup();
        c->clear_light_sky(0);
        built++;
    }
    PASS_TIMER_STOP(enable_timer_log_light, "Cleared %zu chunks in %.2f ms (%.2f ms per) (Pass 1)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);

    /* First Light Pass */
    PASS_TIMER_START();
    for (auto it = chunks_light_order.begin(); it != chunks_light_order.end(); it++)
    {
        chunk_cubic_t* c = *it;
        if (c->dirty_level != chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL)
            continue;
        c->light_pass_block_grab_from_neighbors();
        c->light_pass_block_propagate_internals();
        c->light_pass_sky_grab_from_neighbors();
        c->light_pass_sky_propagate_internals();
        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_0;
        built++;
    }
    PASS_TIMER_STOP(enable_timer_log_light, "Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 1)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);
    last_perf_light_pass1.duration = elapsed;
    last_perf_light_pass1.built = built;

    /* Fast-forward cull pass */
    for (auto it = chunks_light_order.begin(); it != chunks_light_order.end(); it++)
    {
        chunk_cubic_t* c = *it;
        if (BETWEEN_INCL(c->dirty_level, chunk_cubic_t::DIRTY_LEVEL_MESH, chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_0))
        {
            bool light_can_leave = c->can_light_leave();
            if (c->renderer_hints.opaque_sides || !light_can_leave)
                c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_MESH;
            if (c->renderer_hints.uniform_opaque)
            {
                c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_MESH;

                if (!light_can_leave)
                {
                    c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_NONE;
                    c->free_renderer_resources();
                }
            }
        }
    }

    /* Second Light Pass */
    PASS_TIMER_START();
    for (auto it = chunks_light_order.begin(); it != chunks_light_order.end(); it++)
    {
        chunk_cubic_t* c = *it;
        if (c->dirty_level != chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_0)
            continue;
        c->light_pass_block_grab_from_neighbors();
        c->light_pass_block_propagate_internals();
        c->light_pass_sky_grab_from_neighbors();
        c->light_pass_sky_propagate_internals();
        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_1;
        built++;
    }
    PASS_TIMER_STOP(enable_timer_log_light, "Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 2)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);
    last_perf_light_pass2.duration = elapsed;
    last_perf_light_pass2.built = built;

    /* Third Light Pass */
    PASS_TIMER_START();
    for (auto it = chunks_light_order.begin(); it != chunks_light_order.end(); it++)
    {
        chunk_cubic_t* c = *it;
        if (c->dirty_level != chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_1)
            continue;
        c->light_pass_block_grab_from_neighbors();
        c->light_pass_block_propagate_internals();
        c->light_pass_sky_grab_from_neighbors();
        c->light_pass_sky_propagate_internals();
        if (c->renderer_hints.uniform_air)
            c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_NONE;
        else
            c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_MESH;
        built++;
    }
    PASS_TIMER_STOP(enable_timer_log_light, "Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 3)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);
    last_perf_light_pass3.duration = elapsed;
    last_perf_light_pass3.built = built;

    /* Mesh Pass */
    PASS_TIMER_START();
    int throttle = r_mesh_throttle.get();
    glm::ivec3 pos_cam(glm::ivec3(glm::round(get_camera_pos())) >> 4);
    for (auto it = chunks_render_order.rbegin(); it != chunks_render_order.rend(); it++)
    {
        chunk_cubic_t* c = *it;
        if (c->dirty_level != chunk_cubic_t::DIRTY_LEVEL_MESH || !c->visible)
            continue;
        /* Bypass mesh throttle for nearby chunks (To stop holes from being punched in the world) */
        if (throttle < 0 && (abs(c->pos.x - pos_cam.x) > 1 || abs(c->pos.y - pos_cam.y) > 1 || abs(c->pos.z - pos_cam.z) > 1))
            continue;
        build_mesh(c);
        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_NONE;
        built++;
        throttle--;
    }
    PASS_TIMER_STOP(enable_timer_log_mesh, "Built %zu meshes in %.2f ms (%.2f ms per)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);
    last_perf_mesh_pass.duration = elapsed;
    last_perf_mesh_pass.built = built;
}

void level_t::set_block(const glm::ivec3 pos, const itemstack_t block, chunk_cubic_t*& cache)
{
    const block_id_t type = block.id;
    const Uint8 metadata = block.damage;

    if (!mc_id::is_block(type))
    {
        TRACE("Preposterous id field %d:%u at <%d, %d, %d>", type, metadata, pos.x, pos.y, pos.z);
        return;
    }
    if (metadata > 15)
    {
        const char* name = mc_id::get_name_from_item_id(type, metadata);
        TRACE("Preposterous metadata field %d(%s):%u at <%d, %d, %d>", type, name, metadata, pos.x, pos.y, pos.z);
        return;
    }

    /* Attempt to find chunk, if cache doesn't match */
    glm::ivec3 chunk_pos = pos >> 4;
    glm::ivec3 block_pos = pos & 0x0F;
    if (!cache || cache->pos != chunk_pos)
    {
        auto it = cmap.find(chunk_pos);
        if (it == cmap.end())
        {
            TRACE("Unable to find chunk <%d, %d, %d>", chunk_pos.x, chunk_pos.y, chunk_pos.z);
            return;
        }

        cache = it->second;
    }

    /* Get existing blocks data to help determine which chunks need rebuilding */
    block_id_t old_type = cache->get_type(block_pos.x, block_pos.y, block_pos.z);

    /* Set type */
    cache->set_type(block_pos.x, block_pos.y, block_pos.z, type);
    cache->set_metadata(block_pos.x, block_pos.y, block_pos.z, metadata);

    /* Surrounding chunks do not need updating if the replacement has an equal effect on lighting */
    if (mc_id::is_transparent(old_type) == mc_id::is_transparent(type) && mc_id::get_light_level(old_type) == mc_id::get_light_level(type))
        return;

    if (cache->dirty_level < chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL)
        cache->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL;

    bool within_bounds_x = BETWEEN_INCL(block_pos.x, 1, SUBCHUNK_SIZE_X - 1);
    bool within_bounds_y = BETWEEN_INCL(block_pos.y, 1, SUBCHUNK_SIZE_Y - 1);
    bool within_bounds_z = BETWEEN_INCL(block_pos.z, 1, SUBCHUNK_SIZE_Z - 1);

    bool within_bounds = (within_bounds_x && within_bounds_y && within_bounds_z);

    /* No reason to poke other chunks if they won't get affected */
    if (within_bounds && cache->renderer_hints.opaque_sides)
        return;

    /* This isn't efficient, but it works the best, so */
    for (int i = 0; i < 27; i++)
    {
        const int ix = (i / 9) % 3 - 1;
        const int iy = (i / 3) % 3 - 1;
        const int iz = i % 3 - 1;

        chunk_cubic_t* c = cache->find_chunk(cache, chunk_pos + glm::ivec3(ix, iy, iz));

        chunk_cubic_t::dirty_level_t dirt_face = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL;

        if (c && c->dirty_level < dirt_face)
            c->dirty_level = dirt_face;
    }
}

bool level_t::get_block(const glm::ivec3 pos, block_id_t& type, Uint8& metadata)
{
    /* Attempt to find chunk */
    glm::ivec3 chunk_pos = pos >> 4;
    glm::ivec3 block_pos = pos & 0x0F;
    auto it = cmap.find(chunk_pos);
    if (it == cmap.end())
    {
        TRACE("Unable to find chunk <%d, %d, %d>", chunk_pos.x, chunk_pos.y, chunk_pos.z);
        return false;
    }

    chunk_cubic_t* c = it->second;

    type = c->get_type(block_pos.x, block_pos.y, block_pos.z);
    metadata = c->get_metadata(block_pos.x, block_pos.y, block_pos.z);

    return true;
}

bool level_t::get_block(const glm::ivec3 pos, itemstack_t& item, chunk_cubic_t*& cache)
{
    glm::ivec3 chunk_pos = pos >> 4;
    glm::ivec3 block_pos = pos & 0x0F;

    /* Attempt to find chunk, if cache doesn't match */
    if (!cache || cache->pos != chunk_pos)
    {
        auto it = cmap.find(chunk_pos);
        if (it == cmap.end())
        {
            TRACE("Unable to find chunk <%d, %d, %d>", chunk_pos.x, chunk_pos.y, chunk_pos.z);
            return false;
        }

        cache = it->second;
    }

    item.id = cache->get_type(block_pos.x, block_pos.y, block_pos.z);
    item.damage = cache->get_metadata(block_pos.x, block_pos.y, block_pos.z);

    return true;
}

void level_t::set_terrain(texture_terrain_t* const _terrain)
{
    terrain = _terrain;
    clear_mesh(false);

    if (!terrain)
        return;

    /* Create missing entity mesh */
    std::vector<terrain_vertex_t> missing_verts;
    {
        Uint8 ao = 0;
        Uint8 light_block = 15;
        Uint8 light_sky = 15;

        /**
         * Ordered +XYZ then -XYZ
         */
        mc_id::terrain_face_t faces[6];

        faces[0] = terrain->get_face(mc_id::FACE_WOOL_COLORED_RED);
        faces[1] = terrain->get_face(mc_id::FACE_WOOL_COLORED_LIME);
        faces[2] = terrain->get_face(mc_id::FACE_WOOL_COLORED_BLUE);
        faces[3] = faces[0], faces[4] = faces[1], faces[5] = faces[2];

        Uint8 scale = 1;
        short coord_min = -127;
        short coord_max = 128;

        /* Positive Y */
        {
            float c = 1.0f;
            missing_verts.push_back({ { scale, coord_max, coord_max, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[1].corners[0] });
            missing_verts.push_back({ { scale, coord_max, coord_max, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[1].corners[2] });
            missing_verts.push_back({ { scale, coord_min, coord_max, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[1].corners[1] });
            missing_verts.push_back({ { scale, coord_min, coord_max, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[1].corners[3] });
        }

        /* Negative Y */
        {
            float c = 0.5f;
            missing_verts.push_back({ { scale, coord_min, coord_min, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[4].corners[1] });
            missing_verts.push_back({ { scale, coord_max, coord_min, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[4].corners[0] });
            missing_verts.push_back({ { scale, coord_min, coord_min, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[4].corners[3] });
            missing_verts.push_back({ { scale, coord_max, coord_min, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[4].corners[2] });
        }

        /* Positive X */
        {
            float c = 1.0f;
            missing_verts.push_back({ { scale, coord_max, coord_min, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[0].corners[3] });
            missing_verts.push_back({ { scale, coord_max, coord_max, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[0].corners[1] });
            missing_verts.push_back({ { scale, coord_max, coord_min, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[0].corners[2] });
            missing_verts.push_back({ { scale, coord_max, coord_max, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[0].corners[0] });
        }

        /* Negative X */
        {
            float c = 0.5f;
            missing_verts.push_back({ { scale, coord_min, coord_max, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[3].corners[1] });
            missing_verts.push_back({ { scale, coord_min, coord_max, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[3].corners[0] });
            missing_verts.push_back({ { scale, coord_min, coord_min, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[3].corners[3] });
            missing_verts.push_back({ { scale, coord_min, coord_min, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[3].corners[2] });
        }

        /* Positive Z */
        {
            float c = 1.0f;
            missing_verts.push_back({ { scale, coord_max, coord_max, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[2].corners[1] });
            missing_verts.push_back({ { scale, coord_min, coord_max, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[2].corners[0] });
            missing_verts.push_back({ { scale, coord_max, coord_min, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[2].corners[3] });
            missing_verts.push_back({ { scale, coord_min, coord_min, coord_max, ao }, { c, c, c, light_block, light_sky }, faces[2].corners[2] });
        }

        /* Negative Z */
        {
            float c = 0.5f;
            missing_verts.push_back({ { scale, coord_min, coord_min, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[5].corners[3] });
            missing_verts.push_back({ { scale, coord_min, coord_max, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[5].corners[1] });
            missing_verts.push_back({ { scale, coord_max, coord_min, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[5].corners[2] });
            missing_verts.push_back({ { scale, coord_max, coord_max, coord_min, ao }, { c, c, c, light_block, light_sky }, faces[5].corners[0] });
        }
    }
    glBindVertexArray(ent_missing_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ent_missing_vbo);
    glBufferData(GL_ARRAY_BUFFER, missing_verts.size() * sizeof(missing_verts[0]), missing_verts.data(), GL_STATIC_DRAW);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, pos));
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, col));
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, tex));
    glBindVertexArray(0);

    ent_missing_vert_count = missing_verts.size();
}

void level_t::render_entities()
{
    const glm::f64vec3 camera_pos_capture = get_camera_pos();

    ecs.sort<entity_transform_t>([&camera_pos_capture](const entity_transform_t& a, const entity_transform_t& b) -> bool {
        const float adist = glm::distance(a.pos, camera_pos_capture);
        const float bdist = glm::distance(b.pos, camera_pos_capture);
        return adist < bdist;
    });

    glBindVertexArray(ent_missing_vao);

    auto view = ecs.view<const entity_id_t, const entity_transform_t>();
    view.use<const entity_transform_t>();
    for (auto [entity, id, pos] : view.each())
    {
        // shader_terrain->set_model(pos.get_mat());
        glDrawElements(GL_TRIANGLES, ent_missing_vert_count / 4 * 6, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

static convar_float_t cvr_r_damage_tilt_magnitude {
    "r_damage_tilt_magnitude",
    15.f,
    0.f,
    45.f,
    "Magnitude in degrees of the damage tilt",
    CONVAR_FLAG_SAVE,
};

static convar_float_t cvr_r_damage_tilt_rate {
    "r_damage_tilt_rate",
    10.f,
    1.f,
    100.f,
    "Duration in milliseconds/degree of the damage tilt",
    CONVAR_FLAG_SAVE,
};

void level_t::render_stage_prepare(const glm::ivec2 win_size)
{
    const int render_distance = (render_distance_override > 0) ? render_distance_override : r_render_distance.get();

    cull_chunks(win_size, render_distance);

    build_dirty_meshes();

    tick();
}

bool level_t::mesh_queue_upload_item(SDL_GPUCopyPass* const copy_pass, mesh_queue_info_t& item)
{
    chunk_cubic_t* c = get_chunk(item.pos);

    /* Mesh is for non-existent chunk, pop it */
    if (!c)
        return true;

    struct TBO
    {
        SDL_GPUTransferBuffer* tbo = nullptr;
        void* pointer = nullptr;
        TBO(Uint32 size)
        {
            SDL_GPUTransferBufferCreateInfo cinfo_tbo = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size, .props = 0 };
            tbo = SDL_CreateGPUTransferBuffer(state::gpu_device, &cinfo_tbo);
            if (!tbo)
                return;
            pointer = SDL_MapGPUTransferBuffer(state::gpu_device, tbo, 0);
        }
        ~TBO()
        {
            if (pointer)
                SDL_UnmapGPUTransferBuffer(state::gpu_device, tbo);
            if (tbo)
                SDL_ReleaseGPUTransferBuffer(state::gpu_device, tbo);
        }
    };

    TBO tbo(item.vertex_data_size);
    if (!tbo.pointer)
        return false;

    SDL_GPUBufferCreateInfo cinfo_vbo = {
        .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
        .size = item.vertex_data_size,
        .props = SDL_CreateProperties(),
    };
    char name[128];
    stbsp_snprintf(name, IM_ARRAYSIZE(name), "[Level][Chunk]: <%d, %d, %d>: Vertex Buffer", item.pos.x, item.pos.y, item.pos.z);
    SDL_SetStringProperty(cinfo_vbo.props, SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING, name);
    SDL_GPUBuffer* vbo = SDL_CreateGPUBuffer(state::gpu_device, &cinfo_vbo);
    SDL_DestroyProperties(cinfo_vbo.props);

    if (!vbo)
        return false;

    memcpy(tbo.pointer, item.vertex_data, item.vertex_data_size);

    SDL_GPUTransferBufferLocation loc_tbo = {
        .transfer_buffer = tbo.tbo,
        .offset = 0,
    };
    SDL_GPUBufferRegion region_vbo = {
        .buffer = vbo,
        .offset = 0,
        .size = item.vertex_data_size,
    };
    SDL_UploadToGPUBuffer(copy_pass, &loc_tbo, &region_vbo, 0);

    SDL_ReleaseGPUBuffer(state::gpu_device, c->vbo);
    c->vbo = vbo;
    c->quad_count = item.quad_count;
    c->quad_count_overlay = item.quad_count_overlay;
    c->quad_count_translucent = item.quad_count_translucent;

    return true;
}

void level_t::render_stage_copy(SDL_GPUCopyPass* const copy_pass)
{
    lightmap.update(copy_pass);

    for (int i = 0; mesh_queue.size() && i < r_mesh_throttle.get() * 4; i++)
    {
        mesh_queue_info_t item = mesh_queue.front();
        mesh_queue.pop_front();

        if (mesh_queue_upload_item(copy_pass, item))
            item.release_data();
        else
            mesh_queue.push_back(item);
    }
}

static convar_int_t cvr_r_wireframe {
    "r_wireframe",
    0,
    0,
    1,
    "Render world in wireframe mode",
    CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY,
};

void level_t::render_stage_render(
    SDL_GPUCommandBuffer* const command_buffer, SDL_GPURenderPass* const render_pass, const glm::ivec2 win_size, const float delta_time)
{
    const int render_distance = (render_distance_override > 0) ? render_distance_override : r_render_distance.get();

    glm::mat4 mat_proj = glm::perspective(glm::radians(fov), (float)win_size.x / (float)win_size.y, 0.03125f, render_distance * 32.0f);

    glm::mat4 mat_cam = glm::lookAt(get_camera_pos(), get_camera_pos() + camera_direction, camera_up);
    damage_tilt = SDL_clamp(damage_tilt, 0.0f, 1.0f);
    if (gamemode == mc_id::GAMEMODE_CREATIVE || gamemode == mc_id::GAMEMODE_SPECTATOR)
        damage_tilt = 0.0f;
    mat_cam = glm::rotate(glm::mat4(1.0f), -glm::radians(damage_tilt * cvr_r_damage_tilt_magnitude.get()), glm::vec3(0.f, 0.f, 1.f)) * mat_cam;
    damage_tilt -= delta_time / (cvr_r_damage_tilt_magnitude.get() * cvr_r_damage_tilt_rate.get() / 1000.0f);

    {
        const glm::i64vec3 cpos(glm::round(get_camera_pos() / 16.0f));
        const glm::i64vec3 camera_pos_diff = cpos - last_render_order_cpos;
        const glm::f64vec3 float_cpos(cpos);

        if (camera_pos_diff != glm::i64vec3(0, 0, 0))
            request_render_order_sort = 1;

        if (request_render_order_sort)
            TRACE("Render order sort requested @ %ld", SDL_GetTicks());

        if (request_render_order_sort || SDL_GetTicks() - last_render_order_sort_time > 5000)
        {
            std::sort(chunks_render_order.begin(), chunks_render_order.end(), [&float_cpos](const chunk_cubic_t* const a, const chunk_cubic_t* const b) {
                const float adist = glm::distance(glm::f64vec3(a->pos), float_cpos);
                const float bdist = glm::distance(glm::f64vec3(b->pos), float_cpos);
                return adist > bdist;
            });
            last_render_order_sort_time = SDL_GetTicks();
            request_render_order_sort = 0;
            last_render_order_cpos = cpos;
        }
    }

    SDL_GPUTextureSamplerBinding binding_tex[] = {
        terrain->binding,
        SDL_GPUTextureSamplerBinding { .texture = lightmap.tex_id, .sampler = lightmap.sampler_linear },
    };

    SDL_BindGPUFragmentSamplers(render_pass, 0, binding_tex, SDL_arraysize(binding_tex));

    struct alignas(16) ubo_model_t
    {
        glm::vec4 model;
    };

    struct alignas(16) ubo_world_t
    {
        glm::mat4 camera;
        glm::mat4 projection;
    } ubo_world = { .camera = mat_cam, .projection = mat_proj };

    struct alignas(16) ubo_tint_t
    {
        glm::vec4 tint;
    } ubo_tint = { .tint = glm::vec4(1) };

    struct alignas(16) ubo_frag_t
    {
        Uint32 use_texture;
    } ubo_frag = { .use_texture = !!(state::game_resources->use_texture) };

    SDL_PushGPUFragmentUniformData(command_buffer, 0, &ubo_frag, sizeof(ubo_frag));
    SDL_PushGPUVertexUniformData(command_buffer, 0, &ubo_world, sizeof(ubo_world));
    SDL_PushGPUVertexUniformData(command_buffer, 1, &ubo_tint, sizeof(ubo_tint));

    if (state::pipeline_shader_terrain_opaque)
    {
        SDL_PushGPUDebugGroup(command_buffer, "Opaque");
        SDL_BindGPUGraphicsPipeline(render_pass, state::pipeline_shader_terrain_opaque);
        for (auto it = chunks_render_order.rbegin(); it != chunks_render_order.rend(); it = next(it))
        {
            chunk_cubic_t* c = *it;
            if (!c->vbo || c->quad_count == 0 || !c->visible)
                continue;
            glm::vec3 translate(c->pos * glm::ivec3(SUBCHUNK_SIZE_X, SUBCHUNK_SIZE_Y, SUBCHUNK_SIZE_Z));
            ubo_model_t ubo_model = { .model = glm::vec4(translate.x, translate.y, translate.z, 0.0f) };
            SDL_PushGPUVertexUniformData(command_buffer, 2, &ubo_model, sizeof(ubo_model));
            SDL_BindGPUVertexStorageBuffers(render_pass, 0, &c->vbo, 1);
            SDL_DrawGPUPrimitives(render_pass, 4, c->quad_count, 0, 0);
        }
        SDL_PopGPUDebugGroup(command_buffer);
    }

    if (state::pipeline_shader_terrain_overlay)
    {
        SDL_PushGPUDebugGroup(command_buffer, "Overlay");
        SDL_BindGPUGraphicsPipeline(render_pass, state::pipeline_shader_terrain_overlay);
        for (auto it = chunks_render_order.rbegin(); it != chunks_render_order.rend(); it = next(it))
        {
            chunk_cubic_t* c = *it;
            if (!c->vbo || c->quad_count_overlay == 0 || !c->visible)
                continue;
            glm::vec3 translate(c->pos * glm::ivec3(SUBCHUNK_SIZE_X, SUBCHUNK_SIZE_Y, SUBCHUNK_SIZE_Z));
            ubo_model_t ubo_model = { .model = glm::vec4(translate.x, translate.y, translate.z, 0.0f) };
            SDL_PushGPUVertexUniformData(command_buffer, 2, &ubo_model, sizeof(ubo_model));
            SDL_BindGPUVertexStorageBuffers(render_pass, 0, &c->vbo, 1);
            SDL_DrawGPUPrimitives(render_pass, 4, c->quad_count_overlay, 0, c->quad_count);
        }
        SDL_PopGPUDebugGroup(command_buffer);
    }

    render_entities();

    if (state::pipeline_shader_terrain_translucent_depth)
    {
        SDL_PushGPUDebugGroup(command_buffer, "Translucent Depth");
        SDL_BindGPUGraphicsPipeline(render_pass, state::pipeline_shader_terrain_translucent_depth);
        for (chunk_cubic_t* c : chunks_render_order)
        {
            if (!c->vbo || c->quad_count_translucent == 0 || !c->visible)
                continue;
            glm::vec3 translate(c->pos * glm::ivec3(SUBCHUNK_SIZE_X, SUBCHUNK_SIZE_Y, SUBCHUNK_SIZE_Z));
            ubo_model_t ubo_model = { .model = glm::vec4(translate.x, translate.y, translate.z, 0.0f) };
            SDL_PushGPUVertexUniformData(command_buffer, 2, &ubo_model, sizeof(ubo_model));
            SDL_BindGPUVertexStorageBuffers(render_pass, 0, &c->vbo, 1);
            SDL_DrawGPUPrimitives(render_pass, 4, c->quad_count_translucent, 0, c->quad_count + c->quad_count_overlay);
        }
        SDL_PopGPUDebugGroup(command_buffer);
    }

    if (state::pipeline_shader_terrain_translucent)
    {
        SDL_PushGPUDebugGroup(command_buffer, "Translucent Color");
        SDL_BindGPUGraphicsPipeline(render_pass, state::pipeline_shader_terrain_translucent);
        for (chunk_cubic_t* c : chunks_render_order)
        {
            if (!c->vbo || c->quad_count_translucent == 0 || !c->visible)
                continue;
            glm::vec3 translate(c->pos * glm::ivec3(SUBCHUNK_SIZE_X, SUBCHUNK_SIZE_Y, SUBCHUNK_SIZE_Z));
            ubo_model_t ubo_model = { .model = glm::vec4(translate.x, translate.y, translate.z, 0.0f) };
            SDL_PushGPUVertexUniformData(command_buffer, 2, &ubo_model, sizeof(ubo_model));
            SDL_BindGPUVertexStorageBuffers(render_pass, 0, &c->vbo, 1);
            SDL_DrawGPUPrimitives(render_pass, 4, c->quad_count_translucent, 0, c->quad_count + c->quad_count_overlay);
        }
        SDL_PopGPUDebugGroup(command_buffer);
    }
}

void level_t::remove_chunk(const glm::ivec3 pos)
{
    auto it_map = cmap.find(pos);

    chunk_cubic_t* mapped_del = NULL;

    if (it_map != cmap.end())
    {
        mapped_del = it_map->second;
        delete it_map->second;
        cmap.erase(it_map);
    }

    for (auto it_vec = chunks_render_order.begin(); it_vec != chunks_render_order.end();)
    {
        /* Remove self as neighbor from neighbors */
        /* It would more efficient to do this by traversing mapped_del's neighbors, but this is more thorough */
        // clang-format off
        if ((*it_vec)->neighbors.neg_x == mapped_del) (*it_vec)->neighbors.neg_x = NULL;
        if ((*it_vec)->neighbors.pos_x == mapped_del) (*it_vec)->neighbors.pos_x = NULL;
        if ((*it_vec)->neighbors.neg_y == mapped_del) (*it_vec)->neighbors.neg_y = NULL;
        if ((*it_vec)->neighbors.pos_y == mapped_del) (*it_vec)->neighbors.pos_y = NULL;
        if ((*it_vec)->neighbors.neg_z == mapped_del) (*it_vec)->neighbors.neg_z = NULL;
        if ((*it_vec)->neighbors.pos_z == mapped_del) (*it_vec)->neighbors.pos_z = NULL;
        // clang-format on

        if ((*it_vec)->pos != pos)
            it_vec++;
        else
        {
            if (*it_vec != mapped_del)
            {
                dc_log_warn("Duplicate chunk <%d, %d, %d> erased!", (*it_vec)->pos.x, (*it_vec)->pos.y, (*it_vec)->pos.z);
                delete *it_vec;
            }
            it_vec = chunks_render_order.erase(it_vec);
        }
    }

    for (auto it_vec = chunks_light_order.begin(); it_vec != chunks_light_order.end();)
    {
        if ((*it_vec)->pos != pos)
            it_vec++;
        else
        {
            if (*it_vec != mapped_del)
            {
                dc_log_warn("Duplicate chunk <%d, %d, %d> erased!", (*it_vec)->pos.x, (*it_vec)->pos.y, (*it_vec)->pos.z);
                delete *it_vec;
            }
            it_vec = chunks_light_order.erase(it_vec);
        }
    }

    request_render_order_sort = 1;
    request_light_order_sort = 1;
}

void level_t::add_chunk(chunk_cubic_t* const c)
{
    if (!c)
    {
        dc_log_error("Chunk is NULL!");
        return;
    }

    decltype(cmap)::iterator it = cmap.find(c->pos);

    if (it != cmap.end())
    {
        dc_log_error("Chunk is duplicate!");
        return;
    }

    cmap[c->pos] = c;

    c->renderer_hints.hints_set = 0;

    chunks_light_order.push_back(c);
    chunks_render_order.push_back(c);

    /* Assign neighbors */
    c->neighbors.pos_x = (it = cmap.find(c->pos + glm::ivec3(+1, +0, +0))) != cmap.end() ? it->second : NULL;
    c->neighbors.neg_x = (it = cmap.find(c->pos + glm::ivec3(-1, +0, +0))) != cmap.end() ? it->second : NULL;
    c->neighbors.pos_y = (it = cmap.find(c->pos + glm::ivec3(+0, +1, +0))) != cmap.end() ? it->second : NULL;
    c->neighbors.neg_y = (it = cmap.find(c->pos + glm::ivec3(+0, -1, +0))) != cmap.end() ? it->second : NULL;
    c->neighbors.pos_z = (it = cmap.find(c->pos + glm::ivec3(+0, +0, +1))) != cmap.end() ? it->second : NULL;
    c->neighbors.neg_z = (it = cmap.find(c->pos + glm::ivec3(+0, +0, -1))) != cmap.end() ? it->second : NULL;

    /* Assign self as neighbor to neighbors */
    // clang-format off
    if (c->neighbors.pos_x != NULL) c->neighbors.pos_x->neighbors.neg_x = c;
    if (c->neighbors.neg_x != NULL) c->neighbors.neg_x->neighbors.pos_x = c;
    if (c->neighbors.pos_y != NULL) c->neighbors.pos_y->neighbors.neg_y = c;
    if (c->neighbors.neg_y != NULL) c->neighbors.neg_y->neighbors.pos_y = c;
    if (c->neighbors.pos_z != NULL) c->neighbors.pos_z->neighbors.neg_z = c;
    if (c->neighbors.neg_z != NULL) c->neighbors.neg_z->neighbors.pos_z = c;
    // clang-format on

    request_render_order_sort = 1;
    request_light_order_sort = 1;
}

level_t::level_t(texture_terrain_t* const _terrain)
{
    /* Create missing entity mesh OpenGL resources */
    glGenVertexArrays(1, &ent_missing_vao);
    tetra::gl_obj_label(GL_VERTEX_ARRAY, ent_missing_vao, "[Level][Entity]: Missing: VAO");
    glBindVertexArray(ent_missing_vao);

    glGenBuffers(1, &ent_missing_vbo);
    tetra::gl_obj_label(GL_BUFFER, ent_missing_vbo, "[Level][Entity]: Missing: VBO");
    glBindBuffer(GL_ARRAY_BUFFER, ent_missing_vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    set_terrain(_terrain);

    last_tick = SDL_GetTicks() / 50;

    player_eid = ecs.create();

    /* For testing */
    entity_physics_t physics;
    physics.reset_to_entity_defaults(ENT_ID_CREEPER);
    ecs.emplace<entity_physics_t>(player_eid, physics);
    ecs.emplace<entity_transform_t>(player_eid, entity_transform_t { .pos = { -30.0, 1280.0, -30.0 } });

    music = glm::mix(0.125f, 0.825f, SDL_randf());

    generator_create();
}

void level_t::mesh_queue_info_t::release_data()
{
    if (vertex_freefunc)
    {
        vertex_freefunc(vertex_data);
        vertex_data = NULL;
    }
}

level_t::~level_t()
{
    for (auto it : mesh_queue)
        it.release_data();

    glBindVertexArray(0);
    glDeleteBuffers(1, &ent_missing_vbo);
    glDeleteVertexArrays(1, &ent_missing_vao);
    for (chunk_cubic_t* c : chunks_render_order)
        delete c;

    generator_destroy();
}

bool level_t::gamemode_set(int x)
{
    if (mc_id::gamemode_is_valid(x))
    {
        gamemode = mc_id::gamemode_t(x);
        return true;
    }
    else
    {
        dc_log_error("Invalid game mode specified: %d", x);
        return false;
    }
}

void level_t::clear()
{
    chunks_light_order.clear();
    chunks_render_order.clear();
    cmap.clear();

    // for(auto entity: ecs.view<entt::entity>(entt::exclude<T>)) { ... }
    for (auto entity : ecs.view<ent_id_t>())
    {
        if (entity != player_eid)
            ecs.destroy(entity);
    }
}

level_t::dimension_switch_result level_t::dimension_switch(const int dim)
{
    if (!mc_id::dimension_is_valid(dim))
    {
        dc_log_error("Invalid dimension specified: %d", dim);
        return DIM_SWITCH_INVALID_DIM;
    }

    const mc_id::dimension_t new_dim = mc_id::dimension_t(dim);

    if (new_dim == dimension)
        return DIM_SWITCH_ALREADY_IN_USE;

    dc_log("Switching dimension from %d to %d", dimension, new_dim);

    if (music < glm::mix(0.3f, 0.6f, SDL_randf()))
        music = glm::mix(0.4f, 0.7f, SDL_randf());

    sound_engine.kill_all();

    dimension = new_dim;

    clear();

    switch (dimension)
    {
    case mc_id::DIMENSION_END:
        lightmap.set_preset(lightmap_t::LIGHTMAP_PRESET_END);
        break;
    case mc_id::DIMENSION_NETHER:
        lightmap.set_preset(lightmap_t::LIGHTMAP_PRESET_NETHER);
        break;
    case mc_id::DIMENSION_OVERWORLD: /* Fall-through */
    default:
        lightmap.set_preset(lightmap_t::LIGHTMAP_PRESET_OVERWORLD);
        break;
    }

    return DIM_SWITCH_SUCCESSFUL;
}

void level_t::tick()
{
    Uint64 start_time = SDL_GetTicks();
    mc_tick_t cur_tick = start_time / 50;

    int iterations = 0;
    for (; last_tick < cur_tick && iterations < 250; iterations++)
    {
        tick_real();
        last_tick++;
    }

    Uint64 diff = SDL_GetTicks() - start_time;
    if (diff > 250)
        dc_log_warn("Call to level_t::tick() took more than %lu ms! (%d iterations)", start_time, iterations);
    if (diff >= 250)
        dc_log_warn("Call to level_t::tick() maxed out iterations");
}

static convar_int_t cvr_mc_enable_physics {
    "mc_enable_physics",
    0,
    0,
    1,
    "Enable physics (Experimental)",
    CONVAR_FLAG_INT_IS_BOOL | CONVAR_FLAG_DEV_ONLY,
};

static convar_int_t cvr_a_delay_mood { "a_delay_mood", 6000, 20, 30000, "Maximum value for mood counter (MC Ticks)", CONVAR_FLAG_SAVE };
static convar_int_t cvr_a_delay_min_music_game { "a_delay_min_music_game", 3000, 20, 30000, "Minimum value for music counter (MC Ticks)", CONVAR_FLAG_SAVE };
static convar_int_t cvr_a_delay_max_music_game { "a_delay_max_music_game", 6000, 20, 30000, "Maximum value for music counter (MC Ticks)", CONVAR_FLAG_SAVE };

void level_t::tick_real()
{
    /* Modify mood counter */
    {
        float light_block = 0;
        float light_sky = 0;

        /* Fetch block */
        const glm::vec3 offset = (glm::vec3(SDL_randf(), SDL_randf(), SDL_randf()) - 0.5f) * 17.0f;
        const glm::ivec3 pos = glm::round(offset + get_camera_pos());
        chunk_cubic_t* c = get_chunk(pos >> 4);
        if (c)
        {
            light_block = c->get_light_block(pos.x & 0x0F, pos.y & 0x0F, pos.z & 0x0F);
            light_sky = c->get_light_sky(pos.x & 0x0F, pos.y & 0x0F, pos.z & 0x0F);
        }

        const float coeff = 1.0f / cvr_a_delay_mood.get();

        if (light_sky)
            mood -= light_sky * 4.0f * coeff;
        else
            mood -= (light_block - 1.0f) * coeff;

        mood = SDL_max(0.0f, mood);

        if (mood >= 1.0f)
        {
            sound_info_t sinfo;
            if (sound_resources && sound_resources->get_sound("minecraft:ambient.cave.cave", sinfo))
                sound_engine.request_source(sinfo, glm::vec3(0), true);
            mood = 0.0f;
        }
    }

    /* Modify music counter */
    {
        const float coeff_min = 1.0f / cvr_a_delay_min_music_game.get();
        const float coeff_max = 1.0f / cvr_a_delay_max_music_game.get();
        const float coeff = glm::mix(coeff_min, coeff_max, SDL_randf());

        music = SDL_max(0.0f, music + coeff);

        if (music >= 1.0f && sound_resources)
        {
            sound_info_t sinfo;
            bool acquired_sound = 0;
            switch (dimension_get())
            {
            case mc_id::DIMENSION_END:
                acquired_sound = sound_resources->get_sound("minecraft:music.game.end", sinfo);
                break;
            case mc_id::DIMENSION_NETHER:
                acquired_sound = sound_resources->get_sound("minecraft:music.game.nether", sinfo);
                break;
            case mc_id::DIMENSION_OVERWORLD:
                switch (gamemode_get())
                {
                case mc_id::GAMEMODE_SPECTATOR:
                case mc_id::GAMEMODE_CREATIVE:
                    acquired_sound = sound_resources->get_sound("minecraft:music.game.creative", sinfo);
                    break;
                default:
                    acquired_sound = sound_resources->get_sound("minecraft:music.game", sinfo);
                    break;
                }
                break;
            }
            if (acquired_sound)
                sound_engine.request_source(sinfo, glm::f64vec3(0.0), 1);
            music = 0.0f;
        }
    }

    lightmap.set_world_time(++mc_time);

    for (auto [entity, counter] : ecs.view<entity_timed_destroy_t>().each())
    {
        counter.counter--;
        if (!counter.server_entity && counter.counter < 0)
            ecs.destroy(entity);
    }

    for (auto [entity, health] : ecs.view<entity_health_t>().each())
    {
        health.update_effect_counter--;
        if (health.update_effect_counter < 1)
        {
            ecs.patch<entity_health_t>(entity, [](entity_health_t& h) {
                h.last = h.cur;
                h.update_effect_counter = 0;
            });
        }
    }

    for (auto [entity, food] : ecs.view<entity_food_t>().each())
    {
        food.update_effect_counter--;
        if (food.update_effect_counter < 1)
        {
            ecs.patch<entity_food_t>(entity, [](entity_food_t& f) {
                f.last = f.cur;
                f.satur_last = f.satur_cur;
                f.update_effect_counter = 0;
            });
        }
    }

    if (!cvr_mc_enable_physics.get())
        return;

    bool block_has_collision[256];
    for (int i = 0; i < IM_ARRAYSIZE(block_has_collision); i++)
        block_has_collision[i] = mc_id::block_has_collision(i);

    chunk_cubic_t* chunk_cache = NULL;

    ecs.patch<entity_transform_t>(player_eid, [=](entity_transform_t& transform) { transform.pos = foot_pos; });

    /* Formulae from both: https://minecraft.wiki/w/Entity#Motion_of_entities and https://github.com/OrHy3/MinecraftMotionTools */
    for (auto [entity, transform, physics] : ecs.view<entity_transform_t, entity_physics_t>().each())
    {
        glm::f64vec3 new_velocity;
        /* Tick velocity */
        {
            glm::f64vec3 v_i = physics.vel;
            glm::f64vec3 v_f;
            const double drag_h = physics.flags.on_ground ? physics.drag_horizontal_on_ground : physics.drag_horizontal;
            const double drag_v = physics.drag_vertical;

            v_f = v_i * (glm::f64vec3(1.0f) - glm::f64vec3 { drag_h, drag_v, drag_h });

            double a = physics.acceleration * (1.0 - (1.0 - drag_v)) / drag_v;
            if (physics.flags.apply_drag_after_accel)
                a *= (1.0 - drag_v);
            new_velocity = v_f - glm::f64vec3 { 0.0, a, 0.0 };
        }

        if (physics.flags.update_velocity_before_position)
            physics.vel = new_velocity;

        glm::f64vec3 delta = physics.vel - glm::f64vec3(0.0, ((physics.flags.apply_accel_to_position) ? physics.acceleration : 0.0f), 0.0);

        /* TODO: Collision checking here */

        /* Write out values */
        transform.pos += delta;
        physics.vel = new_velocity;
    }

    foot_pos = ecs.get<entity_transform_t>(player_eid).pos;
}
