/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ian Hangartner <icrashstuff at outlook dot com>
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

#include "gui_registrar.h"
#include "imgui.h"
#include "tetra/util/convar.h"
#include "tetra/util/physfs/physfs.h"

static convar_int_t gui_physfs_browser("gui_physfs_browser", 0, 0, 1, "Display the PhysicsFS (physfs) browser", CONVAR_FLAG_INT_IS_BOOL);

static const ImGuiTreeNodeFlags tree_flags_dir = ImGuiTreeNodeFlags_SpanAllColumns;
static const ImGuiTreeNodeFlags tree_flags_file = tree_flags_dir | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen;

/**
 * Definitely not the most efficient and it might trash a drive but it is simple and this will only be used for diagnostic purposes so it is fine
 */
static void recurse_path(std::string& path, const char* name)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    PHYSFS_Stat stat;
    PHYSFS_stat(path.c_str(), &stat);

    bool is_dir = stat.filetype == PHYSFS_FILETYPE_DIRECTORY;
    bool open = ImGui::TreeNodeEx(name, is_dir ? tree_flags_dir : tree_flags_file) && is_dir;

    ImGui::TableNextColumn();
    if (is_dir)
        ImGui::TextDisabled("--");
    else
        ImGui::Text("%lld", stat.filesize);

    ImGui::TableNextColumn();
    switch (stat.filetype)
    {
    case PHYSFS_FILETYPE_DIRECTORY:
        ImGui::TextUnformatted("Directory");
        break;
    case PHYSFS_FILETYPE_REGULAR:
        ImGui::TextUnformatted("File");
        break;
    case PHYSFS_FILETYPE_SYMLINK:
        ImGui::TextUnformatted("Symlink");
        break;
    case PHYSFS_FILETYPE_OTHER:
        ImGui::TextUnformatted("Other");
        break;
    default:
        ImGui::TextUnformatted("Unknown");
        break;
    }

    ImGui::TableNextColumn();
    ImGui::TextUnformatted(stat.readonly ? "R" : "RW");

    if (open)
    {
        char** rc = PHYSFS_enumerateFiles(path.c_str());
        std::string new_path = path + "/";
        size_t erase_loc = path.size() + 1;
        for (size_t i = 0; rc[i] != NULL && rc[i][0] != '\0'; i++)
        {
            new_path.erase(erase_loc);
            new_path.append(std::string(rc[i]));
            recurse_path(new_path, rc[i]);
        }
        PHYSFS_freeList(rc);
        ImGui::TreePop();
    }
}

/**
 * Displays a tree table representation of the PhysicsFS file structure
 */
static void display_fs(std::string path = "", const char* name = "/")
{
    ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoSavedSettings;

    if (ImGui::BeginTable("physfs_dir_browser", 4, flags))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("1234567890").x);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Directory ").x);
        ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Flags").x);
        ImGui::TableHeadersRow();

        recurse_path(path, name);
        ImGui::EndTable();
    }
}

static bool render_physfs_browser()
{
    if (gui_physfs_browser.get())
    {
        ImGui::SetNextWindowSize(ImVec2(640, 400), ImGuiCond_FirstUseEver);

        if (ImGui::BeginCVR("PhysicsFS browser", &gui_physfs_browser))
        {
            if (ImGui::CollapsingHeader("Info"))
            {
                if (ImGui::TreeNode("Version"))
                {
                    ImGui::BulletText("Compiled against: PhysicsFS v%d.%d.%d", PHYSFS_VER_MAJOR, PHYSFS_VER_MINOR, PHYSFS_VER_PATCH);

                    PHYSFS_Version ver;
                    PHYSFS_getLinkedVersion(&ver);
                    ImGui::BulletText("Linked against: PhysicsFS v%d.%d.%d", ver.major, ver.minor, ver.patch);

                    ImGui::TreePop();
                }

                ImGui::BulletText("Write Dir: \"%s\"", PHYSFS_getWriteDir());

                ImGui::BulletText("Base Dir: \"%s\"", PHYSFS_getBaseDir());

                if (ImGui::TreeNode("Search paths"))
                {
                    char** paths = PHYSFS_getSearchPath();
                    if (*paths == NULL)
                        ImGui::BulletText("Non detected");
                    for (int i = 0; paths[i] != NULL; i++)
                        ImGui::BulletText("[%i]: \"%s\"", i, paths[i]);
                    PHYSFS_freeList(paths);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("CD rom paths"))
                {
                    char** paths = PHYSFS_getCdRomDirs();
                    if (*paths == NULL)
                        ImGui::BulletText("Non detected");
                    for (int i = 0; paths[i] != NULL; i++)
                        ImGui::BulletText("[%i]: \"%s\"", i, paths[i]);
                    PHYSFS_freeList(paths);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Loaded archive drivers"))
                {
                    const PHYSFS_ArchiveInfo** supported_archives = PHYSFS_supportedArchiveTypes();
                    if (*supported_archives == NULL)
                        ImGui::BulletText("Non loaded");
                    for (int i = 0; supported_archives[i] != NULL; i++)
                        ImGui::BulletText("\"%s\" (%s)", supported_archives[i]->extension, supported_archives[i]->description);
                    ImGui::TreePop();
                }
            }
            if (ImGui::CollapsingHeader("Browser", ImGuiTreeNodeFlags_DefaultOpen))
                display_fs("", "/");
        }
        ImGui::End();
    }

    return gui_physfs_browser.get();
}

static gui_register_menu reg_gui(render_physfs_browser);
