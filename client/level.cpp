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
#include <GL/glew.h>
#include <GL/glu.h>
#include <SDL3/SDL_opengl.h>

#include "level.h"

#include <algorithm>
#include <glm/ext/matrix_transform.hpp>

#include "tetra/util/convar.h"
#include <SDL3/SDL.h>

#include "tetra/tetra_gl.h"

#ifndef NDEBUG
#define FORCE_OPT_LIGHT 1
#define FORCE_OPT_MESH 1
#else
#define FORCE_OPT_LIGHT 0
#define FORCE_OPT_MESH 0
#endif

#define LEVEL_T_ENABLE_SSE2 1

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
            c->free_gl();
        if (c->dirty_level < chunk_cubic_t::DIRTY_LEVEL_MESH)
            c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_MESH;
    }
}

/* Ideally this would use the Gribb/Hartmann method to extract the planes from a projection/camera matrix to get the plane normals */
void level_t::cull_chunks(const glm::ivec2 win_size, const int render_distance)
{
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
    const glm::ivec3 camera_half_chunk_pos = (glm::ivec3(camera_pos) >> 3) - glm::ivec3(1, 1, 1);
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

void level_t::build_dirty_meshes()
{
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

    /* Clear Light Pass */
    PASS_TIMER_START();
    for (auto it = chunks_light_order.begin(); it != chunks_light_order.end(); it++)
    {
        chunk_cubic_t* c = *it;
        if (c->dirty_level != chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL)
            continue;
        c->clear_light_block(0);
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
        light_pass(c->pos.x, c->pos.y, c->pos.z, true);
        light_pass_sky(c->pos.x, c->pos.y, c->pos.z, true);
        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_0;
        built++;
    }
    PASS_TIMER_STOP(enable_timer_log_light, "Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 1)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);
    last_perf_light_pass1.duration = elapsed;
    last_perf_light_pass1.built = built;

    /* Second Light Pass */
    PASS_TIMER_START();
    for (auto it = chunks_light_order.begin(); it != chunks_light_order.end(); it++)
    {
        chunk_cubic_t* c = *it;
        if (c->dirty_level != chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_0)
            continue;
        light_pass(c->pos.x, c->pos.y, c->pos.z, false);
        light_pass_sky(c->pos.x, c->pos.y, c->pos.z, false);
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
        light_pass(c->pos.x, c->pos.y, c->pos.z, false);
        light_pass_sky(c->pos.x, c->pos.y, c->pos.z, false);
        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_2;
        built++;
    }
    PASS_TIMER_STOP(enable_timer_log_light, "Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 3)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);
    last_perf_light_pass3.duration = elapsed;
    last_perf_light_pass3.built = built;

    /* Fourth Light Pass */
    PASS_TIMER_START();
    for (auto it = chunks_light_order.begin(); it != chunks_light_order.end(); it++)
    {
        chunk_cubic_t* c = *it;
        if (c->dirty_level != chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_2)
            continue;
        light_pass(c->pos.x, c->pos.y, c->pos.z, false);
        light_pass_sky(c->pos.x, c->pos.y, c->pos.z, false);
        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_MESH;
        built++;
    }
    PASS_TIMER_STOP(enable_timer_log_light, "Lit %zu chunks in %.2f ms (%.2f ms per) (Pass 4)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);
    last_perf_light_pass4.duration = elapsed;
    last_perf_light_pass4.built = built;

    /* Mesh Pass */
    PASS_TIMER_START();
    int throttle = r_mesh_throttle.get();
    for (auto it = chunks_render_order.rbegin(); it != chunks_render_order.rend() && throttle > 0; it++)
    {
        chunk_cubic_t* c = *it;
        if (c->dirty_level != chunk_cubic_t::DIRTY_LEVEL_MESH || !c->visible)
            continue;
        build_mesh(c->pos.x, c->pos.y, c->pos.z);
        c->dirty_level = chunk_cubic_t::DIRTY_LEVEL_NONE;
        built++;
        throttle--;
    }
    PASS_TIMER_STOP(enable_timer_log_mesh, "Built %zu meshes in %.2f ms (%.2f ms per)", built, elapsed / 1000000.0, elapsed / built / 1000000.0);
    last_perf_mesh_pass.duration = elapsed;
    last_perf_mesh_pass.built = built;
}

/**
 * Force inlining in debug builds
 *
 * Without this debug builds are *very*, *very* slow (~6x slower)
 */
#if (FORCE_OPT_LIGHT)
#pragma GCC push_options
#pragma GCC optimize("O3")
#endif

struct chunk_cross_t
{
    chunk_cubic_t* c = NULL;
    chunk_cubic_t* pos_x = NULL;
    chunk_cubic_t* pos_y = NULL;
    chunk_cubic_t* pos_z = NULL;
    chunk_cubic_t* neg_x = NULL;
    chunk_cubic_t* neg_y = NULL;
    chunk_cubic_t* neg_z = NULL;

    chunk_cross_t() { }
};

static convar_int_t cvr_use_sse2("use_sse2", 0, 0, 1, "Use manually SSE2 optimized routines", CONVAR_FLAG_SAVE | CONVAR_FLAG_INT_IS_BOOL);

#if defined(__SSE2__) && LEVEL_T_ENABLE_SSE2

#include <emmintrin.h>

#define ENABLE_VERIFY_SSE2 0

#if ENABLE_VERIFY_SSE2
/**
 * Check that each element of a [u8 x 16] vector is within a certain range, otherwise assert
 *
 * @param vec [u8 x 16] vector to check
 * @param min Minimum value for each element
 * @param max Maximum value for each element
 */
static void verify_packed_8_in_range(const __m128i& vec, Uint8 min, Uint8 max)
{
    /* The volatile keyword is to make contents always viewable in the debugger */
    volatile Uint8 verify[16];

    _mm_storeu_si128((__m128i*)verify, vec);

    SDL_assert(BETWEEN_INCL(verify[0x00], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x01], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x02], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x03], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x04], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x05], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x06], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x07], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x08], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x09], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x0A], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x0B], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x0C], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x0D], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x0E], min, max));
    SDL_assert(BETWEEN_INCL(verify[0x0F], min, max));
}
#else
#define verify_packed_8_in_range(vec, min, max) (void)(void(__m128i(vec)), void(Uint8(min)), void(Uint8(max)))
#endif /* #if ENABLE_VERIFY_SSE2 */

#endif /* #if defined(__SSE2__) && LEVEL_T_ENABLE_SSE2 */

void level_t::light_pass(const int chunk_x, const int chunk_y, const int chunk_z, const bool local_only)
{
    /** Index: C +XYZ -XYZ */
    chunk_cross_t cross;

    auto it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(0, 0, 0));
    cross.c = ((it != cmap.end()) ? it->second : NULL);

    if (!local_only)
    {
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+1, +0, +0));
        cross.pos_x = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(-1, +0, +0));
        cross.neg_x = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, +1, +0));
        cross.pos_y = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, -1, +0));
        cross.neg_y = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, +0, +1));
        cross.pos_z = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, +0, -1));
        cross.neg_z = it != cmap.end() ? it->second : NULL;
    }

    if (!cross.c)
    {
        dc_log_error("Attempt made to update lighting for unloaded chunk at chunk coordinates: %d, %d, %d", chunk_x, chunk_y, chunk_z);
        return;
    }

    bool is_transparent[256];
    Uint8 light_levels[256];

    for (int i = 0; i < IM_ARRAYSIZE(is_transparent); i++)
        is_transparent[i] = mc_id::is_transparent(i);

    for (int i = 0; i < IM_ARRAYSIZE(light_levels); i++)
        light_levels[i] = mc_id::get_light_level(i);

    Uint32 total = 0;
    Uint8 min_level = 15;
    Uint8 max_level = 0;
    /* Initial scan of light levels */
    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_VOLUME; dat_it++)
    {
        const int y = (dat_it) & 0x0F;
        const int z = (dat_it >> 4) & 0x0F;
        const int x = (dat_it >> 8) & 0x0F;

        Uint8 type = cross.c->get_type(x, y, z);

        Uint8 lvl = light_levels[type];

        if (!is_transparent[type])
        {
            if (lvl != 0)
                cross.c->set_light_block(x, y, z, lvl);
            continue;
        }

        /* Shift light value from range [0, 15] to [1, 16] */
        lvl++;

        /** Index: +XYZ -XYZ, all values are *ALWAYS* in the range [1, 16] */
        Uint8 sur_levels[6] = { 0 };

        switch (x)
        {
        case 0:
            sur_levels[0] = Uint8(cross.c->get_light_block(x + 1, y, z));
            sur_levels[3] = cross.neg_x ? (Uint8(cross.neg_x->get_light_block(SUBCHUNK_SIZE_X - 1, y, z))) : 0;
            break;
        case (SUBCHUNK_SIZE_X - 1):
            sur_levels[0] = cross.pos_x ? (Uint8(cross.pos_x->get_light_block(0, y, z))) : 0;
            sur_levels[3] = Uint8(cross.c->get_light_block(x - 1, y, z));
            break;
        default:
            sur_levels[0] = Uint8(cross.c->get_light_block(x + 1, y, z));
            sur_levels[3] = Uint8(cross.c->get_light_block(x - 1, y, z));
            break;
        }

        switch (y)
        {
        case 0:
            sur_levels[1] = Uint8(cross.c->get_light_block(x, y + 1, z));
            sur_levels[4] = cross.neg_y ? (Uint8(cross.neg_y->get_light_block(x, SUBCHUNK_SIZE_Y - 1, z))) : 0;
            break;
        case (SUBCHUNK_SIZE_Y - 1):
            sur_levels[1] = cross.pos_y ? (Uint8(cross.pos_y->get_light_block(x, 0, z))) : 0;
            sur_levels[4] = Uint8(cross.c->get_light_block(x, y - 1, z));
            break;
        default:
            sur_levels[1] = Uint8(cross.c->get_light_block(x, y + 1, z));
            sur_levels[4] = Uint8(cross.c->get_light_block(x, y - 1, z));
            break;
        }

        switch (z)
        {
        case 0:
            sur_levels[2] = Uint8(cross.c->get_light_block(x, y, z + 1));
            sur_levels[5] = cross.neg_z ? (Uint8(cross.neg_z->get_light_block(x, y, SUBCHUNK_SIZE_Z - 1))) : 0;
            break;
        case (SUBCHUNK_SIZE_Z - 1):
            sur_levels[2] = cross.pos_z ? (Uint8(cross.pos_z->get_light_block(x, y, 0))) : 0;
            sur_levels[5] = Uint8(cross.c->get_light_block(x, y, z - 1));
            break;
        default:
            sur_levels[2] = Uint8(cross.c->get_light_block(x, y, z + 1));
            sur_levels[5] = Uint8(cross.c->get_light_block(x, y, z - 1));
            break;
        }

        /* Shift light value from range [0, 15] to [1, 16] */
        lvl = SDL_max(lvl, cross.c->get_light_block(x, y, z) + 1);

        /* This SDL_max() isn't necessary because lvl is guaranteed to be at least one by now */
        // lvl = SDL_max(1, lvl);

        lvl = SDL_max(lvl, sur_levels[0]);
        lvl = SDL_max(lvl, sur_levels[1]);
        lvl = SDL_max(lvl, sur_levels[2]);
        lvl = SDL_max(lvl, sur_levels[3]);
        lvl = SDL_max(lvl, sur_levels[4]);
        lvl = SDL_max(lvl, sur_levels[5]);

        /* Move lvl from range [1,16] to [0,15] */
        lvl--;

        cross.c->set_light_block(x, y, z, lvl);
        min_level = SDL_min(min_level, lvl);
        max_level = SDL_max(max_level, lvl);
        total += lvl;
    }

    /* If the light level is uniform then no propagation is required for a local pass */
    if (local_only && total % SUBCHUNK_SIZE_VOLUME == 0)
        return;

#if defined(__SSE2__) && LEVEL_T_ENABLE_SSE2
    const __m128i one = _mm_set1_epi8(1);
    verify_packed_8_in_range(one, 1, 1);

    if (!cvr_use_sse2.get())
        goto bypass_sse2;

    static_assert(SUBCHUNK_SIZE_X == 16);
    static_assert(SUBCHUNK_SIZE_Y == 16);
    static_assert(SUBCHUNK_SIZE_Z == 16);
    static_assert(sizeof(chunk_cubic_t::data_light_block) == SUBCHUNK_SIZE_VOLUME / 2);
    static_assert(sizeof(__m128i) == (SUBCHUNK_INDEX(0, 16, 0) - SUBCHUNK_INDEX(0, 0, 0)));
    static_assert(sizeof(__m128i) * 16 == (SUBCHUNK_INDEX(0, 0, 16) - SUBCHUNK_INDEX(0, 0, 0)));

    /* Unpack light data */
    alignas(16) Uint8 data_light[SUBCHUNK_SIZE_VOLUME];
    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_VOLUME; dat_it += 2)
    {
        const Uint8 packed = cross.c->data_light_block[dat_it >> 1];
        data_light[dat_it] = packed & 0x0F;
        data_light[dat_it + 1] = (packed >> 4) & 0x0F;
    }

    __m128i last_strip;

    /* Propagate light (SSE2) (Block) */
    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_X * SUBCHUNK_SIZE_Z * (max_level - min_level); dat_it++)
    {
        const int vol_it = dat_it & 0xFF;
        const int z = (dat_it >> 0) & 0x0F;
        const int x = (dat_it >> 4) & 0x0F;

        const Uint8* const data_block_at_vol = cross.c->data_block + (vol_it << 4);
        __m128i* const data_light_at_vol = (__m128i*)data_light + vol_it;

        /* TODO: is __v16qu portable to other compilers? */
        __v16qu mask;
        mask[0x00] = is_transparent[data_block_at_vol[0x00]] ? 0xFF : 0x00;
        mask[0x01] = is_transparent[data_block_at_vol[0x01]] ? 0xFF : 0x00;
        mask[0x02] = is_transparent[data_block_at_vol[0x02]] ? 0xFF : 0x00;
        mask[0x03] = is_transparent[data_block_at_vol[0x03]] ? 0xFF : 0x00;
        mask[0x04] = is_transparent[data_block_at_vol[0x04]] ? 0xFF : 0x00;
        mask[0x05] = is_transparent[data_block_at_vol[0x05]] ? 0xFF : 0x00;
        mask[0x06] = is_transparent[data_block_at_vol[0x06]] ? 0xFF : 0x00;
        mask[0x07] = is_transparent[data_block_at_vol[0x07]] ? 0xFF : 0x00;
        mask[0x08] = is_transparent[data_block_at_vol[0x08]] ? 0xFF : 0x00;
        mask[0x09] = is_transparent[data_block_at_vol[0x09]] ? 0xFF : 0x00;
        mask[0x0A] = is_transparent[data_block_at_vol[0x0A]] ? 0xFF : 0x00;
        mask[0x0B] = is_transparent[data_block_at_vol[0x0B]] ? 0xFF : 0x00;
        mask[0x0C] = is_transparent[data_block_at_vol[0x0C]] ? 0xFF : 0x00;
        mask[0x0D] = is_transparent[data_block_at_vol[0x0D]] ? 0xFF : 0x00;
        mask[0x0E] = is_transparent[data_block_at_vol[0x0E]] ? 0xFF : 0x00;
        mask[0x0F] = is_transparent[data_block_at_vol[0x0F]] ? 0xFF : 0x00;

        /** Index: +XYZ -XYZ, all values are *ALWAYS* in the range [1, 16] */
        __m128i sur_levels[6];

        switch (x)
        {
        case 0:
            sur_levels[0] = _mm_load_si128(data_light_at_vol + SUBCHUNK_SIZE_Z);
            sur_levels[3] = one;
            break;
        case (SUBCHUNK_SIZE_X - 1):
            sur_levels[0] = one;
            sur_levels[3] = _mm_load_si128(data_light_at_vol - SUBCHUNK_SIZE_Z);
            break;
        default:
            sur_levels[0] = _mm_load_si128(data_light_at_vol + SUBCHUNK_SIZE_Z);
            sur_levels[3] = _mm_load_si128(data_light_at_vol - SUBCHUNK_SIZE_Z);
            break;
        }

        switch (z)
        {
        case 0:
            sur_levels[2] = _mm_load_si128(data_light_at_vol + 1);
            sur_levels[5] = one;
            break;
        case (SUBCHUNK_SIZE_Z - 1):
            sur_levels[2] = one;
            sur_levels[5] = last_strip;
            break;
        default:
            sur_levels[2] = _mm_load_si128(data_light_at_vol + 1);
            sur_levels[5] = last_strip;
            break;
        }

        __m128i lvl = _mm_load_si128(data_light_at_vol);

        sur_levels[1] = _mm_bslli_si128(lvl, 1);
        sur_levels[4] = _mm_bsrli_si128(lvl, 1);

        verify_packed_8_in_range(lvl, 0, 15);
        verify_packed_8_in_range(sur_levels[0], 0, 15);
        verify_packed_8_in_range(sur_levels[1], 0, 15);
        verify_packed_8_in_range(sur_levels[2], 0, 15);
        verify_packed_8_in_range(sur_levels[3], 0, 15);
        verify_packed_8_in_range(sur_levels[4], 0, 15);
        verify_packed_8_in_range(sur_levels[5], 0, 15);

        /* Shift lvl from [0,15] to [1,16] */
        lvl = _mm_add_epi8(lvl, one);
        verify_packed_8_in_range(lvl, 1, 16);

        lvl = _mm_max_epu8(lvl, sur_levels[0]);
        lvl = _mm_max_epu8(lvl, sur_levels[1]);
        lvl = _mm_max_epu8(lvl, sur_levels[2]);
        lvl = _mm_max_epu8(lvl, sur_levels[3]);
        lvl = _mm_max_epu8(lvl, sur_levels[4]);
        lvl = _mm_max_epu8(lvl, sur_levels[5]);
        verify_packed_8_in_range(lvl, 1, 16);

        /* Move lvl from range [1,16] to [0,15] */
        lvl = _mm_sub_epi8(lvl, one);
        verify_packed_8_in_range(lvl, 0, 15);

        /* Mask out lvl */
        lvl = _mm_and_si128(lvl, (__m128i)mask);
        verify_packed_8_in_range(lvl, 0, 15);

        _mm_storeu_si128(data_light_at_vol, lvl);

        last_strip = lvl;
    }

    /* Repack light data */
    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_VOLUME; dat_it += 2)
        cross.c->data_light_block[dat_it >> 1] = (data_light[dat_it] & 0x0F) | ((data_light[dat_it + 1] & 0x0F) << 4);

    return;
