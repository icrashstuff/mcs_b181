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
#ifndef MCS_B181_CLIENT_TEXTURE_TERRAIN_H
#define MCS_B181_CLIENT_TEXTURE_TERRAIN_H

#include "migration_gl.h"

#include <SDL3/SDL.h>
#include <string>
#include <vector>

#include "shared/ids.h"
#include "texture_ids.h"

#define TERRAIN_MAX_MIPMAP_LEVELS 4

struct terrain_vertex_t
{
    struct vtx_pos_ao_t
    {
        Uint32 dat = 0;

        vtx_pos_ao_t() { }
        vtx_pos_ao_t(const Uint16 multiplier, Sint16 x, Sint16 y, Sint16 z, const Uint8 ao)
        {
            x *= multiplier;
            z *= multiplier;
            y *= multiplier;
            assert(x >= -128);
            assert(y >= -128);
            assert(z >= -128);
            assert(x < 384);
            assert(y < 384);
            assert(z < 384);
            assert(ao <= 0x03);
            x += 128;
            y += 128;
            z += 128;
            dat |= (x << 1);
            dat |= (y << 11);
            dat |= (z << 21);
            dat |= ((ao & 0x03) << 30);
        }
    } pos;

    struct vtx_coloring_t
    {
        Uint32 dat = 0;

        vtx_coloring_t() { }
        vtx_coloring_t(const float r, const float g, const float b, const Uint8 light_block, const Uint8 light_sky)
        {
            dat = 0;
            assert(light_block <= 0x0F);
            assert(light_sky <= 0x0F);
            dat |= (Uint8(SDL_clamp(r, 0.0f, 1.0f) * 255.0f));
            dat |= (Uint8(SDL_clamp(g, 0.0f, 1.0f) * 255.0f)) << 8;
            dat |= (Uint8(SDL_clamp(b, 0.0f, 1.0f) * 255.0f)) << 16;
            dat |= (light_block & 0x0F) << 24;
            dat |= (light_sky & 0x0F) << 28;
        }
    } col;

    struct vtx_texturing_t
    {
        Uint32 dat = 0;

        vtx_texturing_t() { }
        vtx_texturing_t(const float u, const float v)
        {
            dat = 0;
            dat |= (Uint16(u * 32768));
            dat |= (Uint16(v * 32768)) << 16;
        }
        vtx_texturing_t(const glm::vec2 uv)
        {
            dat = 0;
            dat |= (Uint16(uv.r * 32768));
            dat |= (Uint16(uv.g * 32768)) << 16;
        }
    } tex;

    /**
     * Sets up an appropriate VAO for handling terrain_vertex_t vertices
     */
    static void create_vao(GLuint* const vao);

    static void create_vbo(GLuint* const vbo, GLuint* const ebo, const std::vector<terrain_vertex_t>& vtx, const std::vector<Uint8>& ind);
    static void create_vbo(GLuint* const vbo, GLuint* const ebo, const std::vector<terrain_vertex_t>& vtx, const std::vector<Uint16>& ind);
    static void create_vbo(GLuint* const vbo, GLuint* const ebo, const std::vector<terrain_vertex_t>& vtx, const std::vector<Uint32>& ind);

    /**
     * Creates a vbo and ebo for a mesh
     *
     * @return The type used for the indicies or GL_NONE if an empty mesh was provided
     */
    static GLenum create_vbo(GLuint* const vbo, GLuint* const ebo, const std::vector<terrain_vertex_t>& vtx);
};

/**
 * TODO-OPT: Make animation code less fragile
 * TODO: Split clock and compass textures and remove clock and compass weirdness
 */
class texture_terrain_t
{
public:
    /**
     * Load and stitch the block/item atlas
     *
     * Must be called inside an active OpenGL context
     *
     * @param path_textures PHYSFS path containing either the subdirectories: blocks/items or block/item (probably: "/_resources/assets/minecraft/textures/")
     * @param fence Fence associated with the initial upload texture upload
     */
    texture_terrain_t(const std::string path_textures);

    ~texture_terrain_t();

