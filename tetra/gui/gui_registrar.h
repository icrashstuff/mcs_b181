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
#ifndef MCS_B181_TETRA_UTIL_GUI_REGISTRAR_H
#define MCS_B181_TETRA_UTIL_GUI_REGISTRAR_H
#include <functional>

/**
 * Interface for adding ImGui menus and overlays without having to add additional functions calls to main.cpp
 *
 * Probably should only be used for misc windows that a normal person won't see
 *
 * TODO-OPT: This might be a bad idea, consider removing this
 */
struct gui_registrar
{
    /**
     * Adds a function to internal overlays array, to be rendered whenever render_overlays() is called
     *
     * @param func Function to render overlay, this must return true if a window was rendered and false otherwise
     */
    static void add_overlay(bool (*func)());

    /**
     * Adds a function to internal menus array, to be rendered whenever render_menus() is called
     *
     * @param func Function to render menu, this must return true if a window was rendered and false otherwise
     */
    static void add_menu(bool (*func)());

    /**
     * Render all registered overlays
     *
     * Returns true if any of the overlay functions return true
     */
    static bool render_overlays();

    /**
     * Render all registered menus
     *
     * Returns true if any of the menu functions return true
     */
    static bool render_menus();
};

/**
 * Repackaged gui_registrar::add_overlay()
 */
struct gui_register_overlay : gui_registrar
{
    gui_register_overlay(bool (*func)()) { add_overlay(func); }
};

/**
 * Repackaged gui_registrar::add_menu()
 */
struct gui_register_menu : gui_registrar
{
    gui_register_menu(bool (*func)()) { add_menu(func); }
};

#endif