bypass_sse2:
#endif

    /* Propagate light (Block) */
    for (int dat_it = SUBCHUNK_SIZE_VOLUME - 1; dat_it >= 0; dat_it--)
    {
        const int y = (dat_it) & 0x0F;
        const int z = (dat_it >> 4) & 0x0F;
        const int x = (dat_it >> 8) & 0x0F;

        if (!is_transparent[cross.c->get_type(x, y, z)])
            continue;

        /** Index: +XYZ -XYZ, all values are *ALWAYS* in the range [1, 16] */
        Uint8 sur_levels[6] = { 0 };

        switch (x)
        {
        case 0:
            sur_levels[0] = Uint8(cross.c->get_light_block(x + 1, y, z));
            sur_levels[3] = 0;
            break;
        case (SUBCHUNK_SIZE_X - 1):
            sur_levels[0] = 0;
            sur_levels[3] = Uint8(cross.c->get_light_block(x - 1, y, z));
            break;
        default:
            sur_levels[0] = Uint8(cross.c->get_light_block(x + 1, y, z));
            sur_levels[3] = Uint8(cross.c->get_light_block(x - 1, y, z));
            break;
        }

        switch (y)
        {
        case 0:
            sur_levels[1] = Uint8(cross.c->get_light_block(x, y + 1, z));
            sur_levels[4] = 0;
            break;
        case (SUBCHUNK_SIZE_Y - 1):
            sur_levels[1] = 0;
            sur_levels[4] = Uint8(cross.c->get_light_block(x, y - 1, z));
            break;
        default:
            sur_levels[1] = Uint8(cross.c->get_light_block(x, y + 1, z));
            sur_levels[4] = Uint8(cross.c->get_light_block(x, y - 1, z));
            break;
        }

        switch (z)
        {
        case 0:
            sur_levels[2] = Uint8(cross.c->get_light_block(x, y, z + 1));
            sur_levels[5] = 0;
            break;
        case (SUBCHUNK_SIZE_Z - 1):
            sur_levels[2] = 0;
            sur_levels[5] = Uint8(cross.c->get_light_block(x, y, z - 1));
            break;
        default:
            sur_levels[2] = Uint8(cross.c->get_light_block(x, y, z + 1));
            sur_levels[5] = Uint8(cross.c->get_light_block(x, y, z - 1));
            break;
        }

        /* Shift light value from range [0, 15] to [1, 16] */
        Uint8 lvl = cross.c->get_light_block(x, y, z) + 1;

        /* This SDL_max() isn't necessary because lvl is guaranteed to be at least one by now */
        // lvl = SDL_max(1, lvl);

        lvl = SDL_max(lvl, sur_levels[0]);
        lvl = SDL_max(lvl, sur_levels[1]);
        lvl = SDL_max(lvl, sur_levels[2]);
        lvl = SDL_max(lvl, sur_levels[3]);
        lvl = SDL_max(lvl, sur_levels[4]);
        lvl = SDL_max(lvl, sur_levels[5]);

        /* Move lvl from range [1,16] to [0,15] */
        lvl--;

        cross.c->set_light_block(x, y, z, lvl);
    }
}

void level_t::light_pass_sky(const int chunk_x, const int chunk_y, const int chunk_z, const bool local_only)
{
    /** Index: C +XYZ -XYZ */
    chunk_cross_t cross;

    auto it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(0, 0, 0));
    cross.c = ((it != cmap.end()) ? it->second : NULL);

    Uint8 top_light = 0;

    if (!local_only)
    {
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+1, +0, +0));
        cross.pos_x = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(-1, +0, +0));
        cross.neg_x = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, +1, +0));
        cross.pos_y = it != cmap.end() ? it->second : NULL;
        if (!cross.pos_y)
            top_light = 15;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, -1, +0));
        cross.neg_y = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, +0, +1));
        cross.pos_z = it != cmap.end() ? it->second : NULL;
        it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(+0, +0, -1));
        cross.neg_z = it != cmap.end() ? it->second : NULL;
    }

    if (!cross.c)
    {
        dc_log_error("Attempt made to update lighting for unloaded chunk at chunk coordinates: %d, %d, %d", chunk_x, chunk_y, chunk_z);
        return;
    }

    bool is_transparent[256];
    bool non_decaying_types[256] = { 1 };

    /* Move this out to something like mc_id::get_sky_light_attenuation() */
    non_decaying_types[BLOCK_ID_LEAVES] = 0;
    non_decaying_types[BLOCK_ID_WATER_FLOWING] = 0;
    non_decaying_types[BLOCK_ID_WATER_SOURCE] = 0;

    for (int i = 0; i < IM_ARRAYSIZE(is_transparent); i++)
        is_transparent[i] = mc_id::is_transparent(i);

    Uint32 total = 0;
    Uint8 min_level = 15;
    Uint8 max_level = 0;
    /* Initial scan of light levels */
    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_VOLUME; dat_it++)
    {
        const int y = (dat_it) & 0x0F;
        const int z = (dat_it >> 4) & 0x0F;
        const int x = (dat_it >> 8) & 0x0F;

        Uint8 type = cross.c->get_type(x, y, z);

        if (!is_transparent[type])
            continue;

        /** Index: +XYZ -XYZ, all values are *ALWAYS* in the range [1, 16] */
        Uint8 sur_levels[6] = { 0 };

        switch (x)
        {
        case 0:
            sur_levels[0] = Uint8(cross.c->get_light_sky(x + 1, y, z));
            sur_levels[3] = cross.neg_x ? (Uint8(cross.neg_x->get_light_sky(SUBCHUNK_SIZE_X - 1, y, z))) : 0;
            break;
        case (SUBCHUNK_SIZE_X - 1):
            sur_levels[0] = cross.pos_x ? (Uint8(cross.pos_x->get_light_sky(0, y, z))) : 0;
            sur_levels[3] = Uint8(cross.c->get_light_sky(x - 1, y, z));
            break;
        default:
            sur_levels[0] = Uint8(cross.c->get_light_sky(x + 1, y, z));
            sur_levels[3] = Uint8(cross.c->get_light_sky(x - 1, y, z));
            break;
        }

        switch (y)
        {
        case 0:
            sur_levels[1] = Uint8(cross.c->get_light_sky(x, y + 1, z));
            sur_levels[4] = cross.neg_y ? (Uint8(cross.neg_y->get_light_sky(x, SUBCHUNK_SIZE_Y - 1, z))) : 0;
            break;
        case (SUBCHUNK_SIZE_Y - 1):
            /* Fallback to full sky light if there is no chunk above */
            sur_levels[1] = cross.pos_y ? (Uint8(cross.pos_y->get_light_sky(x, 0, z))) : top_light;
            sur_levels[4] = Uint8(cross.c->get_light_sky(x, y - 1, z));
            break;
        default:
            sur_levels[1] = Uint8(cross.c->get_light_sky(x, y + 1, z));
            sur_levels[4] = Uint8(cross.c->get_light_sky(x, y - 1, z));
            break;
        }

        switch (z)
        {
        case 0:
            sur_levels[2] = Uint8(cross.c->get_light_sky(x, y, z + 1));
            sur_levels[5] = cross.neg_z ? (Uint8(cross.neg_z->get_light_sky(x, y, SUBCHUNK_SIZE_Z - 1))) : 0;
            break;
        case (SUBCHUNK_SIZE_Z - 1):
            sur_levels[2] = cross.pos_z ? (Uint8(cross.pos_z->get_light_sky(x, y, 0))) : 0;
            sur_levels[5] = Uint8(cross.c->get_light_sky(x, y, z - 1));
            break;
        default:
            sur_levels[2] = Uint8(cross.c->get_light_sky(x, y, z + 1));
            sur_levels[5] = Uint8(cross.c->get_light_sky(x, y, z - 1));
            break;
        }

        /* Shift light value from range [0, 15] to [1, 16] */
        Uint8 lvl = cross.c->get_light_sky(x, y, z) + 1;

        /* This SDL_max() isn't necessary because lvl is guaranteed to be at least one by now */
        // lvl = SDL_max(1, lvl);

        lvl = SDL_max(lvl, sur_levels[0]);
        lvl = SDL_max(lvl, sur_levels[2]);
        lvl = SDL_max(lvl, sur_levels[3]);
        lvl = SDL_max(lvl, sur_levels[4]);
        lvl = SDL_max(lvl, sur_levels[5]);

        /* Sky light does not decay downward through air */
        lvl = SDL_max(lvl, sur_levels[1] + non_decaying_types[type]);

        /* Move lvl from range [1,16] to [0,15] */
        lvl--;

        cross.c->set_light_sky(x, y, z, lvl);
        min_level = SDL_min(min_level, lvl);
        max_level = SDL_max(max_level, lvl);
        total += lvl;
    }

    /* If the light level is uniform then no propagation is required for a local pass */
    if (local_only && total % SUBCHUNK_SIZE_VOLUME == 0)
        return;

#if defined(__SSE2__) && LEVEL_T_ENABLE_SSE2
    const __m128i one = _mm_set1_epi8(1);
    verify_packed_8_in_range(one, 1, 1);

    if (!cvr_use_sse2.get())
        goto bypass_sse2;

    static_assert(SUBCHUNK_SIZE_X == 16);
    static_assert(SUBCHUNK_SIZE_Y == 16);
    static_assert(SUBCHUNK_SIZE_Z == 16);
    static_assert(sizeof(chunk_cubic_t::data_light_sky) == SUBCHUNK_SIZE_VOLUME / 2);
    static_assert(sizeof(__m128i) == (SUBCHUNK_INDEX(0, 16, 0) - SUBCHUNK_INDEX(0, 0, 0)));
    static_assert(sizeof(__m128i) * 16 == (SUBCHUNK_INDEX(0, 0, 16) - SUBCHUNK_INDEX(0, 0, 0)));

    /* Unpack light data */
    alignas(16) Uint8 data_light[SUBCHUNK_SIZE_VOLUME];
    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_VOLUME; dat_it += 2)
    {
        const Uint8 packed = cross.c->data_light_sky[dat_it >> 1];
        data_light[dat_it] = packed & 0x0F;
        data_light[dat_it + 1] = (packed >> 4) & 0x0F;
    }

    __m128i last_strip;

    /* Propagate light (SSE2) (Sky) */
    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_X * SUBCHUNK_SIZE_Z * (max_level - min_level); dat_it++)
    {
        const int vol_it = dat_it & 0xFF;
        const int z = (dat_it >> 0) & 0x0F;
        const int x = (dat_it >> 4) & 0x0F;

        const Uint8* const data_block_at_vol = cross.c->data_block + (vol_it << 4);
        __m128i* const data_light_at_vol = (__m128i*)data_light + vol_it;

        /* TODO: is __v16qu portable to other compilers? */
        __v16qu mask;
        mask[0x00] = is_transparent[data_block_at_vol[0x00]] ? 0xFF : 0x00;
        mask[0x01] = is_transparent[data_block_at_vol[0x01]] ? 0xFF : 0x00;
        mask[0x02] = is_transparent[data_block_at_vol[0x02]] ? 0xFF : 0x00;
        mask[0x03] = is_transparent[data_block_at_vol[0x03]] ? 0xFF : 0x00;
        mask[0x04] = is_transparent[data_block_at_vol[0x04]] ? 0xFF : 0x00;
        mask[0x05] = is_transparent[data_block_at_vol[0x05]] ? 0xFF : 0x00;
        mask[0x06] = is_transparent[data_block_at_vol[0x06]] ? 0xFF : 0x00;
        mask[0x07] = is_transparent[data_block_at_vol[0x07]] ? 0xFF : 0x00;
        mask[0x08] = is_transparent[data_block_at_vol[0x08]] ? 0xFF : 0x00;
        mask[0x09] = is_transparent[data_block_at_vol[0x09]] ? 0xFF : 0x00;
        mask[0x0A] = is_transparent[data_block_at_vol[0x0A]] ? 0xFF : 0x00;
        mask[0x0B] = is_transparent[data_block_at_vol[0x0B]] ? 0xFF : 0x00;
        mask[0x0C] = is_transparent[data_block_at_vol[0x0C]] ? 0xFF : 0x00;
        mask[0x0D] = is_transparent[data_block_at_vol[0x0D]] ? 0xFF : 0x00;
        mask[0x0E] = is_transparent[data_block_at_vol[0x0E]] ? 0xFF : 0x00;
        mask[0x0F] = is_transparent[data_block_at_vol[0x0F]] ? 0xFF : 0x00;

        __v16qu antidecay;
        antidecay[0x00] = non_decaying_types[data_block_at_vol[0x00]];
        antidecay[0x01] = non_decaying_types[data_block_at_vol[0x01]];
        antidecay[0x02] = non_decaying_types[data_block_at_vol[0x02]];
        antidecay[0x03] = non_decaying_types[data_block_at_vol[0x03]];
        antidecay[0x04] = non_decaying_types[data_block_at_vol[0x04]];
        antidecay[0x05] = non_decaying_types[data_block_at_vol[0x05]];
        antidecay[0x06] = non_decaying_types[data_block_at_vol[0x06]];
        antidecay[0x07] = non_decaying_types[data_block_at_vol[0x07]];
        antidecay[0x08] = non_decaying_types[data_block_at_vol[0x08]];
        antidecay[0x09] = non_decaying_types[data_block_at_vol[0x09]];
        antidecay[0x0A] = non_decaying_types[data_block_at_vol[0x0A]];
        antidecay[0x0B] = non_decaying_types[data_block_at_vol[0x0B]];
        antidecay[0x0C] = non_decaying_types[data_block_at_vol[0x0C]];
        antidecay[0x0D] = non_decaying_types[data_block_at_vol[0x0D]];
        antidecay[0x0E] = non_decaying_types[data_block_at_vol[0x0E]];
        antidecay[0x0F] = non_decaying_types[data_block_at_vol[0x0F]];

        /** Index: +XYZ -XYZ, all values are *ALWAYS* in the range [1, 16] */
        __m128i sur_levels[6];

        switch (x)
        {
        case 0:
            sur_levels[0] = _mm_load_si128(data_light_at_vol + SUBCHUNK_SIZE_Z);
            sur_levels[3] = one;
            break;
        case (SUBCHUNK_SIZE_X - 1):
            sur_levels[0] = one;
            sur_levels[3] = _mm_load_si128(data_light_at_vol - SUBCHUNK_SIZE_Z);
            break;
        default:
            sur_levels[0] = _mm_load_si128(data_light_at_vol + SUBCHUNK_SIZE_Z);
            sur_levels[3] = _mm_load_si128(data_light_at_vol - SUBCHUNK_SIZE_Z);
            break;
        }

        switch (z)
        {
        case 0:
            sur_levels[2] = _mm_load_si128(data_light_at_vol + 1);
            sur_levels[5] = one;
            break;
        case (SUBCHUNK_SIZE_Z - 1):
            sur_levels[2] = one;
            sur_levels[5] = last_strip;
            break;
        default:
            sur_levels[2] = _mm_load_si128(data_light_at_vol + 1);
            sur_levels[5] = last_strip;
            break;
        }

        __m128i lvl = _mm_load_si128(data_light_at_vol);

        sur_levels[4] = _mm_bslli_si128(lvl, 1);

        /* Sky light may or may not decay */
        sur_levels[1] = _mm_bsrli_si128(_mm_add_epi8(lvl, (__m128i)antidecay), 1);

        verify_packed_8_in_range(lvl, 0, 15);
        verify_packed_8_in_range(sur_levels[0], 0, 15);
        verify_packed_8_in_range(sur_levels[1], 0, 16);
        verify_packed_8_in_range(sur_levels[2], 0, 15);
        verify_packed_8_in_range(sur_levels[3], 0, 15);
        verify_packed_8_in_range(sur_levels[4], 0, 15);
        verify_packed_8_in_range(sur_levels[5], 0, 15);

        /* Shift lvl from [0,15] to [1,16] */
        lvl = _mm_add_epi8(lvl, one);
        verify_packed_8_in_range(lvl, 1, 16);

        lvl = _mm_max_epu8(lvl, sur_levels[0]);
        lvl = _mm_max_epu8(lvl, sur_levels[1]);
        lvl = _mm_max_epu8(lvl, sur_levels[2]);
        lvl = _mm_max_epu8(lvl, sur_levels[3]);
        lvl = _mm_max_epu8(lvl, sur_levels[4]);
        lvl = _mm_max_epu8(lvl, sur_levels[5]);
        verify_packed_8_in_range(lvl, 1, 16);

        /* Move lvl from range [1,16] to [0,15] */
        lvl = _mm_sub_epi8(lvl, one);
        verify_packed_8_in_range(lvl, 0, 15);

        /* Mask out lvl */
        lvl = _mm_and_si128(lvl, (__m128i)mask);
        verify_packed_8_in_range(lvl, 0, 15);

        _mm_storeu_si128(data_light_at_vol, lvl);

        last_strip = lvl;
    }

    /* Repack light data */
    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_VOLUME; dat_it += 2)
        cross.c->data_light_sky[dat_it >> 1] = (data_light[dat_it] & 0x0F) | ((data_light[dat_it + 1] & 0x0F) << 4);

    return;
