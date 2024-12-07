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
#include "convar.h"
#include "cli_parser.h"
#include "misc.h"
#include "tetra/gui/console.h"
#include "tetra/gui/imgui.h"

#include <SDL3/SDL_assert.h>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <limits.h>

std::vector<convar_t*>* convar_t::get_convar_list()
{
    static std::vector<convar_t*> _vector;
    return &_vector;
}

convar_t* convar_t::get_convar(const char* name)
{
    std::vector<convar_t*>* vector = convar_t::get_convar_list();

    for (size_t i = 0; i < vector->size(); i++)
    {
        if (strcmp(name, vector->at(i)->get_name()) == 0)
        {
            return vector->at(i);
        }
    }

    return NULL;
}

bool convar_t::_atexit = true;

void convar_t::atexit_callback() { _atexit = true; }

void convar_t::atexit_init() { _atexit = false; }

convar_t::~convar_t()
{
    if (!_atexit)
    {
        util::die("An attempt was made to delete convar \"%s\" before atexit conditions\n"
                  "This is a bug (a very critical one)\n"
                  "Program will now exit!",
            _name);
    }
}

static bool check_if_convar_exists(const char* _name)
{
    int exists = convar_t::get_convar(_name) != NULL;
#ifndef NDEBUG
    if (exists == 1)
        util::die("Duplicate convar \"%s\"\n", _name);
#endif
    return exists;
}

convar_int_t::convar_int_t(const char* name, int default_value, int min, int max, const char* help_string, CONVAR_FLAGS flags, std::function<void()> func)
{
    if (min < max)
    {
        _bounded = 1;
        _min = min;
        _max = max;
    }
    _default = default_value;
    _callback = func;
    _value = _default;
    _name = name;
    _help_string = help_string;
    _flags = flags;
    _type = CONVAR_TYPE::CONVAR_TYPE_INT;
    check_if_convar_exists(_name);
    convar_t::get_convar_list()->push_back(this);
    cli_parser::apply_to(this);
}

convar_float_t::convar_float_t(
    const char* name, float default_value, float min, float max, const char* help_string, CONVAR_FLAGS flags, std::function<void()> func)
{
    if (min < max)
    {
        _bounded = 1;
        _min = min;
        _max = max;
    }
    _default = default_value;
    _callback = func;
    _value = _default;
    _name = name;
    _help_string = help_string;
    _flags = flags;
    _type = CONVAR_TYPE::CONVAR_TYPE_FLOAT;
    check_if_convar_exists(_name);
    convar_t::get_convar_list()->push_back(this);
    cli_parser::apply_to(this);
}

convar_string_t::convar_string_t(const char* name, std::string default_value, const char* help_string, CONVAR_FLAGS flags, std::function<void()> func)
{
    _default = default_value;
    _callback = func;
    _value = _default;
    _name = name;
    _help_string = help_string;
    _flags = flags;
    _type = CONVAR_TYPE::CONVAR_TYPE_STRING;
    check_if_convar_exists(_name);
    convar_t::get_convar_list()->push_back(this);
    cli_parser::apply_to(this);
}

#define CONVAR_SET_IMPL(type)                           \
    bool convar_##type##_t::set(type i)                 \
    {                                                   \
        if (_bounded && (i < _min || i > _max))         \
            return false;                               \
        if (_pre_callback && !_pre_callback(_value, i)) \
            return false;                               \
        _value = i;                                     \
        if (_callback)                                  \
            _callback();                                \
        return true;                                    \
    }                                                   \
    bool convar_##type##_t::set_default(type i)         \
    {                                                   \
        if (_bounded && (i < _min || i > _max))         \
            return false;                               \
        _default = i;                                   \
        return true;                                    \
    }

CONVAR_SET_IMPL(int);
CONVAR_SET_IMPL(float);

bool convar_string_t::set(std::string i)
{
    if (_pre_callback && !_pre_callback(_value, i))
        return false;
    _value = i;
    if (_callback)
        _callback();
    return true;
}

bool convar_string_t::set_default(std::string i)
{
    _default = i;
    return true;
}

#define CONVAR_SET_CALLBACK_IMPL(name, type)                                                             \
    void convar_##name##_t::set_post_callback(std::function<void(void)> func, bool call)                 \
    {                                                                                                    \
        _callback = func;                                                                                \
        if (call)                                                                                        \
            _callback();                                                                                 \
    };                                                                                                   \
    void convar_##name##_t::set_pre_callback(std::function<bool(type _prev, type _new)> func, bool call) \
    {                                                                                                    \
        _pre_callback = func;                                                                            \
        if (call)                                                                                        \
            _pre_callback(_value, _value);                                                               \
    };

