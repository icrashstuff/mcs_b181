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

#include "panorama.h"

#include <SDL3/SDL.h>

#include "tetra/gui/console.h"
#include "tetra/gui/imgui.h"
#include "tetra/tetra_gl.h"
#include "tetra/util/stbi.h"

struct pano_vert_t
{
    float x;
    float y;
    float z;
};

static const pano_vert_t pano_verts[] = {
    { -1.0f, +1.0f, -1.0f },
    { -1.0f, -1.0f, -1.0f },
    { +1.0f, -1.0f, -1.0f },
    { +1.0f, -1.0f, -1.0f },
    { +1.0f, +1.0f, -1.0f },
    { -1.0f, +1.0f, -1.0f },

    { -1.0f, -1.0f, +1.0f },
    { -1.0f, -1.0f, -1.0f },
    { -1.0f, +1.0f, -1.0f },
    { -1.0f, +1.0f, -1.0f },
    { -1.0f, +1.0f, +1.0f },
    { -1.0f, -1.0f, +1.0f },

    { +1.0f, -1.0f, -1.0f },
    { +1.0f, -1.0f, +1.0f },
    { +1.0f, +1.0f, +1.0f },
    { +1.0f, +1.0f, +1.0f },
    { +1.0f, +1.0f, -1.0f },
    { +1.0f, -1.0f, -1.0f },

    { -1.0f, -1.0f, +1.0f },
    { -1.0f, +1.0f, +1.0f },
    { +1.0f, +1.0f, +1.0f },
    { +1.0f, +1.0f, +1.0f },
    { +1.0f, -1.0f, +1.0f },
    { -1.0f, -1.0f, +1.0f },

    { -1.0f, +1.0f, -1.0f },
    { +1.0f, +1.0f, -1.0f },
    { +1.0f, +1.0f, +1.0f },
    { +1.0f, +1.0f, +1.0f },
    { -1.0f, +1.0f, +1.0f },
    { -1.0f, +1.0f, -1.0f },

    { -1.0f, -1.0f, -1.0f },
    { -1.0f, -1.0f, +1.0f },
    { +1.0f, -1.0f, -1.0f },
    { +1.0f, -1.0f, -1.0f },
    { -1.0f, -1.0f, +1.0f },
    { +1.0f, -1.0f, +1.0f },
};

struct fbo_vert_t
{
    float x;
    float y;
    float z;

    float u;
    float v;
};

static const fbo_vert_t fbo_verts[] = {
    { -1.0f, +1.0f, 1.0f, 0.001f, 0.999f },
    { -1.0f, -1.0f, 1.0f, 0.001f, 0.001f },
    { +1.0f, -1.0f, 1.0f, 0.999f, 0.001f },
    { +1.0f, -1.0f, 1.0f, 0.999f, 0.001f },
    { +1.0f, +1.0f, 1.0f, 0.999f, 0.999f },
    { -1.0f, +1.0f, 1.0f, 0.001f, 0.999f },
};

panorama_t::~panorama_t()
{
    glDeleteFramebuffers(SDL_arraysize(fbo), fbo);

    glDeleteVertexArrays(1, &pano_vao);
    glDeleteVertexArrays(1, &fbo_vao);

    glDeleteBuffers(1, &pano_vbo);
    glDeleteBuffers(1, &fbo_vbo);

    glDeleteTextures(1, &pano_tex);
    glDeleteTextures(SDL_arraysize(fbo_tex), fbo_tex);
}

bool panorama_t::resize(const glm::ivec2 win_size)
{
    if (win_size.x < 1 || win_size.y < 1)
        return false;

    GLint prev_binding_fbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_binding_fbo);

    last_win_size = win_size;

    const int x = average_tex_width;
    const int y = win_size.y * average_tex_width / win_size.x;

    GLenum fbo_status[SDL_arraysize(fbo)] = {};
    for (int i = 0; i < IM_ARRAYSIZE(fbo); i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glBindTexture(GL_TEXTURE_2D, fbo_tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glViewport(0, 0, x, y);
        glBindTexture(GL_TEXTURE_2D, 0);

        fbo_status[i] = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fbo_status[i] != GL_FRAMEBUFFER_COMPLETE)
            dc_log_warn("FBO status returned 0x%04x", fbo_status[i]);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, prev_binding_fbo);

    bool good = 1;

    for (int i = 0; i < IM_ARRAYSIZE(fbo_status); i++)
        if (!fbo_status[i])
            good = 0;

    return good;
}

