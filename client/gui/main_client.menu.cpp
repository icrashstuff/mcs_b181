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

#include "shared/build_info.h"
#include "tetra/util/convar.h"
#include <algorithm>

static convar_int_t cvr_mc_less_than_one_item_quantities {
    "mc_less_than_one_item_quantities",
    false,
    false,
    true,
    "Render quantities for items stacks with a quantity of less than 1",
    CONVAR_FLAG_SAVE,
};

static convar_int_t cvr_mc_hotbar_show_name {
    "mc_hotbar_show_name",
    true,
    false,
    true,
    "Show the name of currently selected item above the hotbar",
    CONVAR_FLAG_SAVE,
};

static convar_int_t cvr_mc_force_survival_hotbar {
    "mc_hotbar_force_survival",
    false,
    false,
    true,
    "Show survival hotbar elements in non-survival gamemodes",
    CONVAR_FLAG_SAVE | CONVAR_FLAG_DEV_ONLY,
};

static convar_int_t cvr_mc_hotbar_test {
    "mc_hotbar_test",
    false,
    false,
    true,
    "Runs hotbar element values through ranges to test layout and scaling",
    CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_SAVE,
};

static convar_float_t cvr_mc_hotbar_test_intensity {
    "mc_hotbar_test_intensity",
    1.0f,
    0.01f,
    100.0f,
    "Intensity of tests that are enabled by mc_hotbar_test",
    CONVAR_FLAG_DEV_ONLY | CONVAR_FLAG_SAVE,
};

struct client_menu_return_t
{
    /** If this is true the current window will be popped from the stack */
    bool close = 0;

    /** If this is true the stack will be cleared */
    bool clear_stack = 0;

    /** Allow panorama to be rendered behind */
    bool allow_pano = 0;

    /** Allow world to be rendered behind */
    bool allow_world = 1;

    /** Allow fallback dirt background */
    bool allow_dirt = 1;

    /** If this field is non-zero in length then an attempt will be made to open the named window */
    std::string name_to_open;
};

class client_menu_manager_t
{
    std::vector<std::string> stack;
    std::string default_menu;
    std::map<std::string, std::function<client_menu_return_t(mc_gui::mc_gui_ctx* ctx, ImDrawList*)>> menus;

public:
    void add_menu(std::string name, std::function<client_menu_return_t(mc_gui::mc_gui_ctx* ctx, ImDrawList*)> func) { menus[name] = func; }
    void add_menu(std::string name, std::function<client_menu_return_t(mc_gui::mc_gui_ctx* ctx)> func)
    {
        add_menu(name, [=](mc_gui::mc_gui_ctx* ctx, ImDrawList*) -> client_menu_return_t { return func(ctx); });
    }

    void stack_clear() { stack.clear(); }

    void stack_push(std::string name) { stack.push_back(name); }

    int stack_size() { return stack.size(); }

    /** Set the default menu to be used when stack.size() == 0 */
    void set_default(std::string name)
    {
        if (default_menu == name)
            return;
        stack_clear();
        default_menu = name;
    }

    /**
     * @param drawlist This will be passed to the menu to use instead of ImGui::GetBackgroundDrawList()
     */
    client_menu_return_t run_last_in_stack(glm::ivec2 win_size, ImDrawList* drawlist)
    {
        std::string to_render = default_menu;
        if (stack.size())
            to_render = stack.back();

        auto it = menus.find(to_render);

        client_menu_return_t ret;
        ret.allow_pano = 1;
        ret.allow_world = 1;

        while (it == menus.end() && stack.size())
        {
            stack.pop_back();
            to_render = default_menu;
            if (stack.size())
                to_render = stack.back();
            it = menus.find(to_render);
        }

        ImGui::PushID(to_render.c_str());

        if (it != menus.end())
            ret = it->second(mc_gui::global_ctx, drawlist);

        ImGui::PopID();

        if (ret.close && stack.size())
            stack.pop_back();

        if (ret.clear_stack)
            stack_clear();

        if (ret.name_to_open.length())
            stack.push_back(ret.name_to_open);

        return ret;
    }

    client_menu_manager_t() { }
} static client_menu_manager;

static ImVec2 get_viewport_centered_title_bar()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImVec2 ret = viewport->GetWorkCenter();

    ret.y -= viewport->WorkSize.y * 0.35f;

    return ret;
}

static ImVec2 get_viewport_centered_lower_quarter()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImVec2 ret = viewport->GetWorkCenter();

    ret.y += viewport->WorkSize.y * 0.25f;

    return ret;
}

static void menu_title(mc_gui::mc_gui_ctx* ctx, const char* title)
{
    ImGui::SetNextWindowPos(get_viewport_centered_title_bar(), ImGuiCond_Always, ImVec2(0.5, 1.0));
    ImGui::Begin("menu_title", NULL, ctx->default_win_flags);

    mc_gui::text_translated(title);

    ImGui::End();
}

static void menu_done(mc_gui::mc_gui_ctx* ctx, client_menu_return_t& ret)
{
    ImGui::SetNextWindowPos(get_viewport_centered_lower_quarter(), ImGuiCond_Always, ImVec2(0.5, 0.0));
    ImGui::Begin("menu.gui.done", NULL, ctx->default_win_flags);

    if (mc_gui::button_big("gui.done"))
        ret.close = 1;

    ImGui::End();
}

static void text_brand_ver()
{
    const char* brand = mc_gui::get_translation("mcs_b181.brand_client");
    mc_gui::text("%s (%s)-%s (%s)", brand, build_info::ver_string::client().c_str(), build_info::build_mode, build_info::git::refspec);
}

static client_menu_return_t do_main_menu(mc_gui::mc_gui_ctx* ctx)
{
    client_menu_return_t ret;

    ret.allow_pano = 1;

    ImGui::SetNextWindowPos(ImVec2(0.0, 0.0), ImGuiCond_Always, ImVec2(0.0, 0.0));
    ImGui::Begin("Convar Window", NULL, ctx->default_win_flags);
    static Uint64 last_convar_button_press = 0;
    if (ImGui::InvisibleButton("Convar Button", ImGui::GetMainViewport()->Size / 10.0f))
    {
        Uint64 cur_tick = SDL_GetTicks();
        if (cur_tick - last_convar_button_press < 300)
            ret.name_to_open = "menu.convars";
        last_convar_button_press = cur_tick;
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
    ImGui::Begin("Main", NULL, ctx->default_win_flags);

    if (mc_gui::button_big("mcs_b181.menu.test_world"))
    {
        game_t* new_game = new game_t(state::game_resources);
        new_game->create_testworld();
        games.push_back(new_game);
        ret.clear_stack = 1;
    }

    if (mc_gui::button_big("menu.multiplayer"))
    {
        games.push_back(new game_t(cvr_autoconnect_addr.get(), cvr_autoconnect_port.get(), cvr_username.get(), state::game_resources));
        ret.clear_stack = 1;
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + float(10 * ctx->menu_scale));

    if (mc_gui::button_small("menu.options"))
        ret.name_to_open = "menu.options";

    ImGui::SameLine();

    if (mc_gui::button_small("menu.quit"))
        engine_state_target = ENGINE_STATE_EXIT;

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetMainViewport()->Size.y), ImGuiCond_Always, ImVec2(0.0, 1.0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1, 1) * ctx->menu_scale);
    ImGui::Begin("Bottom Text", NULL, ctx->default_win_flags);
    text_brand_ver();
    ImGui::End();
    ImGui::PopStyleVar();

    return ret;
}