bypass_sse2:
#endif

    /* Propagate light (Sky) */
    for (int dat_it = SUBCHUNK_SIZE_VOLUME - 1; dat_it >= 0; dat_it--)
    {
        const int y = (dat_it) & 0x0F;
        const int z = (dat_it >> 4) & 0x0F;
        const int x = (dat_it >> 8) & 0x0F;

        Uint8 type = cross.c->get_type(x, y, z);

        if (!is_transparent[type])
            continue;

        /** Index: +XYZ -XYZ, all values are *ALWAYS* in the range [1, 16] */
        Uint8 sur_levels[6] = { 0 };

        switch (x)
        {
        case 0:
            sur_levels[0] = Uint8(cross.c->get_light_sky(x + 1, y, z));
            sur_levels[3] = 0;
            break;
        case (SUBCHUNK_SIZE_X - 1):
            sur_levels[0] = 0;
            sur_levels[3] = Uint8(cross.c->get_light_sky(x - 1, y, z));
            break;
        default:
            sur_levels[0] = Uint8(cross.c->get_light_sky(x + 1, y, z));
            sur_levels[3] = Uint8(cross.c->get_light_sky(x - 1, y, z));
            break;
        }

        switch (y)
        {
        case 0:
            sur_levels[1] = Uint8(cross.c->get_light_sky(x, y + 1, z));
            sur_levels[4] = 0;
            break;
        case (SUBCHUNK_SIZE_Y - 1):
            sur_levels[1] = 0;
            sur_levels[4] = Uint8(cross.c->get_light_sky(x, y - 1, z));
            break;
        default:
            sur_levels[1] = Uint8(cross.c->get_light_sky(x, y + 1, z));
            sur_levels[4] = Uint8(cross.c->get_light_sky(x, y - 1, z));
            break;
        }

        switch (z)
        {
        case 0:
            sur_levels[2] = Uint8(cross.c->get_light_sky(x, y, z + 1));
            sur_levels[5] = 0;
            break;
        case (SUBCHUNK_SIZE_Z - 1):
            sur_levels[2] = 0;
            sur_levels[5] = Uint8(cross.c->get_light_sky(x, y, z - 1));
            break;
        default:
            sur_levels[2] = Uint8(cross.c->get_light_sky(x, y, z + 1));
            sur_levels[5] = Uint8(cross.c->get_light_sky(x, y, z - 1));
            break;
        }

        /* Shift light value from range [0, 15] to [1, 16] */
        Uint8 lvl = cross.c->get_light_sky(x, y, z) + 1;

        /* This SDL_max() isn't necessary because lvl is guaranteed to be at least one by now */
        // lvl = SDL_max(1, lvl);

        lvl = SDL_max(lvl, sur_levels[0]);
        lvl = SDL_max(lvl, sur_levels[2]);
        lvl = SDL_max(lvl, sur_levels[3]);
        lvl = SDL_max(lvl, sur_levels[4]);
        lvl = SDL_max(lvl, sur_levels[5]);

        /* Sky light does not decay downward through air */
        lvl = SDL_max(lvl, sur_levels[1] + non_decaying_types[type]);

        /* Move lvl from range [1,16] to [0,15] */
        lvl--;

        cross.c->set_light_sky(x, y, z, lvl);
    }
}
#if (FORCE_OPT_LIGHT)
#pragma GCC pop_options
#endif

#if (FORCE_OPT_MESH)
#pragma GCC push_options
#pragma GCC optimize("Og")
#endif

