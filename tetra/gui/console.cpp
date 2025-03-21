/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) Copyright (c) 2014-2022 Omar Cornut
 * SPDX-FileCopyrightText: Copyright (c) 2022, 2024 Ian Hangartner <icrashstuff at outlook dot com>
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

#include "imgui.h"

// System includes
#include <ctype.h> // toupper
#include <functional> // std::function
#include <limits.h> // INT_MIN, INT_MAX
#include <math.h> // sqrtf, powf, cosf, sinf, floorf, ceilf
#include <stdio.h> // sscanf, printf
#include <stdlib.h> // NULL, malloc, free, atoi
#include <unordered_map> // std::unordered_map
#if defined(_MSC_VER) && _MSC_VER <= 1500 // MSVC 2008 or earlier
#include <stddef.h> // intptr_t
#else
#include <stdint.h> // intptr_t
#endif
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_stdinc.h>
#include <iostream>
#include <mutex>
#include <vector>

#include "tetra/util/stb_sprintf.h"

#include "console.h"
#include "tetra/util/convar.h"

#define VA_BUF_LEN 2048
#define ITEM_COUNT_SHRINK_AT 50000
#define ITEM_COUNT_SHRINK_AMOUNT (ITEM_COUNT_SHRINK_AT / 10)

#define sprintf stbsp_sprintf
#define snprintf stbsp_snprintf
#define vsprintf stbsp_vsprintf
#define vsnprintf stbsp_vsnprintf

#define decode_variadic_to_buffer(BUFFER, FMT)              \
    do                                                      \
    {                                                       \
        va_list args;                                       \
        va_start(args, FMT);                                \
        vsnprintf(BUFFER, IM_ARRAYSIZE(BUFFER), FMT, args); \
        BUFFER[IM_ARRAYSIZE(BUFFER) - 1] = 0;               \
        va_end(args);                                       \
    } while (0)

struct log_item_t
{
    /**
     * SDL Tick, units of 0.001s
     */
    Uint64 time;

    char* str;

    const char* str_fname;

    const char* str_func;

    int line;

    dev_console::log_level_t lvl;

    float line_width;

    int num_lines;

