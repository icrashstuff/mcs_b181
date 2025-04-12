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
#include "sound_resources.h"

#include <SDL3/SDL.h>
#include <math.h>

#include "tetra/gui/imgui.h"
#include "tetra/log.h"
#include "tetra/util/physfs/physfs.h"

#include "client/jzon/Jzon.h"

#include "shared/misc.h"

static bool load_json(const std::string path, Jzon::Node& out)
{
    out = Jzon::Node();
    PHYSFS_File* fd = PHYSFS_openRead(path.c_str());

    if (!fd)
    {
        dc_log_error("Error opening: \"%s\"", path.c_str());
        return out;
    }

    std::string json_dat;
    int num_read = 0;
    do
    {
        char buf[1024];
        num_read = PHYSFS_readBytes(fd, buf, SDL_arraysize(buf));
        if (num_read > 0)
            json_dat.append(buf, num_read);
    } while (num_read > 0);
    PHYSFS_close(fd);

    {
        Jzon::Parser parser;
        out = parser.parseString(json_dat);
        if (parser.getError().length())
            dc_log_error("Error parsing: \"%s\", \"%s\"", path.c_str(), parser.getError().c_str());
    }

    return out;
}

static bool load_index(const std::string path_assets_obfuscated, Jzon::Node& out)
{
    /* TODO: Check for any other compatible index files */
    const char* names[] = {
        "indexes/1.8.json",
        "indexes/1.7.10.json",
    };

    for (int i = 0; i < (int)SDL_arraysize(names); i++)
    {
        const std::string path = path_assets_obfuscated + names[i];
        dc_log_trace("Trying: \"%s\"", path.c_str());
        if (load_json(path, out))
            return 1;
    }
    return 0;
}

void sound_resources_t::parse_sound_json(const std::string domain, const std::string path)
{
    Jzon::Node base_node;

    if (!load_json(path, base_node) && !base_node.isObject())
    {
        dc_log_error("Error parsing: \"%s\"", path.c_str());
        return;
    }

    /* Example sound.json
     *
     * {
     *     "event.id": {
     *         "category": "master",
     *         "replace": false,
     *         "sounds": [
     *             "file1",
     *             {
     *                 "type": "event",
     *                 "name": "event.id2"
     *             },
     *             {
     *                 "name": "file/most/fields/are/the/defaults",
     *                 "type": "sound",
     *                 "stream": false,
     *                 "volume": 1.0,
     *                 "pitch": 1.0,
     *                 "weight": 1
     *             }
     *         ]
     *     }
     * }
     */

    for (auto& it_event : base_node)
    {
        const Jzon::Node& node_event = it_event.second;
        sound_info_t info_event;
        info_event.id_sound = domain + ":" + it_event.first;
        info_event.id_sub = node_event.get("subtitle").toString();

        /* Find or emplace a new event object */
        auto it_map = sounds.find(info_event.id_sound);
        if (it_map == sounds.end())
            it_map = sounds.insert(it_map, std::pair(info_event.id_sound, sound_event_t { .total_weight = 0, .entries = std::vector<sound_info_t>() }));

        /* Clear event object if requested */
        if (node_event.get("replace").toBool(0))
        {
            it_map->second.total_weight = 0;
            it_map->second.entries.clear();
        }

        /* Determine category */
        {
            std::string cat = node_event.get("category").toString();
            if (cat == "master")
                info_event.category = sound_info_t::SOUND_CATEGORY_MASTER;
            else if (cat == "music")
                info_event.category = sound_info_t::SOUND_CATEGORY_MUSIC;
            else if (cat == "weather")
                info_event.category = sound_info_t::SOUND_CATEGORY_WEATHER;
            else if (cat == "hostile")
                info_event.category = sound_info_t::SOUND_CATEGORY_HOSTILE;
            else if (cat == "player")
                info_event.category = sound_info_t::SOUND_CATEGORY_PLAYER;
            else if (cat == "record")
                info_event.category = sound_info_t::SOUND_CATEGORY_RECORD;
            else if (cat == "block")
                info_event.category = sound_info_t::SOUND_CATEGORY_BLOCKS;
            else if (cat == "neutral")
                info_event.category = sound_info_t::SOUND_CATEGORY_NEUTRAL;
            else if (cat == "ambient")
                info_event.category = sound_info_t::SOUND_CATEGORY_AMBIENT;
            else
                dc_log_warn("Unknown sound category: \"%s\"", cat.c_str());
        }

        const Jzon::Node& node_sounds = node_event.get("sounds");
        for (size_t i = 0; i < node_sounds.getCount(); i++)
        {
            const Jzon::Node& node_entry = node_sounds.get(i);
            sound_info_t info_entry = info_event;

            if (node_entry.isString())
                info_entry.name = node_entry.toString();
            else if (node_entry.isObject())
            {
                info_entry.name = node_entry.get("name").toString();
                info_entry.flags.stream = node_entry.get("stream").toBool(0);
                info_entry.flags.is_event = (node_entry.get("type").toString() == "event");
                info_entry.weight = node_entry.get("weight").toInt(1);
                info_entry.volume = node_entry.get("volume").toFloat(1.0);
                info_entry.pitch = node_entry.get("pitch").toFloat(1.0);
            }
            else
                continue;

            if (!info_entry.flags.is_event)
                info_entry.path = domain + "/sounds/" + info_entry.name + ".ogg";
            else
                info_entry.path = domain + ":" + info_entry.name;

            info_entry.name = domain + ":" + info_entry.name;

            it_map->second.total_weight += info_entry.weight;
            it_map->second.entries.push_back(info_entry);
        }
    }
}
sound_resources_t::~sound_resources_t() { delete text_filter; }