void level_t::build_mesh(const int chunk_x, const int chunk_y, const int chunk_z)
{
    /** Index: [x+1][y+1][z+1] */
    chunk_cubic_t* rubik[3][3][3];

    for (int i = 0; i < 27; i++)
    {
        const int ix = (i / 9) % 3 - 1;
        const int iy = (i / 3) % 3 - 1;
        const int iz = i % 3 - 1;

        auto it = cmap.find(glm::ivec3(chunk_x, chunk_y, chunk_z) + glm::ivec3(ix, iy, iz));
        rubik[ix + 1][iy + 1][iz + 1] = ((it != cmap.end()) ? it->second : NULL);
    }

    chunk_cubic_t* center = rubik[1][1][1];

    if (!center)
    {
        dc_log_error("Attempt made to build unloaded chunk at chunk coordinates: %d, %d, %d", chunk_x, chunk_y, chunk_z);
        return;
    }

    if (!terrain)
    {
        dc_log_error("A texture atlas is required to build a chunk");
        return;
    }

    std::vector<terrain_vertex_t> vtx_solid;
    std::vector<terrain_vertex_t> vtx_overlay;
    std::vector<terrain_vertex_t> vtx_translucent;

    bool is_transparent[256];
    bool is_translucent[256];
    bool is_leaves_style_transparent[256];

    for (int i = 0; i < IM_ARRAYSIZE(is_transparent); i++)
        is_transparent[i] = mc_id::is_transparent(i);

    for (int i = 0; i < IM_ARRAYSIZE(is_translucent); i++)
        is_translucent[i] = mc_id::is_translucent(i);

    for (int i = 0; i < IM_ARRAYSIZE(is_leaves_style_transparent); i++)
        is_leaves_style_transparent[i] = mc_id::is_leaves_style_transparent(i);

    /** Index: [x+1][y+1][z+1] */
    block_id_t stypes[3][3][3];
    Uint8 smetadata[3][3][3];
    Uint8 slight_block[3][3][3];
    Uint8 slight_sky[3][3][3];

    bool skipped = true;

    for (int dat_it = 0; dat_it < SUBCHUNK_SIZE_X * SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z; dat_it++)
    {
        /* This is to keep the loop body 8 spaces closer to the left margin */
        const int y = (dat_it) & 0x0F;
        const int z = (dat_it >> 4) & 0x0F;
        const int x = (dat_it >> 8) & 0x0F;

        block_id_t type = rubik[1][1][1]->get_type(x, y, z);
        if (type == BLOCK_ID_AIR)
        {
            skipped = true;
            continue;
        }

        std::vector<terrain_vertex_t>* vtx = is_translucent[type] ? &vtx_translucent : &vtx_solid;

        float r = 1.0f, g = 1.0f, b = 1.0f;
        float r_overlay = r, g_overlay = g, b_overlay = b;

        /* std::move does actually increase speed in non-debug situations, don't remove it */
#define SHIFT_BLOCK_INFO(ORG, NEW)                      \
    do                                                  \
    {                                                   \
        stypes NEW = std::move(stypes ORG);             \
        smetadata NEW = std::move(smetadata ORG);       \
        slight_block NEW = std::move(slight_block ORG); \
        slight_sky NEW = std::move(slight_sky ORG);     \
    } while (0)

        /* The last set of block data was in a different vertical slice, therefore the last block should be considered skipped */
        if (y == 0)
            skipped = true;

        /* Shift data from previous slices */
        if (!skipped)
        {
            SHIFT_BLOCK_INFO([0][1][0], [0][0][0]);
            SHIFT_BLOCK_INFO([1][1][0], [1][0][0]);
            SHIFT_BLOCK_INFO([2][1][0], [2][0][0]);
            SHIFT_BLOCK_INFO([0][1][1], [0][0][1]);
            SHIFT_BLOCK_INFO([1][1][1], [1][0][1]);
            SHIFT_BLOCK_INFO([2][1][1], [2][0][1]);
            SHIFT_BLOCK_INFO([0][1][2], [0][0][2]);
            SHIFT_BLOCK_INFO([1][1][2], [1][0][2]);
            SHIFT_BLOCK_INFO([2][1][2], [2][0][2]);

            SHIFT_BLOCK_INFO([0][2][0], [0][1][0]);
            SHIFT_BLOCK_INFO([1][2][0], [1][1][0]);
            SHIFT_BLOCK_INFO([2][2][0], [2][1][0]);
            SHIFT_BLOCK_INFO([0][2][1], [0][1][1]);
            SHIFT_BLOCK_INFO([1][2][1], [1][1][1]);
            SHIFT_BLOCK_INFO([2][2][1], [2][1][1]);
            SHIFT_BLOCK_INFO([0][2][2], [0][1][2]);
            SHIFT_BLOCK_INFO([1][2][2], [1][1][2]);
            SHIFT_BLOCK_INFO([2][2][2], [2][1][2]);
        }
#undef MOVE_WINDOW

        /* When the last block was not skipped then the last slices are valid we do no need to collect the slices of information at y-1 or at y */
        for (int j = ((skipped) ? -1 : 1); j < 2; j++)
        {
            int chunk_iy = 1;
            int local_y = y + j;

            switch (local_y)
            {
            case -1:
                local_y = SUBCHUNK_SIZE_Y - 1;
                chunk_iy--;
                break;
            case SUBCHUNK_SIZE_Y:
                local_y = 0;
                chunk_iy++;
                break;
            default:
                break;
            }

            for (int i = -1; i < 2; i++)
            {
                for (int k = -1; k < 2; k++)
                {
                    int chunk_ix = 1, chunk_iz = 1;
                    int local_x = x + i, local_z = z + k;

                    switch (local_x)
                    {
                    case -1:
                        local_x = SUBCHUNK_SIZE_X - 1;
                        chunk_ix--;
                        break;
                    case SUBCHUNK_SIZE_X:
                        local_x = 0;
                        chunk_ix++;
                        break;
                    default:
                        break;
                    }

                    switch (local_z)
                    {
                    case -1:
                        local_z = SUBCHUNK_SIZE_Z - 1;
                        chunk_iz--;
                        break;
                    case SUBCHUNK_SIZE_Z:
                        local_z = 0;
                        chunk_iz++;
                        break;
                    default:
                        break;
                    }

                    chunk_cubic_t* c = rubik[chunk_ix][chunk_iy][chunk_iz];
                    stypes[i + 1][j + 1][k + 1] = c == NULL ? BLOCK_ID_AIR : c->get_type(local_x, local_y, local_z);
                    smetadata[i + 1][j + 1][k + 1] = c == NULL ? 0 : c->get_metadata(local_x, local_y, local_z);
                    slight_block[i + 1][j + 1][k + 1] = c == NULL ? 0 : c->get_light_block(local_x, local_y, local_z);
                    slight_sky[i + 1][j + 1][k + 1] = c == NULL ? 0 : c->get_light_sky(local_x, local_y, local_z);
                }
            }
        }
        skipped = false;

        Uint8 metadata = smetadata[1][1][1];

        /** Ordered +XYZ then -XYZ for simple blocks */
        mc_id::terrain_face_t faces[6];
        /** Ordered +XYZ then -XYZ for simple blocks */
        mc_id::terrain_face_t faces_overlay[6];
        /** Ordered +XYZ then -XYZ for simple blocks */
        bool use_overlay[6] = { 0 };
#define BLOCK_SIMPLE_NO_CASE(ID_FACE)          \
    do                                         \
    {                                          \
        faces[0] = terrain->get_face(ID_FACE); \
        faces[1] = faces[0];                   \
        faces[2] = faces[0];                   \
        faces[3] = faces[0];                   \
        faces[4] = faces[0];                   \
        faces[5] = faces[0];                   \
    } while (0)
#define BLOCK_SIMPLE(ID_BLOCK, ID_FACE)        \
    case ID_BLOCK:                             \
    {                                          \
        faces[0] = terrain->get_face(ID_FACE); \
        faces[1] = faces[0];                   \
        faces[2] = faces[0];                   \
        faces[3] = faces[0];                   \
        faces[4] = faces[0];                   \
        faces[5] = faces[0];                   \
        break;                                 \
    }
#define BLOCK_TNT(ID_BLOCK, ID_FACE_TOP, ID_FACE_BOTTOM, ID_FACE_SIDE) \
    case ID_BLOCK:                                                     \
    {                                                                  \
        faces[0] = terrain->get_face(ID_FACE_SIDE);                    \
        faces[1] = terrain->get_face(ID_FACE_TOP);                     \
        faces[2] = faces[0];                                           \
        faces[3] = faces[0];                                           \
        faces[4] = terrain->get_face(ID_FACE_BOTTOM);                  \
        faces[5] = faces[0];                                           \
        break;                                                         \
    }
#define BLOCK_LOG(ID_BLOCK, ID_FACE_TOP, ID_FACE_SIDE) \
    case ID_BLOCK:                                     \
    {                                                  \
        faces[0] = terrain->get_face(ID_FACE_SIDE);    \
        faces[1] = terrain->get_face(ID_FACE_TOP);     \
        faces[2] = faces[0];                           \
        faces[3] = faces[0];                           \
        faces[4] = faces[1];                           \
        faces[5] = faces[0];                           \
        break;                                         \
    }

        switch ((item_id_t_)type)
        {
            BLOCK_SIMPLE(BLOCK_ID_AIR, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_STONE, mc_id::FACE_STONE);
        case BLOCK_ID_GRASS:
        {
            BLOCK_SIMPLE_NO_CASE(mc_id::FACE_DIRT);

            faces_overlay[0] = terrain->get_face(mc_id::FACE_GRASS_SIDE_OVERLAY);
            faces_overlay[1] = terrain->get_face(mc_id::FACE_GRASS_TOP);
            faces_overlay[2] = faces_overlay[0];
            faces_overlay[3] = faces_overlay[0];
            faces_overlay[5] = faces_overlay[0];

            use_overlay[0] = true;
            use_overlay[1] = true;
            use_overlay[2] = true;
            use_overlay[3] = true;
            use_overlay[5] = true;

            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_DIRT, mc_id::FACE_DIRT);
            BLOCK_SIMPLE(BLOCK_ID_COBBLESTONE, mc_id::FACE_COBBLESTONE);
        case BLOCK_ID_WOOD_PLANKS:
        {
            /* Multiple plank types are not in Minecraft Beta 1.8.x */
            switch (metadata)
            {
                BLOCK_SIMPLE(WOOD_ID_SPRUCE, mc_id::FACE_PLANKS_SPRUCE);
                BLOCK_SIMPLE(WOOD_ID_BIRCH, mc_id::FACE_PLANKS_BIRCH);
            default: /* Fall through */
                BLOCK_SIMPLE(WOOD_ID_OAK, mc_id::FACE_PLANKS_OAK);
            }
            break;
        }
        case BLOCK_ID_SAPLING:
        {
            switch (metadata % 4)
            {
                BLOCK_SIMPLE(WOOD_ID_SPRUCE, mc_id::FACE_SAPLING_SPRUCE);
                BLOCK_SIMPLE(WOOD_ID_BIRCH, mc_id::FACE_SAPLING_BIRCH);
            default: /* Fall through */
                BLOCK_SIMPLE(WOOD_ID_OAK, mc_id::FACE_SAPLING_OAK);
            }
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_BEDROCK, mc_id::FACE_BEDROCK);

            // BLOCK_SIMPLE(BLOCK_ID_WATER_FLOWING, mc_id::FACE_WATER_FLOW);
            // BLOCK_SIMPLE(BLOCK_ID_WATER_SOURCE, mc_id::FACE_WATER_STILL);
            // BLOCK_SIMPLE(BLOCK_ID_LAVA_FLOWING, mc_id::FACE_LAVA_FLOW);
            // BLOCK_SIMPLE(BLOCK_ID_LAVA_SOURCE, mc_id::FACE_LAVA_STILL);

            BLOCK_SIMPLE(BLOCK_ID_SAND, mc_id::FACE_SAND);
            BLOCK_SIMPLE(BLOCK_ID_GRAVEL, mc_id::FACE_GRAVEL);
            BLOCK_SIMPLE(BLOCK_ID_ORE_GOLD, mc_id::FACE_GOLD_ORE);
            BLOCK_SIMPLE(BLOCK_ID_ORE_IRON, mc_id::FACE_IRON_ORE);
            BLOCK_SIMPLE(BLOCK_ID_ORE_COAL, mc_id::FACE_COAL_ORE);
        case BLOCK_ID_LOG:
        {
            switch (metadata)
            {
                BLOCK_LOG(WOOD_ID_SPRUCE, mc_id::FACE_LOG_SPRUCE_TOP, mc_id::FACE_LOG_SPRUCE);
                BLOCK_LOG(WOOD_ID_BIRCH, mc_id::FACE_LOG_BIRCH_TOP, mc_id::FACE_LOG_BIRCH);
            default: /* Fall through */
                BLOCK_LOG(WOOD_ID_OAK, mc_id::FACE_LOG_OAK_TOP, mc_id::FACE_LOG_OAK);
            }
            break;
        }
        case BLOCK_ID_LEAVES:
        {
            switch (metadata)
            {
                BLOCK_SIMPLE(WOOD_ID_SPRUCE, mc_id::FACE_LEAVES_SPRUCE);
                BLOCK_SIMPLE(WOOD_ID_BIRCH, mc_id::FACE_LEAVES_BIRCH);
            default: /* Fall through */
                BLOCK_SIMPLE(WOOD_ID_OAK, mc_id::FACE_LEAVES_OAK);
            }
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_SPONGE, mc_id::FACE_SPONGE);
            BLOCK_SIMPLE(BLOCK_ID_GLASS, mc_id::FACE_GLASS);
            BLOCK_SIMPLE(BLOCK_ID_ORE_LAPIS, mc_id::FACE_LAPIS_ORE);

            BLOCK_SIMPLE(BLOCK_ID_LAPIS, mc_id::FACE_LAPIS_BLOCK);
        case BLOCK_ID_DISPENSER:
        {
            mc_id::terrain_face_t face_front = terrain->get_face(mc_id::FACE_DISPENSER_FRONT_HORIZONTAL);
            mc_id::terrain_face_t face_side = terrain->get_face(mc_id::FACE_FURNACE_SIDE);

            faces[1] = terrain->get_face(mc_id::FACE_FURNACE_TOP);
            faces[4] = faces[1];

            faces[5] = metadata == 2 ? face_front : face_side;
            faces[2] = metadata == 3 ? face_front : face_side;
            faces[3] = metadata == 4 ? face_front : face_side;
            faces[0] = metadata == 5 ? face_front : face_side;

            break;
        }
            BLOCK_TNT(BLOCK_ID_SANDSTONE, mc_id::FACE_SANDSTONE_TOP, mc_id::FACE_SANDSTONE_BOTTOM, mc_id::FACE_SANDSTONE_NORMAL)
            BLOCK_SIMPLE(BLOCK_ID_NOTE_BLOCK, mc_id::FACE_NOTEBLOCK);
            BLOCK_SIMPLE(BLOCK_ID_BED, mc_id::FACE_BED_HEAD_TOP);
            BLOCK_SIMPLE(BLOCK_ID_RAIL_POWERED, mc_id::FACE_RAIL_GOLDEN_POWERED);
            BLOCK_SIMPLE(BLOCK_ID_RAIL_DETECTOR, mc_id::FACE_RAIL_DETECTOR);
            BLOCK_SIMPLE(BLOCK_ID_PISTON_STICKY, mc_id::FACE_PISTON_TOP_STICKY);
            BLOCK_SIMPLE(BLOCK_ID_COBWEB, mc_id::FACE_WEB);
        case BLOCK_ID_FOLIAGE:
        {
            switch (metadata)
            {
                BLOCK_SIMPLE(0, mc_id::FACE_DEADBUSH);
                BLOCK_SIMPLE(1, mc_id::FACE_FERN);
            default: /* Fall through */
                BLOCK_SIMPLE(2, mc_id::FACE_TALLGRASS);
            }
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_DEAD_BUSH, mc_id::FACE_DEADBUSH);
            BLOCK_SIMPLE(BLOCK_ID_PISTON, mc_id::FACE_PISTON_TOP_NORMAL);
            BLOCK_SIMPLE(BLOCK_ID_PISTON_HEAD, mc_id::FACE_PISTON_TOP_NORMAL);
        case BLOCK_ID_WOOL:
        {
            switch (metadata)
            {
                BLOCK_SIMPLE(WOOL_ID_WHITE, mc_id::FACE_WOOL_COLORED_WHITE)
                BLOCK_SIMPLE(WOOL_ID_ORANGE, mc_id::FACE_WOOL_COLORED_ORANGE)
                BLOCK_SIMPLE(WOOL_ID_MAGENTA, mc_id::FACE_WOOL_COLORED_MAGENTA)
                BLOCK_SIMPLE(WOOL_ID_LIGHT_BLUE, mc_id::FACE_WOOL_COLORED_LIGHT_BLUE)
                BLOCK_SIMPLE(WOOL_ID_YELLOW, mc_id::FACE_WOOL_COLORED_YELLOW)
                BLOCK_SIMPLE(WOOL_ID_LIME, mc_id::FACE_WOOL_COLORED_LIME)
                BLOCK_SIMPLE(WOOL_ID_PINK, mc_id::FACE_WOOL_COLORED_PINK)
                BLOCK_SIMPLE(WOOL_ID_GRAY, mc_id::FACE_WOOL_COLORED_GRAY)
                BLOCK_SIMPLE(WOOL_ID_LIGHT_GRAY, mc_id::FACE_WOOL_COLORED_SILVER)
                BLOCK_SIMPLE(WOOL_ID_CYAN, mc_id::FACE_WOOL_COLORED_CYAN)
                BLOCK_SIMPLE(WOOL_ID_PURPLE, mc_id::FACE_WOOL_COLORED_PURPLE)
                BLOCK_SIMPLE(WOOL_ID_BLUE, mc_id::FACE_WOOL_COLORED_BLUE)
                BLOCK_SIMPLE(WOOL_ID_BROWN, mc_id::FACE_WOOL_COLORED_BROWN)
                BLOCK_SIMPLE(WOOL_ID_GREEN, mc_id::FACE_WOOL_COLORED_GREEN)
                BLOCK_SIMPLE(WOOL_ID_RED, mc_id::FACE_WOOL_COLORED_RED)
                BLOCK_SIMPLE(WOOL_ID_BLACK, mc_id::FACE_WOOL_COLORED_BLACK)
            }
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_UNKNOWN, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_FLOWER_YELLOW, mc_id::FACE_FLOWER_DANDELION);
            BLOCK_SIMPLE(BLOCK_ID_FLOWER_RED, mc_id::FACE_FLOWER_ROSE);
            BLOCK_SIMPLE(BLOCK_ID_MUSHROOM_BLAND, mc_id::FACE_MUSHROOM_BROWN);
            BLOCK_SIMPLE(BLOCK_ID_MUSHROOM_RED, mc_id::FACE_MUSHROOM_RED);

            BLOCK_SIMPLE(BLOCK_ID_GOLD, mc_id::FACE_GOLD_BLOCK);
            BLOCK_SIMPLE(BLOCK_ID_IRON, mc_id::FACE_IRON_BLOCK);

        case BLOCK_ID_SLAB_DOUBLE:
        case BLOCK_ID_SLAB_SINGLE:
        {
            switch (metadata)
            {
                BLOCK_LOG(SLAB_ID_SMOOTH, mc_id::FACE_STONE_SLAB_TOP, mc_id::FACE_STONE_SLAB_SIDE);
                BLOCK_TNT(SLAB_ID_SANDSTONE, mc_id::FACE_SANDSTONE_TOP, mc_id::FACE_SANDSTONE_BOTTOM, mc_id::FACE_SANDSTONE_NORMAL);
                BLOCK_SIMPLE(SLAB_ID_WOOD, mc_id::FACE_PLANKS_OAK);
                BLOCK_SIMPLE(SLAB_ID_COBBLESTONE, mc_id::FACE_COBBLESTONE);
                BLOCK_SIMPLE(SLAB_ID_BRICKS, mc_id::FACE_BRICK);
                BLOCK_SIMPLE(SLAB_ID_BRICKS_STONE, mc_id::FACE_STONEBRICK);
            default: /* Fall through */
                BLOCK_SIMPLE(SLAB_ID_SMOOTH_SIDE, mc_id::FACE_STONE_SLAB_TOP);
            }
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_BRICKS, mc_id::FACE_BRICK);
            BLOCK_TNT(BLOCK_ID_TNT, mc_id::FACE_TNT_TOP, mc_id::FACE_TNT_BOTTOM, mc_id::FACE_TNT_SIDE)
            BLOCK_SIMPLE(BLOCK_ID_BOOKSHELF, mc_id::FACE_BOOKSHELF);
            BLOCK_SIMPLE(BLOCK_ID_COBBLESTONE_MOSSY, mc_id::FACE_COBBLESTONE_MOSSY);
            BLOCK_SIMPLE(BLOCK_ID_OBSIDIAN, mc_id::FACE_OBSIDIAN);

            BLOCK_SIMPLE(BLOCK_ID_TORCH, mc_id::FACE_TORCH_ON);
            BLOCK_SIMPLE(BLOCK_ID_FIRE, mc_id::FACE_FIRE_LAYER_0);
            BLOCK_SIMPLE(BLOCK_ID_SPAWNER, mc_id::FACE_MOB_SPAWNER);

            BLOCK_SIMPLE(BLOCK_ID_STAIRS_WOOD, mc_id::FACE_PLANKS_OAK); //----
            BLOCK_SIMPLE(BLOCK_ID_CHEST, mc_id::FACE_DEBUG); //----
        case BLOCK_ID_REDSTONE:
        {
            r = (metadata + 5) / 20.0f;
            g = 0.0f;
            b = 0.0f;
            faces[0] = terrain->get_face(mc_id::FACE_REDSTONE_DUST_LINE);
            faces[1] = faces[0];
            faces[2] = faces[0];
            faces[3] = faces[0];
            faces[4] = faces[0];
            faces[5] = faces[0];
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_ORE_DIAMOND, mc_id::FACE_DIAMOND_ORE);
            BLOCK_SIMPLE(BLOCK_ID_DIAMOND, mc_id::FACE_DIAMOND_BLOCK);
        case BLOCK_ID_CRAFTING_TABLE:
        {
            faces[0] = terrain->get_face(mc_id::FACE_CRAFTING_TABLE_FRONT);
            faces[1] = terrain->get_face(mc_id::FACE_CRAFTING_TABLE_TOP);
            faces[2] = terrain->get_face(mc_id::FACE_CRAFTING_TABLE_SIDE);

            faces[3] = terrain->get_face(mc_id::FACE_CRAFTING_TABLE_FRONT);
            faces[4] = terrain->get_face(mc_id::FACE_PLANKS_OAK);
            faces[5] = terrain->get_face(mc_id::FACE_CRAFTING_TABLE_SIDE);
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_PLANT_FOOD, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_DIRT_TILLED, mc_id::FACE_DEBUG);
        case BLOCK_ID_FURNACE_OFF: /* Fall through */
        case BLOCK_ID_FURNACE_ON:
        {
            mc_id::terrain_face_t face_front = terrain->get_face(type == BLOCK_ID_FURNACE_OFF ? mc_id::FACE_FURNACE_FRONT_OFF : mc_id::FACE_FURNACE_FRONT_ON);
            mc_id::terrain_face_t face_side = terrain->get_face(mc_id::FACE_FURNACE_SIDE);

            faces[1] = terrain->get_face(mc_id::FACE_FURNACE_TOP);
            faces[4] = faces[1];

            faces[5] = metadata == 2 ? face_front : face_side;
            faces[2] = metadata == 3 ? face_front : face_side;
            faces[3] = metadata == 4 ? face_front : face_side;
            faces[0] = metadata == 5 ? face_front : face_side;

            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_SIGN_STANDING, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_DOOR_WOOD, mc_id::FACE_DOOR_WOOD_UPPER);
            BLOCK_SIMPLE(BLOCK_ID_LADDER, mc_id::FACE_LADDER);
            BLOCK_SIMPLE(BLOCK_ID_RAIL, mc_id::FACE_RAIL_NORMAL);
            BLOCK_SIMPLE(BLOCK_ID_STAIRS_COBBLESTONE, mc_id::FACE_COBBLESTONE); //----
            BLOCK_SIMPLE(BLOCK_ID_SIGN_WALL, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_LEVER, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_PRESSURE_PLATE_STONE, mc_id::FACE_STONE);
            BLOCK_SIMPLE(BLOCK_ID_DOOR_IRON, mc_id::FACE_DOOR_IRON_UPPER);
            BLOCK_SIMPLE(BLOCK_ID_PRESSURE_PLATE_WOOD, mc_id::FACE_PLANKS_OAK);
            BLOCK_SIMPLE(BLOCK_ID_ORE_REDSTONE_OFF, mc_id::FACE_REDSTONE_ORE);
            BLOCK_SIMPLE(BLOCK_ID_ORE_REDSTONE_ON, mc_id::FACE_REDSTONE_ORE);
            BLOCK_SIMPLE(BLOCK_ID_TORCH_REDSTONE_OFF, mc_id::FACE_REDSTONE_TORCH_OFF);
            BLOCK_SIMPLE(BLOCK_ID_TORCH_REDSTONE_ON, mc_id::FACE_REDSTONE_TORCH_ON);
            BLOCK_SIMPLE(BLOCK_ID_BUTTON_STONE, mc_id::FACE_STONE);
            BLOCK_SIMPLE(BLOCK_ID_SNOW, mc_id::FACE_SNOW);
            BLOCK_SIMPLE(BLOCK_ID_ICE, mc_id::FACE_ICE);
            BLOCK_SIMPLE(BLOCK_ID_SNOW_BLOCK, mc_id::FACE_SNOW);
            BLOCK_SIMPLE(BLOCK_ID_CACTUS, mc_id::FACE_CACTUS_SIDE);
            BLOCK_SIMPLE(BLOCK_ID_CLAY, mc_id::FACE_CLAY);
            BLOCK_SIMPLE(BLOCK_ID_SUGAR_CANE, mc_id::FACE_REEDS);
        case BLOCK_ID_JUKEBOX:
        {
            faces[0] = terrain->get_face(mc_id::FACE_JUKEBOX_SIDE);
            faces[1] = terrain->get_face(mc_id::FACE_JUKEBOX_TOP);
            faces[2] = terrain->get_face(mc_id::FACE_JUKEBOX_SIDE);

            faces[3] = terrain->get_face(mc_id::FACE_JUKEBOX_SIDE);
            faces[4] = terrain->get_face(mc_id::FACE_JUKEBOX_SIDE);
            faces[5] = terrain->get_face(mc_id::FACE_JUKEBOX_SIDE);
            break;
        }
        case BLOCK_ID_FENCE_WOOD:
        {
            /* Multiple fence types are not in Minecraft Beta 1.8.x */
            switch (metadata)
            {
                BLOCK_SIMPLE(WOOD_ID_SPRUCE, mc_id::FACE_PLANKS_SPRUCE);
                BLOCK_SIMPLE(WOOD_ID_BIRCH, mc_id::FACE_PLANKS_BIRCH);
            default: /* Fall through */
                BLOCK_SIMPLE(WOOD_ID_OAK, mc_id::FACE_PLANKS_OAK);
            }
            break;
        }
        case BLOCK_ID_PUMPKIN:
        {
            faces[0] = terrain->get_face(mc_id::FACE_PUMPKIN_FACE_OFF);
            faces[1] = terrain->get_face(mc_id::FACE_PUMPKIN_TOP);
            faces[2] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);

            faces[3] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);
            faces[4] = terrain->get_face(mc_id::FACE_PUMPKIN_TOP);
            faces[5] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_NETHERRACK, mc_id::FACE_NETHERRACK);
            BLOCK_SIMPLE(BLOCK_ID_SOULSAND, mc_id::FACE_SOUL_SAND);
            BLOCK_SIMPLE(BLOCK_ID_GLOWSTONE, mc_id::FACE_GLOWSTONE);
            BLOCK_SIMPLE(BLOCK_ID_NETHER_PORTAL, mc_id::FACE_PORTAL);
        case BLOCK_ID_PUMPKIN_GLOWING:
        {
            faces[0] = terrain->get_face(mc_id::FACE_PUMPKIN_FACE_ON);
            faces[1] = terrain->get_face(mc_id::FACE_PUMPKIN_TOP);
            faces[2] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);

            faces[3] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);
            faces[4] = terrain->get_face(mc_id::FACE_PUMPKIN_TOP);
            faces[5] = terrain->get_face(mc_id::FACE_PUMPKIN_SIDE);
            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_CAKE, mc_id::FACE_CAKE_TOP);
            BLOCK_SIMPLE(BLOCK_ID_REPEATER_OFF, mc_id::FACE_REPEATER_OFF);
            BLOCK_SIMPLE(BLOCK_ID_REPEATER_ON, mc_id::FACE_REPEATER_ON);
            BLOCK_SIMPLE(BLOCK_ID_CHEST_LOCKED, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_TRAPDOOR, mc_id::FACE_TRAPDOOR);
        case BLOCK_ID_UNKNOWN_STONE:
        {
            switch (metadata)
            {
                BLOCK_SIMPLE(1, mc_id::FACE_COBBLESTONE);
                BLOCK_SIMPLE(2, mc_id::FACE_STONEBRICK);
            default: /* Fall through */
                BLOCK_SIMPLE(0, mc_id::FACE_STONE);
            }
            break;
        }
        case BLOCK_ID_BRICKS_STONE:
        {
            switch (metadata)
            {
                BLOCK_SIMPLE(STONE_BRICK_ID_MOSSY, mc_id::FACE_STONEBRICK_MOSSY);
                BLOCK_SIMPLE(STONE_BRICK_ID_CRACKED, mc_id::FACE_STONEBRICK_CRACKED);
            default: /* Fall through */
                BLOCK_SIMPLE(STONE_BRICK_ID_REGULAR, mc_id::FACE_STONEBRICK);
            }
            break;
        }
        case BLOCK_ID_MUSHROOM_BLOCK_BLAND:
        case BLOCK_ID_MUSHROOM_BLOCK_RED:
        {
            faces[0] = terrain->get_face(mc_id::FACE_MUSHROOM_BLOCK_INSIDE);
            faces[1] = faces[0];
            faces[2] = faces[0];
            faces[3] = faces[0];
            faces[4] = faces[0];
            faces[5] = faces[0];

            if (metadata == 0 || WOOL_ID_BLUE <= metadata)
                break;
            else if (metadata == WOOL_ID_PURPLE)
            {
                faces[0] = terrain->get_face(mc_id::FACE_MUSHROOM_BLOCK_SKIN_STEM);
                faces[2] = faces[0], faces[3] = faces[0], faces[5] = faces[0];
            }
            else
            {
                bool is_red = type == BLOCK_ID_MUSHROOM_BLOCK_RED;

                mc_id::terrain_face_t mushroom_skin = terrain->get_face(is_red ? mc_id::FACE_MUSHROOM_BLOCK_SKIN_RED : mc_id::FACE_MUSHROOM_BLOCK_SKIN_BROWN);

                if (metadata > 0 && (metadata + 1) % 3 == 1)
                    faces[0] = mushroom_skin;

                if (1 <= metadata && metadata <= 9)
                    faces[1] = mushroom_skin;

                if (7 <= metadata && metadata <= 9)
                    faces[2] = mushroom_skin;

                if (metadata % 3 == 1)
                    faces[3] = mushroom_skin;

                if (1 <= metadata && metadata <= 3)
                    faces[5] = mushroom_skin;
            }

            break;
        }
            BLOCK_SIMPLE(BLOCK_ID_IRON_BARS, mc_id::FACE_IRON_BARS);
            BLOCK_SIMPLE(BLOCK_ID_GLASS_PANE, mc_id::FACE_GLASS);
            BLOCK_LOG(BLOCK_ID_MELON, mc_id::FACE_MELON_TOP, mc_id::FACE_MELON_SIDE)
            BLOCK_SIMPLE(BLOCK_ID_STEM_PUMPKIN, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_STEM_MELON, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_MOSS, mc_id::FACE_VINE);
            BLOCK_SIMPLE(BLOCK_ID_GATE, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_STAIRS_BRICK, mc_id::FACE_DEBUG);
            BLOCK_SIMPLE(BLOCK_ID_STAIRS_BRICK_STONE, mc_id::FACE_DEBUG);
        default:
        {
            faces[0] = terrain->get_face(mc_id::FACE_DEBUG);
            faces[1] = faces[0];
            faces[2] = faces[0];
            faces[3] = faces[0];
            faces[4] = faces[0];
            faces[5] = faces[0];
            break;
        }
        }

        if (type == BLOCK_ID_GRASS)
            r_overlay = 0.2f, g_overlay = 0.8f, b_overlay = 0.2f;
        if (type == BLOCK_ID_MOSS || type == BLOCK_ID_FOLIAGE)
            r = 0.2f, g = 0.8f, b = 0.2f;
        else if (type == BLOCK_ID_LEAVES)
        {
            switch (metadata)
            {
            case WOOD_ID_SPRUCE:
                r = 0.380f, g = 0.600f, b = 0.380f;
                break;
            case WOOD_ID_BIRCH:
                r = 0.502f, g = 0.655f, b = 0.333f;
                break;
            case WOOD_ID_OAK: /* Fall through */
            default:
                r = 0.2f, g = 1.0f, b = 0.2f;
                break;
            }
        }

