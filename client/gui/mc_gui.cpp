/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Portions Copyright (c) 2014-2024 Omar Cornut and Dear ImGui Contributors
 * SPDX-FileCopyrightText: Portions Copyright (c) 2025 Ian Hangartner <icrashstuff at outlook dot com>
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

#include "mc_gui.h"
#include "mc_gui_internal.h"

#include "tetra/gui/console.h"
#include "tetra/tetra_gl.h"
#include "tetra/util/stbi.h"

#include "tetra/gui/imgui_internal.h"

static GLuint load_texture(void* data, int x, int y, std::string label, GLenum edge, GLenum format_color = GL_RGBA, GLenum format_data = GL_UNSIGNED_BYTE)
{
    GLuint ret;
    glGenTextures(1, &ret);
    glBindTexture(GL_TEXTURE_2D, ret);
    tetra::gl_obj_label(GL_TEXTURE, ret, label.c_str());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, edge);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, edge);

    if (data)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, format_color, format_data, data);
    else
    {
        tetra::gl_obj_label(GL_TEXTURE, ret, (label + " (Missing)").c_str());
        Uint8* data_missing = new Uint8[64 * 64 * 4];
        if (data_missing)
        {
            for (y = 0; y < 64; y++)
                for (x = 0; x < 64; x++)
                {
                    data_missing[(y * 64 + x) * 4 + 0] = (x % 2 == y % 2) ? 0 : 255;
                    data_missing[(y * 64 + x) * 4 + 1] = 0;
                    data_missing[(y * 64 + x) * 4 + 2] = (x % 2 == y % 2) ? 0 : 255;
                    data_missing[(y * 64 + x) * 4 + 3] = 255;
                }
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data_missing);
        }
        else
        {
            Uint8 data_missing_missing[] = { 0xC7, 0, 0, 0xC7 };
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE_3_3_2, data_missing_missing);
        }
        delete[] data_missing;
    }

    return ret;
}

static GLuint load_gui_texture(std::string path, GLenum edge = GL_CLAMP_TO_EDGE, std::string prefix = "/_resources/assets/minecraft/textures/gui/")
{
    const std::string label = std::string("[Menu]: Texture: ") + path;
    path = prefix + path;

    int x, y, channels;
    Uint8* data_widgets = stbi_physfs_load(path.c_str(), &x, &y, &channels, 4);

    if (!data_widgets)
        dc_log_error("Unable to load texture: \"%s\"", path.c_str());

    GLuint ret = load_texture(data_widgets, x, y, label, edge);

    stbi_image_free(data_widgets);

    return ret;
}

void mc_gui::mc_gui_ctx::load_resources()
{
    unload_resources();

    /* Load built in translations */
    mc_gui::global_ctx->translations["mcs_b181_client.reload_resources"] = "Reload Resources";
    mc_gui::global_ctx->translations["mcs_b181_client.username"] = "Username";
    mc_gui::global_ctx->translations["mcs_b181_client.menu.test_world"] = "Test world";
    mc_gui::global_ctx->translations["mcs_b181_client.placeholder"] = "Nothing to see here :)";
    mc_gui::global_ctx->translations["mcs_b181_client.mcs_b181_client"] = "mcs_b181_client";

    /* TODO: Load actual translation table */
    mc_gui::global_ctx->translations["gui.done"] = "Done";
    mc_gui::global_ctx->translations["options.title"] = "Options";
    mc_gui::global_ctx->translations["options.on"] = "On";
    mc_gui::global_ctx->translations["options.off"] = "Off";
    mc_gui::global_ctx->translations["options.renderDistance"] = "Render Distance";
    mc_gui::global_ctx->translations["options.guiScale"] = "GUI Scale";
    mc_gui::global_ctx->translations["options.guiScale.auto"] = "Auto";
    mc_gui::global_ctx->translations["options.guiScale.small"] = "Small";
    mc_gui::global_ctx->translations["options.guiScale.normal"] = "Normal";
    mc_gui::global_ctx->translations["options.guiScale.large"] = "Large";

    tex_id_widgets = load_gui_texture("widgets.png");
    tex_id_icons = load_gui_texture("icons.png");

    tex_id_inventory = load_gui_texture("container/inventory.png");
    tex_id_creative_tab_search = load_gui_texture("container/creative_inventory/tab_item_search.png");
    tex_id_creative_tab_inv = load_gui_texture("container/creative_inventory/tab_inventory.png");
    tex_id_creative_tabs = load_gui_texture("container/creative_inventory/tabs.png");
    tex_id_creative_tab_items = load_gui_texture("container/creative_inventory/tab_items.png");
    tex_id_chest_generic = load_gui_texture("container/generic_54.png");
    tex_id_furnace = load_gui_texture("container/furnace.png");
    tex_id_crafting_table = load_gui_texture("container/crafting_table.png");

    tex_id_bg = load_gui_texture("options_background.png", GL_REPEAT);
    tex_id_selectors_resource = load_gui_texture("resource_packs.png");
    tex_id_selectors_server = load_gui_texture("server_selection.png");

    Uint16 data_crosshair[16 * 16] = { 0 };
    for (int i = 3; i < 12; i++)
    {
        data_crosshair[i + 7 * 16] = 0xFFFF;
        data_crosshair[7 + i * 16] = 0xFFFF;
    }
    tex_id_crosshair = load_texture(data_crosshair, 16, 16, "[Menu]: Texture: Crosshair", GL_CLAMP, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4);
}