static void do_in_game_menu__player_list(mc_gui::mc_gui_ctx* const ctx, connection_t* const connection)
{
    if (!held_tab)
        return;

    const int old_menu_scale = ctx->menu_scale;
    float font_scale = 1.0f / float(old_menu_scale);
    ctx->menu_scale = 1;

    float max_width_name = ImGui::CalcTextSize("X").x * 16.0f * font_scale;
    auto list = connection->get_player_list();

    for (auto it : list)
    {
        float width = ImGui::CalcTextSize(it.first.c_str()).x * font_scale;
        max_width_name = SDL_max(max_width_name, width);
    }

    jint num_players = connection->get_max_players();

    if (num_players == 0)
        return;

    const int columns = num_players / 20 + 1;
    num_players = (num_players / columns) * columns;
    const float text_height = ImGui::GetTextLineHeight() * font_scale;
    const float line_height = ctx->menu_scale;
    const ImVec2 line_offset = ImVec2(1.0f, 1.0f) * line_height * 0.5f;
    const float spacer_width = ctx->menu_scale;
    const ImVec2 img_size(text_height, text_height);
    const ImVec2 conn_size(ctx->menu_scale * 10, text_height);
    ImVec2 item_size(0, text_height + line_height);
    item_size.x += spacer_width + img_size.x;
    item_size.x += spacer_width + max_width_name;
    item_size.x += spacer_width + conn_size.x;
    item_size.x += spacer_width;

    ImVec2 window_size = item_size * ImVec2(columns, (num_players + columns - 1) / columns) + line_offset * 2.0f;
    ImVec2 window_pos = { SDL_floorf(ImGui::GetMainViewport()->GetWorkCenter().x - line_height), 0.0f };

    ImGui::SetNextWindowSize(window_size + line_offset, ImGuiCond_Always);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(0.5f, -0.05f));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("Player List", NULL, ImGuiWindowFlags_NoDecoration);

    window_size = ImGui::GetWindowSize();
    window_pos = ImGui::GetWindowPos();

    ImGui::SetWindowFontScale(font_scale);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    Uint32 line_col = IM_COL32(255, 255, 255, 192);

    ImVec2 cursor = window_pos;

    const ImVec2 points_ul[] = {
        line_offset + ImVec2 { window_pos.x, window_pos.y + window_size.y },
        line_offset + ImVec2 { window_pos.x, window_pos.y },
        line_offset + ImVec2 { window_pos.x + window_size.x, window_pos.y },
    };
    draw_list->AddPolyline(points_ul, IM_ARRAYSIZE(points_ul), line_col, ImDrawFlags_RoundCornersNone, line_height * 0.5f);

    auto it = list.begin();
    for (jint i = 0; i < num_players; i++)
    {
        cursor = ImVec2(window_pos.x + item_size.x * float(i % columns), window_pos.y + item_size.y * float(i / columns));

        const ImVec2 points[] = {
            line_offset + ImVec2 { cursor.x, item_size.y + cursor.y },
            line_offset + ImVec2 { item_size.x + cursor.x, item_size.y + cursor.y },
            line_offset + ImVec2 { item_size.x + cursor.x, cursor.y },
        };
        draw_list->AddPolyline(points, IM_ARRAYSIZE(points), line_col, ImDrawFlags_RoundCornersNone, line_height * 0.5f);

        cursor += line_offset * 2.0f;

        if (it == list.end())
            continue;

        draw_list->AddImage(ctx->tex_id_bg, cursor, cursor + img_size);

        cursor.x += img_size.x + spacer_width;

        Uint32 col_text = ImGui::GetColorU32(ImGuiCol_Text);
        ImVec4 col_shadow = ImGui::ColorConvertU32ToFloat4(col_text);
        col_shadow.x *= 0.25f;
        col_shadow.y *= 0.25f;
        col_shadow.z *= 0.25f;

        draw_list->AddText(cursor + ImVec2(1, 1) * ctx->menu_scale, ImGui::ColorConvertFloat4ToU32(col_shadow), it->first.c_str());
        draw_list->AddText(cursor, col_text, it->first.c_str());

        cursor.x += max_width_name + spacer_width;

        const int ping = it->second.average();
        int strength = 5;

        strength -= (ping >= 150);
        strength -= (ping >= 300);
        strength -= (ping >= 600);
        strength -= (ping >= 1000);
        strength -= (ping < 0) * 5;

        strength = SDL_clamp(strength, 0, 5);

        ImVec2 uv0(00.0f / 256.0f, (56 - strength * 8) / 256.0f);
        ImVec2 uv1(10.0f / 256.0f, (64 - strength * 8) / 256.0f);

        draw_list->AddImage(ctx->tex_id_icons, cursor, cursor + conn_size, uv0, uv1);

        it++;
    }

    ImGui::End();
    ImGui::PopStyleVar(4);

    ctx->menu_scale = old_menu_scale;
}

#ifndef GLM_TO_IM
#define GLM_TO_IM(A) ImVec2((A).x, (A).y)
#endif

/**
 * Render itemstack
 *
 * @param draw_list List to draw to
 * @param menu_scale Menu scale (for text shadow)
 * @param pos0 Position of upper left corner (in window size coordinates)
 * @param pos1 Position of lower right corner (in window size coordinates)
 * @param item Item to render
 * @param stretch Stretch factors
 * @param stretch_center Point to stretch away from
 */
static void render_item_stack(ImDrawList* const draw_list, const int menu_scale, const ImVec2 pos0, const ImVec2 pos1, const itemstack_t& item,
    const ImVec2 stretch = ImVec2(1.0f, 1.0f), const ImVec2 stretch_center = ImVec2(0.50f, 0.450f))
{
    if (item.id == BLOCK_ID_NONE || item.id == BLOCK_ID_AIR)
        return;

    ImTextureID tex_id(reinterpret_cast<ImTextureID>(&state::game_resources->terrain_atlas->binding));

    mc_id::terrain_face_t face_top = state::game_resources->terrain_atlas->get_face(mc_id::FACE_STONE);
    mc_id::terrain_face_t face_left = state::game_resources->terrain_atlas->get_face(mc_id::FACE_STONE);
    mc_id::terrain_face_t face_right = state::game_resources->terrain_atlas->get_face(mc_id::FACE_STONE);

    /* TODO: Proper rendering of items/blocks */
    if (!mc_id::is_block(item.id) || !mc_id::block_has_collision(item.id))
    {
        ImVec2 uv0(face_top.corners[0].x, face_top.corners[0].y);
        ImVec2 uv1(face_top.corners[3].x, face_top.corners[3].y);
        draw_list->AddImage(tex_id, pos0, pos1, uv0, uv1);
    }
    else
    {
        const ImVec2 size = pos1 - pos0;

        ImVec2 _left_upper = ImVec2(0.05f, 0.226f);
        ImVec2 _left_lower = ImVec2(0.05f, 0.773f);

        ImVec2 _mid_upper = ImVec2(0.50f, 0.010f);
        ImVec2 _mid_mid = ImVec2(0.50f, 0.450f);
        ImVec2 _mid_lower = ImVec2(0.50f, 0.990f);

        ImVec2 _right_upper = ImVec2(0.95f, 0.230f);
        ImVec2 _right_lower = ImVec2(0.95f, 0.773f);

#define VEC2_OFFS(b, a) ImVec2(a.x - b.x, a.y - b.y)
        _left_upper = stretch_center + stretch * VEC2_OFFS(stretch_center, _left_upper);
        _left_lower = stretch_center + stretch * VEC2_OFFS(stretch_center, _left_lower);

        _mid_upper = stretch_center + stretch * VEC2_OFFS(stretch_center, _mid_upper);
        _mid_mid = stretch_center + stretch * VEC2_OFFS(stretch_center, _mid_mid);
        _mid_lower = stretch_center + stretch * VEC2_OFFS(stretch_center, _mid_lower);

        _right_upper = stretch_center + stretch * VEC2_OFFS(stretch_center, _right_upper);
        _right_lower = stretch_center + stretch * VEC2_OFFS(stretch_center, _right_lower);
#undef VEC2_OFFS

        const ImVec2 left_upper = pos0 + size * _left_upper;
        const ImVec2 left_lower = pos0 + size * _left_lower;
        const ImVec2 mid_upper = pos0 + size * _mid_upper;
        const ImVec2 mid_mid = pos0 + size * _mid_mid;
        const ImVec2 mid_lower = pos0 + size * _mid_lower;
        const ImVec2 right_upper = pos0 + size * _right_upper;
        const ImVec2 right_lower = pos0 + size * _right_lower;

        face_top.rotate_90();

        ImVec2 uv_top[4] = {
            GLM_TO_IM(face_top.corners[0]),
            GLM_TO_IM(face_top.corners[1]),
            GLM_TO_IM(face_top.corners[3]),
            GLM_TO_IM(face_top.corners[2]),
        };
        ImVec2 uv_left[4] = {
            GLM_TO_IM(face_left.corners[0]),
            GLM_TO_IM(face_left.corners[1]),
            GLM_TO_IM(face_left.corners[3]),
            GLM_TO_IM(face_left.corners[2]),
        };
        ImVec2 uv_right[4] = {
            GLM_TO_IM(face_right.corners[1]),
            GLM_TO_IM(face_right.corners[0]),
            GLM_TO_IM(face_right.corners[2]),
            GLM_TO_IM(face_right.corners[3]),
        };

        Uint32 col_top = IM_COL32(255, 255, 255, 255);
        Uint32 col_left = IM_COL32(189, 189, 189, 255);
        Uint32 col_right = IM_COL32(216, 216, 216, 255);

        draw_list->AddImageQuad(tex_id, left_upper, mid_mid, right_upper, mid_upper, uv_top[0], uv_top[1], uv_top[2], uv_top[3], col_top);
        draw_list->AddImageQuad(tex_id, left_upper, mid_mid, mid_lower, left_lower, uv_left[0], uv_left[1], uv_left[2], uv_left[3], col_left);
        draw_list->AddImageQuad(tex_id, right_upper, mid_mid, mid_lower, right_lower, uv_right[0], uv_right[1], uv_right[2], uv_right[3], col_right);
    }

    if (item.quantity == 1 || (!cvr_mc_less_than_one_item_quantities.get() && item.quantity < 0))
        return;

    char buf[16];
    snprintf(buf, SDL_arraysize(buf), "%d", item.quantity);

    mc_gui::add_text(draw_list, pos1 - ImGui::CalcTextSize(buf), buf);
}

