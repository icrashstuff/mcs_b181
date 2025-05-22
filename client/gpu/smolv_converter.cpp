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
#include "smol-v/smolv.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum direction_t
{
    spv2smolv,
    smolv2spv,
    unset,
};

[[noreturn]] static void die(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    exit(EXIT_FAILURE);
}

static void print(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

int main(const int argc, const char** argv)
{
    direction_t direction = unset;

    if (argc > 1)
    {
        if (strcmp(argv[1], "spv2smolv") == 0)
            direction = spv2smolv;
        if (strcmp(argv[1], "smolv2spv") == 0)
            direction = smolv2spv;
    }

    if (direction == spv2smolv)
        print("spv2smolv: A simple SPIR-V -> SMOL-V converter\n");
    if (direction == smolv2spv)
        print("smolv2spv: A simple SMOL-V -> SPIR-V converter\n");

    if (argc != 4 || direction == unset)
    {
        print("Usage: smolv_converter (spv2smolv|smolv2spv) input output\n\n");
        print("The first argument must one of the following:\n"
              "  spv2smolv (for SPIR-V -> SMOL-V conversion)\n"
              "  smolv2spv (for SMOL-V -> SPIR-V conversion)\n"
              "\n"
              "input and output must be valid filenames, or '-'\n");
        return EXIT_FAILURE;
    }

    const char* fname_in = argv[2];
    const char* fname_out = argv[3];

    /* Read input data */
    smolv::ByteArray array_in;
    {
        /* Open */
        FILE* in = NULL;
        if (strcmp(fname_in, "-") == 0)
        {
            print("Re-opening stdin in binary mode\n");
            if (!(in = freopen(NULL, "rb", stdin)))
                die("Unable to reopen stdin in binary mode!\n");
        }
        else
        {
            print("Opening \"%s\" for reading\n", fname_in);
            if (!(in = fopen(fname_in, "rb")))
                die("Unable to open \"%s\" for reading!\n", fname_in);
        }

        /* Read */
        while (feof(in) == 0 && ferror(in) == 0)
        {
            uint8_t b;
            while (fread(&b, 1, 1, in))
                array_in.push_back(b);
        }

        if (ferror(in))
            die("Error reading input\n");

        fclose(in);

        print("Input has size: %ld bytes\n", long(array_in.size()));
    }

    /* Convert data */
    smolv::ByteArray array_out;
    {
        if (direction == spv2smolv)
            if (!smolv::Encode(array_in.data(), array_in.size(), array_out))
                die("Error encoding SPIR-V -> SMOL-V");
        if (direction == smolv2spv)
        {
            array_out.resize(smolv::GetDecodedBufferSize(array_in.data(), array_in.size()));
            if (!smolv::Decode(array_in.data(), array_in.size(), array_out.data(), array_out.size()))
                die("Error decoding SMOL-V -> SPIR-V");
        }
        print("Output has size: %ld bytes\n", long(array_out.size()));
    }

    /* Write to output */
    {
        /* Open */
        FILE* out = NULL;
        if (strcmp(fname_out, "-") == 0)
        {
            print("Re-opening stdout in binary mode\n");
            if (!(out = freopen(NULL, "wb", stdout)))
                die("Unable to reopen stdout in binary mode!\n");
        }
        else
        {
            print("Opening \"%s\" for writing\n", fname_out);
            if (!(out = fopen(fname_out, "wb")))
                die("Unable to open \"%s\" for writing!\n", fname_out);
        }

        /* Write */
        if (fwrite(array_out.data(), 1, array_out.size(), out) != array_out.size())
            die("Error writing output");

        fclose(out);
    }

    long diff = long(array_out.size()) - long(array_in.size());
    print("Size change: %ld bytes (%.1f%%)\n", diff, diff * 100.0 / double(array_in.size()));

    return EXIT_SUCCESS;
}