sound_resources_t::sound_resources_t(const std::string path_assets_obfuscated, const std::string path_assets_normal)
{
    text_filter = new ImGuiTextFilter();

    Jzon::Node node_asset_index;
    if (!load_index(path_assets_obfuscated, node_asset_index))
        dc_log_error("Unable to load asset index (Audio)");
    const Jzon::Node node_asset_objects = node_asset_index.get("objects");

    /* Parse sounds.json */
    {
        const std::string domain = "minecraft";
        const std::string pathname_sounds_json = domain + "/sounds.json";

        /* Parse obfuscated */
        const Jzon::Node node_object = node_asset_objects.get(pathname_sounds_json);
        const Jzon::Node node_object_hash = node_object.get("hash");

        if (node_object_hash.isString())
        {
            const std::string hash = node_object_hash.toString();
            const std::string path = path_assets_obfuscated + "objects/" + hash.substr(0, 2) + "/" + hash;
            dc_log("Trying \"%s\" -> \"%s\"", pathname_sounds_json.c_str(), path.c_str());
            parse_sound_json(domain, path);
        }

        /* Parse unobfuscated */
        const std::string path_normal_sounds_json = path_assets_normal + pathname_sounds_json;
        dc_log("Trying  \"%s\" -> \"%s\"", pathname_sounds_json.c_str(), path_normal_sounds_json.c_str());
        parse_sound_json(domain, pathname_sounds_json);
    }

    /* TODO-OPT: Move path resolution to sound_resources::get_sound()? */
    for (auto& it_event : sounds)
    {
        for (auto& it_entry : it_event.second.entries)
        {
            if (it_entry.flags.is_event)
                continue;

            /* Check for file in resource pack/jar */
            std::string path_normal = path_assets_normal + it_entry.path;
            if (PHYSFS_exists(path_normal.c_str()))
            {
                it_entry.path = path_normal;
                TRACE("%s", it_entry.path.c_str());
                continue;
            }

            /* Fallback to assets/objects/ */
            const Jzon::Node node_object = node_asset_objects.get(it_entry.path);
            const Jzon::Node node_object_hash = node_object.get("hash");
            if (node_object_hash.isString())
            {
                std::string hash = node_object_hash.toString();
                it_entry.path = path_assets_obfuscated + "objects/" + hash.substr(0, 2) + "/" + hash;
                TRACE("%s", it_entry.path.c_str());
            }
        }
    }
}

bool sound_resources_t::get_sound(const std::string sound_id, sound_info_t& out) const
{
    auto it_v = sounds.find(sound_id);

    if (it_v == sounds.end())
    {
        dc_log("Unable to find sound with id: %s (Id not in map)", sound_id.c_str());
        return false;
    }

    const auto& event = it_v->second;

    if (!event.entries.size())
    {
        dc_log("Unable to find sound with id: %s (Sound vector empty?!?)", sound_id.c_str());
        return false;
    }

    int weight = SDL_rand(event.total_weight);

    for (auto it : event.entries)
    {
        weight -= it.weight;
        if (weight <= 0)
        {
            out = it;
            if (out.flags.is_event && out.id_sound != out.path)
                return get_sound(out.path, out);
            return 1;
        }
    }

    out = event.entries.back();
    if (out.flags.is_event && out.id_sound != out.path)
        return get_sound(out.path, out);
    return 1;
}