static void render_hotbar(mc_gui::mc_gui_ctx* ctx, ImDrawList* draw_list)
{
    const ImVec2 _hotbar_sel_size(24.0f, 24.0f);
    const ImVec2 _hotbar_item_size(16.0f, 16.0f);
    const ImVec2 _hotbar_square_size(20.0f, 20.0f);
    const ImVec2 _hotbar_size = ImVec2 { _hotbar_square_size.x * 9.0f + 2.0f, _hotbar_square_size.y + 2.0f };

    const float pixel = ctx->menu_scale;

    const ImVec2 hotbar_sel_size = _hotbar_sel_size * pixel;
    const ImVec2 hotbar_item_size = _hotbar_item_size * pixel;
    const ImVec2 hotbar_square_size = _hotbar_square_size * pixel;
    const ImVec2 hotbar_size = _hotbar_size * pixel;

    const ImVec2 view_size = ImGui::GetMainViewport()->Size;
    const ImVec2 view_center = view_size / 2.0f;

    /** Highest Y value of the hotbar */
    const float hotbar_upper_y = view_size.y - hotbar_sel_size.y;

    const float column_x_left = view_center.x - hotbar_size.x / 2.0f;
    const float column_x_right = view_center.x + hotbar_size.x / 2.0f;

    inventory_player_t& inv = game_selected->level->inventory;

    /* Hotbar */
    {
        const ImVec2 tsize(_hotbar_size);
        const ImVec2 tpos(0.0f, 0.0f);

        const ImVec2 uv0 = tpos / 256.0f;
        const ImVec2 uv1 = (tsize + tpos) / 256.0f;

        const ImVec2 pos0 {
            (view_size.x - hotbar_size.x) / 2.0f,
            view_size.y - hotbar_size.y - pixel,
        };
        const ImVec2 pos1(pos0.x + hotbar_size.x, view_size.y - pixel);

        draw_list->AddImage(ctx->tex_id_widgets, pos0, pos1, uv0, uv1);
    }

    /* Hotbar selector */
    {
        const ImVec2 tsize(_hotbar_sel_size);
        const ImVec2 tpos(0.0f, 22.0f);

        const ImVec2 uv0 = tpos / 256.0f;
        const ImVec2 uv1 = (tsize + tpos) / 256.0f;

        int hot_bar_pos = inv.hotbar_sel - inv.hotbar_min;

        const ImVec2 pos0 {
            (view_size.x - hotbar_size.x) / 2.0f + hotbar_square_size.x * hot_bar_pos - pixel,
            hotbar_upper_y,
        };
        const ImVec2 pos1(pos0.x + hotbar_sel_size.x, view_size.y);

        draw_list->AddImage(ctx->tex_id_widgets, pos0, pos1, uv0, uv1);
    }

    /* Hotbar items */
    for (int i = inv.hotbar_min; i <= inv.hotbar_max; i++)
    {
        const float hot_bar_sel = i - inv.hotbar_min;

        const ImVec2 pos0 {
            (view_size.x - hotbar_size.x) / 2.0f + hotbar_square_size.x * hot_bar_sel + pixel * 3.0f,
            view_size.y - hotbar_item_size.y - pixel * 4.0f,
        };
        const ImVec2 pos1(pos0 + hotbar_item_size);

        // const float squish = (SDL_cosf(float(SDL_GetTicks() % 628) / 100.0f) + 1.0f) * 0.5f + 1.0f;
        const float squish = 1.0f;

        render_item_stack(draw_list, ctx->menu_scale, pos0, pos1, inv.items[i], ImVec2(1 / SDL_sqrtf(squish), squish), ImVec2(0.5f, 1.0f));

        continue;
    }

    float lowest_y_value_so_far = hotbar_upper_y;

    bool show_survival_widgets = 0;

    switch (game_selected->level->gamemode_get())
    {
    case mc_id::GAMEMODE_SPECTATOR: /* Fall-through */
    case mc_id::GAMEMODE_CREATIVE: /* Fall-through */
        show_survival_widgets = cvr_mc_force_survival_hotbar.get();
        break;
    case mc_id::GAMEMODE_ADVENTURE: /* Fall-through */
    case mc_id::GAMEMODE_SURVIVAL: /* Fall-through */
    default:
        show_survival_widgets = 1;
        break;
    }

    float lowest_y_value_so_far_experience = lowest_y_value_so_far;

    /* Experience bar + Text */
    if (show_survival_widgets)
    {
        lowest_y_value_so_far -= pixel;

        Sint64 xp_level = 0;
        Sint64 xp_total = 0;

        entity_experience_t* xp = game_selected->level->ecs.try_get<entity_experience_t>(game_selected->level->player_eid);

        if (xp)
        {
            xp_level = xp->level;
            xp_total = xp->total;
        }

        if (cvr_mc_hotbar_test.get())
        {
            static Uint64 last_level_change = 0;
            static Sint64 level = SDL_rand_bits();
            if (SDL_GetTicks() - last_level_change > 150)
            {
                last_level_change = SDL_GetTicks();
                level = SDL_rand_bits();
            }
            xp_level = level;
            xp_total = 5 * (xp_level + xp_level * xp_level) + (xp_level + 1) * 10 * ((SDL_GetTicks() >> 8) % 5) / 4;
        }

        Sint64 xp_progress_cur = xp_total - 5 * (xp_level + xp_level * xp_level);
        Sint64 xp_progress_max = xp_level * 10 + 10;

        double percentage = double(xp_progress_cur) / double(xp_progress_max);
        percentage = SDL_clamp(percentage, 0.0, 1.0);

        /* Bar Background */
        const ImVec2 bar_tsize(182, 5);
        const ImVec2 bar_tpos(0, 64);

        const ImVec2 bar_uv0 = bar_tpos / 256.0f;
        const ImVec2 bar_uv1 = (bar_tpos + bar_tsize) / 256.0f;

        const ImVec2 bar_pos0 {
            view_center.x - bar_tsize.x * pixel * 0.5f,
            lowest_y_value_so_far - bar_tsize.y * pixel,
        };
        const ImVec2 bar_pos1 = bar_pos0 + bar_tsize * pixel;

        draw_list->AddImage(ctx->tex_id_icons, bar_pos0, bar_pos1, bar_uv0, bar_uv1);

        /* Bar fill */
        const ImVec2 bar_filled_pos0 = bar_pos0;
        const ImVec2 bar_filled_pos1 { glm::mix(bar_pos0.x, bar_pos1.x, percentage), bar_pos1.y };

        const ImVec2 bar_filled_uv0 = bar_uv0 + ImVec2(0.0f, bar_tsize.y / 256.0f);
        const ImVec2 bar_filled_uv1 { glm::mix(bar_uv0.x, bar_uv1.x, percentage), bar_uv1.y + bar_tsize.y / 256.0f };

        draw_list->AddImage(ctx->tex_id_icons, bar_filled_pos0, bar_filled_pos1, bar_filled_uv0, bar_filled_uv1);

        lowest_y_value_so_far = bar_pos0.y;

        /* Experience Level Text */
        char buf[32] = "";
        snprintf(buf, SDL_arraysize(buf), "%ld", xp_level);
        char* buf_end = buf + strlen(buf);

        const ImVec2 text_size = ImGui::CalcTextSize(buf);
        const ImVec2 cursor = ImVec2(view_center.x, (bar_pos0.y + bar_pos1.y) * 0.5f) - text_size * ImVec2(0.5f, 1.0f);

        ImVec4 _col_text = ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Text));
        _col_text.x *= 0.502f;
        _col_text.z *= 0.125f;

        const Uint32 col_shadow = IM_COL32_BLACK;
        const Uint32 col_text = ImGui::ColorConvertFloat4ToU32(_col_text);

        draw_list->AddText(cursor + ImVec2(+1, +0) * ctx->menu_scale, col_shadow, buf, buf_end);
        draw_list->AddText(cursor + ImVec2(+0, +1) * ctx->menu_scale, col_shadow, buf, buf_end);
        draw_list->AddText(cursor + ImVec2(+0, -1) * ctx->menu_scale, col_shadow, buf, buf_end);
        draw_list->AddText(cursor + ImVec2(-1, +0) * ctx->menu_scale, col_shadow, buf, buf_end);

        draw_list->AddText(cursor + ImVec2(+1, -1) * ctx->menu_scale, col_shadow, buf, buf_end);
        draw_list->AddText(cursor + ImVec2(-1, +1) * ctx->menu_scale, col_shadow, buf, buf_end);
        draw_list->AddText(cursor + ImVec2(-1, -1) * ctx->menu_scale, col_shadow, buf, buf_end);
        draw_list->AddText(cursor + ImVec2(-1, -1) * ctx->menu_scale, col_shadow, buf, buf_end);

        draw_list->AddText(cursor, col_text, buf, buf_end);

        lowest_y_value_so_far_experience = cursor.y - pixel * 2.0f;
    }

    float lowest_y_value_so_far_left = lowest_y_value_so_far;
    float lowest_y_value_so_far_right = lowest_y_value_so_far;

    /* Health bar */
    if (show_survival_widgets)
    {
        bool effect_poison = false;
        bool effect_wither = false;
        bool effect_absorb = false;

        bool effect_hardcore = false;
        bool effect_mounted = false;

        int health_max = 0;
        int health_cur = 0;
        int health_last = 0;

        bool blink = 0;

        entity_health_t* health = game_selected->level->ecs.try_get<entity_health_t>(game_selected->level->player_eid);

        if (health)
        {
            health_max = health->max;
            health_cur = health->cur;
            health_last = health->last;
            blink = health->update_effect_counter / 2 % 2;
        }

        if (cvr_mc_hotbar_test.get())
        {
            float amp = cvr_mc_hotbar_test_intensity.get();
            health_max = (SDL_cosf(float(SDL_GetTicks() % 6500) * SDL_PI_F * 2.0f / 6500.0f) + 0.95f) * 5 * amp + 10;
            health_cur = health_max * (SDL_cosf(float((SDL_GetTicks() / 500 * 500) % 2500) * SDL_PI_F * 2.0f / 2500.0f) + 0.5f);
            health_last = health_cur - ((SDL_GetTicks() / 250) % 3) + 1;
        }

        bool was_updated = health_cur != health_last;
        bool effect_jiggle = health_cur <= 4;

        if (blink)
            was_updated = 0;

        const ImVec2 tadvance(8.0f, 10.0f);
        const ImVec2 tsize_base(9.0f, 9.0f);
        const float background_count = 4;
        ImVec2 tpos_background;
        ImVec2 tpos_fill;

        if (effect_mounted)
            tpos_background = { 52.0f, 9.0f };
        else if (effect_hardcore)
            tpos_background = { 16.0f, 45.0f };
        else
            tpos_background = { 16.0f, 0.0f };

        tpos_fill = tpos_background + ImVec2(tsize_base.x * background_count, 0.0f);

        if (was_updated)
            tpos_background.x += tsize_base.x;

        if (!effect_mounted)
        {
            if (effect_wither)
                tpos_fill.x += tsize_base.x * 4.0f * 2.0f;
            else if (effect_poison)
                tpos_fill.x += tsize_base.x * 4.0f * 1.0f;
            else if (effect_absorb)
                tpos_fill.x += tsize_base.x * 4.0f * 3.0f;
            else /* Normal */
                tpos_fill.x += 0.0f;
        }

        float new_lowest_y_value_so_far_left = lowest_y_value_so_far_left;
        for (int i = 0; i < (health_max + 1) / 2; i++)
        {
            const bool empty = i * 2 >= health_cur;
            const bool empty_missing = i * 2 >= health_last;
            const bool half = (health_cur - i * 2) == 1;
            const bool half_missing = (health_last - i * 2) == 1;

            ImVec2 jiggle(0.0f, 0.0f);
            if (effect_jiggle)
            {
                int period = 200;
                float x = SDL_GetTicks() % period + (i + i / 10) * (period / 3);
                float jpos = SDL_cosf(x * SDL_PI_F * 2.0f / float(period));
                jiggle.y = SDL_roundf(jpos) * pixel;
            }

            ImVec2 pos0(column_x_left, lowest_y_value_so_far_left - tsize_base.y * pixel);
            pos0 += tadvance * pixel * ImVec2(i % 10, -i / 10);
            pos0.y -= pixel * 1.0f;
            pos0 += jiggle;
            ImVec2 pos1 = pos0 + tsize_base * pixel;

            new_lowest_y_value_so_far_left = pos0.y - jiggle.y;

            ImVec2 bg_uv0 = tpos_background / 256.0f;
            ImVec2 bg_uv1 = bg_uv0 + tsize_base / 256.0f;

            ImVec2 fg_uv0 = (tpos_fill + ImVec2(half ? tsize_base.x : 0.0f, 0.0f)) / 256.0f;
            ImVec2 fg_uv1 = fg_uv0 + tsize_base / 256.0f;

            ImVec2 fg_missing_uv0 = (tpos_fill + ImVec2((half_missing ? 3.0f : 2.0f) * tsize_base.x, 0.0f)) / 256.0f;
            ImVec2 fg_missing_uv1 = fg_missing_uv0 + tsize_base / 256.0f;

            draw_list->AddImage(ctx->tex_id_icons, pos0, pos1, bg_uv0, bg_uv1);
            if (!empty_missing)
                draw_list->AddImage(ctx->tex_id_icons, pos0, pos1, fg_missing_uv0, fg_missing_uv1);
            if (!empty)
                draw_list->AddImage(ctx->tex_id_icons, pos0, pos1, fg_uv0, fg_uv1);
        }
        lowest_y_value_so_far_left = new_lowest_y_value_so_far_left;
    }

    /* Food bar */
    if (show_survival_widgets)
    {
        bool effect_poison = false;

        int food_max = 0;
        int food_cur = 0;
        int food_last = 0;

        float food_satur_cur = 0.0f;
        float food_satur_last = 0.0f;

        bool blink = 0;

        entity_food_t* food = game_selected->level->ecs.try_get<entity_food_t>(game_selected->level->player_eid);

        if (food)
        {
            food_max = food->max;
            food_cur = food->cur;
            food_last = food->last;
            food_satur_cur = food->satur_cur;
            food_satur_last = food->satur_last;
            blink = food->update_effect_counter / 2 % 2;
        }

        if (cvr_mc_hotbar_test.get())
        {
            float amp = cvr_mc_hotbar_test_intensity.get();
            food_max = (SDL_cosf(float(SDL_GetTicks() + 4500 % 8500) * SDL_PI_F * 2.0f / 8500.0f) + 0.95f) * 6 * amp + 11;
            food_cur = food_max * (SDL_cosf(float((SDL_GetTicks() & ~0xFF) % 3500) * SDL_PI_F * 2.0f / 3500.0f) + 0.5f);
            food_last = food_cur - ((SDL_GetTicks() & ~0xFF) % 3) + 1;
        }

        bool was_updated = food_cur != food_last || SDL_fabsf(food_satur_cur - food_satur_last) > 0.25f;
        bool effect_jiggle = food_cur <= 4;

        if (blink)
            was_updated = 0;

        const ImVec2 tadvance(8.0f, 10.0f);
        const ImVec2 tsize_base(9.0f, 9.0f);
        const float background_count = 4;
        ImVec2 tpos_background;
        ImVec2 tpos_fill;

        tpos_background = { 16.0f, 27.0f };

        tpos_fill = tpos_background + ImVec2(tsize_base.x * background_count, 0.0f);

        if (was_updated)
            tpos_background.x += tsize_base.x;

        if (effect_poison)
            tpos_fill.x += tsize_base.x * 4.0f * 1.0f;
        else /* Normal */
            tpos_fill.x += 0.0f;

        float new_lowest_y_value_so_far_right = lowest_y_value_so_far_right;
        for (int i = 0; i < (food_max + 1) / 2; i++)
        {
            const bool empty = i * 2 >= food_cur;
            const bool empty_missing = i * 2 >= food_last;
            const bool half = (food_cur - i * 2) == 1;
            const bool half_missing = (food_last - i * 2) == 1;

            ImVec2 jiggle(0.0f, 0.0f);
            if (effect_jiggle)
            {
                int period = 200;
                float x = SDL_GetTicks() % period + (i + i / 10) * (period / 3);
                float jpos = SDL_cosf(x * SDL_PI_F * 2.0f / float(period));
                jiggle.y = SDL_roundf(jpos) * pixel;
            }

            ImVec2 pos0 {
                column_x_right - tsize_base.x * pixel,
                lowest_y_value_so_far_right - tsize_base.y * pixel,
            };
            pos0 += tadvance * pixel * ImVec2(-i % 10, -i / 10);
            pos0.y -= pixel * 1.0f;
            pos0 += jiggle;
            ImVec2 pos1 = pos0 + tsize_base * pixel;

            new_lowest_y_value_so_far_right = pos0.y - jiggle.y;

            ImVec2 bg_uv0 = tpos_background / 256.0f;
            ImVec2 bg_uv1 = bg_uv0 + tsize_base / 256.0f;

            ImVec2 fg_uv0 = (tpos_fill + ImVec2(half ? tsize_base.x : 0.0f, 0.0f)) / 256.0f;
            ImVec2 fg_uv1 = fg_uv0 + tsize_base / 256.0f;

            ImVec2 fg_missing_uv0 = (tpos_fill + ImVec2((half_missing ? 3.0f : 2.0f) * tsize_base.x, 0.0f)) / 256.0f;
            ImVec2 fg_missing_uv1 = fg_missing_uv0 + tsize_base / 256.0f;

            draw_list->AddImage(ctx->tex_id_icons, pos0, pos1, bg_uv0, bg_uv1);
            if (!empty_missing)
                draw_list->AddImage(ctx->tex_id_icons, pos0, pos1, fg_missing_uv0, fg_missing_uv1);
            if (!empty)
                draw_list->AddImage(ctx->tex_id_icons, pos0, pos1, fg_uv0, fg_uv1);
        }
        lowest_y_value_so_far_right = new_lowest_y_value_so_far_right;
    }

    /* Armor bar */
    if (show_survival_widgets)
    {
        int armor_max = 0;
        int armor_cur = 0;

        if (cvr_mc_hotbar_test.get())
        {
            float amp = cvr_mc_hotbar_test_intensity.get();
            armor_max = SDL_cosf(float(SDL_GetTicks() % 6500) * SDL_PI_F * 2.0f / 6500.0f) * 5.0f * amp + 10.0f * amp;
            armor_cur = armor_max * (SDL_cosf(float((SDL_GetTicks() & ~0xFF) % 3750) * SDL_PI_F * 2.0f / 3750.0f) + 1.0f);
            armor_cur -= amp;
            armor_cur /= 2.0f;
        }

        if (armor_cur < 1)
            armor_max = 0;

        bool effect_jiggle = false;

        const ImVec2 tadvance(8.0f, 10.0f);
        const ImVec2 tsize_base(9.0f, 9.0f);
        const float background_count = 1;
        ImVec2 tpos_background;
        ImVec2 tpos_fill;

        tpos_background = { 16.0f, 9.0f };

        tpos_fill = tpos_background + ImVec2(tsize_base.x * background_count, 0.0f);

        tpos_fill.x += 0.0f;

        float new_lowest_y_value_so_far_left = lowest_y_value_so_far_left;
        for (int i = 0; i < (armor_max + 1) / 2; i++)
        {
            const bool empty = i * 2 >= armor_cur;
            const bool half = (armor_cur - i * 2) == 1;

            ImVec2 jiggle(0.0f, 0.0f);
            if (effect_jiggle)
            {
                int period = 200;
                float x = SDL_GetTicks() % period + (i + i / 10) * (period / 3);
                float jpos = SDL_cosf(x * SDL_PI_F * 2.0f / float(period));
                jiggle.y = SDL_roundf(jpos) * pixel;
            }

            ImVec2 pos0(column_x_left, lowest_y_value_so_far_left - tsize_base.y * pixel);
            pos0 += tadvance * pixel * ImVec2(i % 10, -i / 10);
            pos0.y -= pixel * 1.0f;
            pos0 += jiggle;
            ImVec2 pos1 = pos0 + tsize_base * pixel;

            new_lowest_y_value_so_far_left = pos0.y - jiggle.y;

            ImVec2 bg_uv0 = tpos_background / 256.0f;
            ImVec2 bg_uv1 = bg_uv0 + tsize_base / 256.0f;

            ImVec2 fg_uv0 = (tpos_fill + ImVec2(half ? 0.0f : tsize_base.x, 0.0f)) / 256.0f;
            ImVec2 fg_uv1 = fg_uv0 + tsize_base / 256.0f;

            draw_list->AddImage(ctx->tex_id_icons, pos0, pos1, bg_uv0, bg_uv1);
            if (!empty)
                draw_list->AddImage(ctx->tex_id_icons, pos0, pos1, fg_uv0, fg_uv1);
        }
        lowest_y_value_so_far_left = new_lowest_y_value_so_far_left;
    }

    /* Breath bar */
    if (show_survival_widgets)
    {
        int breath_max = 0;
        int breath_cur = 0;
        int breath_last = 0;

        if (cvr_mc_hotbar_test.get())
        {
            float amp = cvr_mc_hotbar_test_intensity.get();
            breath_max = SDL_cosf(float(SDL_GetTicks() % 20500) * SDL_PI_F * 2.0f / 20500.0f) * 5.0f * amp + 10.0f * amp;
            breath_cur = breath_max * (SDL_cosf(float((SDL_GetTicks() & ~0x0F) % 13750) * SDL_PI_F * 2.0f / 13750.0f) + 1.0f);
            breath_cur -= amp;
            breath_cur /= 2.0f;
            breath_last = breath_last * (SDL_cosf(float((SDL_GetTicks() & ~0xFF) % 3750) * SDL_PI_F * 2.0f / 3750.0f) + 1.0f);
            breath_last -= amp;
            breath_last /= 2.0f;
        }

        if (breath_cur >= breath_max)
            breath_max = 0;

        bool effect_jiggle = false;

        const ImVec2 tadvance(8.0f, 10.0f);
        const ImVec2 tsize_base(9.0f, 9.0f);
        const ImVec2 tpos_fill { 16.0f, 18.0f };

        float new_lowest_y_value_so_far_right = lowest_y_value_so_far_right;
        for (int i = 0; i < (breath_max + 1) / 2; i++)
        {
            const bool empty = i * 2 >= breath_cur;
            const bool half = (empty && i * 2 < breath_last) || (breath_cur - i * 2) == 1;

            ImVec2 jiggle(0.0f, 0.0f);
            if (effect_jiggle)
            {
                int period = 200;
                float x = SDL_GetTicks() % period + (i + i / 10) * (period / 3);
                float jpos = SDL_cosf(x * SDL_PI_F * 2.0f / float(period));
                jiggle.y = SDL_roundf(jpos) * pixel;
            }

            ImVec2 pos0 {
                column_x_right - tsize_base.x * pixel,
                lowest_y_value_so_far_right - tsize_base.y * pixel,
            };
            pos0 += tadvance * pixel * ImVec2(-i % 10, -i / 10);
            pos0.y -= pixel * 1.0f;
            pos0 += jiggle;
            ImVec2 pos1 = pos0 + tsize_base * pixel;

            new_lowest_y_value_so_far_right = pos0.y - jiggle.y;

            ImVec2 fg_uv0 = (tpos_fill + ImVec2(half ? tsize_base.x : 0.0f, 0.0f)) / 256.0f;
            ImVec2 fg_uv1 = fg_uv0 + tsize_base / 256.0f;

            if (!empty || half)
                draw_list->AddImage(ctx->tex_id_icons, pos0, pos1, fg_uv0, fg_uv1);
        }
        lowest_y_value_so_far_right = new_lowest_y_value_so_far_right;
    }

    lowest_y_value_so_far = SDL_min(lowest_y_value_so_far, lowest_y_value_so_far_experience);
    lowest_y_value_so_far = SDL_min(lowest_y_value_so_far, lowest_y_value_so_far_right);
    lowest_y_value_so_far = SDL_min(lowest_y_value_so_far, lowest_y_value_so_far_left);

    /* Item Name */
    if (cvr_mc_hotbar_show_name.get())
    {
        const itemstack_t& item_hand = inv.items[inv.hotbar_sel];
        const char* name = mc_id::get_name_from_item_id(item_hand.id, item_hand.damage);

        if (name && item_hand.id != BLOCK_ID_NONE && item_hand.id != BLOCK_ID_AIR)
        {
            ImVec2 text_size = ImGui::CalcTextSize(name);
            lowest_y_value_so_far -= pixel;
            lowest_y_value_so_far -= text_size.y;
            mc_gui::add_text(draw_list, ImVec2(view_center.x - text_size.x / 2.0f, lowest_y_value_so_far), name);
        }
    }
}