panorama_t::panorama_t()
{
    /* Create frame buffer for post processing */
    glGenFramebuffers(2, fbo);
    glGenTextures(2, fbo_tex);

    for (int i = 0; i < IM_ARRAYSIZE(fbo); i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);

        /* Frame buffer output */
        glBindTexture(GL_TEXTURE_2D, fbo_tex[i]);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_tex[i], 0);
    }

    for (int i = 0; i < IM_ARRAYSIZE(fbo); i++)
    {
        tetra::gl_obj_label(GL_FRAMEBUFFER, fbo[i], "[Panorama]: FBO %d", i);
        tetra::gl_obj_label(GL_TEXTURE, fbo_tex[i], "[Panorama]: FBO %d: Output", i);
    }

    resize(glm::ivec2(32, 32));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* Load and create panorama cube map */
    glGenTextures(1, &pano_tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, pano_tex);

    tetra::gl_obj_label(GL_TEXTURE, pano_tex, "[Panorama][Cube]: Texture");

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_LOD_BIAS, 1.0f);

    const char* path_fmt = "_resources/assets/minecraft/textures/gui/title/background/panorama_%d.png";

    const Uint8 missing_data[] = { 255, 0, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 0, 255, 255 };

    const GLenum assignments[] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    };

    average_tex_width = 0;
    for (int i = 0; i < 6; i++)
    {
        char buf[512];
        snprintf(buf, SDL_arraysize(buf), path_fmt, i);
        int x, y, channels;
        Uint8* img_dat = stbi_physfs_load(buf, &x, &y, &channels, 4);

        stbi_set_flip_vertically_on_load(false);

        if (img_dat)
        {
            glTexImage2D(assignments[i], 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_dat);
            average_tex_width += x;
        }
        else
        {
            glTexImage2D(assignments[i], 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, missing_data);
            average_tex_width += 2;
            dc_log_error("Failed to load: \"%s\"", buf);
        }

        stbi_image_free(img_dat);
    }
    average_tex_width /= 3;

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    /* Create Pano Cube resources */
    glGenVertexArrays(1, &pano_vao);
    tetra::gl_obj_label(GL_VERTEX_ARRAY, pano_vao, "[Panorama][Cube]: VAO");
    glBindVertexArray(pano_vao);

    glGenBuffers(1, &pano_vbo);
    tetra::gl_obj_label(GL_BUFFER, pano_vbo, "[Panorama][Cube]: VBO");
    glBindBuffer(GL_ARRAY_BUFFER, pano_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pano_verts), pano_verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(pano_vert_t), (void*)offsetof(pano_vert_t, x));
    glBindVertexArray(0);

    /* Create FBO display resources */
    glGenVertexArrays(1, &fbo_vao);
    tetra::gl_obj_label(GL_VERTEX_ARRAY, fbo_vao, "[Panorama]: Display: VAO");
    glBindVertexArray(fbo_vao);

    glGenBuffers(1, &fbo_vbo);
    tetra::gl_obj_label(GL_BUFFER, fbo_vbo, "[Panorama]: Display: VBO");
    glBindBuffer(GL_ARRAY_BUFFER, fbo_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fbo_verts), fbo_verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(fbo_vert_t), (void*)offsetof(fbo_vert_t, x));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(fbo_vert_t), (void*)offsetof(fbo_vert_t, u));
    glBindVertexArray(0);

    tick_offset = SDL_rand_bits() | Uint64(SDL_rand_bits()) << 32;
}

/**
 * Draw imgui widgets for controlling the panorama
 */
