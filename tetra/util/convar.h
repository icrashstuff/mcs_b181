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
#ifndef MPH_TETRA_UTILS_CONVAR_H
#define MPH_TETRA_UTILS_CONVAR_H

#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <vector>

#include "tetra/gui/imgui.h"

typedef Uint32 CONVAR_FLAGS;
enum CONVAR_FLAGS_ : Uint32
{
    /**
     * Applies to convar_int_t
     *
     * When parsing the command line a convar set but given no value will be set to true
     */
    CONVAR_FLAG_INT_IS_BOOL = (1 << 0),

    /**
     * Applies to all convars
     *
     * If set then the convar will be saved to a file if changed from the default
     */
    CONVAR_FLAG_SAVE = (1 << 1),

    /**
     * Applies to all convars
     *
     * If set then the convar will be hidden from tab auto complete and imgui_edit, but will be manually accessible
     */
    CONVAR_FLAG_HIDDEN = (1 << 2),

    /**
     * Applies to all convars
     *
     * If the dev convar is not set then this acts like CONVAR_FLAG_HIDDEN
     */
    CONVAR_FLAG_DEV_ONLY = (1 << 3),
};

/**
 * NOTE: Once a convar has been created, it must not be destroyed before program exit, doing so will lead to an abort(3) call
 */
class convar_t
{
public:
    enum class CONVAR_TYPE
    {
        CONVAR_TYPE_INT,
        CONVAR_TYPE_FLOAT,
        CONVAR_TYPE_STRING,
    };

    ~convar_t();

    inline CONVAR_TYPE get_convar_type() const { return _type; }

    inline CONVAR_FLAGS get_convar_flags() const { return _flags; }

    inline const char* get_help_string() const { return _help_string; }

    inline const char* get_name() const { return _name; }

    /**
     * Returns true if the dev convar is set
     */
    static bool dev();

    /**
     * Prints help text to log
     */
    virtual void log_help() = 0;

    /**
     * Creates an appropriate imgui widget for editing the convar
     */
    virtual bool imgui_edit() = 0;

    /**
     * Get convar_t from corresponding name
     */
    static convar_t* get_convar(const char* name);

    static std::vector<convar_t*>* get_convar_list();

    /**
     * Command called when convar is accessed
     */
    virtual int convar_command(const int argc, const char** argv) = 0;

    /**
     * Returns a string that can be run to get the current value
     */
    virtual std::string get_convar_command() = 0;

    /**
     * Sets _atexit to true, allows convars to be deleted without creating a bunch of error messages
     */
    static void atexit_callback();

    /**
     * Sets _atexit to false, put this as close to int main() as possible
     */
    static void atexit_init();

protected:
    static bool _atexit;

    CONVAR_TYPE _type;
    CONVAR_FLAGS _flags;

    const char* _help_string;
    const char* _name;
};

class convar_int_t : public convar_t
{
public:
    convar_int_t(
        const char* name, int default_value, int min, int max, const char* help_string, CONVAR_FLAGS flags = 0, std::function<void()> post_callback = NULL);

    inline int get() const { return _value; }

    inline int get_min() const { return _min; }

    inline int get_max() const { return _max; }

    inline int get_default() const { return _default; }

    /**
     * Steps:
     * 1. Performs bounds checking
     * 2. Calls pre_callback (if set)
     * 3. Sets the value
     * 4. Calls post_callback (if set)
     *
     * Returns true if all went well, returns false if any step failed
     */
    bool set(int i);

    /**
     * Steps:
     * 1. Performs bounds checking
     * 2. Sets the default value
     *
     * Returns true if all went well, returns false if any step failed
     */
    bool set_default(int i);

    /**
     * Sets the pre-callback for the convar which is called before setting the convar
     *
     * @param func Callback function, NOTE: If the function does not return true then the set() call will never work
     * @param call Whether to call the callback after setting it
     */
    void set_pre_callback(std::function<bool(int _prev, int _new)> func, bool call = false);

    /**
     * Sets the post-callback for the convar which is called after the convar is set
     *
     * @param func Callback function
     * @param call Whether to call the callback after setting it
     */
    void set_post_callback(std::function<void()> func, bool call = false);