#define UAO(X) (!is_transparent[stypes X])
#define UBL(X) (slight_block X * is_transparent[stypes X])
#define USL(X) (slight_sky X * is_transparent[stypes X])

        /* ============ BEGIN: IS_TORCH ============ */
        if (type == BLOCK_ID_TORCH || type == BLOCK_ID_TORCH_REDSTONE_ON || type == BLOCK_ID_TORCH_REDSTONE_OFF)
        {
            const Uint8 ao[] = { 0, 0, 0, 0 };
            const Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };
            const Uint8 sl[] = { slight_sky[1][1][1], slight_sky[1][1][1], slight_sky[1][1][1], slight_sky[1][1][1] };

            /* Positive Y */
            {
                glm::vec2 cs = faces[1].corners[3] - faces[1].corners[0];
                faces[1].corners[0] += cs * glm::vec2(0.4375f, 0.375f);
                faces[1].corners[3] = faces[1].corners[0] + cs / 8.0f;

                faces[1].corners[1] = { faces[1].corners[3].x, faces[1].corners[0].y };
                faces[1].corners[2] = { faces[1].corners[0].x, faces[1].corners[3].y };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 10), Sint16(z * 16 + 9), ao[3] },
                    { r, g, b, bl[3], sl[3] },
                    faces[1].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 10), Sint16(z * 16 + 7), ao[1] },
                    { r, g, b, bl[1], sl[1] },
                    faces[1].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 10), Sint16(z * 16 + 9), ao[2] },
                    { r, g, b, bl[2], sl[2] },
                    faces[1].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 10), Sint16(z * 16 + 7), ao[0] },
                    { r, g, b, bl[0], sl[0] },
                    faces[1].corners[3],
                });
            }

            /* Positive X */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 0), Sint16(z * 16 + 0), ao[0] },
                    { r, g, b, bl[0], sl[0] },
                    faces[0].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 16), Sint16(z * 16 + 0), ao[1] },
                    { r, g, b, bl[1], sl[1] },
                    faces[0].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 0), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], sl[2] },
                    faces[0].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 9), Sint16(y * 16 + 16), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], sl[3] },
                    faces[0].corners[0],
                });
            }

            /* Negative X */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 16), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], sl[3] },
                    faces[3].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 16), Sint16(z * 16 + 0), ao[1] },
                    { r, g, b, bl[1], sl[1] },
                    faces[3].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 0), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], sl[2] },
                    faces[3].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 7), Sint16(y * 16 + 0), Sint16(z * 16 + 0), ao[0] },
                    { r, g, b, bl[0], sl[0] },
                    faces[3].corners[2],
                });
            }

            /* Positive Z */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 16), Sint16(z * 16 + 9), ao[3] },
                    { r, g, b, bl[3], sl[3] },
                    faces[2].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 16), Sint16(z * 16 + 9), ao[1] },
                    { r, g, b, bl[1], sl[1] },
                    faces[2].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 0), Sint16(z * 16 + 9), ao[2] },
                    { r, g, b, bl[2], sl[2] },
                    faces[2].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 0), Sint16(z * 16 + 9), ao[0] },
                    { r, g, b, bl[0], sl[0] },
                    faces[2].corners[2],
                });
            }

            /* Negative Z */
            {
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 0), Sint16(z * 16 + 7), ao[0] },
                    { r, g, b, bl[0], sl[0] },
                    faces[5].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 16), Sint16(z * 16 + 7), ao[1] },
                    { r, g, b, bl[1], sl[1] },
                    faces[5].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 0), Sint16(z * 16 + 7), ao[2] },
                    { r, g, b, bl[2], sl[2] },
                    faces[5].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 16), Sint16(z * 16 + 7), ao[3] },
                    { r, g, b, bl[3], sl[3] },
                    faces[5].corners[0],
                });
            }
        }
        /* ============ END: IS_TORCH ============ */
        /* ============ BEGIN: IS_CROSS_BLOCK ============ */
        else if (type == BLOCK_ID_FLOWER_RED || type == BLOCK_ID_FLOWER_YELLOW || type == BLOCK_ID_COBWEB || type == BLOCK_ID_MUSHROOM_BLAND
            || type == BLOCK_ID_MUSHROOM_RED || type == BLOCK_ID_FOLIAGE || type == BLOCK_ID_DEAD_BUSH || type == BLOCK_ID_SAPLING
            || type == BLOCK_ID_SUGAR_CANE)
        {
            /* Positive X */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 0), Sint16(z * 16 + 1), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][1] },
                    faces[0].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][1] },
                    faces[0].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][1] },
                    faces[0].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][1] },
                    faces[0].corners[0],
                });
            }

            /* Negative X */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][1] },
                    faces[3].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][1] },
                    faces[3].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][1] },
                    faces[3].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 0), Sint16(z * 16 + 1), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][1] },
                    faces[3].corners[2],
                });
            }

            /* Positive Z */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][1] },
                    faces[2].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 16), Sint16(z * 16 + 0), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][1] },
                    faces[2].corners[0],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][1] },
                    faces[2].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 0), Sint16(y * 16 + 0), Sint16(z * 16 + 0), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][1] },
                    faces[2].corners[2],
                });
            }

            /* Negative Z */
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1], slight_block[1][1][1] };

                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 0), Sint16(z * 16 + 1), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][1] },
                    faces[5].corners[3],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 1), Sint16(y * 16 + 16), Sint16(z * 16 + 1), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][1] },
                    faces[5].corners[1],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 0), Sint16(z * 16 + 15), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][1] },
                    faces[5].corners[2],
                });
                vtx->push_back({
                    { 1, Sint16(x * 16 + 15), Sint16(y * 16 + 16), Sint16(z * 16 + 15), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][1] },
                    faces[5].corners[0],
                });
            }
        }
        /* ============ END: IS_CROSS_BLOCK  ============*/
        /* ============ BEGIN: IS_FLUID ============ */
        else if (type == BLOCK_ID_LAVA_FLOWING || type == BLOCK_ID_LAVA_SOURCE || type == BLOCK_ID_WATER_FLOWING || type == BLOCK_ID_WATER_SOURCE)
        {
            /* Simplify block id checking */
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    for (int k = 0; k < 3; k++)
                    {
                        if (stypes[i][j][k] == BLOCK_ID_LAVA_FLOWING)
                            stypes[i][j][k] = BLOCK_ID_LAVA_SOURCE;
                        else if (stypes[i][j][k] == BLOCK_ID_WATER_FLOWING)
                            stypes[i][j][k] = BLOCK_ID_WATER_SOURCE;
                    }
            type = stypes[1][1][1];

            bool is_water = (type == BLOCK_ID_WATER_SOURCE);
            std::vector<terrain_vertex_t>* vtx_fluid = vtx;

            mc_id::terrain_face_t face_flow = terrain->get_face(is_water ? mc_id::FACE_WATER_FLOW : mc_id::FACE_LAVA_FLOW);
            mc_id::terrain_face_t face_still = terrain->get_face(is_water ? mc_id::FACE_WATER_STILL : mc_id::FACE_LAVA_STILL);

#define IS_SAME_FLUID(A, B) (A == B)
#define IS_FLUID(A) ((A) == BLOCK_ID_WATER_SOURCE || (A) == BLOCK_ID_LAVA_SOURCE)

            struct fluid_corners_t
            {
                Uint8 zero;
                Uint8 posx;
                Uint8 posz;
                Uint8 both;

                fluid_corners_t() { }
                fluid_corners_t(int set)
                    : fluid_corners_t(set, set, set, set)
                {
                }
                fluid_corners_t(int z, int px, int pz, int b)
                {
                    zero = SDL_clamp(z, 0, 16);
                    posx = SDL_clamp(px, 0, 16);
                    posz = SDL_clamp(pz, 0, 16);
                    both = SDL_clamp(b, 0, 16);
                }

                fluid_corners_t operator/(int a)
                {
                    fluid_corners_t t;
                    t.zero = zero / a;
                    t.posx = posx / a;
                    t.posz = posz / a;
                    t.both = both / a;
                    return t;
                }
            };
            /** Fluid depths (For deciding texture info) */
            fluid_corners_t corner_depths;
            /** Actual mesh heights*/
            fluid_corners_t corner_heights;

            int fluid_force_flow = 0;

            /* When nearby block is same override normal corner? */
            /* Block above is fluid, skip decision making*/
            if (IS_FLUID(stypes[1][2][1]))
            {
                corner_depths = 8;
                corner_heights = 16;
            }
            else
            {
#define FLUID_MAX_META 8
#define FLUID_DEPTH_FROM_METADATA(METADATA) ((METADATA < FLUID_MAX_META) ? FLUID_MAX_META - METADATA : FLUID_MAX_META)
#define FLUID_DEPTH_IF_SAME(SUBSCRIPT) (IS_SAME_FLUID(stypes SUBSCRIPT, type) ? FLUID_DEPTH_FROM_METADATA(smetadata SUBSCRIPT) : 0)
#define FLUID_DEPTH_IF_SAME_MAX(VAR, SUBSCRIPT)                                                              \
    do                                                                                                       \
    {                                                                                                        \
        Uint8 NEW_##VAR = (IS_FLUID(stypes SUBSCRIPT) ? FLUID_DEPTH_FROM_METADATA(smetadata SUBSCRIPT) : 0); \
        corner_depths.VAR = SDL_max(NEW_##VAR, corner_depths.VAR);                                           \
    } while (0)

                corner_depths = FLUID_DEPTH_FROM_METADATA(metadata);
                corner_heights = 0;

                /* Max a corner if the 2x2 region above the corner contains the same fluid */
                bool max_zero = IS_FLUID(stypes[0][2][0]) || IS_FLUID(stypes[1][2][0]) || IS_FLUID(stypes[0][2][1]);
                bool max_posx = IS_FLUID(stypes[1][2][0]) || IS_FLUID(stypes[2][2][0]) || IS_FLUID(stypes[2][2][1]);
                bool max_posz = IS_FLUID(stypes[0][2][2]) || IS_FLUID(stypes[1][2][2]) || IS_FLUID(stypes[0][2][1]);
                bool max_both = IS_FLUID(stypes[1][2][2]) || IS_FLUID(stypes[2][2][2]) || IS_FLUID(stypes[2][2][1]);

                if (FLUID_DEPTH_FROM_METADATA(smetadata[1][1][0]) < corner_depths.zero)
                    fluid_force_flow = 1;
                if (FLUID_DEPTH_FROM_METADATA(smetadata[1][1][0]) < corner_depths.posx)
                    fluid_force_flow = 1;

                if (FLUID_DEPTH_FROM_METADATA(smetadata[0][1][1]) < corner_depths.zero)
                    fluid_force_flow = 2;
                if (FLUID_DEPTH_FROM_METADATA(smetadata[0][1][1]) < corner_depths.posz)
                    fluid_force_flow = 2;

                if (FLUID_DEPTH_FROM_METADATA(smetadata[1][1][2]) < corner_depths.both)
                    fluid_force_flow = 3;
                if (FLUID_DEPTH_FROM_METADATA(smetadata[1][1][2]) < corner_depths.posz)
                    fluid_force_flow = 3;

                if (FLUID_DEPTH_FROM_METADATA(smetadata[2][1][1]) < corner_depths.both)
                    fluid_force_flow = 4;
                if (FLUID_DEPTH_FROM_METADATA(smetadata[2][1][1]) < corner_depths.posx)
                    fluid_force_flow = 4;

                FLUID_DEPTH_IF_SAME_MAX(zero, [0][1][0]);
                FLUID_DEPTH_IF_SAME_MAX(zero, [1][1][0]);
                FLUID_DEPTH_IF_SAME_MAX(zero, [0][1][1]);

                FLUID_DEPTH_IF_SAME_MAX(both, [2][1][2]);
                FLUID_DEPTH_IF_SAME_MAX(both, [1][1][2]);
                FLUID_DEPTH_IF_SAME_MAX(both, [2][1][1]);

                FLUID_DEPTH_IF_SAME_MAX(posx, [2][1][0]);
                FLUID_DEPTH_IF_SAME_MAX(posx, [1][1][0]);
                FLUID_DEPTH_IF_SAME_MAX(posx, [2][1][1]);

                FLUID_DEPTH_IF_SAME_MAX(posz, [0][1][2]);
                FLUID_DEPTH_IF_SAME_MAX(posz, [0][1][1]);
                FLUID_DEPTH_IF_SAME_MAX(posz, [1][1][2]);

                corner_depths.zero = max_zero ? FLUID_MAX_META : corner_depths.zero;
                corner_depths.posx = max_posx ? FLUID_MAX_META : corner_depths.posx;
                corner_depths.posz = max_posz ? FLUID_MAX_META : corner_depths.posz;
                corner_depths.both = max_both ? FLUID_MAX_META : corner_depths.both;

                corner_heights.zero = 1 + corner_depths.zero + (corner_depths.zero == FLUID_MAX_META) * 2;
                corner_heights.posx = 1 + corner_depths.posx + (corner_depths.posx == FLUID_MAX_META) * 2;
                corner_heights.posz = 1 + corner_depths.posz + (corner_depths.posz == FLUID_MAX_META) * 2;
                corner_heights.both = 1 + corner_depths.both + (corner_depths.both == FLUID_MAX_META) * 2;

#define FLUID_RAISE(X) ((stypes X) != BLOCK_ID_AIR)
                corner_heights.zero += (FLUID_RAISE([0][1][0]) + FLUID_RAISE([1][1][0]) + FLUID_RAISE([0][1][1]));
                corner_heights.posx += (FLUID_RAISE([2][1][0]) + FLUID_RAISE([1][1][0]) + FLUID_RAISE([2][1][1]));
                corner_heights.posz += (FLUID_RAISE([0][1][2]) + FLUID_RAISE([0][1][1]) + FLUID_RAISE([1][1][2]));
                corner_heights.both += (FLUID_RAISE([2][1][2]) + FLUID_RAISE([2][1][1]) + FLUID_RAISE([1][1][2]));

                corner_heights.zero = max_zero ? 16 : SDL_clamp(corner_heights.zero, 1, 16);
                corner_heights.posx = max_posx ? 16 : SDL_clamp(corner_heights.posx, 1, 16);
                corner_heights.posz = max_posz ? 16 : SDL_clamp(corner_heights.posz, 1, 16);
                corner_heights.both = max_both ? 16 : SDL_clamp(corner_heights.both, 1, 16);
            }

            glm::vec2 face_flow_tsize = (face_flow.corners[3] - face_flow.corners[0]);

            /* Positive Y */
            if (!IS_SAME_FLUID(stypes[1][2][1], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                int slope_left = corner_depths.zero - corner_depths.posx;
                int slope_bot = corner_depths.zero - corner_depths.posz;
                int slope_right = corner_depths.both - corner_depths.posz;
                int slope_top = corner_depths.both - corner_depths.posx;
                int slope_zeroboth = corner_depths.both - corner_depths.zero;
                int slope_posxposz = corner_depths.posx - corner_depths.posz;

                mc_id::terrain_face_t face_top;

                bool is_still = slope_left == 0 && slope_bot == 0 && slope_right == 0 && slope_top == 0;
                if (!is_still)
                    fluid_force_flow = false;
                if (fluid_force_flow)
                    is_still = false;
                float flow_rot = 0.0f;

                if (is_still)
                    face_top = face_still;
                else if (fluid_force_flow)
                {
                    switch (fluid_force_flow)
                    {
                    case 2:
                        flow_rot = 0.0f;
                        break;
                    case 3:
                        flow_rot = 90.0f;
                        break;
                    case 4:
                        flow_rot = 180.0f;
                        break;
                    case 1: /* Fall through */
                    default:
                        flow_rot = 270.0f;
                        break;
                    }
                }
                else if (slope_left == -slope_right && slope_top == -slope_bot) /* Axis aligned flow */
                {
                    if (slope_left > 0)
                        flow_rot -= 180.0f;

                    if (slope_top > 0)
                        flow_rot -= 90.0f;
                    if (slope_bot > 0)
                        flow_rot += 90.0f;
                }
                else if (slope_zeroboth != 0 || slope_posxposz != 0) /* Diagonal Flow */
                {
                    int slope_to_use = (SDL_abs(slope_posxposz) < SDL_abs(slope_zeroboth)) ? slope_zeroboth : slope_posxposz;
                    flow_rot = 45.0f;

                    if (slope_to_use < 0)
                        flow_rot += 90.0f;

                    if (slope_posxposz < slope_zeroboth)
                        flow_rot += 90.0f;

                    if (slope_posxposz < slope_zeroboth && slope_to_use > 0)
                        flow_rot += 180.0f;

                    // TODO-OPT: Add 22.5 degree adjustments
                }
                else if (slope_zeroboth == 0 && slope_posxposz == 0)
                {
                    if (corner_depths.both > corner_depths.posx)
                        flow_rot = 45.0f;
                    else
                        flow_rot = 225.0f;
                }
                else
                {
                    face_top = terrain->get_face(mc_id::FACE_DEBUG);
                    is_still = true;
                }

                if (!is_still)
                {
                    float radius = SDL_sqrtf(2.0f) * 0.25f;

                    face_top.corners[0] = glm::vec2(glm::cos(glm::radians(135.0f + flow_rot)), glm::sin(glm::radians(135.0f + flow_rot))) * radius;
                    face_top.corners[1] = glm::vec2(glm::cos(glm::radians(225.0f + flow_rot)), glm::sin(glm::radians(225.0f + flow_rot))) * radius;
                    face_top.corners[2] = glm::vec2(glm::cos(glm::radians(45.0f + flow_rot)), glm::sin(glm::radians(45.0f + flow_rot))) * radius;
                    face_top.corners[3] = glm::vec2(glm::cos(glm::radians(315.0f + flow_rot)), glm::sin(glm::radians(315.0f + flow_rot))) * radius;

                    face_top.corners[0] = (face_top.corners[0] + glm::vec2(0.5f, 0.5f)) * face_flow_tsize + face_flow.corners[0];
                    face_top.corners[1] = (face_top.corners[1] + glm::vec2(0.5f, 0.5f)) * face_flow_tsize + face_flow.corners[0];
                    face_top.corners[2] = (face_top.corners[2] + glm::vec2(0.5f, 0.5f)) * face_flow_tsize + face_flow.corners[0];
                    face_top.corners[3] = (face_top.corners[3] + glm::vec2(0.5f, 0.5f)) * face_flow_tsize + face_flow.corners[0];
                }

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + corner_heights.both), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][2][1] },
                    face_top.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + corner_heights.posx), Sint16(z * 16 + 00), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][2][1] },
                    face_top.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + corner_heights.posz), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][2][1] },
                    face_top.corners[2],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + corner_heights.zero), Sint16(z * 16 + 00), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][2][1] },
                    face_top.corners[0],
                });
            }

            /* Negative Y */
            if (is_transparent[stypes[1][0][1]] && !IS_SAME_FLUID(stypes[1][0][1], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][0][1] },
                    face_still.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][0][1] },
                    face_still.corners[0],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][0][1] },
                    face_still.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][0][1] },
                    face_still.corners[2],
                });
            }

            fluid_corners_t corner_tex_heights;
            corner_tex_heights.zero = 16 - corner_heights.zero;
            corner_tex_heights.posx = 16 - corner_heights.posx;
            corner_tex_heights.posz = 16 - corner_heights.posz;
            corner_tex_heights.both = 16 - corner_heights.both;

