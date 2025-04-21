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
#include "touch.h"

#include "client/state.h"
#include "tetra/util/convar.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <math.h>

static convar_int_t cvr_mc_gui_simulate_touch {
    "mc_gui_simulate_touch",
    1,
    0,
    1,
    "Convert real mouse inputs to touch inputs for the touch control scheme",
    CONVAR_FLAG_SAVE | CONVAR_FLAG_INT_IS_BOOL,
};

/**
 * Supposedly setting SDL_HINT_MOUSE_TOUCH_EVENTS to "1" should produce a similar effect to this function, but I can't get it to work - Ian
 */
static void event_mouse_to_touch(const SDL_Event& src, SDL_TouchFingerEvent& out)
{
    static bool mouse_held = 0;
    if (!cvr_mc_gui_simulate_touch.get())
        return;

    switch (src.type)
    {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (src.button.which == SDL_TOUCH_MOUSEID) /* Prevent double presses */
            break;
        if (!mouse_held)
        {
            int w, h;
            SDL_GetWindowSize(state::window, &w, &h);
            out = SDL_TouchFingerEvent {
                .type = SDL_EVENT_FINGER_DOWN,
                .reserved = 0,
                .timestamp = src.common.timestamp,
                .touchID = SDL_TOUCH_MOUSEID,
                .fingerID = 0,
                .x = float(src.button.x) / float(w),
                .y = float(src.button.y) / float(h),
                .dx = 0,
                .dy = 0,
                .pressure = 0,
                .windowID = SDL_GetWindowID(state::window),
            };
            mouse_held = 1;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (src.motion.which == SDL_TOUCH_MOUSEID) /* Prevent double presses */
            break;
        if (mouse_held)
        {
            int w, h;
            SDL_GetWindowSize(state::window, &w, &h);
            out = SDL_TouchFingerEvent {
                .type = SDL_EVENT_FINGER_MOTION,
                .reserved = 0,
                .timestamp = src.common.timestamp,
                .touchID = SDL_TOUCH_MOUSEID,
                .fingerID = 0,
                .x = float(src.motion.x) / float(w),
                .y = float(src.motion.y) / float(h),
                .dx = float(src.motion.xrel) / float(w),
                .dy = float(src.motion.yrel) / float(h),
                .pressure = 0,
                .windowID = SDL_GetWindowID(state::window),
            };
        }
        break;
    case SDL_EVENT_MOUSE_REMOVED:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (src.button.which == SDL_TOUCH_MOUSEID) /* Prevent double presses */
            break;
        if (mouse_held)
        {
            int w, h;
            SDL_GetWindowSize(state::window, &w, &h);
            out = SDL_TouchFingerEvent {
                .type = SDL_EVENT_FINGER_UP,
                .reserved = 0,
                .timestamp = src.common.timestamp,
                .touchID = SDL_TOUCH_MOUSEID,
                .fingerID = 0,
                .x = float(src.button.x) / float(w),
                .y = float(src.button.y) / float(h),
                .dx = 0,
                .dy = 0,
                .pressure = 0,
                .windowID = SDL_GetWindowID(state::window),
            };
            mouse_held = 0;
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        if (mouse_held)
        {
            out = SDL_TouchFingerEvent {
                .type = SDL_EVENT_FINGER_CANCELED,
                .reserved = 0,
                .timestamp = src.common.timestamp,
                .touchID = SDL_TOUCH_MOUSEID,
                .fingerID = 0,
                .x = 0,
                .y = 0,
                .dx = 0,
                .dy = 0,
                .pressure = 0,
                .windowID = SDL_GetWindowID(state::window),
            };
            mouse_held = 0;
        }
        break;
    default:
        break;
    }
}

#define LEFT_CLICK_THRESH_MAX (275ul * 1000ul * 1000ul)
#define RIGHT_CLICK_THRESH_MIN (300ul * 1000ul * 1000ul)

void touch_handler_t::touch_finger_data_t::recalculate_deltas()
{
    max_delta_x = SDL_max(fabsf(x - initial_x), max_delta_x);
    max_delta_y = SDL_max(fabsf(y - initial_y), max_delta_y);
}

bool touch_handler_t::get_button_left_hold()
{
    Uint64 cur_time = SDL_GetTicksNS();
    if (cur_time - last_left_hold < 125ul * 1000ul * 1000ul)
        return false;
    last_left_hold = cur_time;
    for (auto& it : fingers)
    {
        if (it.second.area_camera && it.second.within_deadzone_prior_to_threshold && (cur_time - it.second.initial_timestamp) >= RIGHT_CLICK_THRESH_MIN)
            return true;
    }
    return false;
}

ImVec2 touch_handler_t::get_move_factors(bool& held_ctrl) const
{
    if (!world_has_focus)
        return ImVec2(0, 0);

    for (auto& it : fingers)
    {
        if (!it.second.area_move)
            continue;
        ImVec2 move_size = corner_camera_move1 - corner_camera_move0;
        ImVec2 displace = corner_camera_move0 + move_size * 0.5f;
        ImVec2 values(it.second.x - displace.x, -(it.second.y - displace.y));
        values /= move_size * 0.5f;
        if (fabsf(values.x) > 1.2f || fabsf(values.y) > 1.2f)
            held_ctrl = 1;
        if (fabsf(values.x) > 1.0f)
            values.x = copysignf(1.0f, values.x);
        if (fabsf(values.y) > 1.0f)
            values.y = copysignf(1.0f, values.y);
        return values;
    }

    held_ctrl = 0;
    return ImVec2(0, 0);
}

float touch_handler_t::get_vertical_factor() const
{
    if (!world_has_focus)
        return 0;

    for (auto& it : fingers)
    {
        if (!it.second.area_vert)
            continue;
        ImVec2 vert_size = corner_camera_vert1 - corner_camera_vert0;
        ImVec2 displace = corner_camera_vert0 + vert_size * 0.5f;
        ImVec2 values(it.second.x - displace.x, -(it.second.y - displace.y));
        values /= vert_size * 0.5f;
        if (fabsf(values.x) > 1.0f)
            values.x = copysignf(1.0f, values.x);
        if (fabsf(values.y) > 1.0f)
            values.y = copysignf(1.0f, values.y);
        return values.y;
    }

    return 0;
}

void touch_handler_t::draw_imgui(ImDrawList* const drawlist, const ImVec2 pos0, const ImVec2 pos1) const
{
    const ImVec2 size = pos1 - pos0;
    drawlist->AddRectFilled(pos0, pos0 + size, IM_COL32(72, 72, 72, 255));
    drawlist->AddRectFilled(pos0 + size * corner_camera_exclude0, pos0 + size * corner_camera_exclude1, IM_COL32(255, 72, 72, 255));
    drawlist->AddRectFilled(pos0 + size * corner_camera_move0, pos0 + size * corner_camera_move1, IM_COL32(72, 255, 72, 255));
    drawlist->AddRectFilled(pos0 + size * corner_camera_vert0, pos0 + size * corner_camera_vert1, IM_COL32(72, 255, 72, 255));
    for (auto& it : fingers)
    {
        ImVec2 p_start = pos0 + size * ImVec2(it.second.initial_x, it.second.initial_y);
        ImVec2 p_final = pos0 + size * ImVec2(it.second.x, it.second.y);
        drawlist->AddCircleFilled(p_start, 10, IM_COL32(0, 255, 0, 255));
        drawlist->AddCircleFilled(p_final, 10, IM_COL32(127 + 127 * it.second.area_move, 0, 127 + 127 * it.second.area_camera, 255));
        drawlist->AddLine(p_start, p_final, IM_COL32(0, 0, 255, 255), 4);
        if (it.second.area_move)
            drawlist->AddText(p_final += ImVec2(0, ImGui::GetTextLineHeight()), IM_COL32(255, 255, 255, 255), "area_move");
        if (it.second.area_vert)
            drawlist->AddText(p_final += ImVec2(0, ImGui::GetTextLineHeight()), IM_COL32(255, 255, 255, 255), "area_vert");
        if (it.second.area_camera)
            drawlist->AddText(p_final += ImVec2(0, ImGui::GetTextLineHeight()), IM_COL32(255, 255, 255, 255), "area_camera");
        if (it.second.within_deadzone)
            drawlist->AddText(p_final += ImVec2(0, ImGui::GetTextLineHeight()), IM_COL32(255, 255, 255, 255), "within_deadzone");
        if (it.second.within_deadzone_prior_to_threshold)
            drawlist->AddText(p_final += ImVec2(0, ImGui::GetTextLineHeight()), IM_COL32(255, 255, 255, 255), "within_deadzone_prior_to_threshold");
    }

    bool held = 0;
    ImVec2 tpos = pos1;
    ImVec2 move_factors = get_move_factors(held);
    drawlist->AddText(tpos += ImVec2(0, ImGui::GetTextLineHeight()), IM_COL32(255, 255, 255, 255), std::to_string(fingers.size()).c_str());
    drawlist->AddText(tpos += ImVec2(0, ImGui::GetTextLineHeight()), IM_COL32(255, 255, 255, 255), std::to_string(move_factors.y).c_str());
    drawlist->AddText(tpos += ImVec2(0, ImGui::GetTextLineHeight()), IM_COL32(255, 255, 255, 255), std::to_string(move_factors.x).c_str());
    drawlist->AddText(tpos += ImVec2(0, ImGui::GetTextLineHeight()), IM_COL32(255, 255, 255, 255), std::to_string(get_vertical_factor()).c_str());
}

void touch_handler_t::set_world_focus(const bool world_has_input)
{
    if (world_has_input == world_has_focus)
        return;
    fingers.clear();
    held_right = 0;
    camera_dx = 0;
    camera_dy = 0;
    world_has_focus = world_has_input;
}

void touch_handler_t::feed_event(const SDL_Event& event_src)
{
    if (!world_has_focus)
        return;

    SDL_TouchFingerEvent event = event_src.tfinger;
    event_mouse_to_touch(event_src, event);

    auto tfinger_id = std::pair(event.touchID, event.fingerID);

    if (event.type == SDL_EVENT_FINGER_DOWN || (event.type == SDL_EVENT_FINGER_MOTION && fingers.find(tfinger_id) == fingers.end()))
    {
        touch_finger_data_t dat = {
            .initial_timestamp = event.timestamp,
            .x = event.x,
            .y = event.y,
            .initial_x = event.x,
            .initial_y = event.y,
            .max_delta_x = 0.0f,
            .max_delta_y = 0.0f,
            .area_move = 0,
            .area_vert = 0,
            .area_camera = 0,
            .within_deadzone = 1,
            .within_deadzone_prior_to_threshold = 1,
        };

        if (dat.x < corner_camera_exclude1.x && dat.y > corner_camera_exclude0.y)
        {
            if (dat.x > corner_camera_move0.x && dat.y < corner_camera_move1.y)
                dat.area_move = 1;
        }
        else if (dat.x < corner_camera_vert1.x && dat.y < corner_camera_vert1.y && dat.x > corner_camera_vert0.x && dat.y > corner_camera_vert0.y)
            dat.area_vert = 1;
        else
            dat.area_camera = 1;

        fingers[tfinger_id] = dat;
    }
    if (event.type == SDL_EVENT_FINGER_UP && fingers.find(tfinger_id) != fingers.end())
    {
        auto it = fingers.find(tfinger_id);
        touch_finger_data_t& dat = it->second;
        dat.x = event.x;
        dat.y = event.y;
        dat.recalculate_deltas();

        if (dat.area_camera && dat.within_deadzone && (event.timestamp - dat.initial_timestamp) < LEFT_CLICK_THRESH_MAX)
            held_right = 1;

        fingers.erase(it);
    }
    if (event.type == SDL_EVENT_FINGER_MOTION && fingers.find(tfinger_id) != fingers.end())
    {
        auto it = fingers.find(tfinger_id);
        touch_finger_data_t& dat = it->second;
        dat.x = event.x;
        dat.y = event.y;
        dat.recalculate_deltas();

        if (dat.max_delta_x > 0.05f || dat.max_delta_y > 0.1f)
            dat.within_deadzone = 0;

        if (!dat.within_deadzone && (event.timestamp - dat.initial_timestamp) < RIGHT_CLICK_THRESH_MIN)
            dat.within_deadzone_prior_to_threshold = 0;

        if (dat.area_camera)
        {
            glm::ivec2 win_size(0);
            SDL_GetWindowSize(SDL_GetWindowFromID(event.windowID), &win_size.x, &win_size.y);

            camera_dx += event.dx * float(win_size.x);
            camera_dy += event.dy * float(win_size.y);
        }
    }
    if (event.type == SDL_EVENT_FINGER_CANCELED)
    {
        auto it = fingers.find(tfinger_id);
        if (it != fingers.end())
            fingers.erase(it);
    }
}
