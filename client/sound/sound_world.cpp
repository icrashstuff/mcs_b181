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

#include "miniaudio_unifdef.h"

#include "sound_world.h"

#include "stb_vorbis.h"

#include "tetra/log.h"
#include "tetra/util/convar.h"

#include "shared/misc.h"

#include "miniaudio_physfs.hpp"

#if defined(SDL_PLATFORM_APPLE) || defined(SDL_PLATFORM_ANDROID)
#define SOUND_ENGINE_LOW_RESOURCE true
#else
#define SOUND_ENGINE_LOW_RESOURCE false
#endif

#undef PHYSFS_file

static convar_int_t cvr_a_sources_max {
    "a_sources_num",
    SOUND_ENGINE_LOW_RESOURCE ? 32 : 255,
    1,
    65535,
    "Number of OpenAL sources that the sound engine can use",
    CONVAR_FLAG_SAVE | CONVAR_FLAG_DEV_ONLY,
};

#define CONVARS_FOR_SOUND_CAT(SUFFIX, CATEGORY) \
    static convar_int_t cvr_a_volume_##SUFFIX { \
        "a_volume_" #SUFFIX,                    \
        100,                                    \
        0,                                      \
        100,                                    \
        "Volume of category " #CATEGORY,        \
        CONVAR_FLAG_SAVE,                       \
    };

#define ADD_SWITCH_CASE(CASE, ACTION) \
    case CASE:                        \
        ACTION;                       \
        break;

CONVARS_FOR_SOUND_CAT(master, sound_info_t::SOUND_CATEGORY_MASTER);

CONVARS_FOR_SOUND_CAT(music, sound_info_t::SOUND_CATEGORY_MUSIC);
CONVARS_FOR_SOUND_CAT(weather, sound_info_t::SOUND_CATEGORY_WEATHER);
CONVARS_FOR_SOUND_CAT(hostile, sound_info_t::SOUND_CATEGORY_HOSTILE);
CONVARS_FOR_SOUND_CAT(player, sound_info_t::SOUND_CATEGORY_PLAYER);

CONVARS_FOR_SOUND_CAT(record, sound_info_t::SOUND_CATEGORY_RECORD);
CONVARS_FOR_SOUND_CAT(blocks, sound_info_t::SOUND_CATEGORY_BLOCKS);
CONVARS_FOR_SOUND_CAT(neutral, sound_info_t::SOUND_CATEGORY_NEUTRAL);
CONVARS_FOR_SOUND_CAT(ambient, sound_info_t::SOUND_CATEGORY_AMBIENT);

#define MA_CALL(RESULT, _CALL)                                                                         \
    do                                                                                                 \
    {                                                                                                  \
        RESULT = _CALL;                                                                                \
        if (RESULT != MA_SUCCESS)                                                                      \
            dc_log_error("[MINIAUDIO]: %s, while calling: %s", ma_result_description(RESULT), #_CALL); \
        SDL_assert(RESULT == MA_SUCCESS);                                                              \
    } while (0);

bool sound_world_t::source_is_valid(const source_id_t source) const
{
    if (!source)
        return false;

    const Uint16 id = source & 0xFFFF;
    const Uint32 counter = source >> 16;

    if (id > slots.size())
        return false;

    return slots[id].counter == counter && slots[id].in_use;
}

int sound_world_t::get_num_slots() const { return slots.size(); }

int sound_world_t::get_num_slots_active() const
{
    int num_slots = 0;

    for (auto& s : slots)
        num_slots += s.in_use;

    return num_slots;
}

#if !(SOUND_WORLD_SEPERATE_ENGINES)
SDL_AtomicInt sound_world_t::engine_ref_counter = { 0 };
ma_engine* sound_world_t::engine = nullptr;
#endif

static void audio_data_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
    ma_engine* engine = static_cast<ma_engine*>(userdata);

    if (!total_amount || !userdata || !stream)
        return;

    Uint32 frame_size = ma_get_bytes_per_frame(ma_format_f32, ma_engine_get_channels(engine));
    Uint32 num_frames = Uint32(total_amount) / ma_get_bytes_per_frame(ma_format_f32, ma_engine_get_channels(engine));
    ma_uint64 num_frames_read = 0;

    if (!num_frames || !frame_size)
        return;

    Uint8* buffer = (Uint8*)malloc(total_amount);
    if (!buffer)
        return;

    ma_engine_read_pcm_frames(engine, buffer, num_frames, &num_frames_read);

    SDL_PutAudioStreamData(stream, buffer, int(num_frames_read) * int(frame_size));

    free(buffer);
}