    /**
     * Get the color of the log message associated with lvl
     */
    ImVec4 get_color()
    {
        switch (lvl)
        {
        case dev_console::LEVEL_FATAL:
            return ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
        case dev_console::LEVEL_ERROR:
            return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        case dev_console::LEVEL_WARN:
            return ImVec4(1.0f, 0.4f, 0.2f, 1.0f);
        case dev_console::LEVEL_TRACE:
            return ImVec4(0.2f, 0.4f, 1.0f, 1.0f);
        case dev_console::LEVEL_INTERNAL_CMD:
            return ImVec4(1.0f, 0.8f, 0.6f, 1.0f);
        default:
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    void format_str(char* buf, size_t buf_len)
    {
        const char* lvl_txt;
        switch (lvl)
        {
        case dev_console::LEVEL_WARN:
            lvl_txt = "[warn]";
            break;
        case dev_console::LEVEL_ERROR:
            lvl_txt = "[error]";
            break;
        case dev_console::LEVEL_FATAL:
            lvl_txt = "[fatal]";
            break;
        case dev_console::LEVEL_TRACE:
            lvl_txt = "[trace]";
            break;
        default:
            lvl_txt = NULL;
            break;
        }

        if (lvl < 0)
        {
            size_t slen = strlen(str) + 1;
            memcpy(buf, str, SDL_min(buf_len, slen));
            buf[SDL_min(buf_len, slen) - 1] = '\0';
        }
        else if (lvl_txt)
            snprintf(buf, buf_len, "[%s:%s:%d]%s: %s", str_fname, str_func, line, lvl_txt, str);
        else
            snprintf(buf, buf_len, "[%s:%s:%d]: %s", str_fname, str_func, line, str);
    }
};

// Demonstrate creating a simple console window, with scrolling, filtering, completion and history.
// For the console example, we are using a more C++ like approach of declaring a class to hold both data and functions.
struct AppConsole
{
#define MAX_INPUT_LENGTH 1024
    // #define DEBUG_EXEC_MAPPED_COMMAND
    char InputBuf[MAX_INPUT_LENGTH];
    ImVector<log_item_t> Items;
    ImVector<const char*> commands_vec;
    ImVector<char*> History;
    int HistoryPos; // -1: new line, 0..History.Size-1 browsing history.
    ImGuiTextFilter Filter;
    bool AutoScroll;
    bool ScrollToBottom;
    bool forceReclaimFocus;
    bool console_fullscreen_bool;
    std::mutex mutex_log;

    std::unordered_map<std::string, std::function<int(const int, const char**)>> commands_map;

    AppConsole()
    {
        forceReclaimFocus = false;
        ClearLog();
        memset(InputBuf, 0, sizeof(InputBuf));
        HistoryPos = -1;

        commands_vec.push_back("help");
        commands_vec.push_back("history");
        commands_vec.push_back("echo");

#ifdef DEBUG_EXEC_MAPPED_COMMAND
        AddCommand("c", [=](const int argc, const char** argv) -> int {
            for (int i = 0; i < argc; i++)
                AddLog("DEBUG_EXEC_MAPPED_COMMAND: argv[%d]=\"%s\"", i, argv[i]);
            return 0;
        });
#endif

        AddCommand("clear", [=]() -> int {
            ClearLog();
            return 0;
        });
        AddCommand("_crash_nullptr_dereference", [=]() -> int {
            char* a = nullptr;
            a[0] = 0;

            return 1;
        });
        AddCommand("_crash_stackoverflow", [=]() -> int {
            struct c
            {
                int b = 1;
                void crash()
                {
                    if (b != 0)
                    {
                        crash();
                    }
                }
            };
            c b;
            b.crash();

            return 1;
        });

        AddCommand("_crash_local_convar", [=]() -> int {
            convar_int_t cl_crash_local_convar("cl_crash_local_convar", 0, 0, 0, 0);
            return 1;
        });

        AddCommand("_con_test_returncode", [=](const int argc, const char* argv[]) -> int {
            if (argc > 1)
            {
                return atoi(argv[1]);
            }
            return 1;
        });

        AddCommand("_con_test_log_fatal", [=](const int argc, const char* argv[]) -> int {
            dc_log_fatal("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            return 0;
        });

        AddCommand("_con_test_log_error", [=](const int argc, const char* argv[]) -> int {
            dc_log_error("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            return 0;
        });

        AddCommand("_con_test_log_warn", [=](const int argc, const char* argv[]) -> int {
            dc_log_warn("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            return 0;
        });

        AddCommand("_con_test_log_info", [=](const int argc, const char* argv[]) -> int {
            dc_log("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            return 0;
        });

        AddCommand("_con_test_log_trace", [=](const int argc, const char* argv[]) -> int {
            dc_log_trace("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            return 0;
        });

        AddCommand("_con_test_log_internal", [=](const int argc, const char* argv[]) -> int {
            dc_log_internal("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            return 0;
        });

        AddCommand("_con_test_log_all", [=](const int argc, const char* argv[]) -> int {
            dc_log_fatal("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            dc_log_error("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            dc_log_warn("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            dc_log("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            dc_log_trace("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            dc_log_internal("argc = %d, argv[0] = \"%s\"", argc, argv[0]);
            return 0;
        });

        AddCommand("_con_test_log_multiline_internal", [=](const int argc, const char* argv[]) -> int {
            dc_log_internal("argc = %d, argv[0] = \"%s\"\nline 2\nline 3\nline 4", argc, argv[0]);
            return 0;
        });

        AddCommand("_con_test_log_multiline_warn", [=](const int argc, const char* argv[]) -> int {
            dc_log_warn("argc = %d, argv[0] = \"%s\"\nline 2\nline 3\nline 4", argc, argv[0]);
            return 0;
        });

        AddCommand("_con_test_log_multiline_warn_trailing", [=](const int argc, const char* argv[]) -> int {
            dc_log_warn("argc = %d, argv[0] = \"%s\"\nline 2\nline 3\nline 4\n\n", argc, argv[0]);
            return 0;
        });

        AddCommand("_con_test_log_multiline_error_mult", [=]() -> int {
            char buf[64];
            for (int i = 0; i < IM_ARRAYSIZE(buf); i++)
                buf[i] = (i % 2 == 0) ? 'X' : '\n';
            buf[IM_ARRAYSIZE(buf) - 1] = '\n';
            for (int i = 1; i < IM_ARRAYSIZE(buf); i += 2)
            {
                dc_log_error("Num newlines: %d%.*send", i, i, buf);
            }
            return 0;
        });

        AddCommand("_con_test_log_wall", [=]() -> int {
            for (int i = Items.Size; i > 0; i--)
                dev_console::add_log((dev_console::log_level_t)((i % 7) - 2), "str_fname", "str_func", i, "%d%x%d", i, i, i);
            return 0;
        });

        AutoScroll = true;
        ScrollToBottom = false;
    }
    ~AppConsole()
    {
        ClearLog();
        for (int i = 0; i < History.Size; i++)
            free(History[i]);
    }

    // Portable helpers
    static int Stricmp(const char* s1, const char* s2)
    {
        int d;
        while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1)
        {
            s1++;
            s2++;
        }
        return d;
    }
    static int Strnicmp(const char* s1, const char* s2, int n)
    {
        int d = 0;
        while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1)
        {
            s1++;
            s2++;
            n--;
        }
        return d;
    }
    static char* Strdup(const char* s)
    {
        IM_ASSERT(s);
        size_t len = strlen(s) + 1;
        void* buf = malloc(len);
        IM_ASSERT(buf);
        return (char*)memcpy(buf, (const void*)s, len);
    }
    static void Strtrim(char* s)
    {
        char* str_end = s + strlen(s);
        while (str_end > s && str_end[-1] == ' ')
            str_end--;
        *str_end = 0;
    }

    void ClearLog()
    {
        std::lock_guard<std::mutex> lock(mutex_log);
        for (int i = 0; i < Items.Size; i++)
            free(Items[i].str);
        Items.clear();
    }

    void AddLogQuiet(const char* fmt, ...) IM_FMTARGS(2)
    {
        char buf[VA_BUF_LEN];
        decode_variadic_to_buffer(buf, fmt);

        log_item_t l;
        l.time = SDL_GetTicks();
        l.str = Strdup(buf);
        l.str_fname = __FILE_NAME__;
        l.str_func = __func__;
        l.line = __LINE__;

        if (strstr(buf, "[error]"))
            l.lvl = dev_console::LEVEL_ERROR;
        else if (strstr(buf, "[warn]"))
            l.lvl = dev_console::LEVEL_WARN;
        else if (strstr(buf, "[trace]"))
            l.lvl = dev_console::LEVEL_TRACE;
        else if (strncmp(buf, "# ", 2) == 0)
            l.lvl = dev_console::LEVEL_INTERNAL_CMD;
        else
            l.lvl = dev_console::LEVEL_INTERNAL;

        push_back_log(l, true);
    }

    /**
     * Push back the a log item to Items and if necessary shrink Items
     *
     * Also populates the fields: line_width and num_lines
     *
     * @param l The log item to push back
     * @param quiet Whether to print the log item to the console or not
     */
    void push_back_log(log_item_t l, bool quiet)
    {
        char buf[VA_BUF_LEN];
        l.format_str(buf, IM_ARRAYSIZE(buf));
        l.line_width = SDL_utf8strlen(buf);
        l.num_lines = 0;

        char* lstart = buf;
        char* lend = buf;
        for (char* i = buf; *i != '\0'; i++)
        {
            if (*i != '\n')
                continue;
            lend = i;
            float len = SDL_utf8strnlen(lstart, lend - lstart);
            if (len > l.line_width)
                l.line_width = len;
            lstart = i;
            l.num_lines++;
        }

        float len = SDL_utf8strnlen(lstart, lend - lstart);
        if (len > l.line_width)
            l.line_width = len;

        l.line_width *= dev_console::add_log_font_width;

        if (l.num_lines == 0)
            l.num_lines = 1;

        if (!quiet)
        {
            size_t buf_len = strlen(buf);
            if (buf_len > 0 && buf[buf_len - 1] == '\n')
                printf("%s", buf);
            else
                printf("%s\n", buf);
        }

        std::lock_guard<std::mutex> lock(mutex_log);
        Items.push_back(l);

        if (Items.Size > ITEM_COUNT_SHRINK_AT)
        {
            for (int i = 0; i < ITEM_COUNT_SHRINK_AMOUNT; i++)
                free(Items[i].str);

            memmove(Items.Data, Items.Data + ITEM_COUNT_SHRINK_AMOUNT, (Items.Size - ITEM_COUNT_SHRINK_AMOUNT) * sizeof(Items[0]));
            Items.resize(Items.Size - ITEM_COUNT_SHRINK_AMOUNT);
        }
    }

    void AddLog(const char* fmt, ...) IM_FMTARGS(2)
    {
        char buf[VA_BUF_LEN];
        decode_variadic_to_buffer(buf, fmt);

        log_item_t l;
        l.time = SDL_GetTicks();
        l.str = Strdup(buf);
        l.str_fname = __FILE_NAME__;
        l.str_func = "";
        l.line = -1;

        if (strstr(buf, "[error]"))
            l.lvl = dev_console::LEVEL_ERROR;
        else if (strstr(buf, "[warn]"))
            l.lvl = dev_console::LEVEL_WARN;
        else if (strstr(buf, "[trace]"))
            l.lvl = dev_console::LEVEL_TRACE;
        else if (strncmp(buf, "# ", 2) == 0)
            l.lvl = dev_console::LEVEL_INTERNAL_CMD;
        else
            l.lvl = dev_console::LEVEL_INTERNAL;

        push_back_log(l, false);
    }

    inline void render_item(log_item_t l, float line_height, float line_height_spacing, ImVec4& last_color)
    {
        ImVec2 rect(l.line_width, line_height * (l.num_lines - 1) + line_height_spacing);
        if (ImGui::IsRectVisible(rect))
        {
            char buf[VA_BUF_LEN];

            l.format_str(buf, IM_ARRAYSIZE(buf));
            l.line_width = ImGui::CalcTextSize(buf).x + ImGui::GetStyle().ItemSpacing.x * 2;

            ImVec4 new_color = l.get_color();

            if (new_color != last_color)
            {
                if (last_color.x > -0.5f)
                    ImGui::PopStyleColor();
                ImGui::PushStyleColor(ImGuiCol_Text, new_color);
            }

            last_color = new_color;

            ImGui::TextUnformatted(buf);
        }
        else
        {
            rect.y = line_height * l.num_lines;
            ImGui::Dummy(rect);
        }
    }

    void Draw(const char* title, bool* p_open)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImVec2 pos = viewport->WorkPos;
        pos.x = viewport->WorkSize.x * 0.95;
        pos.y = viewport->WorkSize.y / 2;

        ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.5f));

        ImVec2 size = viewport->WorkSize;
        size.x *= 0.5;
        size.y *= 0.8;
        ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);

        ImGuiWindowFlags window_flags = 0;

        static ImGuiIO& io = ImGui::GetIO();
        (void)io;

        // If for whatever reason it is desirable to have fullscreen console
        if (*p_open == true && console_fullscreen_bool)
        {
            window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowPos(viewport->WorkPos);
        }

        if (!ImGui::Begin(title, p_open, window_flags))
        {
            ImGui::End();
            return;
        }

        // As a specific feature guaranteed by the library, after calling Begin() the last Item represents the title bar.
        // So e.g. IsItemHovered() will return true when hovering the title bar.
        // Here we create a context menu only available from the title bar.
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Close Console"))
                *p_open = false;
            ImGui::EndPopup();
        }

        if (ImGui::Button("Clear"))
        {
            ClearLog();
        }
        ImGui::SameLine();
        bool copy_to_clipboard = ImGui::Button("Copy");

        // ImGui::Separator();
        ImGui::SameLine();

        // Options menu
        if (ImGui::BeginPopup("Options"))
        {
            ImGui::Checkbox("Auto-scroll", &AutoScroll);
            ImGui::EndPopup();
        }

        // Options, Filter
        if (ImGui::Button("Options"))
            ImGui::OpenPopup("Options");
        ImGui::SameLine();
        Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
        ImGui::SameLine();
        ImGui::Text("| %d entries", Items.Size);
        ImGui::Separator();

        // Reserve enough left-over height for 1 separator + 1 input text
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::Selectable("Clear"))
                ClearLog();
            ImGui::EndPopup();
        }

        // Display every line as a separate entry so we can cconst char* cmd = command_line;
        // Change their color or add custom widgets.
        // If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
        // NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping
        // to only process visible items. The clipper will automatically measure the height of your first item and then
        // "seek" to display only items in the visible area.
        // To use the clipper we can replace your standard loop:
        //      for (int i = 0; i < Items.Size; i++)
        //   With:
        //      ImGuiListClipper clipper;
        //      clipper.Begin(Items.Size);
        //      while (clipper.Step())
        //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        // - That your items are evenly spaced (same height)
        // - That you have cheap random access to your elements (you can access them given their index,
        //   without processing all the ones before)
        // You cannot this code as-is if a filter is active because it breaks the 'cheap random-access' property.
        // We would need random-access on the post-filtered list.
        // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices
        // or offsets of items that passed the filtering test, recomputing this array when user changes the filter,
        // and appending newly elements as they are inserted. This is left as a task to the user until we can manage
        // to improve this example code!
        // If your items are of variable height:
        // - Split them into same height items would be simpler and facilitate random-seeking into your list.
        // - Consider using manual call to IsRectVisible() and skipping extraneous decoration from your items.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
        if (copy_to_clipboard)
            ImGui::LogToClipboard();

        {
            std::lock_guard<std::mutex> lock(mutex_log);

            float line_height = ImGui::GetTextLineHeight();
            float line_height_spacing = ImGui::GetTextLineHeightWithSpacing();

            ImVec4 last_color(-1, -1, -1, -1);

            for (int i = 0, j = 2; i < Items.Size; i++, j++)
            {
                const char* item = Items[i].str;
                if (!Filter.PassFilter(item))
                    continue;
                render_item(Items[i], line_height, line_height_spacing, last_color);
            }

            if (last_color.x > -0.5f)
                ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(10, (line_height + line_height_spacing) / 5.0f));
        }

        if (copy_to_clipboard)
            ImGui::LogFinish();

        if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
            ImGui::SetScrollHereY(1.0f);
        ScrollToBottom = false;

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::Separator();

        // Command-line
        bool reclaim_focus = false;
        ImGuiInputTextFlags input_text_flags
            = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
        input_text_flags |= ImGuiInputTextFlags_CallbackEdit;

        int line_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(line_width - ImGui::CalcTextSize("Input  ").x);
        if (ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags, &TextEditCallbackStub, (void*)this))
        {
            char* s = InputBuf;
            Strtrim(s);
            if (s[0])
                ExecCommand(s);
            strcpy(s, "");
            reclaim_focus = true;
        }
        // Auto-focus on window apparition
        ImGui::SetItemDefaultFocus();
        if (reclaim_focus || forceReclaimFocus)
        {
            ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
            forceReclaimFocus = false;
        }
        ImGui::End();
    }

    void draw_overlay(const char* title, dev_console::log_level_t max_lvl)
    {
        std::vector<int> filter_items;
        filter_items.reserve(12);

        std::lock_guard<std::mutex> lock(mutex_log);
        Uint64 sdl_tick_cur = SDL_GetTicks();
        int num_lines = 0;
        for (int i = Items.Size - 1; i >= 0; i--)
        {
            Uint64 tdiff = sdl_tick_cur - Items[i].time;
            if (tdiff >= 2500 && (tdiff > 7500 || num_lines >= 8))
            {
                i = -1;
                continue;
            }
            if (tdiff < 2500 && (tdiff > 2500 || num_lines >= 12))
            {
                i = -1;
                continue;
            }

            if (Items[i].lvl < 0 || Items[i].lvl > max_lvl)
                continue;

            num_lines += Items[i].num_lines;
            filter_items.push_back(i);
        }

        if (!filter_items.size())
            return;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration;
        window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
        window_flags |= ImGuiWindowFlags_NoSavedSettings;
        window_flags |= ImGuiWindowFlags_NoFocusOnAppearing;
        window_flags |= ImGuiWindowFlags_NoNav;
        window_flags |= ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoInputs;

        if (dev_console::overlay_font)
            ImGui::PushFont(dev_console::overlay_font);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

        if (!ImGui::Begin(title, NULL, window_flags))
        {
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor();
            if (dev_console::overlay_font)
                ImGui::PopFont();
            ImGui::End();
            return;
        }

        float line_height = ImGui::GetTextLineHeightWithSpacing();
        float line_height_spacing = ImGui::GetTextLineHeightWithSpacing();

        ImVec4 last_color(-1, -1, -1, -1);

        for (int i = filter_items.size() - 1; i >= 0; i--)
            render_item(Items[filter_items[i]], line_height, line_height_spacing, last_color);

        if (last_color.x > -0.5f)
            ImGui::PopStyleColor();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
        if (dev_console::overlay_font)
            ImGui::PopFont();
        ImGui::End();
    }

    void AddCommand(const char* commandName, std::function<int(const int, const char**)> func)
    {
        auto mit = commands_map.find(commandName);
        if (mit == commands_map.end())
        {
            commands_vec.push_back(commandName);
            commands_map.insert(std::make_pair(commandName, func));
        }
    }

    void AddCommand(const char* commandName, std::function<int()> func)
    {
        auto mit = commands_map.find(commandName);
        if (mit == commands_map.end())
        {
            commands_vec.push_back(commandName);
            commands_map.insert(std::make_pair(commandName, [=](const int, const char*[]) -> int { return func(); }));
        }
    }

    void ExecCommand(const char* command_line, bool quiet = false)
    {
        int errorCode = 0;
        if (!quiet)
            AddLog("# %s\n", command_line);

        // Insert into history. First find match and delete it so it can be pushed to the back.
        // This isn't trying to be smart or optimal.
        HistoryPos = -1;
        for (int i = History.Size - 1; i >= 0; i--)
            if (Stricmp(History[i], command_line) == 0)
            {
                free(History[i]);
                History.erase(History.begin() + i);
                break;
            }
        History.push_back(Strdup(command_line));

        std::string command_line_str(command_line);
        // Process command
        if (Stricmp(command_line, "help") == 0)
        {
            AddLog("Commands:");
            for (int i = 0; i < commands_vec.Size; i++)
                if (commands_vec[i][0] != '_')
                    AddLog("- %s", commands_vec[i]);
            AddLog("Convars:");
            auto convar_list = convar_t::get_convar_list();
            for (size_t i = 0; i < convar_list->size(); i++)
            {
                convar_t* cvr = convar_list->at(i);
                CONVAR_FLAGS cvr_flags = cvr->get_convar_flags();
                if (!(cvr_flags & CONVAR_FLAG_HIDDEN) && !(!convar_t::dev() && (cvr_flags & CONVAR_FLAG_DEV_ONLY)))
                    AddLog("- %s", cvr->get_name());
            }
        }
        else if (Stricmp(command_line, "history") == 0)
        {
            int first = History.Size - 10;
            for (int i = first > 0 ? first : 0; i < History.Size; i++)
                AddLog("%3d: %s\n", i, History[i]);
        }
        else if (command_line_str.rfind("echo", 3) == 0)
        {
            if (command_line_str.size() > 4)
                AddLog("%s", &command_line[5]);
            else
                AddLog("\n");
        }
        else
        {
            size_t command_cur_pos = 0;
            size_t command_split_pos = 0;
            size_t command_line_len = strlen(command_line);
            do
            {
                size_t command_discarded_pos = 0;
                int emc_r = ExecMappedCommand(&command_line[command_cur_pos], command_discarded_pos, command_split_pos, errorCode);
                command_cur_pos += command_discarded_pos;
                switch (emc_r)
                {
                case 0:
                    break;
                case 1:
                    if (command_split_pos)
                        dc_log_error("Unknown command: '%.*s'\n", (int)command_split_pos, &command_line[command_cur_pos]);
                    else
                        dc_log_error("Unknown command: '%s'\n", &command_line[command_cur_pos]);
                    break;
                case 2:
                    dc_log_error("Input number of arguments\n");
                    break;
                case 4:
                    dc_log_error("Unterminated quote\n");
                    break;
                case 5:
                    dc_log_error("No matching function for convar type\n");
                    break;
                default:
                    dc_log_error("An unknown error %d occurred in ExecMappedCommand", emc_r);
                    break;
                }
                if (errorCode != 0)
                {
                    if (command_split_pos)
                        dc_log_error(
                            "Command: '%.*s' exited with nonzero exit code of %d\n", (int)command_split_pos, &command_line[command_cur_pos], errorCode);
                    else
                        dc_log_error("Command: '%s' exited with nonzero exit code of %d\n", &command_line[command_cur_pos], errorCode);
                }
                ScrollToBottom = !quiet;
                command_cur_pos += command_split_pos;
            } while (command_split_pos != 0 && command_cur_pos <= command_line_len);
        }

        // On command input, we scroll to bottom even if AutoScroll==false
        ScrollToBottom = !quiet;
    }

    /*
     * Parses command_line into argc and argv using custom parser and runs the command if in the
     * command map
     *
     * Supports quotes and semicolons
     *
     * FIXME-OPT: This is an extremely ugly and extremely fragile piece of code that probably contains some sort of vulnerability
     */
    int ExecMappedCommand(const char* command_line, size_t& command_discarded_pos, size_t& command_split_pos, int& errorCode)
    {
        size_t CMDL_len = strlen(command_line);
        if (CMDL_len < 1)
            return 0;

        size_t CMDL_argv_size = 4096;
        size_t CMDL_overrun_size = 64;
        size_t argc = 0;
        char* argv_buffer = (char*)SDL_calloc(CMDL_overrun_size + CMDL_argv_size + CMDL_len, sizeof(char));
        char** argv = (char**)SDL_calloc(CMDL_overrun_size + CMDL_argv_size, sizeof(char*));

        // dc_log_trace("command_line: \"%s\"\n",command_line);
        // dc_log_trace("command_line_len: %lu\n",CMDL_len);

        size_t argv_array_i = 0;
        size_t argv_buffer_pos_start = 0;
        size_t argv_buffer_pos_cur = 0;
        char last_char = '\0';
        char cur_char = '\0';
        char next_char = '\0';
        bool in_quote = 0;
        command_split_pos = 0;
        // dc_log_trace("command_line: \"%s\"\n",command_line);
        for (size_t i = 0; i < CMDL_len; i++)
        {
#ifdef DEBUG_EXEC_MAPPED_COMMAND
#define switchf_c(char_case)                                                                                                                            \
    dc_log_trace("case     \'%c\': last_char=\'%c\' cur_char=\'%c\' in_quote=\'%d\' i=%02ld argv_buffer_pos_cur=%02ld argv_array_i=%02ld\n", char_case, \
        last_char, cur_char, in_quote, i, argv_buffer_pos_cur, argv_array_i)
#define switchf_d()                                                                                                                                            \
    dc_log_trace("case default: last_char=\'%c\' cur_char=\'%c\' in_quote=\'%d\' i=%02ld argv_buffer_pos_cur=%02ld argv_array_i=%02ld\n", last_char, cur_char, \
        in_quote, i, argv_buffer_pos_cur, argv_array_i)
#else
#define switchf_c(char_case)
#define switchf_d(char_case)
#endif
#define switch_default_op()                            \
    do                                                 \
    {                                                  \
        argv_buffer[argv_buffer_pos_cur++] = cur_char; \
        last_char = cur_char;                          \
    } while (0)

            if (argv_array_i > 4096)
                return 2;
            cur_char = command_line[i];
            if (i < (CMDL_len - 1))
                next_char = command_line[i];
            else
                next_char = '\0';
            switch (cur_char)
            {
            case ';':
                switchf_c(';');
                if (in_quote)
                    switch_default_op();
                else if (argv_buffer_pos_cur == 0)
                {
                    command_discarded_pos++;
                    dc_log_trace("Semicolon at start of buffer, discarding...");
                }
                else
                {
                    // Crude way of splitting commands
                    command_split_pos = i;
                    i = CMDL_len - 1;
                    dc_log_trace("Semicolon outside of quote, parsing stopped, executing command");
                }
                break;
            case '\0':
                break;
            case '"':
                switchf_c('"');
                if (last_char != '\\')
                {
                    in_quote = !in_quote;
                }
                else
                {
                    if (argv_buffer_pos_cur > 0)
                    {
                        argv_buffer_pos_cur--;
                        // argv_buffer[argv_buffer_pos_cur++] = '\0';
                    }
                    switch_default_op();
                }
                break;
            case '\'':
                switchf_c('\'');
                argv_buffer[argv_buffer_pos_cur] = cur_char;
                last_char = cur_char;
                break;
            case ' ':
                switchf_c(' ');
                if (argv_buffer_pos_cur == 0)
                {
                    command_discarded_pos++;
                    dc_log_trace("Space at start of buffer discarding");
                }
                else if (!in_quote)
                {
                    if (last_char == ' ') /*argv_buffer_pos_cur = argv_buffer_pos_cur - 1*/
                        ;
                    else if (last_char != '\0' || last_char != ' ' || next_char != '\0')
                    {
                        // argv_buffer[argv_buffer_pos_cur++] = '\0';
                        argv[argv_array_i++] = &argv_buffer[argv_buffer_pos_start];
                        argv_buffer_pos_start = ++argv_buffer_pos_cur;
                        // AddLog("%d: %s",__LINE__,argv[argv_array_i-1]);
                        cur_char = '\0';
                    }
                    last_char = ' ';
                }
                else
                {
                    switch_default_op();
                }
                break;
            default:
                switchf_d();
                // if(last_char == '\\' && cur_char == '\\')
                switch_default_op();
                break;
            }
        }
        argv[argv_array_i] = &argv_buffer[argv_buffer_pos_start];
        if (in_quote)
            return 4;
        argc = argv_array_i + 1;

#ifdef DEBUG_EXEC_MAPPED_COMMAND
        dc_log_trace("Parsing loop done, argc=%lu", argc);
        for (size_t i = 0; i < argc; i++)
        {
            dc_log_trace("argv[%ld]=%p\n", i, (void*)argv[i]);
            if (argv[i] != NULL)
                dc_log_trace("argv[%ld]=\"%s\"\n", i, argv[i]);
        }

        dc_log_trace("BEGIN argv_buffer_dump\n");
        for (size_t i = 0; i < CMDL_overrun_size + CMDL_argv_size + CMDL_len; i++)
        {
            char c = argv_buffer[i];
            if (c != '\\' && (32 <= c && c <= 126))
                putc(c, stderr);
            switch (c)
            {
            case '\0':
                fprintf(stderr, "\\0");
                break;
            case '\\':
                fprintf(stderr, "\\");
                break;
            default:
                break;
            }
        }
        fprintf(stderr, "\n");
        fflush_unlocked(stderr);
        dc_log_trace("END   argv_buffer_dump\n");

        dc_log_trace("Command to execute: \"%s\"\n", argv[0]);
#endif

        int r = 1;
        if (argv[0] != NULL)
        {
            std::string stringy((const char*)argv[0]);

            if (argc == 1 && argv[0][0] == '\0')
            {
#ifdef DEBUG_EXEC_MAPPED_COMMAND
                dc_log_trace("Empty command, skipping");
#endif
                errorCode = 0;
                r = 0;
            }
            else
            {
                auto mit = commands_map.find(stringy);
                if (mit != commands_map.end())
                {
                    errorCode = mit->second(argc, (const char**)argv);
                    r = 0;
                }
                else
                {
                    convar_t* cvr = convar_t::get_convar(argv[0]);
                    if (cvr)
                    {
                        errorCode = cvr->convar_command(argc, (const char**)argv);
                        r = 0;
                    }
                }
            }
        }

        SDL_free(argv_buffer);
        SDL_free(argv);
        return r;
    }

    // In C++11 you'd be better off using lambdas for this sort of forwarding callbacks
    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data)
    {
        AppConsole* console = (AppConsole*)data->UserData;
        return console->TextEditCallback(data);
    }

    int TextEditCallback(ImGuiInputTextCallbackData* data)
    {
        // AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
        static bool skip_next_tab = 1;
        switch (data->EventFlag)
        {
        case ImGuiInputTextFlags_CallbackEdit:
        {
            skip_next_tab = 1;
            break;
        }
        case ImGuiInputTextFlags_CallbackCompletion:
        {
            // Example of TEXT COMPLETION

            // Locate beginning of current word
            const char* word_end = data->Buf + data->CursorPos;
            const char* word_start = word_end;
            while (word_start > data->Buf)
            {
                const char c = word_start[-1];
                if (c == ' ' || c == '\t' || c == ',' || c == ';')
                    break;
                word_start--;
            }

            // Build a list of candidates
            ImVector<const char*> candidates;
            for (int i = 0; i < commands_vec.Size; i++)
            {
                if (commands_vec[i][0] == '_' && word_start[0] != '_')
                    continue;
                if (Strnicmp(commands_vec[i], word_start, (int)(word_end - word_start)) == 0)
                    candidates.push_back(commands_vec[i]);
            }

            auto convar_list = convar_t::get_convar_list();
            for (size_t i = 0; i < convar_list->size(); i++)
            {
                convar_t* cvr = convar_list->at(i);
                CONVAR_FLAGS cvr_flags = cvr->get_convar_flags();
                if ((cvr_flags & CONVAR_FLAG_HIDDEN) || (!convar_t::dev() && (cvr_flags & CONVAR_FLAG_DEV_ONLY)))
                    continue;
                if (Strnicmp(cvr->get_name(), word_start, (int)(word_end - word_start)) == 0)
                    candidates.push_back(cvr->get_name());
            }

            if (candidates.Size == 0)
            {
                // No match
                AddLog("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
            }
            else if (candidates.Size == 1)
            {
                // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                data->InsertChars(data->CursorPos, candidates[0]);
                data->InsertChars(data->CursorPos, " ");
            }
            else
            {
                // Multiple matches. Complete as much as we can..
                // So inputing "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
                int match_len = (int)(word_end - word_start);
                for (;;)
                {
                    int c = 0;
                    bool all_candidates_matches = true;
                    for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
                        if (i == 0)
                            c = toupper(candidates[i][match_len]);
                        else if (c == 0 || c != toupper(candidates[i][match_len]))
                            all_candidates_matches = false;
                    if (!all_candidates_matches)
                        break;
                    match_len++;
                }

                if (match_len > 0)
                {
                    data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                    data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
                }

                if (skip_next_tab)
                {
                    AddLogQuiet("Press TAB again to view possible matches (%d)\n", candidates.size());
                    skip_next_tab = false;
                }
                else
                {
                    skip_next_tab = true;
                    // List matches
                    AddLogQuiet("Possible matches:\n");
                    for (int i = 0; i < candidates.Size; i++)
                        AddLogQuiet("- %s\n", candidates[i]);
                }
            }

            break;
        }
        case ImGuiInputTextFlags_CallbackHistory:
        {
            // Example of HISTORY
            const int prev_history_pos = HistoryPos;
            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (HistoryPos == -1)
                    HistoryPos = History.Size - 1;
                else if (HistoryPos > 0)
                    HistoryPos--;
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (HistoryPos != -1)
                    if (++HistoryPos >= History.Size)
                        HistoryPos = -1;
            }

            // A better implementation would preserve the data on the current input line along with cursor position.
            if (prev_history_pos != HistoryPos)
            {
                const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, history_str);
            }
        }
        }
        return 0;
    }
};

