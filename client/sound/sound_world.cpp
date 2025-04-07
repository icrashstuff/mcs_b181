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

#include "sound_world.h"

#include <al.h>
#include <alc.h>

#include "stb_vorbis.h"

#include "tetra/log.h"
#include "tetra/util/convar.h"

#include "shared/misc.h"

#if defined(SDL_PLATFORM_APPLE) || defined(SDL_PLATFORM_ANDROID)
#define SOUND_ENGINE_LOW_RESOURCE true
#else
#define SOUND_ENGINE_LOW_RESOURCE false
#endif

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

CONVARS_FOR_SOUND_CAT(master, sound_info_t::SOUND_CATEGORY_MASTER);

CONVARS_FOR_SOUND_CAT(music, sound_info_t::SOUND_CATEGORY_MUSIC);
CONVARS_FOR_SOUND_CAT(weather, sound_info_t::SOUND_CATEGORY_WEATHER);
CONVARS_FOR_SOUND_CAT(hostile, sound_info_t::SOUND_CATEGORY_HOSTILE);
CONVARS_FOR_SOUND_CAT(player, sound_info_t::SOUND_CATEGORY_PLAYER);

CONVARS_FOR_SOUND_CAT(record, sound_info_t::SOUND_CATEGORY_RECORD);
CONVARS_FOR_SOUND_CAT(blocks, sound_info_t::SOUND_CATEGORY_BLOCKS);
CONVARS_FOR_SOUND_CAT(neutral, sound_info_t::SOUND_CATEGORY_NEUTRAL);
CONVARS_FOR_SOUND_CAT(ambient, sound_info_t::SOUND_CATEGORY_AMBIENT);

