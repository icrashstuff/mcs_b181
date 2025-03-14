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
#include "tetra/tetra_gl.h"

#include <SDL3/SDL.h>

#include "shaders.h"

#include "tetra/log.h"
#include "tetra/util/physfs/physfs.h"

#define READ_FILE_CHUNK 1024
static std::vector<char> read_file(PHYSFS_file* fd)
{
    std::vector<char> dat;
    dat.resize(READ_FILE_CHUNK);
    size_t so_far = 0;
    int read;
    while ((read = PHYSFS_readBytes(fd, dat.data() + so_far, READ_FILE_CHUNK)) > 0)
    {
        so_far += read;
        dat.resize(so_far + READ_FILE_CHUNK);
    }

    dat.resize(so_far);

    return dat;
}

/**
 * Create and compile a shader
 *
 * @param shader Pointer to store shader ID
 * @param shader_type One of GL_VERTEX_SHADER, GL_FRAGMENT_SHADER
 * @param fd File descriptor for the shader source code
 */
static bool shader_create_compile(GLuint* shader, GLenum shader_type, PHYSFS_file* fd)
{
    int result = 0;
    std::vector<char> source_vec = read_file(fd);
    const char* source = source_vec.data();
    int source_len = source_vec.size();

    *shader = glCreateShader(shader_type);
    glShaderSource(*shader, 1, &source, &source_len);
    glCompileShader(*shader);
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &result);

    return result;
}

static std::string shader_get_log(GLuint shader_id)
{
    char buf[2048];
    GLsizei log_len;

    glGetShaderInfoLog(shader_id, SDL_arraysize(buf), &log_len, buf);

    return buf;
}

/**
 * Create and link a program, DOES NOT delete the input shaders
 *
 * @param program Pointer to store program ID
 * @param source Source code for shader
 */
static bool program_create_link(GLuint* program, GLuint shader_vertex, GLuint shader_fragment)
{
    int result = 0;

    *program = glCreateProgram();

    glAttachShader(*program, shader_vertex);
    glAttachShader(*program, shader_fragment);
    glLinkProgram(*program);
    glDetachShader(*program, shader_vertex);
    glDetachShader(*program, shader_fragment);

    glGetProgramiv(*program, GL_LINK_STATUS, &result);

    return result;
}

static std::string program_get_log(GLuint shader_id)
{
    char buf[2048];
    GLsizei log_len;

    glGetProgramInfoLog(shader_id, SDL_arraysize(buf), &log_len, buf);

    return buf;
}

void shader_t::build()
{
    glDeleteProgram(id);
    id = 0;
    loc_model = -1;
    loc_camera = -1;
    loc_projection = -1;

    GLuint id_vtx;
    GLuint id_frag;

    bool status_vtx;
    bool status_frag;
    {
        PHYSFS_file* fd_vtx = PHYSFS_openRead(path_vtx.c_str());
        PHYSFS_file* fd_frag = PHYSFS_openRead(path_frag.c_str());

        if (!fd_vtx)
            dc_log_error("Failed to open vert shader \"%s\" for reading", path_vtx.c_str());
        if (!fd_frag)
            dc_log_error("Failed to open frag shader \"%s\" for reading", path_frag.c_str());

        if (!fd_vtx || !fd_frag)
        {
            PHYSFS_close(fd_vtx);
            PHYSFS_close(fd_frag);
            return;
        }

        status_vtx = shader_create_compile(&id_vtx, GL_VERTEX_SHADER, fd_vtx);
        status_frag = shader_create_compile(&id_frag, GL_FRAGMENT_SHADER, fd_frag);

        if (gl_prefix.length())
        {
            tetra::gl_obj_label(GL_SHADER, id_vtx, "%s: Shader Vert", gl_prefix.c_str());
            tetra::gl_obj_label(GL_SHADER, id_frag, "%s: Shader Frag", gl_prefix.c_str());
        }
        else
        {
            tetra::gl_obj_label(GL_SHADER, id_vtx, "Shader Vert: \"%s\"", path_vtx.c_str());
            tetra::gl_obj_label(GL_SHADER, id_frag, "Shader Frag: \"%s\"", path_frag.c_str());
        }

        PHYSFS_close(fd_vtx);
        PHYSFS_close(fd_frag);
    }

    if (!status_vtx)
        dc_log_error("Failed to compile vert shader \"%s\":\n\"%s\"", path_vtx.c_str(), shader_get_log(id_vtx).c_str());
    if (!status_frag)
        dc_log_error("Failed to compile frag shader\"%s\":\n\"%s\"", path_frag.c_str(), shader_get_log(id_frag).c_str());

    if (!status_vtx || !status_frag)
    {
        glDeleteShader(id_vtx);
        glDeleteShader(id_frag);
        return;
    }

    if (!program_create_link(&id, id_vtx, id_frag))
    {
        dc_log_error("Failed to link \"%s\" and \"%s\":\n\"%s\"", path_vtx.c_str(), path_frag.c_str(), program_get_log(id).c_str());
        glDeleteProgram(id);
        glDeleteShader(id_vtx);
        glDeleteShader(id_frag);
        return;
    }

    if (gl_prefix.length())
        tetra::gl_obj_label(GL_PROGRAM, id, "%s: Program", gl_prefix.c_str());
    else
        tetra::gl_obj_label(GL_PROGRAM, id, "Program: \"%s\"+\"%s\"", path_vtx.c_str(), path_frag.c_str());

    loc_model = glGetUniformLocation(id, "model");
    loc_camera = glGetUniformLocation(id, "camera");
    loc_projection = glGetUniformLocation(id, "projection");
}

static std::vector<shader_t*> all_shaders;

shader_t::shader_t(const std::string _path_vtx, const std::string _path_frag, const std::string _gl_prefix)
    : path_vtx(_path_vtx)
    , path_frag(_path_frag)
    , gl_prefix(_gl_prefix)
{
    all_shaders.push_back(this);
}

shader_t::~shader_t()
{
    for (auto it = all_shaders.begin(); it != all_shaders.end();)
    {
        if (*it != this)
            it = next(it);
        else
            it = all_shaders.erase(it);
    }
}

void shader_t::build_all()
{
    Uint64 tick_shader_start = SDL_GetTicksNS();
    dc_log("Building %zu shaders", all_shaders.size());
    size_t built = 0;
    for (shader_t* s : all_shaders)
    {
        s->build();
        built++;
    }
    double elapsed = (SDL_GetTicksNS() - tick_shader_start) / 1000000.0;
    dc_log("Compiled %zu shader%s in %.2f ms (%.2f per)", built, built == 1 ? "" : "s", elapsed, elapsed / double(SDL_max(1, built)));
}
