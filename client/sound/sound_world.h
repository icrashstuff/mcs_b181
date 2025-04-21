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

#ifndef MCS_B181__CLIENT__SOUND__SOUND_WORLD_H
#define MCS_B181__CLIENT__SOUND__SOUND_WORLD_H

#include "sound_resources.h"

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_stdinc.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

/* TODO: Get a unified engine to not crash and burn */
#define SOUND_WORLD_SEPERATE_ENGINES 1

/* Forward declarations */
struct ma_engine;
struct ma_sound;

/** Sound engine, that somewhat mimics the sound engines of java and bedrock */
class sound_world_t
{
public:
    /**
     * Unique and opaque identifier for a source
     *
     * NOTE: An identifier equaling 0 is a null identifier, however an identifier may have a slot id of 0 as long as the counter is non-zero
     *
     * Slot id, bits 0...15
     * Counter, bits 16..63
     */
    typedef Uint64 source_id_t;

    /**
     * Create sound world
     *
     * Number of sources possible is driven by the equation SDL_min(cvr_a_sources_max.get(), max_sources)
     *
     * @param max_sources Maximum number of sources before considering the convar: a_sources_max
     */
    sound_world_t(const Uint16 max_sources = 0xFFFF);

    ~sound_world_t();

    /**
     * Check if source identifier corresponds to a valid identifier
     *
     * @param id Source to check
     */
    bool source_is_valid(const source_id_t id) const;

    /**
     * Request that a source play a sound
     *
     * @param info Definition of sound
     * @param pos Initial position to place sound
     * @param relative Controls whether or not the position is relative to the listener
     *
     * @returns 0 on failure (Most likely source exhaustion), a valid identifier otherwise
     */
    source_id_t request_source(const sound_info_t& info, const glm::f64vec3 pos, const bool relative = false);

    /**
     * Set position of source
     *
     * @param id Source to modify
     * @param pos Position of source
     * @param relative Controls whether or not the position is relative to the listener
     */
    void source_set_pos(const source_id_t id, const glm::f64vec3 pos, const bool relative = false);

    /**
     * Stop source from playing
     *
     * @param id Source to kill
     */
    void source_kill(const source_id_t id);

    /**
     * Kill all sources
     */
    void kill_all();

    /** Suspend sound engine */
    void suspend();

    /** Resume sound engine from suspend */
    void resume();

    int get_num_slots_active() const;

    int get_num_slots() const;

    bool is_music_playing() const;

    void kill_music();

    /**
     * Updates positions, updates stream buffers, and marks any finished slots as available
     */
    void update(const glm::f64vec3 listener_pos, const glm::vec3 listener_direction, const glm::vec3 listener_up);

    /** Render ImGui widgets for viewing/managing this object */
    void imgui_contents();

private:
    bool suspended = 0;

    struct slot_t
    {
        bool in_use = 0;

        /** Number of times the slot has hosted a sound */
        unsigned long counter = 1;

        sound_info_t::sound_category_t category = sound_info_t::SOUND_CATEGORY_MASTER;

        ma_sound* source = nullptr;

        bool pos_is_relative = 0;
        glm::f64vec3 pos;

        sound_info_t info;

        /** Allocates slot_t::source */
        void init();
        /** Frees slot_t::source */
        void destroy();
    };

    std::vector<slot_t> slots;

#if SOUND_WORLD_SEPERATE_ENGINES
    SDL_AtomicInt engine_ref_counter = { 0 };
    ma_engine* engine = nullptr;
    SDL_AudioStream* output = nullptr;
#else
    static SDL_AtomicInt engine_ref_counter;
    static ma_engine* engine;
    static SDL_AudioStream* output = nullptr;
#endif
};

#endif