static client_menu_return_t do_in_game_menu(mc_gui::mc_gui_ctx* ctx, ImDrawList* draw_list)
{
    client_menu_return_t ret;

    ret.allow_pano = 0;
    ret.allow_world = 1;

    if (!game_selected)
        return ret;

    if (game_selected->connection)
        do_in_game_menu__player_list(ctx, game_selected->connection);

    if (!cvr_mc_gui_mobile_controls.get())
        return ret;
    /* Mobile controls only past this point */

    ImGuiViewport* vprt = ImGui::GetMainViewport();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ctx->menu_scale, ctx->menu_scale));
    ImGui::SetNextWindowPos(vprt->GetCenter() * ImVec2(1, 0), ImGuiCond_Always, ImVec2(0.5, 0));
    ImGui::Begin("Top Buttons", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
    if (mc_gui::button_tiny("F3"))
        cvr_debug_screen.set(!cvr_debug_screen.get());
    ImGui::SameLine();
    if (mc_gui::button_tiny("Menu"))
        ret.name_to_open = "menu.game";
    ImGui::PopStyleVar(2);

    ImGui::End();
    ImGui::PopStyleVar(1);

    /* Horizontal joystick */
    {
        ImVec2 touch0 = vprt->Size * touch_handler.corner_camera_move0;
        ImVec2 touch1 = vprt->Size * touch_handler.corner_camera_move1;
        ImVec2 touch_size = touch1 - touch0;
        ImVec2 touch_center = touch0 + touch_size * 0.5f;
        ImVec2 cursor_size = touch_size * 0.2f;
        draw_list->AddRectFilled(touch_center - cursor_size / 2.f, touch_center + cursor_size / 2.f, IM_COL32(72, 72, 72, 40), ctx->menu_scale * 10.0f);
        draw_list->AddRect(touch0, touch1, IM_COL32(72, 72, 72, 128), ctx->menu_scale * 10.0f, 0, ctx->menu_scale);
        draw_list->AddRectFilled(touch0, touch1, IM_COL32(72, 72, 72, 128), ctx->menu_scale * 10.0f);

        bool a;
        ImVec2 cursor_pos = touch_center + touch_size * ImVec2(0.5f, -0.5f) * touch_handler.get_move_factors(a);
        Uint32 cursor_col = IM_COL32(72, held_ctrl ? 128 : 72, 72, held_ctrl ? 160 : 128);
        draw_list->AddRectFilled(cursor_pos - cursor_size / 2.f, cursor_pos + cursor_size / 2.f, cursor_col, ctx->menu_scale * 10.0f);
    }

    /* Raise/lower slider */
    {
        ImVec2 touch0 = vprt->Size * touch_handler.corner_camera_vert0;
        ImVec2 touch1 = vprt->Size * touch_handler.corner_camera_vert1;
        ImVec2 touch_size = touch1 - touch0;
        ImVec2 touch_center = touch0 + touch_size * 0.5f;
        ImVec2 cursor_size = touch_size * 0.2f;
        cursor_size.x = SDL_min(touch_size.x, SDL_max(cursor_size.x, cursor_size.y));
        draw_list->AddRect(touch0, touch1, IM_COL32(72, 72, 72, 128), ctx->menu_scale * 10.0f, 0, ctx->menu_scale);
        draw_list->AddRectFilled(touch_center - cursor_size / 2.f, touch_center + cursor_size / 2.f, IM_COL32(72, 72, 72, 40), ctx->menu_scale * 10.0f);
        draw_list->AddRectFilled(touch0, touch1, IM_COL32(72, 72, 72, 128), ctx->menu_scale * 10.0f);

        ImVec2 cursor_pos = touch_center + touch_size * ImVec2(0, -0.5f * touch_handler.get_vertical_factor());
        Uint32 cursor_col = IM_COL32(72, 72, 72, 128);
        draw_list->AddRectFilled(cursor_pos - cursor_size / 2.f, cursor_pos + cursor_size / 2.f, cursor_col, ctx->menu_scale * 10.0f);
    }

    return ret;
}