#define ALC_CALL(_CALL)                                                                                                  \
    do                                                                                                                   \
    {                                                                                                                    \
        _CALL;                                                                                                           \
        ALCenum ALC_CALL_err = alcGetError(device);                                                                      \
        SDL_assert(ALC_CALL_err == ALC_NO_ERROR);                                                                        \
        if (ALC_CALL_err != ALC_NO_ERROR)                                                                                \
            dc_log_error("[ALC]: %s(0x%04X), while calling: " #_CALL, alcGetString(device, ALC_CALL_err), ALC_CALL_err); \
    } while (0);

#define AL_CALL(_CALL)                                                                                               \
    do                                                                                                               \
    {                                                                                                                \
        _CALL;                                                                                                       \
        ALenum AL_CALL_errcode = alGetError();                                                                       \
        SDL_assert(AL_CALL_errcode == AL_NO_ERROR);                                                                  \
        if (AL_CALL_errcode != AL_NO_ERROR)                                                                          \
            dc_log_error("[AL]: %s(0x%04X), while calling: " #_CALL, alGetString(AL_CALL_errcode), AL_CALL_errcode); \
    } while (0);

static void reset_al_source(const ALuint al_source)
{
    if (!al_source)
        return;

    AL_CALL(alSourceStop(al_source));

    ALint src_type = 0;
    AL_CALL(alGetSourcei(al_source, AL_SOURCE_TYPE, &src_type));

    if (src_type == AL_STATIC)
    {
        ALint buffer = 0;
        AL_CALL(alGetSourcei(al_source, AL_BUFFER, &buffer));
        AL_CALL(alSourcei(al_source, AL_BUFFER, 0));
        AL_CALL(alDeleteBuffers(1, (ALuint*)&buffer));
    }

    if (src_type == AL_STREAMING)
    {
        ALint num_buffers = 0;
        AL_CALL(alGetSourcei(al_source, AL_BUFFERS_PROCESSED, &num_buffers));
        for (int i = 0; i < num_buffers; i++)
        {
            ALuint buffer = 0;
            AL_CALL(alSourceUnqueueBuffers(al_source, 1, &buffer));
            AL_CALL(alDeleteBuffers(1, &buffer));
        }
    }

    AL_CALL(alSourcei(al_source, AL_BUFFER, 0));
}

static void destroy_al_source(ALuint al_source)
{
    if (!al_source)
        return;

    reset_al_source(al_source);

    AL_CALL(alDeleteSources(1, &al_source));
}

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

sound_world_t::sound_world_t()
{
    dc_log("Initializing sound engine");
    Uint64 sdl_tick_start_ns = SDL_GetTicksNS();

    ALC_CALL(device = alcOpenDevice(NULL));
    if (!device)
    {
        dc_log_error("Failed to initalize sound engine! (alcOpenDevice)");
        return;
    }

    ALC_CALL(context = alcCreateContext(device, NULL));
    if (!context)
    {
        dc_log_error("Failed to initalize sound engine! (alcCreateContext)");
        return;
    }

    ALC_CALL(alcMakeContextCurrent(context));

    dc_log("OpenAL info");
    dc_log("*** AL Vendor:     %s ***", alGetString(AL_VENDOR));
    dc_log("*** AL Version:    %s ***", alGetString(AL_VERSION));
    dc_log("*** AL Renderer:   %s ***", alGetString(AL_RENDERER));

    slots.resize(cvr_a_sources_max.get());

    ALC_CALL(alcMakeContextCurrent(NULL));
    dc_log("Sound engine intialized in %.3f ms", double(SDL_GetTicksNS() - sdl_tick_start_ns) / 1000000.0);
}

static convar_float_t cvr_a_stream_cache_chunk_size {
    "a_stream_cache_chunk_size",
    0.25f,
    0.125f,
    1.0f,
    "Size of chunks for streaming buffer queue (Unit: seconds)",
    CONVAR_FLAG_SAVE,
};

static convar_int_t cvr_a_stream_cache_length {
    "a_stream_cache_length",
    10,
    1,
    32,
    "Size of buffer queue (in seconds) for streaming sources",
    CONVAR_FLAG_SAVE,
};

/**
 * Streams data to stream source in chunks
 *
 * @param decoder Stream to use
 * @param al_source OpenAL source to manage
 * @param temp_buffer Temporary buffer (Pass this between successive calls if possible!)
 *
 * @returns True if finished, false if not
 */
static bool update_stream_source(stb_vorbis* const decoder, const ALuint al_source, std::vector<short>& temp_buffer)
{
    auto enqueue_buffer = [](stb_vorbis* const decoder, const ALuint al_source, const ALuint al_buffer, std::vector<short>& temp_buffer) -> bool {
        stb_vorbis_info vinfo = stb_vorbis_get_info(decoder);
        ALenum format = AL_NONE;
        switch (vinfo.channels)
        {
        case 1:
            format = AL_FORMAT_MONO16;
            break;
        case 2:
            format = AL_FORMAT_STEREO16;
            break;
        default:
            break;
        }

        const int target_samples = vinfo.sample_rate * cvr_a_stream_cache_chunk_size.get();
        const int max_samples = target_samples + vinfo.max_frame_size;

        temp_buffer.resize(SDL_max(temp_buffer.size(), size_t(max_samples * vinfo.channels)));

        int done = 0;
        int num_samples = 0;
        int num_samples_remaining = max_samples;
        int num_samples_last_frame = 0;
        do
        {
            short* head = temp_buffer.data() + num_samples * vinfo.channels;
            num_samples_last_frame = stb_vorbis_get_frame_short_interleaved(decoder, vinfo.channels, head, num_samples_remaining * vinfo.channels);

            num_samples += num_samples_last_frame;
            num_samples_remaining -= num_samples_last_frame;

            done = num_samples_last_frame == 0;
        } while (num_samples_last_frame > 0 && num_samples_remaining > 0 && num_samples <= target_samples && !done);

        AL_CALL(alBufferData(al_buffer, format, temp_buffer.data(), num_samples * vinfo.channels * 2, vinfo.sample_rate));

        AL_CALL(alSourceQueueBuffers(al_source, 1, &al_buffer));

        return done;
    };

    const int max_buffers = SDL_max(1, cvr_a_stream_cache_length.get() * 1.0f / cvr_a_stream_cache_chunk_size.get());
    int max_buffers_per_call = SDL_max(1, 1.0f / cvr_a_stream_cache_chunk_size.get());

    ALint buffers_queued = 0;
    ALint buffers_processed = 0;
    AL_CALL(alGetSourcei(al_source, AL_BUFFERS_QUEUED, &buffers_queued));
    AL_CALL(alGetSourcei(al_source, AL_BUFFERS_PROCESSED, &buffers_processed));

    int done = 0;

    /* Re-queue processed buffers */
    for (int i = 0; i < buffers_processed && max_buffers_per_call > 0 && !done; i++)
    {
        ALuint t = 0;
        AL_CALL(alSourceUnqueueBuffers(al_source, 1, &t);)
        TRACE("al_source: %d, requeuing buffer: %d", al_source, t);
        done = enqueue_buffer(decoder, al_source, t, temp_buffer);
        max_buffers_per_call--;
    }

    /* Create new buffers (if necessary) and queue them */
    for (; buffers_queued < max_buffers && max_buffers_per_call > 0 && !done; buffers_queued++)
    {
        ALuint t = 0;
        AL_CALL(alGenBuffers(1, &t));
        TRACE("al_source: %d, creating buffer: %d", al_source, t);
        done = enqueue_buffer(decoder, al_source, t, temp_buffer);
        max_buffers_per_call--;
    }

    return done;
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
        dc_log_warn("Sound \"%s\" \"%s\" cannot be played because of general slot exhaustion", info.id_sound.c_str(), info.name.c_str());
        return 0;
    }

    slot_t& slot = slots[slot_id];
    slot.info = info;
    slot.category = category;
    slot.pos_is_relative = relative;
    slot.pos = pos;

    dc_log_trace("Playing sound \"%s\" \"%s\" on slot %d", slot.info.id_sound.c_str(), info.name.c_str(), slot_id);

    ALC_CALL(alcMakeContextCurrent(context));

    if (!slot.al_source)
        AL_CALL(alGenSources(1, &slot.al_source));

    if (slot.stream_decoder)
    {
        stb_vorbis_close(slot.stream_decoder);
        slot.stream_decoder = NULL;
    }

    /* We shouldn't need to stop the source, but might as well */
    AL_CALL(alSourceStop(slot.al_source));
    reset_al_source(slot.al_source);

    AL_CALL(alSourcef(slot.al_source, AL_GAIN, slot.info.volume));
    AL_CALL(alSourcef(slot.al_source, AL_PITCH, slot.info.pitch));

    int error = 0;
    if (!slot.info.flags.stream)
    {
        int channels = 0;
        int sample_rate = 0;
        short* output = NULL;
        const int sample_count = stb_vorbis_decode_filename(slot.info.path.c_str(), &channels, &sample_rate, &output);

        int data_len = sample_count * channels * 2;

        ALenum format = AL_NONE;
        switch (channels)
        {
        case 1:
            format = AL_FORMAT_MONO16;
            break;
        case 2:
            format = AL_FORMAT_STEREO16;
            break;
        default:
            break;
        }

        error = (format == AL_NONE || !output || !data_len);

        ALuint al_buffer = 0;

        if (!error)
        {
            AL_CALL(alGenBuffers(1, &al_buffer));
            AL_CALL(alBufferData(al_buffer, format, output, data_len, sample_rate));
        }

        AL_CALL(alSourcei(slot.al_source, AL_BUFFER, al_buffer));

        free(output);
    }
    else
    {
        slot.stream_decoder = stb_vorbis_open_filename(slot.info.path.c_str(), &error, NULL);

        if (error)
        {
            dc_log_error("Unable to stream \"%s\"", slot.info.path.c_str());
            stb_vorbis_close(slot.stream_decoder);
            slot.stream_decoder = nullptr;
        }

        std::vector<short> temp_stream_buffer;
        if (slot.stream_decoder && update_stream_source(slot.stream_decoder, slot.al_source, temp_stream_buffer))
        {
            stb_vorbis_close(slot.stream_decoder);
            slot.stream_decoder = nullptr;
        }
    }

    if (error)
    {
        ALC_CALL(alcMakeContextCurrent(NULL));
        slot.in_use = 0;
        return 0;
    }

    AL_CALL(alSourcePlay(slot.al_source));

    ALC_CALL(alcMakeContextCurrent(NULL));

    return source_id_t(slot_id) | (source_id_t(++slot.counter) << 16);
}

void sound_world_t::suspend()
{
    if (suspended)
        return;
    suspended = 1;
    ALC_CALL(alcMakeContextCurrent(context));
    for (auto& it : slots)
    {
        if (it.in_use)
            AL_CALL(alSourcePause(it.al_source));
    }
    ALC_CALL(alcMakeContextCurrent(NULL));
}

void sound_world_t::resume()
{
    if (!suspended)
        return;
    suspended = 0;
    ALC_CALL(alcMakeContextCurrent(context));
    for (auto& it : slots)
    {
        if (it.in_use)
            AL_CALL(alSourcePlay(it.al_source));
    }
    ALC_CALL(alcMakeContextCurrent(NULL));
}

bool sound_world_t::is_music_playing() const { return slots[0].in_use && slots[0].category == sound_info_t::SOUND_CATEGORY_MUSIC; }

void sound_world_t::kill_music()
{
    ALC_CALL(alcMakeContextCurrent(context));
    if (!is_music_playing())
        return;
    source_kill(source_id_t(0) | (source_id_t(slots[0].counter) << 16));
    ALC_CALL(alcMakeContextCurrent(NULL));
}

void sound_world_t::source_kill(const source_id_t id)
{
    ALC_CALL(alcMakeContextCurrent(context));
    if (source_is_valid(id))
    {
        slot_t& slot = slots[id & 0xFFFF];
        slot.in_use = 0;
        if (slot.stream_decoder)
            stb_vorbis_close(slot.stream_decoder);
        slot.stream_decoder = NULL;
        reset_al_source(slot.al_source);
    }
    ALC_CALL(alcMakeContextCurrent(NULL));
}

void sound_world_t::kill_all()
{
    ALC_CALL(alcMakeContextCurrent(context));
    for (auto& it : slots)
    {
        if (it.al_source)
            AL_CALL(alSourceStop(it.al_source));
    }
    for (auto& it : slots)
    {
        if (it.al_source)
            reset_al_source(it.al_source);
        it.in_use = 0;
        if (it.stream_decoder)
            stb_vorbis_close(it.stream_decoder);
        it.stream_decoder = NULL;
    }
    ALC_CALL(alcMakeContextCurrent(NULL));
}

sound_world_t::~sound_world_t()
{
    ALC_CALL(alcMakeContextCurrent(context));
    for (auto& it : slots)
    {
        destroy_al_source(it.al_source);
        if (it.stream_decoder)
            stb_vorbis_close(it.stream_decoder);
    }
    ALC_CALL(alcMakeContextCurrent(NULL));

    ALC_CALL(alcDestroyContext(context));
    ALC_CALL(alcCloseDevice(device));
}

#define ADD_SWITCH_CASE(CASE, ACTION) \
    case CASE:                        \
        ACTION;                       \
        break;

void sound_world_t::update(const glm::f64vec3 listener_pos, const glm::vec3 listener_direction, const glm::vec3 listener_up)
{
    if (suspended)
        return;

    ALC_CALL(alcMakeContextCurrent(context));
    ALfloat orientation[6];
    orientation[0] = listener_direction.x;
    orientation[1] = listener_direction.y;
    orientation[2] = listener_direction.z;
    orientation[3] = listener_up.x;
    orientation[4] = listener_up.y;
    orientation[5] = listener_up.z;
    AL_CALL(alListenerfv(AL_ORIENTATION, orientation));
    AL_CALL(alListener3f(AL_POSITION, 0, 0, 0));

    std::vector<short> temp_stream_buffer;
    for (size_t slot_id = 0; slot_id < slots.size(); slot_id++)
    {
        auto& slot = slots[slot_id];
        if (!slot.in_use)
            continue;

        glm::f64vec3 pos = slot.pos;

        if (!slot.pos_is_relative)
            pos -= listener_pos;

        AL_CALL(alSource3f(slot.al_source, AL_POSITION, pos.x, pos.y, pos.z));

        ALint state = 0;
        AL_CALL(alGetSourcei(slot.al_source, AL_SOURCE_STATE, &state));

        bool paused = state == AL_PAUSED;
        bool playing = state == AL_PLAYING;

        if (!paused && !playing && !slot.stream_decoder)
        {
            dc_log_trace("Stopping sound \"%s\" on slot %zu", slot.info.id_sound.c_str(), slot_id);
            AL_CALL(alSourceStop(slot.al_source));
            slot.in_use = 0;
        }

        if (slot.stream_decoder && update_stream_source(slot.stream_decoder, slot.al_source, temp_stream_buffer))
        {
            stb_vorbis_close(slot.stream_decoder);
            slot.stream_decoder = nullptr;
        }

        float volume = 100;
        switch (slot.category)
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

        volume *= 0.01f;
        volume *= (cvr_a_volume_master.get() * 0.01f);

        alSourcef(slot.al_source, AL_GAIN, volume * slot.info.volume);
    }

    ALC_CALL(alcMakeContextCurrent(NULL));
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

    ALC_CALL(alcMakeContextCurrent(context));
    for (size_t slot_id = 0; slot_id < slots.size(); slot_id++)
    {
        auto& slot = slots[slot_id];
        std::string s;
        s = "Slot ";
        s += std::to_string(slot_id);

        if (slot.in_use)
        {
            if (slot.stream_decoder)
                s += "(STREAM)";

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
            FIELD("Stream:", "%d", slot.stream_decoder != nullptr);

            if (slot.stream_decoder)
            {
                ALint num_buffers = 0;
                ALint num_buffers_processed = 0;

                AL_CALL(alGetSourcei(slot.al_source, AL_BUFFERS_QUEUED, &num_buffers));
                AL_CALL(alGetSourcei(slot.al_source, AL_BUFFERS_PROCESSED, &num_buffers_processed));

                FIELD("Buffers:", "%d", num_buffers);
                FIELD("Buffers: Pending", "%d", num_buffers - num_buffers_processed);
                FIELD("Buffers: Processed", "%d", num_buffers_processed);

                FIELD("Position:", "<%.1f, %.1f, %.1f>", slot.pos.x, slot.pos.y, slot.pos.z);
                FIELD("Position: is_relative:", "%d", slot.pos_is_relative);

                float x = 0.0f, y = 0.0f, z = 0.0f;
                AL_CALL(alGetSource3f(slot.al_source, AL_POSITION, &x, &y, &z));
                FIELD("AL_POSITION:", "<%.1f, %.1f, %.1f>", x, y, z);
                x = 0.0f, y = 0.0f, z = 0.0f;
                AL_CALL(alGetSource3f(slot.al_source, AL_VELOCITY, &x, &y, &z));
                FIELD("AL_VELOCITY:", "<%.1f, %.1f, %.1f>", x, y, z);

                float gain = 0.0f;
                AL_CALL(alGetSourcef(slot.al_source, AL_GAIN, &gain));
                FIELD("AL_GAIN:", "%.3f", gain);

                float pitch = 0.0f;
                AL_CALL(alGetSourcef(slot.al_source, AL_PITCH, &pitch));
                FIELD("AL_PITCH:", "%.3f", pitch);
            }

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
    ALC_CALL(alcMakeContextCurrent(NULL));

    ImGui::EndChild();
}