#define ADD_SWITCH_CASE(CASE, ACTION) \
    case CASE:                        \
        ACTION;                       \
        break;

const char* sound_info_t::sound_category_to_str(sound_category_t category)
{
    const char* category_name = "Unknown";
    switch (category)
    {
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_MASTER, category_name = "master");
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_MUSIC, category_name = "music");
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_WEATHER, category_name = "weather");
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_HOSTILE, category_name = "hostile");
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_PLAYER, category_name = "player");
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_RECORD, category_name = "record");
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_BLOCKS, category_name = "block");
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_NEUTRAL, category_name = "neutral");
        ADD_SWITCH_CASE(sound_info_t::SOUND_CATEGORY_AMBIENT, category_name = "ambient");
    default:
        break;
    }
    return category_name;
}

#define FIELD(name, fmt, ...)            \
    do                                   \
    {                                    \
        ImGui::TableNextRow();           \
        ImGui::TableNextColumn();        \
        ImGui::TextUnformatted(name);    \
        ImGui::TableNextColumn();        \
        ImGui::Text(fmt, ##__VA_ARGS__); \
    } while (0)
void sound_resources_t::imgui_contents(bool& play_sound, sound_info_t& sound_to_play)
{
    play_sound = 0;

    if (text_filter)
        text_filter->Draw("Event Filter (inc,-exc)");

    ImGui::Separator();

    ImGui::BeginChild("sound_resources::imgui_contents Sound List", ImGui::GetContentRegionAvail());

    if (!ImGui::BeginTable("sound_resources_t::imgui_contents table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::EndChild();
        return;
    }

    for (const auto& it_event : sounds)
    {
        if (text_filter && !text_filter->PassFilter(it_event.first.c_str()))
            continue;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const bool tree_open_it_event = ImGui::TreeNode(it_event.first.c_str());
        ImGui::TableNextColumn();
        ImGui::PushID(it_event.first.c_str());
        if (ImGui::Button("Play event"))
            play_sound = get_sound(it_event.first, sound_to_play);
        ImGui::PopID();
        if (!tree_open_it_event)
            continue;

        ImGui::PushID(it_event.first.c_str());
        for (const auto& it_sound : it_event.second.entries)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const bool tree_open_it_sound = ImGui::TreeNode(it_sound.name.c_str());
            if (!it_sound.flags.is_event)
            {
                ImGui::TableNextColumn();
                ImGui::PushID(it_sound.name.c_str());
                if (ImGui::Button("Play File"))
                {
                    play_sound = 1;
                    sound_to_play = it_sound;
                }
                ImGui::PopID();
            }
            if (!tree_open_it_sound)
                continue;

            const float weight_percentage = float(it_sound.weight * 100) / float(it_event.second.total_weight);
            const int weight_percentage_decimals = SDL_clamp(int(log2(100.0f / weight_percentage)), 0, 6);
            FIELD("Weight", "%d/%d (%.*f%%)", it_sound.weight, it_event.second.total_weight, weight_percentage_decimals, weight_percentage);
            if (!it_sound.flags.is_event)
            {
                FIELD("Category", "%d (%s)", it_sound.category, sound_info_t::sound_category_to_str(it_sound.category));

                FIELD("Is Stream", "%s", it_sound.flags.stream ? "true" : "false");
                FIELD("Is Event", "%s", it_sound.flags.is_event ? "true" : "false");

                FIELD("Volume multiplier", "%.4f", it_sound.volume);
                FIELD("Pitch multiplier", "%.4f", it_sound.pitch);

                FIELD("Resource ID: Sound", "%s", it_sound.id_sound.c_str());
                FIELD("Resource ID: Subtitle", "%s", it_sound.id_sub.c_str());
                FIELD("Path", "%s", it_sound.path.c_str());
            }
            else
            {
                FIELD("Event name", "%s", it_sound.path.c_str());
            }

            ImGui::TreePop();
        } /* for (const auto& it_sound : it_event.second) */
        ImGui::PopID();

        ImGui::TreePop();
    }
    ImGui::EndTable();
    ImGui::EndChild();
}