sound_world_t::sound_world_t(const Uint16 max_sources)
{
    dc_log("Initializing sound engine");
    Uint64 sdl_tick_start_ns = SDL_GetTicksNS();

    engine = new ma_engine;
    ma_result result;

    slots.resize(SDL_min(cvr_a_sources_max.get(), max_sources));
    for (auto& slot : slots)
        slot.init();

    if (SDL_AtomicIncRef(&engine_ref_counter) != 0)
    {
        dc_log("Sound engine already initialized");
        return;
    }

    static ma_engine_config engine_cfg = ma_engine_config_init();
    engine_cfg.noDevice = MA_TRUE;
    engine_cfg.pResourceManagerVFS = &ma_vfs_callbacks_physfs;
    engine_cfg.channels = 2;
    engine_cfg.sampleRate = 48000;

    MA_CALL(result, ma_engine_init(&engine_cfg, engine));

    if (!engine)
    {
        dc_log_error("Failed to initialize sound engine");
        return;
    }

    SDL_AudioSpec spec = {
        .format = SDL_AUDIO_F32,
        .channels = int(ma_engine_get_channels(engine)),
        .freq = int(ma_engine_get_sample_rate(engine)),
    };

    output = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_data_callback, engine);

    SDL_ResumeAudioStreamDevice(output);

    dc_log("Sound Engine info");
    dc_log("*** MA Version:    %s ***", ma_version_string());
    dc_log("*** MA Channels:   %d ***", ma_engine_get_channels(engine));
    dc_log("*** MA Samplerate: %d hz ***", ma_engine_get_sample_rate(engine));
    dc_log("*** SDL Driver:    %s ***", SDL_GetCurrentAudioDriver());

    dc_log("Sound engine initialized in %.3f ms", double(SDL_GetTicksNS() - sdl_tick_start_ns) / 1000000.0);
}

sound_world_t::~sound_world_t()
{
    kill_all();
    for (auto& slot : slots)
        slot.destroy();

    if (SDL_AtomicDecRef(&engine_ref_counter))
    {
        dc_log("Destroying sound engine");
        SDL_PauseAudioStreamDevice(output);
        SDL_DestroyAudioStream(output);
        ma_engine_uninit(engine);
        delete engine;
    }
}

void sound_world_t::slot_t::init()
{
    destroy();
    source = new ma_sound;
}

void sound_world_t::slot_t::destroy()
{
    delete source;
    source = NULL;
}

static float get_category_volume(const sound_info_t::sound_category_t category)
{
    float volume = 100;
    switch (category)
    {
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_MUSIC, volume = cvr_a_volume_music.get());
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_WEATHER, volume = cvr_a_volume_weather.get());
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_HOSTILE, volume = cvr_a_volume_hostile.get());
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_PLAYER, volume = cvr_a_volume_player.get());

        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_RECORD, volume = cvr_a_volume_record.get());
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_BLOCKS, volume = cvr_a_volume_blocks.get());
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_NEUTRAL, volume = cvr_a_volume_neutral.get());
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_AMBIENT, volume = cvr_a_volume_ambient.get());
    default:
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_MASTER, volume = 100);
    }

    return volume * 0.01f;
}

