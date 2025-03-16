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
#ifndef MCS_B181__CLIENT__SOUND__SOUND_RESOURCES_H
#define MCS_B181__CLIENT__SOUND__SOUND_RESOURCES_H

#include <map>
#include <string>
#include <vector>

struct sound_info_t
{
    enum sound_category_t
    {
        SOUND_CATEGORY_MASTER,

        SOUND_CATEGORY_MUSIC,
        SOUND_CATEGORY_WEATHER,
        SOUND_CATEGORY_HOSTILE,
        SOUND_CATEGORY_PLAYER,

        SOUND_CATEGORY_RECORD,
        SOUND_CATEGORY_BLOCKS,
        SOUND_CATEGORY_NEUTRAL,
        SOUND_CATEGORY_AMBIENT,

        SOUND_CATEGORY_COUNT,
    };

    static const char* sound_category_to_str(sound_category_t);

    /** Rarity weighting */
    int weight = 1;

    struct
    {
        /** If true then the sound should use buffer queuing  */
        bool stream : 1;

        /** Used internally when selecting a sound */
        bool is_event : 1;
    } flags = { .stream = 0, .is_event = 0 };

    /** Sound category (Used for volume sliders) */
    sound_category_t category = sound_category_t::SOUND_CATEGORY_MASTER;

    /** Volume multiplier */
    float volume = 1.0f;

    /** Pitch multiplier */
    float pitch = 1.0f;

    /** Sound resource id (ex. minecraft:mob.horse.skeleton.death) */
    std::string id_sound;

    /** Subtitle resource id (ex. minecraft:subtitles.mob.horse.skeleton.death) */
    std::string id_sub;

    /** Value of "name" field from sounds.json with the domain prepended */
    std::string name;

    /** Full physfs path (or if(is_event) then the event id) */
    std::string path;
};

/* Forward declare */
struct ImGuiTextFilter;

struct sound_resources_t
{
    /**
     * Create sound resources object
     *
     * @param path_assets_obfuscated Physfs path to the obfuscated assets (What is stored in .minecraft/assets/) (Must end with /)
     * @param path_assets_normal Physfs path to assets with un-obfuscated names (Must end with /)
     */
    sound_resources_t(const std::string path_assets_obfuscated, const std::string path_assets_normal);

    ~sound_resources_t();

    /**
     * Get sound for corresponding resource identifier (Recursing if necessary)
     *
     * @param sound_id Resource identifier (ex. minecraft:mob.horse.skeleton.death)
     * @param out Destination for sound info
     *
     * @returns true if a sound was found, false otherwise
     */
    bool get_sound(const std::string sound_id, sound_info_t& out) const;

    /**
     * Display imgui widgets for viewing the data structure
     *
     * @param play_sound Caller should play the sound described by sound_to_play, if this is set to true
     * @param sound_to_play Destination for sound info if play_sound == true
     */
    void imgui_contents(bool& play_sound, sound_info_t& sound_to_play);

private:
    struct sound_event_t
    {
        int total_weight = 0;
        std::vector<sound_info_t> entries;
    };

    /**
     * Parse a sound.json file for a given domain from a given path
     *
     * @param domain Resource identifier domain
     * @param path Full physfs path to the json
     */
    void parse_sound_json(const std::string domain, const std::string path);

    std::map<std::string, sound_event_t> sounds;

    ImGuiTextFilter* text_filter;
};

#endif