#define FLUID_CALC_SIDE_HEIGHTS(CORNER1, CORNER2)                                                                               \
    mc_id::terrain_face_t face_side;                                                                                            \
    mc_id::terrain_face_t face_side_tri;                                                                                        \
                                                                                                                                \
    Uint8 max_corner_tex_heights = SDL_max(corner_tex_heights.CORNER1, corner_tex_heights.CORNER2);                             \
    Uint8 min_corner_tex_heights = SDL_min(corner_tex_heights.CORNER1, corner_tex_heights.CORNER2);                             \
                                                                                                                                \
    Uint8 max_corner_heights = SDL_max(corner_heights.CORNER1, corner_heights.CORNER2);                                         \
    Uint8 min_corner_heights = SDL_min(corner_heights.CORNER1, corner_heights.CORNER2);                                         \
                                                                                                                                \
    bool which_height = corner_heights.CORNER1 > corner_heights.CORNER2;                                                        \
                                                                                                                                \
    face_side.corners[0] = glm::vec2(0.0f, 0.5f * max_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0];     \
    face_side.corners[1] = glm::vec2(0.5f, 0.5f * max_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0];     \
    face_side.corners[2] = glm::vec2(0.0f, 0.5f) * face_flow_tsize + face_flow.corners[0];                                      \
    face_side.corners[3] = glm::vec2(0.5f, 0.5f) * face_flow_tsize + face_flow.corners[0];                                      \
                                                                                                                                \
    face_side_tri.corners[0] = glm::vec2(0.0f, 0.5f * min_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0]; \
    face_side_tri.corners[1] = glm::vec2(0.5f, 0.5f * min_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0]; \
    face_side_tri.corners[2] = glm::vec2(0.0f, 0.5f * max_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0]; \
    face_side_tri.corners[3] = glm::vec2(0.5f, 0.5f * max_corner_tex_heights / 16.0f) * face_flow_tsize + face_flow.corners[0];

            /* Positive X */
            if (is_transparent[stypes[2][1][1]] && !IS_SAME_FLUID(stypes[2][1][1], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                FLUID_CALC_SIDE_HEIGHTS(posx, both);

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[0] },
                    { r, g, b, bl[0], slight_sky[2][1][1] },
                    face_side.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[1] },
                    { r, g, b, bl[1], slight_sky[2][1][1] },
                    face_side.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[2][1][1] },
                    face_side.corners[2],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[2][1][1] },
                    face_side.corners[0],
                });

                if (max_corner_heights != min_corner_heights)
                {
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[0] },
                        { r, g, b, bl[0], slight_sky[2][1][1] },
                        face_side_tri.corners[3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + corner_heights.posx), Sint16(z * 16 + 00), ao[1] },
                        { r, g, b, bl[1], slight_sky[2][1][1] },
                        face_side_tri.corners[which_height ? 1 : 3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[2] },
                        { r, g, b, bl[2], slight_sky[2][1][1] },
                        face_side_tri.corners[2],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + corner_heights.both), Sint16(z * 16 + 16), ao[3] },
                        { r, g, b, bl[3], slight_sky[2][1][1] },
                        face_side_tri.corners[which_height ? 2 : 0],
                    });
                }
            }

            /* Negative X */
            if (is_transparent[stypes[0][1][1]] && !IS_SAME_FLUID(stypes[0][1][1], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                FLUID_CALC_SIDE_HEIGHTS(zero, posz);

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[0][1][1] },
                    face_side.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[1] },
                    { r, g, b, bl[1], slight_sky[0][1][1] },
                    face_side.corners[0],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[0][1][1] },
                    face_side.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[0] },
                    { r, g, b, bl[0], slight_sky[0][1][1] },
                    face_side.corners[2],
                });

                if (max_corner_heights != min_corner_heights)
                {

                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + corner_heights.posz), Sint16(z * 16 + 16), ao[3] },
                        { r, g, b, bl[3], slight_sky[0][1][1] },
                        face_side_tri.corners[!which_height ? 1 : 3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + corner_heights.zero), Sint16(z * 16 + 00), ao[1] },
                        { r, g, b, bl[1], slight_sky[0][1][1] },
                        face_side_tri.corners[!which_height ? 2 : 0],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[2] },
                        { r, g, b, bl[2], slight_sky[0][1][1] },
                        face_side_tri.corners[3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[0] },
                        { r, g, b, bl[0], slight_sky[0][1][1] },
                        face_side_tri.corners[2],
                    });
                }
            }

            /* Positive Z */
            if (is_transparent[stypes[1][1][2]] && !IS_SAME_FLUID(stypes[1][1][2], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                FLUID_CALC_SIDE_HEIGHTS(both, posz);

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][2] },
                    face_side.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][2] },
                    face_side.corners[0],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][2] },
                    face_side.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 16), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][2] },
                    face_side.corners[2],
                });

                if (max_corner_heights != min_corner_heights)
                {
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + ((which_height) ? max_corner_heights : min_corner_heights)), Sint16(z * 16 + 16), ao[3] },
                        { r, g, b, bl[3], slight_sky[1][1][2] },
                        face_side_tri.corners[which_height ? 1 : 3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + ((!which_height) ? max_corner_heights : min_corner_heights)), Sint16(z * 16 + 16), ao[1] },
                        { r, g, b, bl[1], slight_sky[1][1][2] },
                        face_side_tri.corners[which_height ? 2 : 0],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[2] },
                        { r, g, b, bl[2], slight_sky[1][1][2] },
                        face_side_tri.corners[3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 16), ao[0] },
                        { r, g, b, bl[0], slight_sky[1][1][2] },
                        face_side_tri.corners[2],
                    });
                }
            }
            /* Negative Z */
            if (is_transparent[stypes[1][1][0]] && !IS_SAME_FLUID(stypes[1][1][0], type))
            {
                Uint8 ao[] = { 0, 0, 0, 0 };

                Uint8 bl[] = { Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]), Uint8(slight_block[1][1][1]) };

                FLUID_CALC_SIDE_HEIGHTS(zero, posx);

                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[0] },
                    { r, g, b, bl[0], slight_sky[1][1][0] },
                    face_side.corners[3],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[1] },
                    { r, g, b, bl[1], slight_sky[1][1][0] },
                    face_side.corners[1],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + 00), Sint16(z * 16 + 00), ao[2] },
                    { r, g, b, bl[2], slight_sky[1][1][0] },
                    face_side.corners[2],
                });
                vtx_fluid->push_back({
                    { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[3] },
                    { r, g, b, bl[3], slight_sky[1][1][0] },
                    face_side.corners[0],
                });

                if (max_corner_heights != min_corner_heights)
                {
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[0] },
                        { r, g, b, bl[0], slight_sky[1][1][0] },
                        face_side_tri.corners[3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 00), Sint16(y * 16 + ((which_height) ? max_corner_heights : min_corner_heights)), Sint16(z * 16 + 00), ao[1] },
                        { r, g, b, bl[1], slight_sky[1][1][0] },
                        face_side_tri.corners[which_height ? 1 : 3],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + min_corner_heights), Sint16(z * 16 + 00), ao[2] },
                        { r, g, b, bl[2], slight_sky[1][1][0] },
                        face_side_tri.corners[2],
                    });
                    vtx_fluid->push_back({
                        { 1, Sint16(x * 16 + 16), Sint16(y * 16 + ((!which_height) ? max_corner_heights : min_corner_heights)), Sint16(z * 16 + 00), ao[3] },
                        { r, g, b, bl[3], slight_sky[1][1][0] },
                        face_side_tri.corners[which_height ? 2 : 0],
                    });
                }
            }