sound_world_t::source_id_t sound_world_t::request_source(const sound_info_t& info, const glm::f64vec3 pos, const bool relative)
{
    if (suspended)
        return 0;

    sound_info_t::sound_category_t category = info.category;
    /* Ensure category is valid */
    switch (category)
    {
    case sound_info_t::SOUND_CATEGORY_MASTER:
    case sound_info_t::SOUND_CATEGORY_MUSIC:
    case sound_info_t::SOUND_CATEGORY_WEATHER:
    case sound_info_t::SOUND_CATEGORY_HOSTILE:
    case sound_info_t::SOUND_CATEGORY_PLAYER:
    case sound_info_t::SOUND_CATEGORY_RECORD:
    case sound_info_t::SOUND_CATEGORY_BLOCKS:
    case sound_info_t::SOUND_CATEGORY_NEUTRAL:
    case sound_info_t::SOUND_CATEGORY_AMBIENT:
        break;
    default:
        dc_log_warn("Unknown category: %d, falling back to master category", category);
        category = sound_info_t::SOUND_CATEGORY_MASTER;
        break;
    }

    /* Don't play a sound if it will be muted */
    if (get_category_volume(category) * info.volume <= 0.001f)
        return 0;

    /* TODO: Investigate if playing background music directly through SDL would work on iOS */
    /* Reserve slot0 for either music or ambient sounds */
    const bool force_slot0 = category == sound_info_t::SOUND_CATEGORY_MUSIC;
    const bool allow_slot0 = force_slot0 || category == sound_info_t::SOUND_CATEGORY_AMBIENT;

    int slot_id = -1;
    size_t max_slot_id = slots.size();
    if (force_slot0)
        max_slot_id = 1;
    for (size_t i = !allow_slot0; i < max_slot_id; i++)
    {
        if (slots[i].in_use)
            continue;
        slots[i].in_use = 1;
        slot_id = i;
        break;
    }

    if (slot_id < 0)
    {
        dc_log_warn("Sound \"%s\" \"%s\" cannot be played because of slot exhaustion", info.id_sound.c_str(), info.name.c_str());
        return 0;
    }

    slot_t& slot = slots[slot_id];
    slot.info = info;
    slot.category = category;
    slot.pos_is_relative = relative;
    slot.pos = pos;

    dc_log_trace("Playing sound \"%s\" \"%s\" on slot %d", slot.info.id_sound.c_str(), info.name.c_str(), slot_id);

    ma_uint32 sound_flags = MA_SOUND_FLAG_ASYNC;

    if (slot.info.flags.stream)
        sound_flags |= MA_SOUND_FLAG_STREAM;

    if (slot.category == sound_info_t::SOUND_CATEGORY_MUSIC)
        sound_flags |= MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;

    ma_result result;
    MA_CALL(result, ma_sound_init_from_file(engine, slot.info.path.c_str(), sound_flags, NULL, NULL, slot.source));

    if (result != MA_SUCCESS)
    {
        slot.in_use = 0;
        return 0;
    }

    ma_sound_set_pitch(slot.source, slot.info.pitch);
    ma_sound_set_volume(slot.source, get_category_volume(slot.category) * slot.info.volume);

    MA_CALL(result, ma_sound_start(slot.source));

    if (result != MA_SUCCESS)
    {
        ma_sound_uninit(slot.source);
        slot.in_use = 0;
        return 0;
    }

    return source_id_t(slot_id) | (source_id_t(++slot.counter) << 16);
}

void sound_world_t::suspend()
{
    if (suspended)
        return;
    suspended = 1;
    dc_log("Suspending");
    for (auto& it : slots)
    {
        if (it.in_use && !ma_sound_at_end(it.source))
            ma_node_set_state(it.source, ma_node_state_stopped);
    }
}

void sound_world_t::resume()
{
    if (!suspended)
        return;
    suspended = 0;
    dc_log("Resuming");
    for (auto& it : slots)
    {
        if (it.in_use && !ma_sound_at_end(it.source))
            ma_node_set_state(it.source, ma_node_state_started);
    }
}

bool sound_world_t::is_music_playing() const { return slots[0].in_use && slots[0].category == sound_info_t::SOUND_CATEGORY_MUSIC; }

void sound_world_t::kill_music()
{
    if (!is_music_playing())
        return;
    source_kill(source_id_t(0) | (source_id_t(slots[0].counter) << 16));
}

void sound_world_t::source_kill(const source_id_t id)
{
    if (source_is_valid(id))
    {
        slot_t& slot = slots[id & 0xFFFF];
        ma_sound_stop(slot.source);
        ma_sound_uninit(slot.source);
        slot.in_use = 0;
    }
}

void sound_world_t::source_set_pos(const source_id_t id, const glm::f64vec3 pos, const bool relative)
{
    if (source_is_valid(id))
    {
        slot_t& slot = slots[id & 0xFFFF];
        slot.pos = pos;
        slot.pos_is_relative = relative;
    }
}

void sound_world_t::kill_all()
{
    for (auto& it : slots)
        ma_sound_stop(it.source);
    for (auto& it : slots)
    {
        if (it.in_use)
            ma_sound_uninit(it.source);
        it.in_use = 0;
    }
}

void sound_world_t::update(const glm::f64vec3 listener_pos, const glm::vec3 listener_direction, const glm::vec3 listener_up)
{
    if (suspended)
        return;

    ma_engine_listener_set_position(engine, 0, 0, 0, 0);
    ma_engine_listener_set_direction(engine, 0, listener_direction.x, listener_direction.y, listener_direction.z);
    ma_engine_listener_set_world_up(engine, 0, listener_up.x, listener_up.y, listener_up.z);
    ma_engine_set_volume(engine, cvr_a_volume_master.get() * 0.01f);

    std::vector<short> temp_stream_buffer;
    for (size_t slot_id = 0; slot_id < slots.size(); slot_id++)
    {
        auto& slot = slots[slot_id];
        if (!slot.in_use)
            continue;
        if (ma_sound_at_end(slot.source))
        {
            ma_sound_uninit(slot.source);
            slot.in_use = 0;
            continue;
        }

        glm::f64vec3 pos = slot.pos;

        if (!slot.pos_is_relative)
            pos -= listener_pos;

        ma_sound_set_position(slot.source, pos.x, pos.y, pos.z);

        ma_sound_set_volume(slot.source, get_category_volume(slot.category) * slot.info.volume);
    }
}

