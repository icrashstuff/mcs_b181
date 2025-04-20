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
#ifndef MCS_B181__CLIENT__TOUCH_H_INCLUDED
#define MCS_B181__CLIENT__TOUCH_H_INCLUDED

#include "tetra/gui/imgui.h"
#include <SDL3/SDL_events.h>
#include <map>

/* Forward declarations */
struct game_t;

struct touch_handler_t
{
    /**
     * Get move factors
     *
     * @param held_ctrl [out] If touch_handler has focus, then this will be set to whether the player should sprint or not
     */
    ImVec2 get_move_factors(bool& held_ctrl) const;
    float get_dx()
    {
        float ret = camera_dx;
        camera_dx = 0.0f;
        return ret;
    }
    float get_dy()
    {
        float ret = camera_dy;
        camera_dy = 0.0f;
        return ret;
    }

    bool get_button_left_hold();
    bool get_button_right_hold()
    {
        bool ret = held_right;
        held_right = 0;
        return ret;
    }

    /**
     * Set world focus state
     */
    void set_world_focus(const bool world_has_input);

    /**
     * Feed events
     *
     * If the world doesn't have focus then all events will be ignored
     *
     * @param event Event to feed
     */
    void feed_event(const SDL_Event& event);

    void draw_imgui(ImDrawList* const drawlist, const ImVec2 pos0, const ImVec2 pos1) const;

    ImVec2 corner_camera_exclude0 = { 0.00f, 0.55f };
    ImVec2 corner_camera_exclude1 = { 0.25f, 1.0f };
    ImVec2 corner_camera_move0 = { 0.025f, 0.55f };
    ImVec2 corner_camera_move1 = { 0.225f, 0.95f };
    ImVec2 corner_camera_vert0 = { 0.775f, 0.55f };
    ImVec2 corner_camera_vert1 = { 0.975f, 0.95f };

private:
    struct touch_finger_data_t
    {
        Uint64 initial_timestamp = 0;
        float x = 0;
        float y = 0;
        float initial_x = 0;
        float initial_y = 0;
        float max_delta_x = 0;
        float max_delta_y = 0;

        bool area_move;
        bool area_camera;
        bool within_deadzone;
        bool within_deadzone_prior_to_threshold;

        void recalculate_deltas();
    };

    Uint64 last_left_hold = 0;

    float camera_dx = 0.0f;
    float camera_dy = 0.0f;
    bool held_right = 0;

    std::map<std::pair<SDL_TouchID, SDL_FingerID>, touch_finger_data_t> fingers;
    bool world_has_focus = 0;
};

#endif
