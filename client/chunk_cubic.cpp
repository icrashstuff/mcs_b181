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
#include "chunk_cubic.h"

/* There is no special reason that this function uses |= instead of +=, other than if one happens to be faster (which is unlikely) it would probably be |= */
void chunk_cubic_t::update_renderer_hints()
{
    bool is_trans[256];

    for (size_t i = 0; i < SDL_arraysize(is_trans); i++)
        is_trans[i] = mc_id::is_transparent(i);

    memset(&renderer_hints, 0, sizeof(renderer_hints));

    /* Very important to set this *AFTER* setting all the fields to zero, otherwise you will be confused where your frame rate went (*facepalm*) */
    renderer_hints.hints_set = 1;

    static_assert(SDL_arraysize(data_block) == SUBCHUNK_SIZE_VOLUME);
    static_assert(SDL_arraysize(data_block) % sizeof(Uint64) == 0);

    /* Check for a uniform chunk of air (This loop will likely be auto-vectorized) */
    Uint32 total_block = 0;
    for (int i = 0; i < SUBCHUNK_SIZE_VOLUME; i++)
        total_block |= data_block[i];
    if (total_block == 0)
    {
        renderer_hints.uniform_air = 1;
        renderer_hints.uniform_opaque = 0;
        renderer_hints.opaque_sides = 0;
        renderer_hints.opaque_face_pos_x = 0;
        renderer_hints.opaque_face_pos_y = 0;
        renderer_hints.opaque_face_pos_z = 0;
        renderer_hints.opaque_face_neg_x = 0;
        renderer_hints.opaque_face_neg_y = 0;
        renderer_hints.opaque_face_neg_z = 0;
        return;
    }

    /* Check for a uniform chunk of opaque to light blocks */
    Uint32 total_trans = 0;
    for (int i = 0; i < SUBCHUNK_SIZE_VOLUME; i++)
        total_trans |= is_trans[data_block[i]];
    if (total_trans == 0)
    {
        renderer_hints.uniform_air = 0;
        renderer_hints.uniform_opaque = 1;
        renderer_hints.opaque_sides = 1;
        renderer_hints.opaque_face_pos_x = 1;
        renderer_hints.opaque_face_pos_y = 1;
        renderer_hints.opaque_face_pos_z = 1;
        renderer_hints.opaque_face_neg_x = 1;
        renderer_hints.opaque_face_neg_y = 1;
        renderer_hints.opaque_face_neg_z = 1;
        return;
    }

    Uint32 has_trans_pos_x = 0;
    Uint32 has_trans_pos_y = 0;
    Uint32 has_trans_pos_z = 0;
    Uint32 has_trans_neg_x = 0;
    Uint32 has_trans_neg_y = 0;
    Uint32 has_trans_neg_z = 0;

    /* Check faces for transparency */
    // int y = (dat_it) & 0x0F;
    // int z = (dat_it >> 4) & 0x0F;
    // int x = (dat_it >> 8) & 0x0F;
    // clang-format off
    for (int z = 0; z < SUBCHUNK_SIZE_Z; z++) for (int y = 0; y < SUBCHUNK_SIZE_Y; y++) has_trans_pos_x |= is_trans[data_block[SUBCHUNK_INDEX(SUBCHUNK_SIZE_X - 1, y, z)]];
    for (int z = 0; z < SUBCHUNK_SIZE_Z; z++) for (int y = 0; y < SUBCHUNK_SIZE_Y; y++) has_trans_neg_x |= is_trans[data_block[SUBCHUNK_INDEX(0, y, z)]];
    for (int x = 0; x < SUBCHUNK_SIZE_X; x++) for (int z = 0; z < SUBCHUNK_SIZE_Z; z++) has_trans_pos_y |= is_trans[data_block[SUBCHUNK_INDEX(x, SUBCHUNK_SIZE_Y - 1, z)]];
    for (int x = 0; x < SUBCHUNK_SIZE_X; x++) for (int z = 0; z < SUBCHUNK_SIZE_Z; z++) has_trans_neg_y |= is_trans[data_block[SUBCHUNK_INDEX(x, 0, z)]];
    for (int x = 0; x < SUBCHUNK_SIZE_X; x++) for (int y = 0; y < SUBCHUNK_SIZE_Y; y++) has_trans_pos_z |= is_trans[data_block[SUBCHUNK_INDEX(x, y, SUBCHUNK_SIZE_Z - 1)]];
    for (int x = 0; x < SUBCHUNK_SIZE_X; x++) for (int y = 0; y < SUBCHUNK_SIZE_Y; y++) has_trans_neg_z |= is_trans[data_block[SUBCHUNK_INDEX(x, y, 0)]];
    // clang-format on

    renderer_hints.opaque_face_pos_x = (has_trans_pos_x == 0);
    renderer_hints.opaque_face_pos_y = (has_trans_pos_y == 0);
    renderer_hints.opaque_face_pos_z = (has_trans_pos_z == 0);
    renderer_hints.opaque_face_neg_x = (has_trans_neg_x == 0);
    renderer_hints.opaque_face_neg_y = (has_trans_neg_y == 0);
    renderer_hints.opaque_face_neg_z = (has_trans_neg_z == 0);

    /* Check if all faces are opaque and set the appropriate hint */
    if ((has_trans_pos_x | has_trans_pos_y | has_trans_pos_z | has_trans_neg_x | has_trans_neg_y | has_trans_neg_z) == 0)
        renderer_hints.opaque_sides = 1;
}
