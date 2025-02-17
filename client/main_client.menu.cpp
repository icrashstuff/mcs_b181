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
    std::map<std::string, std::function<client_menu_return_t(mc_gui::mc_gui_ctx* ctx)>> menus;

public:
    void add_menu(std::string name, std::function<client_menu_return_t(mc_gui::mc_gui_ctx* ctx)> func) { menus[name] = func; }

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

    client_menu_return_t run_last_in_stack(glm::ivec2 win_size)
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
            ret = it->second(mc_gui::global_ctx);

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

static client_menu_return_t do_main_menu(mc_gui::mc_gui_ctx* ctx)
{
    client_menu_return_t ret;

    ret.allow_pano = 1;

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5, 0.5));
    ImGui::Begin("Main", NULL, ctx->default_win_flags);

    if (mc_gui::button_big("mcs_b181_client.menu.test_world"))
    {
        games.push_back(new game_t(cvr_autoconnect_addr.get(), cvr_autoconnect_port.get(), cvr_username.get(), true, game_resources));
        ret.clear_stack = 1;
    }

    if (mc_gui::button_big("menu.multiplayer"))
    {
        games.push_back(new game_t(cvr_autoconnect_addr.get(), cvr_autoconnect_port.get(), cvr_username.get(), false, game_resources));
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
    mc_gui::text_translated("mcs_b181_client.mcs_b181_client");
    ImGui::End();
    ImGui::PopStyleVar();

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

    if (connection->get_status() != connection_t::CONNECTION_ACTIVE)
    {
        ImGui::SetNextWindowPos(get_viewport_centered_lower_quarter(), ImGuiCond_Always, ImVec2(0.5, 0.0));
        ImGui::Begin("menu.gui.cancel", NULL, ctx->default_win_flags);

        if (mc_gui::button_big(connection->get_status() < connection_t::CONNECTION_ACTIVE ? "gui.cancel" : "gui.toMenu"))
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
        mouse_grabbed = 1;
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
    mc_gui::text_translated("mcs_b181_client.mcs_b181_client");
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

    if (convar_t::dev() && mc_gui::button_big("mcs_b181_client.reload_resources"))
    {
        deinitialize_resources();
        initialize_resources();
    }

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

        const char* translation = mc_gui::get_translation("mcs_b181_client.username");
        float translation_width = ImGui::CalcTextSize(translation).x;
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

static client_menu_return_t do_menu_options_controls(mc_gui::mc_gui_ctx* ctx)
{
    client_menu_return_t ret;

    menu_title(ctx, "controls.title");

    ImGui::SetNextWindowPos(get_viewport_centered_title_bar(), ImGuiCond_Always, ImVec2(0.5, 0.0));
    ImGui::Begin("menu.options.controls", NULL, ctx->default_win_flags);

    mc_gui::text_translated("mcs_b181_client.placeholder");

    ImGui::End();

    menu_done(ctx, ret);

    return ret;
}

namespace mc_gui
{
static void init()
{
    mc_gui::global_ctx->menu_scale = 1;
    ImGuiContext* last_ctx = ImGui::GetCurrentContext();
    imgui_ctx_main_menu = ImGui::CreateContext();
    {
        ImGui::SetCurrentContext(imgui_ctx_main_menu);
        ImGui::GetIO().IniFilename = NULL;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        if (!ImGui_ImplSDL3_InitForOpenGL(tetra::window, tetra::gl_context))
            util::die("Failed to initialize Dear Imgui SDL3 backend\n");
        if (!ImGui_ImplOpenGL3_Init("#version 330 core"))
            util::die("Failed to initialize Dear Imgui OpenGL3 backend\n");

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

    client_menu_manager = client_menu_manager_t();

    client_menu_manager.add_menu("loading", do_loading_menu);

    client_menu_manager.add_menu("menu.game", do_game_menu);
    client_menu_manager.add_menu("menu.title", do_main_menu);
    client_menu_manager.add_menu("menu.options", do_menu_options);
    client_menu_manager.add_menu("menu.options.video", do_menu_options_video);
    client_menu_manager.add_menu("menu.options.controls", do_menu_options_controls);
}

static void deinit()
{
    if (!imgui_ctx_main_menu)
        return;

    ImGuiContext* last_ctx = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(imgui_ctx_main_menu);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    imgui_ctx_main_menu = NULL;
    ImGui::SetCurrentContext(last_ctx);
}
}

static const glm::ivec2 menu_scale_step = glm::ivec2(320, 240);

static convar_int_t cvr_mc_gui_scale("mc_gui_scale", 0, 0, 4, "", CONVAR_FLAG_SAVE);
