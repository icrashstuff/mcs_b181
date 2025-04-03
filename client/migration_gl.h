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
 *
 * OpenGL function stubs to make mcs_b181_client compile and (kinda) run
 */
#ifndef MCS_B181__CLIENT__MIGRATION_GL_H
#define MCS_B181__CLIENT__MIGRATION_GL_H

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_stdinc.h>

#ifdef GL_TEXTURE_2D
#error "Conflicting OpenGL header loaded"
#endif

typedef Sint32 GLsizei;
typedef Sint32 GLint;
typedef Uint32 GLenum;
typedef Uint32 GLuint;
typedef char GLchar;
typedef bool GLboolean;
typedef float GLfloat;
typedef float GLclampf;
typedef size_t GLsizeiptr;

#define GL_NONE 0
#define GL_FALSE 0
#define GL_TRUE 1

#define GL_CLAMP_TO_EDGE 0x812F
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_TEXTURE_1D 0x0DE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02
#define GL_TRIANGLES 0x0004
#define GL_DEPTH_FUNC 0x0B74
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_INT 0x1405
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_VERTEX_ARRAY 0x8074
#define GL_BUFFER 0x82E0
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_CULL_FACE 0x0B44
#define GL_FRONT_AND_BACK 0x0408
#define GL_TEXTURE_LOD_BIAS 0x8501
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_RGBA 0x1908
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_MAX_TEXTURE_LOD_BIAS 0x84FD
#define GL_TEXTURE_LOD_BIAS 0x8501
#define GL_STATIC_DRAW 0x88E4
#define GL_ARRAY_BUFFER 0x8892
#define GL_UNSIGNED_SHORT 0x1403
#define GL_RGB 0x1907
#define GL_DEPTH_TEST 0x0B71
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_EQUAL 0x0202
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_SHADER 0x82E1
#define GL_PROGRAM 0x82E2
#define GL_TEXTURE 0x1702
#define GL_NEAREST_MIPMAP_LINEAR 0x2702

SDL_FORCE_INLINE void glEnable(GLenum) { }
SDL_FORCE_INLINE void glDisable(GLenum) { }
SDL_FORCE_INLINE void glGenTextures(GLsizei n, GLuint* v) { memset(v, 0, n * sizeof(n)); }
SDL_FORCE_INLINE void glDeleteTextures(GLsizei, const GLuint*) { }
SDL_FORCE_INLINE void glBindTexture(GLenum, GLuint) { }
SDL_FORCE_INLINE void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) { }
SDL_FORCE_INLINE void glTexParameterf(GLenum, GLenum, GLfloat) { }
SDL_FORCE_INLINE void glTexParameteri(GLenum, GLenum, GLint) { }

#define MIGRATION_GL__SHADER_STATUS 1

SDL_FORCE_INLINE GLuint glCreateProgram() { return MIGRATION_GL__SHADER_STATUS; }
SDL_FORCE_INLINE GLuint glCreateShader(GLenum) { return MIGRATION_GL__SHADER_STATUS; }
SDL_FORCE_INLINE void glGetShaderiv(GLuint, GLenum, GLint* v) { *v = MIGRATION_GL__SHADER_STATUS; }
SDL_FORCE_INLINE void glGetProgramiv(GLuint, GLenum, GLint* v) { *v = MIGRATION_GL__SHADER_STATUS; }

SDL_FORCE_INLINE void glGetProgramInfoLog(GLuint, GLsizei max_len, GLsizei* len, GLchar* out)
{
    *len = 0;
    memset(out, '\0', max_len);
}
SDL_FORCE_INLINE void glGetShaderInfoLog(GLuint, GLsizei max_len, GLsizei* len, GLchar* out) { glGetProgramInfoLog(0, max_len, len, out); }
SDL_FORCE_INLINE void glLinkProgram(GLuint) { }
SDL_FORCE_INLINE void glDeleteProgram(GLuint) { }
SDL_FORCE_INLINE void glDeleteShader(GLuint) { }
SDL_FORCE_INLINE void glDetachShader(GLuint, GLuint) { }
SDL_FORCE_INLINE void glVertexAttribIPointer(GLuint, GLint, GLenum, GLsizei, const void*) { }
SDL_FORCE_INLINE void glBindVertexArray(GLuint) { }
SDL_FORCE_INLINE void glDeleteVertexArrays(GLsizei, const GLuint*) { }
SDL_FORCE_INLINE void glGenVertexArrays(GLsizei n, GLuint* v) { memset(v, 0, n * sizeof(n)); }
SDL_FORCE_INLINE void glDeleteBuffers(GLsizei, const GLuint*) { }
SDL_FORCE_INLINE void glGenBuffers(GLsizei n, GLuint* v) { memset(v, 0, n * sizeof(n)); }
SDL_FORCE_INLINE void glBindBuffer(GLenum, GLuint) { }
SDL_FORCE_INLINE void glBufferData(GLenum, GLsizeiptr, const void*, GLenum) { }
SDL_FORCE_INLINE void glEnableVertexAttribArray(GLuint) { }
SDL_FORCE_INLINE void glAttachShader(GLuint, GLuint) { }
SDL_FORCE_INLINE void glCompileShader(GLuint) { }
SDL_FORCE_INLINE void glBlendFunc(GLenum, GLenum) { }
SDL_FORCE_INLINE void glActiveTexture(GLenum) { }
SDL_FORCE_INLINE void glAlphaFunc(GLenum, GLclampf) { }
SDL_FORCE_INLINE void glPolygonMode(GLenum, GLenum) { }
SDL_FORCE_INLINE void glShaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*) { }
SDL_FORCE_INLINE GLint glGetUniformLocation(GLuint, const GLchar*) { return -1; }
SDL_FORCE_INLINE void glPixelStorei(GLenum, GLint) { }
SDL_FORCE_INLINE void glGetTexParameterfv(GLenum, GLenum, GLfloat* v) { *v = 0; }
SDL_FORCE_INLINE void glGetTexParameteriv(GLenum, GLenum, GLint* v) { *v = 0; }
SDL_FORCE_INLINE void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) { }
SDL_FORCE_INLINE void glUniform1i(GLint, GLint) { }
SDL_FORCE_INLINE void glUniform1f(GLint, GLfloat) { }
SDL_FORCE_INLINE void glUniform2f(GLint, GLfloat, GLfloat) { }
SDL_FORCE_INLINE void glUniform3f(GLint, GLfloat, GLfloat, GLfloat) { }
SDL_FORCE_INLINE void glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat) { }
SDL_FORCE_INLINE void glUniformMatrix3fv(GLint, GLsizei, GLboolean, const GLfloat*) { }
SDL_FORCE_INLINE void glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat*) { }
SDL_FORCE_INLINE void glUseProgram(GLuint) { }
SDL_FORCE_INLINE void glDepthFunc(GLenum) { }
SDL_FORCE_INLINE void glDepthMask(GLboolean) { }
SDL_FORCE_INLINE void glGetBooleanv(GLenum, GLboolean* v) { *v = 0; }
SDL_FORCE_INLINE void glGetFloatv(GLenum, GLfloat* v) { *v = 0; }
SDL_FORCE_INLINE void glGetIntegerv(GLenum, GLint* v) { *v = 0; }
SDL_FORCE_INLINE void glDrawElements(GLenum, GLsizei, GLenum, const void*) { }

namespace tetra
{
SDL_FORCE_INLINE void gl_obj_label(GLenum, GLuint, const GLchar*, ...) { }
};

#endif