#undef FLUID_CALC_SIDE_HEIGHTS
        }
        /* ============ END: IS_FLUID ============ */
        /* ============ BEGIN: IS_NORMAL ============ */
        else
        {
            const bool do_face_pos_y = (is_transparent[stypes[1][2][1]] && (is_leaves_style_transparent[type] || stypes[1][2][1] != type));
            const bool do_face_neg_y = (is_transparent[stypes[1][0][1]] && (is_leaves_style_transparent[type] || stypes[1][0][1] != type));
            const bool do_face_pos_x = (is_transparent[stypes[2][1][1]] && (is_leaves_style_transparent[type] || stypes[2][1][1] != type));
            const bool do_face_neg_x = (is_transparent[stypes[0][1][1]] && (is_leaves_style_transparent[type] || stypes[0][1][1] != type));
            const bool do_face_pos_z = (is_transparent[stypes[1][1][2]] && (is_leaves_style_transparent[type] || stypes[1][1][2] != type));
            const bool do_face_neg_z = (is_transparent[stypes[1][1][0]] && (is_leaves_style_transparent[type] || stypes[1][1][0] != type));

            /* Quick reject */
            if (!do_face_pos_y && !do_face_neg_y && !do_face_pos_x && !do_face_neg_x && !do_face_pos_z && !do_face_neg_z)
                continue;

            /* BEGIN: Preliminary Smooth Lighting Calculations */
            struct corner_t
            {
                Uint16 sum, divisor;
                corner_t() { sum = 0, divisor = 0; }
                corner_t(const int _sum, const int _divisor) { sum = _sum, divisor = _divisor; }
                corner_t(const Uint16 _sum, const Uint16 _divisor) { sum = _sum, divisor = _divisor; }
                void operator+=(const corner_t& rh) { sum += rh.sum, divisor += rh.divisor; }
                void operator+=(const int& rh)
                {
                    sum += rh;
                    divisor++;
                }
                Uint8 get() const { return sum / divisor; }
            };

            const std::function<std::pair<corner_t, corner_t> const(const int, const int, const int, const bool, const bool, const bool)> calc_corner
                = [&](const int _x, const int _y, const int _z, const bool f_x, const bool f_y, const bool f_z) -> std::pair<corner_t, corner_t> const {
                corner_t corner_b, corner_s;

                uint_fast8_t mask = f_x | (f_y << 1) | (f_z << 2);

                /* Check that mask has only 1 flag */
                assert(mask && !(mask & (mask - 1)));

                const int x = 1 + _x;
                const int y = 1 + _y;
                const int z = 1 + _z;

                bool v_x = 0, v_y = 0, v_z = 0;
                bool v_diag_xy = 0, v_diag_xz = 0, v_diag_zy = 0;
                bool v_diag_xzy = 0;

                switch (mask)
                {
                case 0x01:
                    v_x = is_transparent[stypes[x][1][1]];
                    break;
                case 0x02:
                    v_y = is_transparent[stypes[1][y][1]];
                    break;
                case 0x04:
                    v_z = is_transparent[stypes[1][1][z]];
                    break;
                }

                if (mask & ~0x01)
                    v_diag_zy = (v_z || v_y) && is_transparent[stypes[1][y][z]];

                if (mask & ~0x02)
                    v_diag_xz = (v_x || v_z) && is_transparent[stypes[x][1][z]];

                if (mask & ~0x04)
                    v_diag_xy = (v_x || v_y) && is_transparent[stypes[x][y][1]];

                /* By this point, two diagonals have been calculated */
                v_diag_xzy = (v_diag_xy || v_diag_xz || v_diag_zy) && is_transparent[stypes[x][y][z]];

                if (mask & 0x01 || !v_diag_zy)
                    v_diag_zy = v_diag_xzy && is_transparent[stypes[1][y][z]];

                if (mask & 0x02 || !v_diag_xz)
                    v_diag_xz = v_diag_xzy && is_transparent[stypes[x][1][z]];

                if (mask & 0x04 || !v_diag_xy)
                    v_diag_xy = v_diag_xzy && is_transparent[stypes[x][y][1]];

                if (!v_x)
                    v_x = (v_diag_xy || v_diag_xz) && is_transparent[stypes[x][1][1]];
                if (!v_y)
                    v_y = (v_diag_xy || v_diag_zy) && is_transparent[stypes[1][y][1]];
                if (!v_z)
                    v_z = (v_diag_xz || v_diag_zy) && is_transparent[stypes[1][1][z]];

#define CORNER_IF_COND(COND, COORDS)         \
    do                                       \
        if (COND)                            \
        {                                    \
            corner_s += slight_sky COORDS;   \
            corner_b += slight_block COORDS; \
        }                                    \
    while (0)

                CORNER_IF_COND(v_x, [x][1][1]);
                CORNER_IF_COND(v_y, [1][y][1]);
                CORNER_IF_COND(v_z, [1][1][z]);

                CORNER_IF_COND(v_diag_xy, [x][y][1]);
                CORNER_IF_COND(v_diag_xz, [x][1][z]);
                CORNER_IF_COND(v_diag_zy, [1][y][z]);

                CORNER_IF_COND(v_diag_xzy, [x][y][z]);

#undef CORNER_IF_COND

                return std::pair(corner_b, corner_s);
            };

            /* END: Preliminary Smooth Lighting Calculations */

            /* Positive Y */
            if (do_face_pos_y)
            {
                Uint8 ao[] = {
                    Uint8(UAO([0][2][0]) + UAO([1][2][0]) + UAO([0][2][1])),
                    Uint8(UAO([2][2][0]) + UAO([1][2][0]) + UAO([2][2][1])),
                    Uint8(UAO([0][2][2]) + UAO([0][2][1]) + UAO([1][2][2])),
                    Uint8(UAO([2][2][2]) + UAO([1][2][2]) + UAO([2][2][1])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(-1, 1, -1, 0, 1, 0);
                templ[1] = calc_corner(+1, 1, -1, 0, 1, 0);
                templ[2] = calc_corner(-1, 1, +1, 0, 1, 0);
                templ[3] = calc_corner(+1, 1, +1, 0, 1, 0);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r, g, b, bl[3].get(), sl[3].get() },
                    faces[1].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r, g, b, bl[1].get(), sl[1].get() },
                    faces[1].corners[2],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[2] },
                    { r, g, b, bl[2].get(), sl[2].get() },
                    faces[1].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[0] },
                    { r, g, b, bl[0].get(), sl[0].get() },
                    faces[1].corners[3],
                });

                if (use_overlay[1])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                        { r_overlay, g_overlay, b_overlay, bl[3].get(), sl[3].get() },
                        faces_overlay[1].corners[0],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[1] },
                        { r_overlay, g_overlay, b_overlay, bl[1].get(), sl[1].get() },
                        faces_overlay[1].corners[2],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[2] },
                        { r_overlay, g_overlay, b_overlay, bl[2].get(), sl[2].get() },
                        faces_overlay[1].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[0] },
                        { r_overlay, g_overlay, b_overlay, bl[0].get(), sl[0].get() },
                        faces_overlay[1].corners[3],
                    });
                }
            }

            /* Negative Y */
            if (do_face_neg_y)
            {
                Uint8 ao[] = {
                    Uint8(UAO([0][0][0]) + UAO([1][0][0]) + UAO([0][0][1])),
                    Uint8(UAO([2][0][0]) + UAO([1][0][0]) + UAO([2][0][1])),
                    Uint8(UAO([0][0][2]) + UAO([0][0][1]) + UAO([1][0][2])),
                    Uint8(UAO([2][0][2]) + UAO([1][0][2]) + UAO([2][0][1])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(-1, -1, -1, 0, 1, 0);
                templ[1] = calc_corner(+1, -1, -1, 0, 1, 0);
                templ[2] = calc_corner(-1, -1, +1, 0, 1, 0);
                templ[3] = calc_corner(+1, -1, +1, 0, 1, 0);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r, g, b, bl[0].get(), sl[0].get() },
                    faces[4].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[1] },
                    { r, g, b, bl[1].get(), sl[1].get() },
                    faces[4].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r, g, b, bl[2].get(), sl[2].get() },
                    faces[4].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[3] },
                    { r, g, b, bl[3].get(), sl[3].get() },
                    faces[4].corners[2],
                });

                if (use_overlay[4])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                        { r_overlay, g_overlay, b_overlay, bl[0].get(), sl[0].get() },
                        faces_overlay[4].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[1] },
                        { r_overlay, g_overlay, b_overlay, bl[1].get(), sl[1].get() },
                        faces_overlay[4].corners[0],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[2] },
                        { r_overlay, g_overlay, b_overlay, bl[2].get(), sl[2].get() },
                        faces_overlay[4].corners[3],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[3] },
                        { r_overlay, g_overlay, b_overlay, bl[3].get(), sl[3].get() },
                        faces_overlay[4].corners[2],
                    });
                }
            }

            /* Positive X */
            if (do_face_pos_x)
            {
                Uint8 ao[] = {
                    Uint8(UAO([2][0][0]) + UAO([2][1][0]) + UAO([2][0][1])),
                    Uint8(UAO([2][2][0]) + UAO([2][1][0]) + UAO([2][2][1])),
                    Uint8(UAO([2][0][2]) + UAO([2][0][1]) + UAO([2][1][2])),
                    Uint8(UAO([2][2][2]) + UAO([2][1][2]) + UAO([2][2][1])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(+1, -1, -1, 1, 0, 0);
                templ[1] = calc_corner(+1, +1, -1, 1, 0, 0);
                templ[2] = calc_corner(+1, -1, +1, 1, 0, 0);
                templ[3] = calc_corner(+1, +1, +1, 1, 0, 0);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r, g, b, bl[0].get(), sl[0].get() },
                    faces[0].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r, g, b, bl[1].get(), sl[1].get() },
                    faces[0].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r, g, b, bl[2].get(), sl[2].get() },
                    faces[0].corners[2],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r, g, b, bl[3].get(), sl[3].get() },
                    faces[0].corners[0],
                });

                if (use_overlay[0])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[0] },
                        { r_overlay, g_overlay, b_overlay, bl[0].get(), sl[0].get() },
                        faces_overlay[0].corners[3],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[1] },
                        { r_overlay, g_overlay, b_overlay, bl[1].get(), sl[1].get() },
                        faces_overlay[0].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[2] },
                        { r_overlay, g_overlay, b_overlay, bl[2].get(), sl[2].get() },
                        faces_overlay[0].corners[2],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                        { r_overlay, g_overlay, b_overlay, bl[3].get(), sl[3].get() },
                        faces_overlay[0].corners[0],
                    });
                }
            }

            /* Negative X */
            if (do_face_neg_x)
            {
                Uint8 ao[] = {
                    Uint8(UAO([0][0][0]) + UAO([0][1][0]) + UAO([0][0][1])),
                    Uint8(UAO([0][2][0]) + UAO([0][1][0]) + UAO([0][2][1])),
                    Uint8(UAO([0][0][2]) + UAO([0][0][1]) + UAO([0][1][2])),
                    Uint8(UAO([0][2][2]) + UAO([0][1][2]) + UAO([0][2][1])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(-1, -1, -1, 1, 0, 0);
                templ[1] = calc_corner(-1, +1, -1, 1, 0, 0);
                templ[2] = calc_corner(-1, -1, +1, 1, 0, 0);
                templ[3] = calc_corner(-1, +1, +1, 1, 0, 0);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r, g, b, bl[3].get(), sl[3].get() },
                    faces[3].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r, g, b, bl[1].get(), sl[1].get() },
                    faces[3].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r, g, b, bl[2].get(), sl[2].get() },
                    faces[3].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r, g, b, bl[0].get(), sl[0].get() },
                    faces[3].corners[2],
                });

                if (use_overlay[3])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[3] },
                        { r_overlay, g_overlay, b_overlay, bl[3].get(), sl[3].get() },
                        faces_overlay[3].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[1] },
                        { r_overlay, g_overlay, b_overlay, bl[1].get(), sl[1].get() },
                        faces_overlay[3].corners[0],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[2] },
                        { r_overlay, g_overlay, b_overlay, bl[2].get(), sl[2].get() },
                        faces_overlay[3].corners[3],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                        { r_overlay, g_overlay, b_overlay, bl[0].get(), sl[0].get() },
                        faces_overlay[3].corners[2],
                    });
                }
            }

            /* Positive Z */
            if (do_face_pos_z)
            {
                Uint8 ao[] = {
                    Uint8(UAO([0][0][2]) + UAO([1][0][2]) + UAO([0][1][2])),
                    Uint8(UAO([0][2][2]) + UAO([0][1][2]) + UAO([1][2][2])),
                    Uint8(UAO([2][0][2]) + UAO([1][0][2]) + UAO([2][1][2])),
                    Uint8(UAO([2][2][2]) + UAO([1][2][2]) + UAO([2][1][2])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(-1, -1, +1, 0, 0, 1);
                templ[1] = calc_corner(+1, -1, +1, 0, 0, 1);
                templ[2] = calc_corner(-1, +1, +1, 0, 0, 1);
                templ[3] = calc_corner(+1, +1, +1, 0, 0, 1);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                    { r, g, b, bl[3].get(), sl[3].get() },
                    faces[2].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[1] },
                    { r, g, b, bl[2].get(), sl[2].get() },
                    faces[2].corners[0],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[2] },
                    { r, g, b, bl[1].get(), sl[1].get() },
                    faces[2].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[0] },
                    { r, g, b, bl[0].get(), sl[0].get() },
                    faces[2].corners[2],
                });

                if (use_overlay[2])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 1), ao[3] },
                        { r_overlay, g_overlay, b_overlay, bl[3].get(), sl[3].get() },
                        faces_overlay[2].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 1), ao[1] },
                        { r_overlay, g_overlay, b_overlay, bl[2].get(), sl[2].get() },
                        faces_overlay[2].corners[0],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 1), ao[2] },
                        { r_overlay, g_overlay, b_overlay, bl[1].get(), sl[1].get() },
                        faces_overlay[2].corners[3],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 1), ao[0] },
                        { r_overlay, g_overlay, b_overlay, bl[0].get(), sl[0].get() },
                        faces_overlay[2].corners[2],
                    });
                }
            }

            /* Negative Z */
            if (do_face_neg_z)
            {
                Uint8 ao[] = {
                    Uint8(UAO([0][0][0]) + UAO([1][0][0]) + UAO([0][1][0])),
                    Uint8(UAO([0][2][0]) + UAO([0][1][0]) + UAO([1][2][0])),
                    Uint8(UAO([2][0][0]) + UAO([1][0][0]) + UAO([2][1][0])),
                    Uint8(UAO([2][2][0]) + UAO([1][2][0]) + UAO([2][1][0])),
                };

                std::pair<corner_t, corner_t> templ[4];

                templ[0] = calc_corner(-1, -1, -1, 0, 0, 1);
                templ[1] = calc_corner(+1, -1, -1, 0, 0, 1);
                templ[2] = calc_corner(-1, +1, -1, 0, 0, 1);
                templ[3] = calc_corner(+1, +1, -1, 0, 0, 1);

                corner_t bl[4] = { templ[0].first, templ[1].first, templ[2].first, templ[3].first };
                corner_t sl[4] = { templ[0].second, templ[1].second, templ[2].second, templ[3].second };

                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                    { r, g, b, bl[0].get(), sl[0].get() },
                    faces[5].corners[3],
                });
                vtx->push_back({
                    { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[1] },
                    { r, g, b, bl[2].get(), sl[2].get() },
                    faces[5].corners[1],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[2] },
                    { r, g, b, bl[1].get(), sl[1].get() },
                    faces[5].corners[2],
                });
                vtx->push_back({
                    { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[3] },
                    { r, g, b, bl[3].get(), sl[3].get() },
                    faces[5].corners[0],
                });

                if (use_overlay[5])
                {
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 0), Sint16(z + 0), ao[0] },
                        { r_overlay, g_overlay, b_overlay, bl[0].get(), sl[0].get() },
                        faces_overlay[5].corners[3],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 0), Sint16(y + 1), Sint16(z + 0), ao[1] },
                        { r_overlay, g_overlay, b_overlay, bl[2].get(), sl[2].get() },
                        faces_overlay[5].corners[1],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 0), Sint16(z + 0), ao[2] },
                        { r_overlay, g_overlay, b_overlay, bl[1].get(), sl[1].get() },
                        faces_overlay[5].corners[2],
                    });
                    vtx_overlay.push_back({
                        { 16, Sint16(x + 1), Sint16(y + 1), Sint16(z + 0), ao[3] },
                        { r_overlay, g_overlay, b_overlay, bl[3].get(), sl[3].get() },
                        faces_overlay[5].corners[0],
                    });
                }
            }
        }
        /* ============ END: IS_NORMAL ============ */
    }

    TRACE("Chunk: <%d, %d, %d>, Vertices (Solid): %zu, Indices: %zu", chunk_x, chunk_y, chunk_z, vtx_solid.size(), vtx_solid.size() / 4 * 6);
    TRACE("Chunk: <%d, %d, %d>, Vertices (Trans): %zu, Indices: %zu", chunk_x, chunk_y, chunk_z, vtx_translucent.size(), vtx_translucent.size() / 4 * 6);
    TRACE("Chunk: <%d, %d, %d>, Vertices (Overlay): %zu, Indices: %zu", chunk_x, chunk_y, chunk_z, vtx_overlay.size(), vtx_overlay.size() / 4 * 6);

    if (!vtx_solid.size() && !vtx_translucent.size())
    {
        center->index_type = GL_NONE;
        center->index_count = 0;
        return;
    }

    if (!center->vao)
    {
        terrain_vertex_t::create_vao(&center->vao);
        tetra::gl_obj_label(GL_VERTEX_ARRAY, center->vao, "[Level][Chunk]: <%d, %d, %d>: VAO", center->pos.x, center->pos.y, center->pos.z);
    }
    glBindVertexArray(center->vao);
    if (!center->vbo)
    {
        glGenBuffers(1, &center->vbo);
        tetra::gl_obj_label(GL_BUFFER, center->vbo, "[Level][Chunk]: <%d, %d, %d>: VBO", center->pos.x, center->pos.y, center->pos.z);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    }

    center->index_type = GL_UNSIGNED_INT;
    center->index_count = vtx_solid.size() / 4 * 6;
    center->index_count_overlay = vtx_overlay.size() / 4 * 6;
    center->index_count_translucent = vtx_translucent.size() / 4 * 6;

    for (terrain_vertex_t v : vtx_overlay)
        vtx_solid.push_back(v);

    for (terrain_vertex_t v : vtx_translucent)
        vtx_solid.push_back(v);

    glBindBuffer(GL_ARRAY_BUFFER, center->vbo);
    glBufferData(GL_ARRAY_BUFFER, vtx_solid.size() * sizeof(vtx_solid[0]), vtx_solid.data(), GL_STATIC_DRAW);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, pos));
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, col));
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(terrain_vertex_t), (void*)offsetof(terrain_vertex_t, tex));
}

