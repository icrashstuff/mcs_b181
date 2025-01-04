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

#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <string>
#include <vector>

/**
 * TODO: Mimmaping - The structure is there it just needs to be implemented
 * TODO-OPT: Make animation code less fragile
 * TODO: Merge later updates clock and compass into single texture?
 * TODO: Add mechanism to retrieve UV data, maybe get_texture("id.png")?
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
     */
    texture_terrain_t(std::string path_textures);

    ~texture_terrain_t();

    /**
     * Set the clock rotation
     *
     * @param flail If true then the clock flails around, if false then it follows mc_time
     * @param mc_time Current minecraft time (in 20hz ticks)
     */
    void set_mc_time(bool flail, Uint64 _mc_time) { clock_flail = flail, clock_mc_time = _mc_time; }

    /**
     * Set the compass rotation
     *
     * @param flail If true then the compass flails around, if false then it follows rotation
     * @param rotation Compass direction in degrees
     */
    void set_compass_rotation(bool flail, float rotation) { compass_flail = flail, compass_rotation = rotation; };

    /**
     * Update and upload the terrain texture
     *
     * Must be called inside the same OpenGL context as the constructor
     *
     * NOTE: The time values used for determining animations (except the clock & compass) are to tied SDL_GetTicks()
     */
    void update();

    /**
     * Show an ImGui child window for inspecting internals
     *
     * @param id Force a specific window id, NULL for automatic
     */
    bool imgui_view(const char* id = NULL);

    GLuint tex_id = 0;

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
        Uint8* data_mipmaped[5] = { 0 };
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
        int* frame_offsets = 0;
        char* name = 0;
    };

    /* TODO-OPT: Move this to a smaller data structure? (texture_post_pack_t)*/
    std::vector<texture_pre_pack_t> textures;
    std::vector<texture_pre_pack_t> anim_textures;
};
#endif