    void log_help();

    bool imgui_edit();

    int convar_command(const int argc, const char** argv);

    std::string get_convar_command();

protected:
    int _value;
    int _default;
    int _bounded;
    int _min;
    int _max;
    std::function<bool(int _prev, int _new)> _pre_callback = nullptr;
    std::function<void()> _callback = nullptr;
};

class convar_float_t : public convar_t
{
public:
    convar_float_t(const char* name, float default_value, float min, float max, const char* help_string, CONVAR_FLAGS flags = 0,
        std::function<void()> post_callback = NULL);

    inline float get() const { return _value; }

    inline float get_min() const { return _min; }

    inline float get_max() const { return _max; }

    inline float get_default() const { return _default; }

    /**
     * Steps:
     * 1. Performs bounds checking
     * 2. Calls pre_callback (if set)
     * 3. Sets the value
     * 4. Calls post_callback (if set)
     *
     * Returns true if all went well, returns false if any step failed
     */
    bool set(float i);

    /**
     * Steps:
     * 1. Performs bounds checking
     * 2. Sets the default value
     *
     * Returns true if all went well, returns false if any step failed
     */
    bool set_default(float i);

    /**
     * Sets the pre-callback for the convar which is called before setting the convar
     *
     * @param func Callback function, NOTE: If the function does not return true then the set() call will never work
     * @param call Whether to call the callback after setting it
     */
    void set_pre_callback(std::function<bool(float _prev, float _new)> func, bool call = false);

    /**
     * Sets the post-callback for the convar which is called after the convar is set
     *
     * @param func Callback function
     * @param call Whether to call the callback after setting it
     */
    void set_post_callback(std::function<void()> func, bool call = false);

    void log_help();

    bool imgui_edit();

    int convar_command(const int argc, const char** argv);

    std::string get_convar_command();

protected:
    float _value;
    float _default;
    int _bounded;
    float _min;
    float _max;
    std::function<bool(float _prev, float _new)> _pre_callback = nullptr;
    std::function<void()> _callback = nullptr;
};

class convar_string_t : public convar_t
{
public:
    convar_string_t(const char* name, std::string default_value, const char* help_string, CONVAR_FLAGS flags = 0, std::function<void()> post_callback = NULL);

    inline std::string get() const { return _value; }

    inline std::string get_default() const { return _default; }

    /**
     * Steps:
     * 1. Calls pre_callback (if set)
     * 2. Sets the value
     * 3. Calls post_callback (if set)
     *
     * Returns true if all went well, returns false if any step failed
     */
    bool set(std::string i);

    /**
     * Steps:
     * 1. Sets the default value
     *
     * Returns true if all went well, returns false if any step failed
     */
    bool set_default(std::string i);

    /**
     * Sets the pre-callback for the convar which is called before setting the convar
     *
     * @param func Callback function, NOTE: If the function does not return true then the set() call will never work
     * @param call Whether to call the callback after setting it
     */
    void set_pre_callback(std::function<bool(std::string _prev, std::string _new)> func, bool call = false);

    /**
     * Sets the post-callback for the convar which is called after the convar is set
     *
     * @param func Callback function
     * @param call Whether to call the callback after setting it
     */
    void set_post_callback(std::function<void()> func, bool call = false);

    void log_help();

    bool imgui_edit();

    int convar_command(const int argc, const char** argv);

    std::string get_convar_command();

protected:
    std::string _value;
    std::string _default;
    std::function<bool(std::string _prev, std::string _new)> _pre_callback = nullptr;
    std::function<void()> _callback = nullptr;
};

namespace ImGui
{
/**
 * Wrapper for ImGui::Begin that replaces bool p_open with convar_int_t p_open
 */
bool BeginCVR(const char* name, convar_int_t* p_open = NULL, ImGuiWindowFlags flags = 0);

/**
 * Wrapper for ImGui::Checkbox that replaces bool* v with convar_int_t* v
 */
bool Checkbox(const char* label, convar_int_t* v);
};

#endif