/**
 * Display loading screens
 */
static client_menu_return_t do_loading_menu(mc_gui::mc_gui_ctx* ctx)
{
    client_menu_return_t ret;

    ret.allow_pano = 0;

    if (!game_selected)
    {
        ret.close = 1;
        return ret;
    }

    connection_t* connection = game_selected->connection;
    if (!connection)
    {
        ret.close = 1;
        return ret;
    }

    ret.allow_world = connection->get_in_world();

    if (ret.allow_world)
        return ret;

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 1.0));
    ImGui::Begin("Upper", NULL, ctx->default_win_flags);

    mc_gui::text_translated(game_selected->connection->status_msg.c_str());

    ImGui::End();

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.0));
    ImGui::Begin("Lower", NULL, ctx->default_win_flags);

    ImGui::PushTextWrapPos(ImGui::GetMainViewport()->Size.x * 0.95f);
    mc_gui::text_translated(game_selected->connection->status_msg_sub.c_str());
    ImGui::PopTextWrapPos();

    ImGui::End();

    const char* button_text = NULL;

    switch (connection->loading_button)
    {
    case connection_t::LOADING_BUTTON_NONE:
        button_text = NULL;
        break;
    case connection_t::LOADING_BUTTON_CANCEL:
        button_text = "gui.cancel";
        break;
    case connection_t::LOADING_BUTTON_BACK_TO_MENU:
        button_text = "gui.toMenu";
        break;
    }

    if (button_text)
    {
        ImGui::SetNextWindowPos(get_viewport_centered_lower_quarter(), ImGuiCond_Always, ImVec2(0.5, 0.0));
        ImGui::Begin("menu.gui.cancel", NULL, ctx->default_win_flags);

        if (mc_gui::button_big(button_text))
        {
            for (auto it = games.begin(); it != games.end();)
            {
                if (*it == game_selected)
                {
                    delete *it;
                    game_selected = NULL;
                    it = games.erase(it);
                }
                else
                    it = next(it);
            }

            ret.clear_stack = 1;
        }

        ImGui::End();
    }

    return ret;
}

