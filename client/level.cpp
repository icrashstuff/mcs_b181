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
    __m128i last_strip = one;
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
    __m128i last_strip = one;
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

    /* Update surrounding chunks (This also updates *ALL* above and below) */
    for (chunk_cubic_t* c : chunks_light_order)
    {
        chunk_cubic_t::dirty_level_t dirt_face = chunk_cubic_t::DIRTY_LEVEL_NONE;
        switch (abs(chunk_pos.x - c->pos.x) + abs(chunk_pos.z - c->pos.z))
        {
        case 0:
        case 1:
            dirt_face = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_INTERNAL;
            break;
        case 2:
            dirt_face = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_0;
            break;
        case 3:
            dirt_face = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_1;
            break;
        case 4:
            dirt_face = chunk_cubic_t::DIRTY_LEVEL_LIGHT_PASS_EXT_2;
            break;
        default:
            break;
        }
        if (c->dirty_level < dirt_face)
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

    const glm::f64vec3 camera_pos_capture = get_camera_pos();

    ecs.sort<entity_transform_t>([&camera_pos_capture](const entity_transform_t& a, const entity_transform_t& b) -> bool {
        const float adist = glm::distance(a.pos, camera_pos_capture);
        const float bdist = glm::distance(b.pos, camera_pos_capture);
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

void level_t::render(const glm::ivec2 win_size, const float delta_time)
{
    const int render_distance = (render_distance_override > 0) ? render_distance_override : r_render_distance.get();

    cull_chunks(win_size, render_distance);

    build_dirty_meshes();

    tick();

    glUseProgram(shader_terrain->id);
    glm::mat4 mat_proj = glm::perspective(glm::radians(fov), (float)win_size.x / (float)win_size.y, 0.03125f, render_distance * 32.0f);
    shader_terrain->set_projection(mat_proj);

    glm::mat4 mat_cam = glm::lookAt(get_camera_pos(), get_camera_pos() + camera_direction, camera_up);
    damage_tilt = SDL_clamp(damage_tilt, 0.0f, 1.0f);
    if (gamemode == mc_id::GAMEMODE_CREATIVE || gamemode == mc_id::GAMEMODE_SPECTATOR)
        damage_tilt = 0.0f;
    mat_cam = glm::rotate(glm::mat4(1.0f), -glm::radians(damage_tilt * cvr_r_damage_tilt_magnitude.get()), glm::vec3(0.f, 0.f, 1.f)) * mat_cam;
    damage_tilt -= delta_time / (cvr_r_damage_tilt_magnitude.get() * cvr_r_damage_tilt_rate.get() / 1000.0f);
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
        if (c->index_type == GL_NONE || c->index_count == 0 || !c->visible || !c->vao)
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
        if (c->index_type == GL_NONE || c->index_count_overlay == 0 || !c->visible || !c->vao)
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
        if (c->index_type == GL_NONE || c->index_count_translucent == 0 || !c->visible || !c->vao)
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

    /* For testing */
    entity_physics_t physics;
    physics.reset_to_entity_defaults(ENT_ID_CREEPER);
    ecs.emplace<entity_physics_t>(player_eid, physics);
    ecs.emplace<entity_transform_t>(player_eid, entity_transform_t { .pos = { -30.0, 1280.0, -30.0 } });

    generator_create();
}

level_t::~level_t()
{
    glBindVertexArray(0);
    glDeleteBuffers(1, &ebo);
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

    if (music < 0.6f)
        music = 0.7f;

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