#include "gui_registrar.h"

static AppConsole _devConsole;

bool dev_console::shown = false;

void dev_console::show_hide()
{
    if ((shown = !shown) == true)
        _devConsole.forceReclaimFocus = true;
}

static convar_int_t console_fullscreen("console_fullscreen", false, false, true, "Make console fill the whole work area", CONVAR_FLAG_INT_IS_BOOL);

void dev_console::render()
{
    if (shown)
    {
        _devConsole.console_fullscreen_bool = console_fullscreen.get();
        _devConsole.Draw("Developer Console", &shown);
    }
}

static convar_int_t console_overlay(
    "console_overlay", 0, 0, 4, "Dev console overlay console level [0: Fatal, 1: Error, 2: Warn, 3: Info, 4: Trace]", CONVAR_FLAG_DEV_ONLY);

static bool draw_console_overlay()
{
    dev_console::log_level_t max_lvl;

    switch (console_overlay.get())
    {
    case 0:
        max_lvl = dev_console::LEVEL_FATAL;
        break;
    case 1:
        max_lvl = dev_console::LEVEL_ERROR;
        break;
    case 2:
        max_lvl = dev_console::LEVEL_WARN;
        break;
    case 3:
        max_lvl = dev_console::LEVEL_INFO;
        break;
    case 4:
        max_lvl = dev_console::LEVEL_TRACE;
        break;
    default:
        return false;
    }

    _devConsole.draw_overlay("Developer Overlay", max_lvl);

    return true;
}

static gui_register_overlay reg_console_overlay(draw_console_overlay);

void dev_console::add_command(const char* name, std::function<int(const int, const char**)> func) { _devConsole.AddCommand(name, func); }
void dev_console::add_command(const char* name, std::function<int()> func) { _devConsole.AddCommand(name, func); }

void dev_console::run_command(const char* fmt, ...)
{
    char buf[MAX_INPUT_LENGTH];
    decode_variadic_to_buffer(buf, fmt);
    _devConsole.ExecCommand(buf, true);
}

void dev_console::add_log(dev_console::log_level_t lvl, const char* fname, const char* func, int line, const char* fmt, ...)
{
    char buf[VA_BUF_LEN];
    decode_variadic_to_buffer(buf, fmt);

    log_item_t l;
    l.time = SDL_GetTicks();
    l.str = _devConsole.Strdup(buf);
    l.lvl = lvl;
    l.str_fname = fname;
    l.str_func = func;
    l.line = line;

    _devConsole.push_back_log(l, false);
}