static client_menu_return_t do_game_menu(mc_gui::mc_gui_ctx* ctx)
{
    client_menu_return_t ret;

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
    ImGui::Begin("Main", NULL, ctx->default_win_flags);

    if (mc_gui::button_big("menu.returnToGame"))
    {
        ret.clear_stack = 1;
        world_has_input = 1;
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + float(40 * ctx->menu_scale) + ImGui::GetStyle().ItemSpacing.y * 2.0f);

    if (mc_gui::button_big("menu.options"))
        ret.name_to_open = "menu.options";

    if (mc_gui::button_big((game_selected && game_selected->connection) ? "menu.disconnect" : "menu.returnToMenu"))
    {
        for (auto it = games.begin(); it != games.end();)
        {
            if (*it == game_selected)
            {
                delete *it;
                game_selected = NULL;
                it = games.erase(it);
            }
            else
                it = next(it);
        }

        ret.clear_stack = 1;
    }

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetMainViewport()->Size.y), ImGuiCond_Always, ImVec2(0.0, 1.0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1, 1) * ctx->menu_scale);
    ImGui::Begin("Bottom Text", NULL, ctx->default_win_flags);
    text_brand_ver();
    ImGui::End();
    ImGui::PopStyleVar();

    return ret;
}

static client_menu_return_t do_menu_options(mc_gui::mc_gui_ctx* ctx)
{
    client_menu_return_t ret;

    menu_title(ctx, "options.title");

    ImGui::SetNextWindowPos(get_viewport_centered_title_bar(), ImGuiCond_Always, ImVec2(0.5, 0.0));
    ImGui::Begin("menu.options", NULL, ctx->default_win_flags);

    if (mc_gui::button_big("options.video"))
        ret.name_to_open = "menu.options.video";

    if (mc_gui::button_big("options.controls"))
        ret.name_to_open = "menu.options.controls";

    if (mc_gui::button_big("options.sounds"))
        ret.name_to_open = "menu.options.sound";

    if (convar_t::dev() && mc_gui::button_big("mcs_b181.reload_resources"))
        reload_resources = 1;

    if (convar_t::dev())
    {
        static bool allow_world = ret.allow_world;
        ImGui::Checkbox("World", &allow_world);
        ret.allow_world = allow_world;
        ImGui::SameLine();

        static bool allow_pano = ret.allow_pano;
        ImGui::Checkbox("Pano", &allow_pano);
        ret.allow_pano = allow_pano;
        ImGui::SameLine();

        static bool allow_dirt = ret.allow_dirt;
        ImGui::Checkbox("Dirt", &allow_dirt);
        ret.allow_dirt = allow_dirt;
    }

    if (!game_selected)
    {
        char temp[17] = "";
        strncpy(temp, cvr_username.get().c_str(), SDL_arraysize(temp));

        const char* translation = mc_gui::get_translation("mcs_b181.username");
        float translation_width = ImGui::CalcTextSize(translation).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        ImGui::SetNextItemWidth(SDL_max(ctx->get_width_large() - translation_width, ctx->get_width_large() / 2));
        if (ImGui::InputText(translation, temp, SDL_arraysize(temp)))
            cvr_username.set(temp);
    }

    ImGui::End();

    menu_done(ctx, ret);

    return ret;
}