void mc_gui::mc_gui_ctx::unload_resources()
{
#define DEL_TEX(TEX_ID)               \
    do                                \
    {                                 \
        glDeleteTextures(1, &TEX_ID); \
        TEX_ID = 0;                   \
    } while (0)

    DEL_TEX(tex_id_widgets);
    DEL_TEX(tex_id_icons);

    DEL_TEX(tex_id_inventory);
    DEL_TEX(tex_id_creative_tab_search);
    DEL_TEX(tex_id_creative_tab_inv);
    DEL_TEX(tex_id_creative_tabs);
    DEL_TEX(tex_id_creative_tab_items);
    DEL_TEX(tex_id_chest_generic);
    DEL_TEX(tex_id_furnace);
    DEL_TEX(tex_id_crafting_table);

    DEL_TEX(tex_id_bg);
    DEL_TEX(tex_id_selectors_resource);
    DEL_TEX(tex_id_selectors_server);

    DEL_TEX(tex_id_crosshair);

    translations.clear();

#undef DEL_TEX
}

/* This isn't very efficient but it works well enough */
void mc_gui::render_text_clipped(const ImVec2& pos_min, const ImVec2& pos_max, const char* text, const char* text_end, const ImVec2* text_size_if_known,
    const ImVec2& align, const ImRect* clip_rect)
{
    ImVec4 col_shadow = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Text));
    col_shadow.x *= 0.25f;
    col_shadow.y *= 0.25f;
    col_shadow.z *= 0.25f;

    const char* text_display_end = ImGui::FindRenderedTextEnd(text, text_end);

    ImGui::PushStyleColor(ImGuiCol_Text, col_shadow);
    const ImVec2 shadow_off = ImVec2(1, 1) * global_ctx->menu_scale;
    ImGui::RenderTextClipped(pos_min + shadow_off, pos_max + shadow_off, text, text_display_end, text_size_if_known, align, clip_rect);
    ImGui::PopStyleColor();

    ImGui::RenderTextClipped(pos_min, pos_max, text, text_display_end, text_size_if_known, align, clip_rect);
}

/* Slightly modified version of ImGui::ButtonEx */
ImGuiButtonFlags mc_gui::button(const char* label, const ImVec2& size_arg, ImGuiButtonFlags flags)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;
    label = get_translation(label);

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    if ((flags & ImGuiButtonFlags_AlignTextBaseLine)
        && style.FramePadding.y < window->DC.CurrLineTextBaseOffset) // Try to vertically align buttons that are smaller/have no padding so that text baseline
                                                                     // matches (bit hacky, since it shouldn't be a flag)
        pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
    ImVec2 size
        = ImGui::CalcItemSize(size_arg * global_ctx->menu_scale, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered = 0, held = 0, pressed = 0;
    pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

    /* BEGIN Extracts from ImGui::ButtonBehavior() */
    const ImGuiID test_owner_id = (flags & ImGuiButtonFlags_NoTestKeyOwner) ? ImGuiKeyOwner_Any : id;
    int mouse_button_clicked = -1;
    int mouse_button_released = -1;
    for (int button = 0; button < 3; button++)
        if (flags & (ImGuiButtonFlags_MouseButtonLeft << button)) // Handle ImGuiButtonFlags_MouseButtonRight and ImGuiButtonFlags_MouseButtonMiddle here.
        {
            if (ImGui::IsMouseClicked(button, ImGuiInputFlags_None, test_owner_id) && mouse_button_clicked == -1)
            {
                mouse_button_clicked = button;
            }
            if (ImGui::IsMouseReleased(button, test_owner_id) && mouse_button_released == -1)
            {
                mouse_button_released = button;
            }
        }
    /* END Extracts from ImGui::ButtonBehavior() */

    // Render
    ImGui::RenderNavHighlight(bb, id);

    ImVec2 tcords_min(0, float(66 + ((held || hovered || pressed) ? 20 : 0)));
    ImVec2 tcords_max(200, float(86 + ((held || hovered || pressed) ? 20 : 0)));
    render_widget(bb.Min, bb.Max, (ImTextureID)(uintptr_t)global_ctx->tex_id_widgets, tcords_min, tcords_max, ImVec2(256, 256));

    if (g.LogEnabled)
        ImGui::LogSetNextTextDecoration("[", "]");

    if (held || hovered || pressed)
    {
        ImVec4 col_text = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Text));
        col_text.z *= 0.6f;
        ImGui::PushStyleColor(ImGuiCol_Text, col_text);
    }
    mc_gui::render_text_clipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);
    if (held || hovered || pressed)
        ImGui::PopStyleColor();

    ImGuiButtonFlags ret = 0;
    if (pressed && mouse_button_released == 0)
        ret |= ImGuiButtonFlags_MouseButtonLeft;
    if (pressed && mouse_button_released == 1)
        ret |= ImGuiButtonFlags_MouseButtonRight;
    if (pressed && mouse_button_released == 2)
        ret |= ImGuiButtonFlags_MouseButtonMiddle;

    if (pressed && !ret)
        ret = ImGuiButtonFlags_MouseButtonLeft;

    return ret;
}

