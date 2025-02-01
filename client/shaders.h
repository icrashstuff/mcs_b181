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
#ifndef MCS_B181_CLIENT_SHADERS_H
#define MCS_B181_CLIENT_SHADERS_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>

struct shader_t
{
    /**
     * @param _path_vtx PHYSFS path to the vertex shader source
     * @param _path_frag PHYSFS path to the fragment shader source
     */
    shader_t(const std::string path_vtx, const std::string path_frag);

    ~shader_t();

    /**
     * Builds and links the shader program specifed by path_vtx and path_frag
     *
     * NOTE: This immediately deletes the shader program (if built)
     */
    void build();

    /**
     * Rebuilds all shaders in the global shader vector
     */
    static void build_all();

    GLuint id = 0;

    GLint loc_model = -1;
    GLint loc_camera = -1;
    GLint loc_projection = -1;

    inline void set_uniform(const std::string name, const int a)
    {
        GLint loc = glGetUniformLocation(id, name.c_str());
        if (loc >= 0)
            glUniform1i(loc, a);
    }

    inline void set_uniform(const std::string name, const float a)
    {
        GLint loc = glGetUniformLocation(id, name.c_str());
        if (loc >= 0)
            glUniform1f(loc, a);
    }

    inline void set_uniform(const std::string name, const glm::mat4 a)
    {
        GLint loc = glGetUniformLocation(id, name.c_str());
        if (loc >= 0)
            glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(a));
    }

    inline void set_uniform(const std::string name, const glm::mat3 a)
    {
        GLint loc = glGetUniformLocation(id, name.c_str());
        if (loc >= 0)
            glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(a));
    }

    inline void set_uniform(const std::string name, const glm::vec4 a)
    {
        GLint loc = glGetUniformLocation(id, name.c_str());
        if (loc >= 0)
            glUniform4f(loc, a.x, a.y, a.z, a.w);
    }

    inline void set_uniform(const std::string name, const glm::vec3 a)
    {
        GLint loc = glGetUniformLocation(id, name.c_str());
        if (loc >= 0)
            glUniform3f(loc, a.x, a.y, a.z);
    }

    inline void set_uniform(const std::string name, const glm::vec2 a)
    {
        GLint loc = glGetUniformLocation(id, name.c_str());
        if (loc >= 0)
            glUniform2f(loc, a.x, a.y);
    }

    inline void set_model(const glm::mat4 mat)
    {
        if (loc_model >= 0)
            glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(mat));
    }

    inline void set_camera(const glm::mat4 mat)
    {
        if (loc_camera >= 0)
            glUniformMatrix4fv(loc_camera, 1, GL_FALSE, glm::value_ptr(mat));
    }

    inline void set_projection(const glm::mat4 mat)
    {
        if (loc_projection >= 0)
            glUniformMatrix4fv(loc_projection, 1, GL_FALSE, glm::value_ptr(mat));
    }

    std::string path_vtx;
    std::string path_frag;
};

#endif
