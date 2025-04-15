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

#ifndef MCS_B181_CLIENT_MC_GUI_H_INCLUDED
#define MCS_B181_CLIENT_MC_GUI_H_INCLUDED

#include "../migration_gl.h"

#include "tetra/gui/imgui.h"

#include "client/lang/lang.h"

#include <SDL3/SDL_gpu.h>

#include <map>
#include <string>

namespace mc_gui
{
struct mc_gui_ctx
{
    ImTextureID tex_id_widgets = 0;
    ImTextureID tex_id_crosshair = 0;
    ImTextureID tex_id_icons = 0;

    ImTextureID tex_id_inventory = 0;
    ImTextureID tex_id_creative_tab_search = 0;
    ImTextureID tex_id_creative_tab_inv = 0;
    ImTextureID tex_id_creative_tabs = 0;
    ImTextureID tex_id_creative_tab_items = 0;
    ImTextureID tex_id_chest_generic = 0;
    ImTextureID tex_id_furnace = 0;
    ImTextureID tex_id_crafting_table = 0;

    ImTextureID tex_id_bg = 0;
    ImTextureID tex_id_water = 0;
    ImTextureID tex_id_selectors_resource = 0;
    ImTextureID tex_id_selectors_server = 0;

    int menu_scale = 0;

    inline int get_width_large(bool scale = true) { return (scale ? menu_scale : 1) * 200; }
    inline int get_width_mid(bool scale = true) { return (scale ? menu_scale : 1) * 149; }
    inline int get_width_small(bool scale = true) { return (scale ? menu_scale : 1) * 98; }
    inline int get_width_tiny(bool scale = true) { return (scale ? menu_scale : 1) * 49; }

    ImGuiWindowFlags default_win_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground;

    translation_map_t translations;

    /**
     * Load all GUI resources
     *
     * @returns GPU fence associated with the copy command buffer
     */
    [[nodiscard]] SDL_GPUFence* load_resources();

    /**
     * Unload/free all GUI resources
     */
    void unload_resources();

    static void load_font_ascii(ImFontAtlas* atlas);
};

enum widget_size_t
{
    SIZE_TINY,
    SIZE_SMALL,
    SIZE_MID,
    SIZE_BIG
};

extern mc_gui_ctx* global_ctx;

/**
 * Return the string matching the translation id
 *
 * NOTE: The string is only guaranteed to be valid until the end of a frame
 */
const char* get_translation(const char* translation_id);

/** Slightly modified version of ImGui::ButtonEx supporting Minecraft style buttons/interactions/translations */
IMGUI_API ImGuiButtonFlags button(const char* label, const ImVec2& size_arg, ImGuiButtonFlags flags);

IMGUI_API ImGuiButtonFlags button(widget_size_t size, const char* label, ImGuiButtonFlags flags = ImGuiButtonFlags_MouseButtonLeft);
IMGUI_API ImGuiButtonFlags button_big(const char* label, ImGuiButtonFlags flags = ImGuiButtonFlags_MouseButtonLeft);
IMGUI_API ImGuiButtonFlags button_mid(const char* label, ImGuiButtonFlags flags = ImGuiButtonFlags_MouseButtonLeft);
IMGUI_API ImGuiButtonFlags button_small(const char* label, ImGuiButtonFlags flags = ImGuiButtonFlags_MouseButtonLeft);
IMGUI_API ImGuiButtonFlags button_tiny(const char* label, ImGuiButtonFlags flags = ImGuiButtonFlags_MouseButtonLeft);

/* TODO: Handle minecraft style coloring */
IMGUI_API void text_unformatted(const char* const text, const char* text_end = NULL);
IMGUI_API void text_translated(const char* translation_id);
IMGUI_API void text(const char* fmt, ...) IM_FMTARGS(1);
IMGUI_API void textv(const char* fmt, va_list args) IM_FMTLIST(1);

/** Add text with a shadow to a drawlist */
void add_text(ImDrawList* draw_list, const ImVec2& pos, const char* text_begin, ImU32 col = ImGui::GetColorU32(ImGuiCol_Text), const char* text_end = NULL);
}

#endif