static void cvr_button_multi(mc_gui::widget_size_t size, const char* cvr_name, const char* translation_id, std::vector<std::pair<int, const char*>>& id_alts)
{
    convar_int_t* cvr = (convar_int_t*)convar_t::get_convar(cvr_name);
    assert(cvr->get_convar_type() == convar_t::CONVAR_TYPE::CONVAR_TYPE_INT);
    std::string buf = mc_gui::get_translation(translation_id);
    buf += ": ";

    const long min = cvr->get_min();
    const long max = cvr->get_max();
    const long val = cvr->get();
    const long range = max + 1 - min;

    const char* right_hand_id = NULL;
    for (auto it : id_alts)
        if (!right_hand_id && it.first == val)
            right_hand_id = mc_gui::get_translation(it.second);

    if (right_hand_id)
        buf += right_hand_id;
    else
        buf += std::to_string(val);

    ImGuiButtonFlags button_ret = mc_gui::button(size, buf.c_str(), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    if (range)
    {
        long in_range = val - min;

        if (button_ret & ImGuiButtonFlags_MouseButtonLeft)
            in_range++;

        if (button_ret & ImGuiButtonFlags_MouseButtonRight)
            in_range--;

        in_range += range;
        in_range %= range;

        if (button_ret)
            cvr->set(min + in_range);
    }
}

static void cvr_button_boolean(mc_gui::widget_size_t size, const char* cvr_name, const char* translation_id)
{
    static std::vector<std::pair<int, const char*>> ids = { { 0, "options.off" }, { 1, "options.on" } };

    return cvr_button_multi(size, cvr_name, translation_id, ids);
}

static client_menu_return_t do_menu_options_video(mc_gui::mc_gui_ctx* ctx)
{
    client_menu_return_t ret;

    menu_title(ctx, "options.videoTitle");

    ImGui::SetNextWindowPos(get_viewport_centered_title_bar(), ImGuiCond_Always, ImVec2(0.5, 0.0));
    ImGui::Begin("menu.options.video", NULL, ctx->default_win_flags);

    {
        static convar_int_t* cvr = (convar_int_t*)convar_t::get_convar("r_render_distance");
        int cvr_val = cvr->get();
        std::string buf = mc_gui::get_translation("options.renderDistance");
        buf += ": %d";
        ImGui::SetNextItemWidth(ctx->get_width_mid());
        if (ImGui::SliderInt("##rdist", &cvr_val, cvr->get_min(), cvr->get_max(), buf.c_str(), ImGuiSliderFlags_AlwaysClamp))
            cvr->set(cvr_val);
    }

    cvr_button_boolean(mc_gui::SIZE_MID, "r_vsync", "options.vsync");
    static std::vector<std::pair<int, const char*>> cvr_scale_alts = {
        { 0, "options.guiScale.auto" },
        { 1, "options.guiScale.small" },
        { 2, "options.guiScale.normal" },
        { 3, "options.guiScale.large" },
    };

    cvr_button_multi(mc_gui::SIZE_MID, "mc_gui_scale", "options.guiScale", cvr_scale_alts);

    ImGui::End();

    menu_done(ctx, ret);

    return ret;
}

static void im_cvr_slider(mc_gui::mc_gui_ctx* ctx, convar_int_t* cvr, const char* translation_id, int width = 0)
{
    SDL_assert(cvr);
    if (!cvr)
        return;
    int cvr_val = cvr->get();
    std::string buf = mc_gui::get_translation(translation_id);
    buf += ": %d";
    ImGui::SetNextItemWidth((width == 0) ? ctx->get_width_mid() : width);
    ImGui::PushID(translation_id);
    ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, ImGui::GetStyle().FramePadding.y * 2);
    if (ImGui::SliderInt("##slider", &cvr_val, cvr->get_min(), cvr->get_max(), buf.c_str(), ImGuiSliderFlags_AlwaysClamp))
        cvr->set(cvr_val);
    ImGui::PopStyleVar();
    ImGui::PopID();
}

static client_menu_return_t do_menu_options_sound(mc_gui::mc_gui_ctx* ctx)
{
    client_menu_return_t ret;

    menu_title(ctx, "options.sounds.title");

    ImGui::SetNextWindowPos(get_viewport_centered_title_bar(), ImGuiCond_Always, ImVec2(0.5, 0.0));
    ImGui::Begin("menu.options.sound", NULL, ctx->default_win_flags);

    static convar_int_t* cvr_a_volume_master = (convar_int_t*)convar_t::get_convar("a_volume_master");
    static convar_int_t* cvr_a_volume_music = (convar_int_t*)convar_t::get_convar("a_volume_music");
    static convar_int_t* cvr_a_volume_weather = (convar_int_t*)convar_t::get_convar("a_volume_weather");
    static convar_int_t* cvr_a_volume_hostile = (convar_int_t*)convar_t::get_convar("a_volume_hostile");
    static convar_int_t* cvr_a_volume_player = (convar_int_t*)convar_t::get_convar("a_volume_player");
    static convar_int_t* cvr_a_volume_record = (convar_int_t*)convar_t::get_convar("a_volume_record");
    static convar_int_t* cvr_a_volume_blocks = (convar_int_t*)convar_t::get_convar("a_volume_blocks");
    static convar_int_t* cvr_a_volume_neutral = (convar_int_t*)convar_t::get_convar("a_volume_neutral");
    static convar_int_t* cvr_a_volume_ambient = (convar_int_t*)convar_t::get_convar("a_volume_ambient");

    im_cvr_slider(ctx, cvr_a_volume_master, "soundCategory.master", -1);

    im_cvr_slider(ctx, cvr_a_volume_music, "soundCategory.music");
    ImGui::SameLine();
    im_cvr_slider(ctx, cvr_a_volume_weather, "soundCategory.weather");
    im_cvr_slider(ctx, cvr_a_volume_hostile, "soundCategory.hostile");
    ImGui::SameLine();
    im_cvr_slider(ctx, cvr_a_volume_player, "soundCategory.player");
    im_cvr_slider(ctx, cvr_a_volume_record, "soundCategory.record");
    ImGui::SameLine();
    im_cvr_slider(ctx, cvr_a_volume_blocks, "soundCategory.block");
    im_cvr_slider(ctx, cvr_a_volume_neutral, "soundCategory.neutral");
    ImGui::SameLine();
    im_cvr_slider(ctx, cvr_a_volume_ambient, "soundCategory.ambient");

    ImGui::End();

    menu_done(ctx, ret);

    return ret;
}

static client_menu_return_t do_menu_options_controls(mc_gui::mc_gui_ctx* ctx)
{
    client_menu_return_t ret;

    menu_title(ctx, "controls.title");

    ImGui::SetNextWindowPos(get_viewport_centered_title_bar(), ImGuiCond_Always, ImVec2(0.5, 0.0));
    ImGui::Begin("menu.options.controls", NULL, ctx->default_win_flags);

    mc_gui::text_translated("mcs_b181.placeholder");

    ImGui::End();

    menu_done(ctx, ret);

    return ret;
}