    /**
     * Set the clock rotation
     *
     * @param flail If true then the clock flails around, if false then it follows mc_time
     * @param mc_time Current minecraft time (in 20hz ticks)
     */
    void set_mc_time(const bool flail, const Uint64 _mc_time) { clock_flail = flail, clock_mc_time = _mc_time; }

    /**
     * Set the compass rotation
     *
     * @param flail If true then the compass flails around, if false then it follows rotation
     * @param rotation Compass direction in degrees
     */
    void set_compass_rotation(const bool flail, const float rotation) { compass_flail = flail, compass_rotation = rotation; };

    /**
     * Update and upload the terrain texture
     *
     * Must be called inside the same OpenGL context as the constructor
     *
     * NOTE: It would probably be wise to call this on a background thread
     * NOTE: The time values used for determining animations (except the clock & compass) are to tied SDL_GetTicks()
     */
    void update(SDL_GPUCopyPass* copy_pass);

    /**
     * Show an ImGui child window for inspecting internals
     *
     * @param id Force a specific window id, NULL for automatic
     */
    bool imgui_view(const char* id = NULL);

    SDL_GPUTextureSamplerBinding binding = { nullptr, nullptr };

    const inline mc_id::terrain_face_t get_face(const mc_id::terrain_face_id_t id)
    {
        if (id < 0 || id > mc_id::FACE_DEBUG)
            return texture_faces[mc_id::FACE_DEBUG].face;
        return texture_faces[id].face;
    }

private:
    /**
     * Dump all mipmap levels to /game/screenshots/terrain_LEVEL.png
     */
    void dump_mipmaps();

    Uint64 time_last_update = 0;

    bool clock_flail = 0;
    int clock_flail_dir_countdown = 0;
    int clock_flail_dir = 0.0;
    int clock_flail_mc_time = 0.0;
    Uint64 clock_mc_time = 0;

    bool compass_flail = 0;
    int compass_flail_dir_countdown = 0;
    float compass_flail_dir = 0.0;
    float compass_flail_rotation = 0.0;
    float compass_rotation = 0.0;

    size_t tex_filled_area = 0;
    size_t tex_base_height = 0;
    size_t tex_base_width = 0;

    /**
     * Mipmaps level 0 -> raw_mipmaps.size()
     */
    std::vector<Uint8*> raw_mipmaps;

    struct texture_pre_pack_t
    {
        Uint8* data_stbi = 0;
        std::vector<Uint8> data_mipmaped[TERRAIN_MAX_MIPMAP_LEVELS + 1] = {};
        int w;
        int h;
        int x = 0;
        int y = 0;
        bool animated = 0;
        bool is_item = 0;
        bool interpolate = 0;
        int frame_time = 0;
        int frame_cur = 0;
        int frame_num_individual = 1;
        int frame_offsets_len = 1;
        std::vector<int> frame_offsets = {};
        std::string name = {};
    };

    struct texture_post_pack_t
    {
        int w = 0, h = 0;
        int x = 0, y = 0;
        mc_id::terrain_face_t face;
        texture_post_pack_t() { }
        texture_post_pack_t(const texture_pre_pack_t a, const mc_id::terrain_face_id_t fid, const double atlas_width, const double atlas_height)
        {
            x = a.x, y = a.y, w = a.w, h = a.h;
            mc_id::terrain_face_t sub_coords = mc_id::get_face_sub_coords(fid);
            face.corners[0] = { (sub_coords.corners[0].x * w + x) / atlas_width, (sub_coords.corners[0].y * h + y) / atlas_height };
            face.corners[1] = { (sub_coords.corners[1].x * w + x) / atlas_width, (sub_coords.corners[1].y * h + y) / atlas_height };
            face.corners[2] = { (sub_coords.corners[2].x * w + x) / atlas_width, (sub_coords.corners[2].y * h + y) / atlas_height };
            face.corners[3] = { (sub_coords.corners[3].x * w + x) / atlas_width, (sub_coords.corners[3].y * h + y) / atlas_height };
        }
    };

    texture_post_pack_t texture_faces[mc_id::FACE_COUNT];

    mc_id::terrain_face_id_t imgui_selected_face = mc_id::FACE_ATLAS;

    std::vector<texture_pre_pack_t> anim_textures;
};
#endif
