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
#include "inventory.h"

#include "ids.h"
#include "tetra/gui/imgui.h"

#include <SDL3/SDL.h>

void itemstack_t::imgui()
{
    ImGui::BeginDisabled(id == -1);
    ImGui::Text("%d:%d\nx%d", id, damage, quantity);
    ImGui::HelpTooltip(mc_id::get_name_from_item_id(id, damage));
    ImGui::EndDisabled();
}

bool itemstack_t::add_stacks(itemstack_t& a, itemstack_t& b)
{
    if (a != b)
        return false;

    /* If either stack is infinite do nothing */
    if (a.quantity < 0 || b.quantity < 0)
        return false;

    int max_quantity = mc_id::get_max_quantity_for_id(a.damage);

    /* Stack a is already full, do nothing */
    if (a.quantity > max_quantity)
        return false;

    int to_move = SDL_min(b.quantity, max_quantity);

    int max_to_move = max_quantity - a.quantity;

    to_move = SDL_min(to_move, max_to_move);

    if (to_move < 0)
        return false;

    a.quantity += to_move;
    b.quantity -= to_move;

    if (b.quantity == 0)
    {
        b.id = ITEM_ID_NONE;
        b.damage = 0;
    }

    return true;
}

static int inv_item_qsort_compare(void* _sort_descending, const void* _a, const void* _b)
{
    itemstack_t a = *(itemstack_t*)_a;
    itemstack_t b = *(itemstack_t*)_b;
    int sort_descending = (uintptr_t)_sort_descending;

    if (!sort_descending)
    {
        a.id = (a.id == -1) ? SDL_MAX_SINT16 : a.id;
        b.id = (b.id == -1) ? SDL_MAX_SINT16 : b.id;
    }

    if (a < b)
        return sort_descending ? 1 : -1;
    else if (a > b)
        return sort_descending ? -1 : 1;
    else
        return 0;
}

bool itemstack_t::sort_stacks(itemstack_t* items, int start, int end, bool sort_descending)
{
    int nmeb = end - start + 1;

    if (!items || start < 0 || end < 0 || nmeb < 2)
        return false;

    /* If changed is 0, then the array is sorted and the for loop can exit*/
    for (int j = 0, changed = 1; j < nmeb && changed; j++)
    {
        SDL_qsort_r(&items[start], nmeb, sizeof(itemstack_t), inv_item_qsort_compare, (void*)(uintptr_t)sort_descending);

        changed = 0;
        for (int i = 0; i < nmeb - 1; i++)
            changed += add_stacks(items[i + start], items[i + start + 1]);
    }

    return true;
}

void inventory_player_t::imgui()
{
    if (ImGui::BeginTable("Upper Inv", 5))
    {
        float col_width = ImGui::CalcTextSize("255:16").x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        ImGui::TableSetupColumn("col_armor____", ImGuiTableColumnFlags_WidthFixed, col_width * 1.0f);
        ImGui::TableSetupColumn("col_spacing", ImGuiTableColumnFlags_WidthStretch, col_width * 1.0f);
        ImGui::TableSetupColumn("col_offhand__", ImGuiTableColumnFlags_WidthFixed, col_width * 1.0f);
        ImGui::TableSetupColumn("col_craft_in_", ImGuiTableColumnFlags_WidthFixed, col_width * 2.0f);
        ImGui::TableSetupColumn("col_craft_out", ImGuiTableColumnFlags_WidthFixed, col_width * 1.0f);
        ImGui::TableNextRow();

        /*======== Armor ========*/
        ImGui::TableNextColumn();
        if (ImGui::BeginTable("Armor", 1, ImGuiTableFlags_Borders))
        {
            for (int i = armor_min; i <= armor_max; i++)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                items[i].imgui();
            }
            ImGui::EndTable();
        }

        /*======== Spacing ========*/
        ImGui::TableNextColumn();

        /*======== Offhand ========*/
        ImGui::TableNextColumn();
        ImGui::SetCursorPosY(ImGui::GetTextLineHeightWithSpacing() * 7.0f);
        if (ImGui::BeginTable("offhand", 1, ImGuiTableFlags_Borders))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            items[hotbar_offhand].imgui();
            ImGui::EndTable();
        }

        /*======== Crafting Input ========*/
        ImGui::TableNextColumn();
        ImGui::SetCursorPosY(ImGui::GetTextLineHeightWithSpacing() * 3.0f);
        if (ImGui::BeginTable("Crafting In", 2, ImGuiTableFlags_Borders))
        {
            for (int i = crafting_min; i <= crafting_max; i++)
            {
                if ((i - crafting_min) % 2 == 0)
                    ImGui::TableNextRow();
                ImGui::TableNextColumn();
                items[i].imgui();
            }
            ImGui::EndTable();
        }

        /*======== Crafting Output ========*/
        ImGui::TableNextColumn();
        ImGui::SetCursorPosY(ImGui::GetTextLineHeightWithSpacing() * 4.0f);
        if (ImGui::BeginTable("Crafting Out", 1, ImGuiTableFlags_Borders))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            items[crafting_out].imgui();
            ImGui::EndTable();
        }
        ImGui::TableNextColumn();
        ImGui::EndTable();
    }

    if (ImGui::Button("Sort Ascending"))
        sort(0);
    ImGui::SameLine();
    if (ImGui::Button("Sort Descending"))
        sort(1);
    ImGui::SameLine();
    ImGui::Text("These buttons do not synchronize changes!");

    if (ImGui::BeginTable("Lower Inv", 9, ImGuiTableFlags_Borders))
    {
        /*======== Main rows ========*/
        for (int i = main_min; i <= main_max; i++)
        {
            if ((i - main_min) % 9 == 0)
                ImGui::TableNextRow();

            ImGui::TableNextColumn();
            items[i].imgui();
        }

        /*======== Hot bar ========*/
        ImGui::TableNextRow(0, ImGui::GetTextLineHeight());
        for (int i = hotbar_min; i <= hotbar_max; i++)
        {
            if ((i - hotbar_min) % 9 == 0)
                ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if (i == hotbar_sel)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 1.0f, 1.0f));
            items[i].imgui();
            if (i == hotbar_sel)
                ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
}