void mc_gui::render_widget(const ImVec2 p_min, const ImVec2 p_max, const ImTextureID id, const ImVec2 uv_min, const ImVec2 uv_max, const ImVec2 tex_size)
{
    ImGuiWindow* const window = ImGui::GetCurrentWindow();

    const ImVec2 size = p_max / float(global_ctx->menu_scale) - p_min / float(global_ctx->menu_scale);
    const ImVec2 size0(SDL_floorf(size.x / 2.0f) + 1.0f, size.y);
    const ImVec2 size1(SDL_ceilf(size.x / 2.0f) + 1.0f, size.y);

    ImVec2 uv0 = uv_min / tex_size;
    ImVec2 uv1 = uv_min / tex_size + ImVec2(size0.x * (1.0f / tex_size.x), size0.y * (1.0f / tex_size.y));
    ImVec2 uv2 = uv_max / tex_size - ImVec2(size1.x * (1.0f / tex_size.x), size1.y * (1.0f / tex_size.y));
    ImVec2 uv3 = uv_max / tex_size;

    window->DrawList->AddImage(id, p_min, p_min + size0 * global_ctx->menu_scale, uv0, uv1, IM_COL32_WHITE);
    window->DrawList->AddImage(id, p_max - size1 * global_ctx->menu_scale, p_max, uv2, uv3, IM_COL32_WHITE);
}

ImGuiButtonFlags mc_gui::button_big(const char* const label, ImGuiButtonFlags flags)
{
    return mc_gui::button(label, ImVec2(global_ctx->get_width_large(false), 20), flags);
}

ImGuiButtonFlags mc_gui::button_mid(const char* const label, ImGuiButtonFlags flags)
{
    return mc_gui::button(label, ImVec2(global_ctx->get_width_mid(false), 20), flags);
}

ImGuiButtonFlags mc_gui::button_small(const char* const label, ImGuiButtonFlags flags)
{
    return mc_gui::button(label, ImVec2(global_ctx->get_width_small(false), 20), flags);
}

ImGuiButtonFlags mc_gui::button(widget_size_t size, const char* const label, ImGuiButtonFlags flags)
{
    switch (size)
    {
    case SIZE_SMALL:
        return button_small(label, flags);
    case SIZE_MID:
        return button_mid(label, flags);
    case SIZE_BIG:
        return button_big(label, flags);
    }
    return mc_gui::button(label, ImVec2(global_ctx->get_width_large(false) * 2, 20), flags);
}

void mc_gui::text_unformatted(const char* text, const char* text_end)
{
    // Accept null ranges
    if (text == text_end)
        text = text_end = "";

    // Calculate length
    if (text_end == NULL)
        text_end = text + strlen(text); // FIXME-OPT

    ImVec4 col_shadow = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Text));
    col_shadow.x *= 0.25f;
    col_shadow.y *= 0.25f;
    col_shadow.z *= 0.25f;

    const ImVec2 pos = ImGui::GetCursorPos();

    ImGui::SetCursorPos(pos + ImVec2(1, 1) * global_ctx->menu_scale);
    ImGui::PushStyleColor(ImGuiCol_Text, col_shadow);
    ImGui::TextEx(text, text_end, ImGuiTextFlags_NoWidthForLargeClippedText);
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(pos);
    ImGui::TextEx(text, text_end, ImGuiTextFlags_NoWidthForLargeClippedText);
}

const char* mc_gui::get_translation(const char* translation_id)
{
    auto it = global_ctx->translations.find(translation_id);

    if (it != global_ctx->translations.end())
        return it->second.c_str();

    return translation_id;
}

void mc_gui::text_translated(const char* translation_id) { text_unformatted(get_translation(translation_id)); }

void mc_gui::text(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    textv(fmt, args);
    va_end(args);
}

void mc_gui::textv(const char* fmt, va_list args)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    const char *text, *text_end;
    ImFormatStringToTempBufferV(&text, &text_end, fmt, args);

    ImVec4 col_shadow = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Text));
    col_shadow.x *= 0.25f;
    col_shadow.y *= 0.25f;
    col_shadow.z *= 0.25f;

    const ImVec2 pos = ImGui::GetCursorPos();

    ImGui::SetCursorPos(pos + ImVec2(1, 1) * global_ctx->menu_scale);
    ImGui::PushStyleColor(ImGuiCol_Text, col_shadow);
    ImGui::TextEx(text, text_end, ImGuiTextFlags_NoWidthForLargeClippedText);
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(pos);
    ImGui::TextEx(text, text_end, ImGuiTextFlags_NoWidthForLargeClippedText);
}