#if (FORCE_OPT_MESH)
#pragma GCC pop_options
#endif

void level_t::set_block(const glm::ivec3 pos, const block_id_t type, const Uint8 metadata)
{
    if (!mc_id::is_block(type))
    {
        dc_log_error("Preposterous id field %d:%u at <%d, %d, %d>", type, metadata, pos.x, pos.y, pos.z);
        return;
    }
    if (metadata > 15)
    {
        const char* name = mc_id::get_name_from_item_id(type, metadata);
        dc_log_error("Preposterous metadata field %d(%s):%u at <%d, %d, %d>", type, name, metadata, pos.x, pos.y, pos.z);
        return;
    }

    /* Attempt to find chunk */
    glm::ivec3 chunk_pos = pos >> 4;
    glm::ivec3 block_pos = pos & 0x0F;
    auto it = cmap.find(chunk_pos);
    if (it == cmap.end())
    {
        dc_log_error("Unable to find chunk <%d, %d, %d>", chunk_pos.x, chunk_pos.y, chunk_pos.z);
        return;
    }

    chunk_cubic_t* c = it->second;

    /* Get existing blocks data to help determine which chunks need rebuilding */
    block_id_t old_type = c->get_type(block_pos.x, block_pos.y, block_pos.z);

    /* Set type */
    c->set_type(block_pos.x, block_pos.y, block_pos.z, type);
    c->set_metadata(block_pos.x, block_pos.y, block_pos.z, metadata);

    /* Surrounding chunks do not need updating if the replacement has an equal effect on lighting */
    if (mc_id::is_transparent(old_type) == mc_id::is_transparent(type) && mc_id::get_light_level(old_type) == mc_id::get_light_level(type))
        return;

    int diff_x = ((block_pos.x & 0x0F) < 8) ? -1 : 1;
    int diff_y = ((block_pos.y & 0x0F) < 8) ? -1 : 1;
    int diff_z = ((block_pos.z & 0x0F) < 8) ? -1 : 1;

    /* TODO: Mark all continuous chunks below as invalid */
    for (int i = 0; i < 8; i++)
    {
        const int ix = (i >> 2) & 1;
        const int iy = (i >> 1) & 1;
        const int iz = i & 1;

        c = ((it = cmap.find(chunk_pos + glm::ivec3(diff_x * ix, diff_y * iy, diff_z * iz))) != cmap.end()) ? it->second : NULL;

        chunk_cubic_t::dirty_level_t dirt_face = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL;

        /* Lower the amount of compute passes */
        int itotal = ix + iy + iz;
        if (itotal == 2)
            dirt_face = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_0;
        else if (itotal == 3)
            dirt_face = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_1;

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
        dc_log_error("Unable to find chunk <%d, %d, %d>", chunk_pos.x, chunk_pos.y, chunk_pos.z);
        return false;
    }

    chunk_cubic_t* c = it->second;

    type = c->get_type(block_pos.x, block_pos.y, block_pos.z);
    metadata = c->get_metadata(block_pos.x, block_pos.y, block_pos.z);

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
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
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
    if (!shader_terrain)
    {
        dc_log_error("The terrain shader is required to render entities");
        return;
    }

    glm::vec3 camera_pos_capture = camera_pos;

    ecs.sort<entity_transform_t>([&camera_pos_capture](const entity_transform_t& a, const entity_transform_t& b) -> bool {
        const glm::vec3 apos(ECOORD_TO_ABSCOORD(a.x) / 32.0f, ECOORD_TO_ABSCOORD(a.y) / 32.0f, ECOORD_TO_ABSCOORD(a.z) / 32.0f);
        const glm::vec3 bpos(ECOORD_TO_ABSCOORD(b.x) / 32.0f, ECOORD_TO_ABSCOORD(b.y) / 32.0f, ECOORD_TO_ABSCOORD(b.z) / 32.0f);

        const float adist = glm::distance(apos, camera_pos_capture);
        const float bdist = glm::distance(bpos, camera_pos_capture);
        return adist < bdist;
    });

    glUseProgram(shader_terrain->id);

    glBindVertexArray(ent_missing_vao);

    auto view = ecs.view<const entity_id_t, const entity_transform_t>();
    view.use<const entity_transform_t>();
    for (auto [entity, id, pos] : view.each())
    {
        shader_terrain->set_model(pos.get_mat());
        glDrawElements(GL_TRIANGLES, ent_missing_vert_count / 4 * 6, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

void level_t::render(const glm::ivec2 win_size)
{
    const int render_distance = (render_distance_override > 0) ? render_distance_override : r_render_distance.get();

    cull_chunks(win_size, render_distance);

    build_dirty_meshes();

    tick();

    glUseProgram(shader_terrain->id);
    const glm::mat4 mat_proj = glm::perspective(glm::radians(fov), (float)win_size.x / (float)win_size.y, 0.03125f, render_distance * 32.0f);
    shader_terrain->set_projection(mat_proj);

    const glm::mat4 mat_cam = glm::lookAt(camera_pos, camera_pos + camera_direction, camera_up);
    shader_terrain->set_camera(mat_cam);

    shader_terrain->set_model(glm::mat4(1.0f));
    shader_terrain->set_uniform("tex_atlas", 0);
    shader_terrain->set_uniform("tex_lightmap", 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, get_terrain()->tex_id_main);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, lightmap.tex_id_linear);
    glActiveTexture(GL_TEXTURE0);

    {
        const glm::i64vec3 ent_camera_pos = ABSCOORD_TO_ECOORD_GLM_LONG(glm::i64vec3(camera_pos * 32.0f));
        const glm::i64vec3 cpos(ECOORD_TO_ABSCOORD_GLM_LONG(ent_camera_pos) >> 5l >> 4l);
        const glm::i64vec3 camera_pos_diff = cpos - last_render_order_cpos;
        const glm::dvec3 float_cpos(cpos);

        if (camera_pos_diff != glm::i64vec3(0, 0, 0))
            request_render_order_sort = 1;

        if (request_render_order_sort)
            TRACE("Render order sort requested @ %ld", SDL_GetTicks());

        if (request_render_order_sort || SDL_GetTicks() - last_render_order_sort_time > 5000)
        {
            std::sort(chunks_render_order.begin(), chunks_render_order.end(), [&float_cpos](const chunk_cubic_t* const a, const chunk_cubic_t* const b) {
                const float adist = glm::distance(glm::dvec3(a->pos), float_cpos);
                const float bdist = glm::distance(glm::dvec3(b->pos), float_cpos);
                return adist > bdist;
            });
            last_render_order_sort_time = SDL_GetTicks();
            request_render_order_sort = 0;
            last_render_order_cpos = cpos;
        }
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, get_terrain()->tex_id_main);

    shader_terrain->set_uniform("allow_translucency", 0);

    struct
    {
        GLboolean blend;
        GLboolean depth_test;
        GLboolean depth_mask;
        GLint depth_func;

        void restore(GLenum pname, GLboolean i)
        {
            void (*fun)(GLenum) = i ? glEnable : glDisable;
            fun(pname);
        }
    } prev_gl_state;

    glGetBooleanv(GL_BLEND, &prev_gl_state.blend);
    glGetBooleanv(GL_DEPTH_TEST, &prev_gl_state.depth_test);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prev_gl_state.depth_mask);
    glGetIntegerv(GL_DEPTH_FUNC, &prev_gl_state.depth_func);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    /* Draw opaque geometry from front to back to reduce unnecessary filling
     * This can actually make a performance difference (I tested it)
     */
    for (auto it = chunks_render_order.rbegin(); it != chunks_render_order.rend(); it = next(it))
    {
        chunk_cubic_t* c = *it;
        if (c->index_type == GL_NONE || c->index_count == 0 || !c->visible)
            continue;
        assert(c->index_type == GL_UNSIGNED_INT);
        glBindVertexArray(c->vao);
        glm::vec3 translate(c->pos * glm::ivec3(SUBCHUNK_SIZE_X, SUBCHUNK_SIZE_Y, SUBCHUNK_SIZE_Z));
        shader_terrain->set_model(glm::translate(glm::mat4(1.0f), translate));
        glDrawElements(GL_TRIANGLES, c->index_count, c->index_type, 0);
    }

    glDepthFunc(GL_EQUAL);
    for (auto it = chunks_render_order.rbegin(); it != chunks_render_order.rend(); it = next(it))
    {
        chunk_cubic_t* c = *it;
        if (c->index_type == GL_NONE || c->index_count_overlay == 0 || !c->visible)
            continue;
        assert(c->index_type == GL_UNSIGNED_INT);
        glBindVertexArray(c->vao);
        glm::vec3 translate(c->pos * glm::ivec3(SUBCHUNK_SIZE_X, SUBCHUNK_SIZE_Y, SUBCHUNK_SIZE_Z));
        shader_terrain->set_model(glm::translate(glm::mat4(1.0f), translate));
        glDrawElements(GL_TRIANGLES, c->index_count_overlay, c->index_type, (void*)(c->index_count * 4));
    }
    glDepthFunc(prev_gl_state.depth_func);

    render_entities();

    shader_terrain->set_uniform("allow_translucency", 1);
    glUseProgram(shader_terrain->id);

    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    /* I am not sure why the internet said to disable depth writes for the translucent pass, but my best guess is that
     * it decreases overhead by stopping translucent stuff from constantly overwriting the buffer
     */
    glDepthMask(GL_FALSE);

    for (chunk_cubic_t* c : chunks_render_order)
    {
        if (c->index_type == GL_NONE || c->index_count_translucent == 0 || !c->visible)
            continue;
        assert(c->index_type == GL_UNSIGNED_INT);
        glBindVertexArray(c->vao);
        glm::vec3 translate(c->pos * glm::ivec3(SUBCHUNK_SIZE_X, SUBCHUNK_SIZE_Y, SUBCHUNK_SIZE_Z));
        shader_terrain->set_model(glm::translate(glm::mat4(1.0f), translate));
        glDrawElements(GL_TRIANGLES, c->index_count_translucent, c->index_type, (void*)((c->index_count + c->index_count_overlay) * 4));
    }

    prev_gl_state.restore(GL_BLEND, prev_gl_state.blend);
    prev_gl_state.restore(GL_DEPTH_TEST, prev_gl_state.depth_test);
    glDepthMask(prev_gl_state.depth_mask);
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
        if ((*it_vec)->pos != pos)
            it_vec++;
        else
        {
            if (*it_vec != mapped_del)
                delete *it_vec;
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
                delete *it_vec;
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
    chunks_light_order.push_back(c);
    chunks_render_order.push_back(c);
    cmap[c->pos] = c;

    request_render_order_sort = 1;
    request_light_order_sort = 1;
}

level_t::level_t(texture_terrain_t* const _terrain)
{
    /* Size the buffer for maximum 36 quads for every block, TODO-OPT: Would multiple smaller buffers be better? */
    size_t quads = SUBCHUNK_SIZE_X * SUBCHUNK_SIZE_Y * SUBCHUNK_SIZE_Z * 6 * 6;
    std::vector<Uint32> ind;
    ind.reserve(quads * 6);
    for (size_t i = 0; i < quads; i++)
    {
        ind.push_back(i * 4 + 0);
        ind.push_back(i * 4 + 1);
        ind.push_back(i * 4 + 2);
        ind.push_back(i * 4 + 2);
        ind.push_back(i * 4 + 1);
        ind.push_back(i * 4 + 3);
    }

    glBindVertexArray(0);
    glGenBuffers(1, &ebo);
    tetra::gl_obj_label(GL_BUFFER, ebo, "[Level]: EBO (Main)");
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ind.size() * sizeof(ind[0]), ind.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

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

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBindVertexArray(0);

    set_terrain(_terrain);

    last_tick = SDL_GetTicks() / 50;

    player_eid = ecs.create();
}

level_t::~level_t()
{
    glBindVertexArray(0);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &ent_missing_vbo);
    glDeleteVertexArrays(1, &ent_missing_vao);
    for (chunk_cubic_t* c : chunks_render_order)
        delete c;
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
    chunks_render_order.clear();
    cmap.clear();
    cmap.clear();

    /* TODO: In the future if the player is stored here, they shouldn't be deleted */
    ecs.clear();
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

void level_t::tick_real()
{
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

    for (auto [entity, transform, velocity] : ecs.view<entity_transform_t, entity_velocity_t>().each())
    {
        transform.x += velocity.vel_x / 20;
        transform.y += velocity.vel_y / 20;
        transform.z += velocity.vel_z / 20;
    }
}