void panorama_t::imgui_widgets()
{
    ImGui::SliderFloat("FOV", &fov, 30.0f, 120.0f);
    ImGui::SliderInt("Blur Radius", &blur_radius, 1, 32);

    ImGui::Text("Yaw: %.2f", last_yaw);
    ImGui::Text("Pitch: %.2f", last_pitch);
    ImGui::Text("Roll: %.2f", last_roll);

    ImGui::SliderInt("Period: Yaw", &period_yaw, 1, 1 << 18, "%d ms");
    ImGui::SliderInt("Period: Pitch", &period_pitch, 1, 1 << 18, "%d ms");
    ImGui::SliderInt("Period: Roll", &period_roll, 1, 1 << 18, "%d ms");

    ImGui::SliderFloat("Range: Pitch", &range_pitch, 0, 179, "%.3f deg");
    ImGui::SliderFloat("Range: Roll", &range_roll, 0, 179, "%.3f deg");

    ImGui::SliderFloat("Zero-Point: Pitch", &zero_pitch, -89, 89, "%.3f deg");
    ImGui::SliderFloat("Zero-Point: Roll", &zero_roll, -89, 89, "%.3f deg");

    const int x = average_tex_width;
    const int y = last_win_size.y * average_tex_width / last_win_size.x;
    ImGui::Image((ImTextureID)(uintptr_t)fbo_tex[0], ImVec2(x, y), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::Image((ImTextureID)(uintptr_t)fbo_tex[1], ImVec2(x, y), ImVec2(0, 1), ImVec2(1, 0));
}

void panorama_t::render(const glm::ivec2 win_size, const bool disable_depth_writes)
{
    if (!resize(win_size))
        return;
    GLboolean prev_depth_test;
    GLboolean prev_depth_mask;
    GLint prev_depth_func;
    GLint prev_binding_fbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_binding_fbo);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prev_depth_mask);
    glGetBooleanv(GL_DEPTH_TEST, &prev_depth_test);
    glGetIntegerv(GL_DEPTH_FUNC, &prev_depth_func);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);

    const glm::mat4 mat_proj = glm::perspective(glm::radians(fov), (float)win_size.x / (float)win_size.y, 0.01f, 10.0f);

    Uint64 off_sdl_tick = SDL_GetTicks() + tick_offset;

    period_yaw = SDL_max(period_yaw, 1);
    period_pitch = SDL_max(period_pitch, 1);
    period_roll = SDL_max(period_roll, 1);

    float yaw = ((off_sdl_tick % period_yaw) * 360.f / float(period_yaw));

    float sin_pitch = SDL_sinf(float(off_sdl_tick % period_pitch) * 2.0 * SDL_PI_F / float(period_pitch));
    float sin_roll = SDL_sinf(float(off_sdl_tick % period_roll) * 2.0 * SDL_PI_F / float(period_roll));

    float pitch = (sin_pitch * .5f + .5f) * (range_pitch)-range_pitch / 2.0f - zero_pitch;
    float roll = (sin_roll * .5f + .5f) * (range_roll)-range_roll / 2.0f - zero_roll;

    last_yaw = yaw;
    last_pitch = pitch;
    last_roll = roll;

    glm::vec3 direction;
    direction.x = SDL_cosf(glm::radians(yaw + 0.125f)) * SDL_cosf(glm::radians(pitch));
    direction.y = SDL_sinf(glm::radians(pitch));
    direction.z = SDL_sinf(glm::radians(yaw + 0.125f)) * SDL_cosf(glm::radians(pitch));
    direction = glm::normalize(direction);

    const glm::vec3 direction_roll = glm::normalize(glm::vec3 { direction.x, 0, direction.z });

    glm::mat4 mat_cam = glm::lookAt(glm::vec3(0, 0, 0), direction, glm::vec3(0, 1, 0));
    mat_cam = glm::rotate(mat_cam, glm::radians(roll), direction_roll);

    glUseProgram(pano_shader.id);
    pano_shader.set_camera(mat_cam);
    pano_shader.set_projection(mat_proj);
    glBindVertexArray(pano_vao);
    glBindTexture(GL_TEXTURE_CUBE_MAP, pano_tex);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    /* Horizontal Blur */
    glUseProgram(fbo_shader_blur.id);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);
    fbo_shader_blur.set_uniform("radius", blur_radius);
    fbo_shader_blur.set_uniform("blur_x", 1.0f);
    fbo_shader_blur.set_uniform("blur_y", 0.0f);
    fbo_shader_blur.set_uniform("gradient_mix", 0.0f);
    glBindVertexArray(fbo_vao);
    glBindTexture(GL_TEXTURE_2D, fbo_tex[0]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    /* Vertical Blur */
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
    fbo_shader_blur.set_uniform("blur_x", 0.0f);
    fbo_shader_blur.set_uniform("blur_y", 1.0f);
    fbo_shader_blur.set_uniform("gradient_mix", 0.2f);
    glBindVertexArray(fbo_vao);
    glBindTexture(GL_TEXTURE_2D, fbo_tex[1]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    if (prev_depth_test)
        glEnable(GL_DEPTH_TEST);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(!disable_depth_writes);

    /* Output Blur */
    glUseProgram(fbo_shader_out.id);
    glBindFramebuffer(GL_FRAMEBUFFER, prev_binding_fbo);
    glViewport(0, 0, win_size.x, win_size.y);
    glBindVertexArray(fbo_vao);
    glBindTexture(GL_TEXTURE_2D, fbo_tex[0]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDepthFunc(prev_depth_func);
    glDepthMask(prev_depth_mask);
}