static client_menu_return_t do_menu_convars(mc_gui::mc_gui_ctx* ctx)
{
    client_menu_return_t ret;

    ImGuiViewport* viewport = ImGui::GetMainViewport();

    /* Title */
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x / 2.0f, 0), ImGuiCond_Always, ImVec2(0.5, 0.0));
    ImGui::Begin("menu_title", NULL, ctx->default_win_flags);
    mc_gui::text_translated("Convars");
    ImGui::End();

    /* Contents */
    ImVec2 size_min(0, viewport->Size.y - ctx->menu_scale * 25.0f * 2.5f);
    ImVec2 size_max(viewport->Size.x * 0.8f, size_min.y);
    ImGui::SetNextWindowSizeConstraints(size_min, size_max);
    ImGui::SetNextWindowPos(ImVec2(viewport->GetWorkCenter().x, ctx->menu_scale * 25.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, ctx->menu_scale);
    ImGui::Begin("menu.options.controls", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);

    static std::vector<convar_t*> convars;
    if (!convars.size())
    {
        convars.insert(convars.end(), convar_t::get_convar_list()->begin(), convar_t::get_convar_list()->end());
        std::sort(convars.begin(), convars.end(), [](convar_t* const a, convar_t* const b) -> bool { return strcmp(a->get_name(), b->get_name()) < 0; });
    }

    for (auto cvr : convars)
    {
        ImGui::SetNextItemWidth(viewport->Size.x * 0.45f);
        cvr->imgui_edit();
    }

    ImGui::End();
    ImGui::PopStyleVar();

    /* Done button */
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x / 2.0f, viewport->Size.y), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::Begin("menu.gui.done", NULL, ctx->default_win_flags);

    if (mc_gui::button_big("Save convars"))
        convar_file_parser::write();

    ImGui::SameLine();

    if (mc_gui::button_big("gui.done"))
        ret.close = 1;

    ImGui::End();

    return ret;
}

namespace mc_gui
{
/* Mostly copy pasted from imgui_impl_sdlgpu3.cpp */
static void init_mc_gui_shaders()
{
    SDL_assert_always(0 && "Update to Vulkan way of doing things");
#if 0
    ImGui_ImplSDLGPU3_Data* bd = (ImGui_ImplSDLGPU3_Data*)ImGui::GetIO().BackendRendererUserData;
    ImGui_ImplSDLGPU3_InitInfo* v = &bd->InitInfo;

    SDL_GPUVertexBufferDescription vertex_buffer_desc[1];
    vertex_buffer_desc[0].slot = 0;
    vertex_buffer_desc[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertex_buffer_desc[0].instance_step_rate = 0;
    vertex_buffer_desc[0].pitch = sizeof(ImDrawVert);

    SDL_GPUVertexAttribute vertex_attributes[3];
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[0].location = 0;
    vertex_attributes[0].offset = offsetof(ImDrawVert, pos);

    vertex_attributes[1].buffer_slot = 0;
    vertex_attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[1].location = 1;
    vertex_attributes[1].offset = offsetof(ImDrawVert, uv);

    vertex_attributes[2].buffer_slot = 0;
    vertex_attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    vertex_attributes[2].location = 2;
    vertex_attributes[2].offset = offsetof(ImDrawVert, col);

    SDL_GPUVertexInputState vertex_input_state = {};
    vertex_input_state.num_vertex_attributes = 3;
    vertex_input_state.vertex_attributes = vertex_attributes;
    vertex_input_state.num_vertex_buffers = 1;
    vertex_input_state.vertex_buffer_descriptions = vertex_buffer_desc;

    SDL_GPURasterizerState rasterizer_state = {};
    rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    rasterizer_state.enable_depth_bias = false;
    rasterizer_state.enable_depth_clip = false;

    SDL_GPUMultisampleState multisample_state = {};
    multisample_state.sample_count = v->MSAASamples;
    multisample_state.enable_mask = false;

    SDL_GPUDepthStencilState depth_stencil_state = {};
    depth_stencil_state.enable_depth_test = false;
    depth_stencil_state.enable_depth_write = false;
    depth_stencil_state.enable_stencil_test = false;

    SDL_GPUColorTargetBlendState blend_state = {};
    blend_state.enable_blend = true;
    blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

    SDL_GPUColorTargetDescription color_target_desc[1] {};
    color_target_desc[0].format = v->ColorTargetFormat;
    color_target_desc[0].blend_state = blend_state;

    SDL_GPUGraphicsPipelineTargetInfo target_info = {};
    target_info.num_color_targets = SDL_arraysize(color_target_desc);
    target_info.color_target_descriptions = color_target_desc;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = bd->VertexShader;
    pipeline_info.fragment_shader = bd->FragmentShader;
    pipeline_info.vertex_input_state = vertex_input_state;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.rasterizer_state = rasterizer_state;
    pipeline_info.multisample_state = multisample_state;
    pipeline_info.depth_stencil_state = depth_stencil_state;
    pipeline_info.target_info = target_info;

    pipeline_imgui_regular = SDL_CreateGPUGraphicsPipeline(state::gpu_device, &pipeline_info);

    color_target_desc[0].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
    color_target_desc[0].blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;

    pipeline_imgui_crosshair = SDL_CreateGPUGraphicsPipeline(state::gpu_device, &pipeline_info);
#endif
}

static void destroy_mc_gui_shaders()
{
    SDL_ReleaseGPUGraphicsPipeline(state::gpu_device, pipeline_imgui_regular);
    SDL_ReleaseGPUGraphicsPipeline(state::gpu_device, pipeline_imgui_crosshair);
}

static void init()
{
    mc_gui::global_ctx->menu_scale = 1;
    ImGuiContext* last_ctx = ImGui::GetCurrentContext();
    imgui_ctx_main_menu = ImGui::CreateContext();
    {
        ImGui::SetCurrentContext(imgui_ctx_main_menu);
        ImGui::GetIO().IniFilename = NULL;
        global_ctx->load_font_ascii(ImGui::GetIO().Fonts);
        if (!ImGui_ImplSDL3_InitForSDLGPU(state::window))
            util::die("Failed to initialize Dear Imgui SDL3 backend\n");
        ImGui_ImplSDLGPU3_InitInfo init_info = {};
        init_info.Device = state::gpu_device;
        init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(state::gpu_device, state::window);
        init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        if (!ImGui_ImplSDLGPU3_Init(&init_info))
            util::die("Failed to initialize Dear Imgui SDLGPU3 backend\n");

        init_mc_gui_shaders();

        ImGuiStyle& style = ImGui::GetStyle();

        for (int i = 0; i < ImGuiCol_COUNT; i++)
        {
            ImVec4 col = style.Colors[i];
            /* Constants pulled from learnopengl.com's article on framebuffers */
            float gray = col.x * 0.2126f + col.y * 0.7152f + col.z * 0.0722f;
            style.Colors[i] = ImVec4(gray, gray, gray, col.w);
        }

        style.Colors[ImGuiCol_Text] = ImVec4 { 224.0f / 255.0f, 224.0f / 255.0f, 224.0f / 255.0f, 1.0f };

        style.Colors[ImGuiCol_Button].w = 1.0f;
        style.Colors[ImGuiCol_ButtonHovered] = style.Colors[ImGuiCol_Button];
        style.Colors[ImGuiCol_ButtonHovered].z = 0.95f;
        style.Colors[ImGuiCol_ButtonActive] = style.Colors[ImGuiCol_ButtonHovered];
        style.Colors[ImGuiCol_ButtonActive].x *= 0.9f;
        style.Colors[ImGuiCol_ButtonActive].y *= 0.9f;
        style.Colors[ImGuiCol_ButtonActive].z *= 0.9f;
    }
    ImGui::SetCurrentContext(last_ctx);

    static bool manager_initialized = 0;
    if (manager_initialized)
        return;
    manager_initialized = 1;

    client_menu_manager = client_menu_manager_t();

    client_menu_manager.add_menu("in_game", do_in_game_menu);

    client_menu_manager.add_menu("loading", do_loading_menu);

    client_menu_manager.add_menu("menu.game", do_game_menu);
    client_menu_manager.add_menu("menu.title", do_main_menu);
    client_menu_manager.add_menu("menu.convars", do_menu_convars);
    client_menu_manager.add_menu("menu.options", do_menu_options);
    client_menu_manager.add_menu("menu.options.video", do_menu_options_video);
    client_menu_manager.add_menu("menu.options.sound", do_menu_options_sound);
    client_menu_manager.add_menu("menu.options.controls", do_menu_options_controls);
}

static void deinit()
{
    if (!imgui_ctx_main_menu)
        return;

    destroy_mc_gui_shaders();

    ImGuiContext* last_ctx = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(imgui_ctx_main_menu);
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    imgui_ctx_main_menu = NULL;
    ImGui::SetCurrentContext(last_ctx);
}
}

static const glm::ivec2 menu_scale_step = glm::ivec2(320, 240);

static convar_int_t cvr_mc_gui_scale("mc_gui_scale", 0, 0, 4, "", CONVAR_FLAG_SAVE);