#include "tetra/util/stb_sprintf.h"

#define FIELD(name, fmt, ...)            \
    do                                   \
    {                                    \
        ImGui::TableNextRow();           \
        ImGui::TableNextColumn();        \
        ImGui::TextUnformatted(name);    \
        ImGui::TableNextColumn();        \
        ImGui::Text(fmt, ##__VA_ARGS__); \
    } while (0)
void sound_world_t::imgui_contents()
{
    ImGui::Text("Suspended: %d", suspended);

    if (ImGui::Button("Suspend"))
        suspend();
    ImGui::SameLine();
    if (ImGui::Button("Resume"))
        resume();

    ImGui::Text("Sources: %d/%d, is_music_playing: %d", get_num_slots_active(), get_num_slots(), is_music_playing());

    if (ImGui::Button("Kill All"))
        kill_all();
    ImGui::SameLine();
    if (ImGui::Button("Kill Music"))
        kill_music();

    cvr_a_sources_max.imgui_edit();

    cvr_a_volume_master.imgui_edit();

    cvr_a_volume_music.imgui_edit();
    cvr_a_volume_weather.imgui_edit();
    cvr_a_volume_hostile.imgui_edit();
    cvr_a_volume_player.imgui_edit();

    cvr_a_volume_record.imgui_edit();
    cvr_a_volume_blocks.imgui_edit();
    cvr_a_volume_neutral.imgui_edit();
    cvr_a_volume_ambient.imgui_edit();

    ImGui::BeginChild("sound_world_t::imgui_contents slot list", ImGui::GetContentRegionAvail());

    for (size_t slot_id = 0; slot_id < slots.size(); slot_id++)
    {
        auto& slot = slots[slot_id];
        std::string s;
        s = "Slot ";
        s += std::to_string(slot_id);

        if (slot.in_use)
        {
            const std::string PAR_L = "(";
            const std::string PAR_R = ")";
            s += PAR_L + slot.info.id_sound.c_str() + PAR_R;
            s += PAR_L + slot.info.name.c_str() + PAR_R;
            s += PAR_L + sound_info_t::sound_category_to_str(slot.category) + PAR_R;
        }

        bool slot_tree_open = ImGui::TreeNode(s.c_str());

        if (!slot_tree_open)
            continue;

        ImGui::PushID(s.c_str());
        if (ImGui::BeginTable("slot info table", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg))
        {
            FIELD("Category:", "%d (%s)", slot.info.category, sound_info_t::sound_category_to_str(slot.info.category));
            FIELD("Stream:", "%d", slot.info.flags.stream);

            FIELD("Position:", "<%.1f, %.1f, %.1f>", slot.pos.x, slot.pos.y, slot.pos.z);
            FIELD("Position: is_relative:", "%d", slot.pos_is_relative);

            ma_vec3f pos = ma_sound_get_position(slot.source);
            ma_vec3f vel = ma_sound_get_velocity(slot.source);
            FIELD("Position:", "<%.1f, %.1f, %.1f>", pos.x, pos.y, pos.z);
            FIELD("Velocity:", "<%.1f, %.1f, %.1f>", vel.x, vel.y, vel.z);
            FIELD("Volume:", "%.3f", ma_sound_get_volume(slot.source));
            FIELD("Pitch:", "%.3f", ma_sound_get_pitch(slot.source));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::TreeNode("Sound Info"))
            {
                FIELD("Weight:", "%d", slot.info.weight);
                FIELD("Category:", "%d (%s)", slot.info.category, sound_info_t::sound_category_to_str(slot.info.category));

                FIELD("Is Stream:", "%s", slot.info.flags.stream ? "true" : "false");
                FIELD("Is Event:", "%s", slot.info.flags.is_event ? "true" : "false");

                FIELD("Volume multiplier:", "%.4f", slot.info.volume);
                FIELD("Pitch multiplier:", "%.4f", slot.info.pitch);

                FIELD("Resource ID: Sound:", "%s", slot.info.id_sound.c_str());
                FIELD("Resource ID: Subtitle:", "%s", slot.info.id_sub.c_str());
                FIELD("Path:", "%s", slot.info.path.c_str());
                ImGui::TreePop();
            }
            ImGui::EndTable();
        }
        ImGui::PopID();

        ImGui::TreePop();
    }

    ImGui::EndChild();
}