CONVAR_SET_CALLBACK_IMPL(int, int);
CONVAR_SET_CALLBACK_IMPL(float, float);
CONVAR_SET_CALLBACK_IMPL(string, std::string);

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(a, min, max) MAX(min, MIN(a, max))

bool convar_int_t::imgui_edit()
{
    int v = _value;
    bool ret = false;
    if (ImGui::InputInt(_name, &v))
    {
        if (_bounded)
            ret = set(CLAMP(v, _min, _max));
        else
            ret = set(v);
    }
    ImGui::SameLine();
    ImGui::HelpMarker(_help_string);
    return ret;
}

bool convar_float_t::imgui_edit()
{
    float v = _value;
    bool ret = false;
    if (ImGui::InputFloat(_name, &v, 0.05f, 0.25f))
    {
        if (_bounded)
            ret = set(CLAMP(v, _min, _max));
        else
            ret = set(v);
    }
    ImGui::SameLine();
    ImGui::HelpMarker(_help_string);
    return ret;
}

bool convar_string_t::imgui_edit()
{
    std::string v = _value;
    bool ret = false;
    if (ImGui::InputText(_name, &v))
    {
        set(v);
    }
    ImGui::SameLine();
    ImGui::HelpMarker(_help_string);
    return ret;
}

void convar_int_t::log_help()
{
    if (_bounded)
        dev_console::add_log("\"%s\": %d (default: %d) (Min: %d, Max: %d)", _name, _value, _default, _min, _max);
    else
        dev_console::add_log("\"%s\": %d (default: %d)", _name, _value, _default);
    if (_help_string && strlen(_help_string) > 0)
        dev_console::add_log("%s", _help_string);
}

void convar_float_t::log_help()
{
    if (_bounded)
        dev_console::add_log("\"%s\": %.3f (default: %.3f) (Min: %.3f, Max: %.3f)", _name, _value, _default, _min, _max);
    else
        dev_console::add_log("\"%s\": %.3f (default: %.3f)", _name, _value, _default);
    if (_help_string && strlen(_help_string) > 0)
        dev_console::add_log("%s", _help_string);
}

void convar_string_t::log_help()
{
    dev_console::add_log("\"%s\": \"%s\" (default: \"%s\")", _name, _value.c_str(), _default.c_str());
    if (_help_string && strlen(_help_string) > 0)
        dev_console::add_log("%s", _help_string);
}

int convar_int_t::convar_command(const int argc, const char** argv)
{
    if (argc != 2)
    {
        log_help();
        return 0;
    }
    if (!argv[1])
        return 1;

    errno = 0;
    char* endptr;
    long v = strtol(argv[1], &endptr, 10);
    if (endptr == argv[1] || *endptr != '\0' || ((v == LONG_MIN || v == LONG_MAX) && errno == ERANGE))
    {
        if (strcmp(argv[1], "true") == 0)
            v = 1;
        else if (strcmp(argv[1], "false") == 0)
            v = 0;
        else
            return 2;
    }
    return set(v) ? 0 : 3;
}

int convar_float_t::convar_command(const int argc, const char** argv)
{
    if (argc != 2)
    {
        log_help();
        return 0;
    }
    if (!argv[1])
        return 1;

    errno = 0;
    char* endptr;
    float v = strtof(argv[1], &endptr);
    if (endptr == argv[1] || *endptr != '\0' || (v == FLT_MIN || errno == ERANGE))
        return 2;
    return set(v) ? 0 : 3;
}

int convar_string_t::convar_command(const int argc, const char** argv)
{
    if (argc != 2)
    {
        log_help();
        return 0;
    }
    if (!argv[1])
        return 1;

    return set(std::string(argv[1])) ? 0 : 3;
}

bool ImGui::BeginCVR(const char* name, convar_int_t* p_open, ImGuiWindowFlags flags)
{
    if (p_open == NULL)
        return ImGui::Begin(name, NULL, flags);

    bool open = p_open->get();

    bool ret = ImGui::Begin(name, &open, flags);

    if (open != p_open->get())
        p_open->set(open);

    return ret;
}

std::string convar_int_t::get_convar_command()
{
    std::string out(get_name());
    out.append(" \"");
    out.append(std::to_string(get()));
    out.append("\"");
    return out;
}
std::string convar_float_t::get_convar_command()
{
    std::string out(get_name());
    out.append(" \"");
    out.append(std::to_string(get()));
    out.append("\"");
    return out;
}
std::string convar_string_t::get_convar_command()
{
    std::string out(get_name());
    out.append(" \"");
    out.append(get());
    out.append("\"");
    return out;
}
