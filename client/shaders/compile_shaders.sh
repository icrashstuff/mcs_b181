#!/bin/sh
# This script requires xxd, glslc, spirv-opt, and SDL_shadercross

set -eu

SMOLV_CONVERTER="$1"

# Discard first parameter
shift 1

# This snippet was copied from build-shaders.sh from SDL3
make_header_binary() {
    echo "// clang-format off" > "compiled/$1.h"
    xxd -i "$1" | sed \
        -e 's/^unsigned /const unsigned /g' \
        -e 's,^const,static const,' \
        >> "compiled/$1.h"
    rm -f "$1"
}
make_header_text() {
    ARRAY_NAME="$(echo "$1" | sed -e 's/\./_/g')"

    echo "// clang-format off" > "compiled/$1.h"
    echo "static const unsigned char ${ARRAY_NAME}[] = R\"(" >> "compiled/$1.h"
    cat  "$1"  >> "compiled/$1.h"
    echo ")\";"  >> "compiled/$1.h"
    echo "static const unsigned int ${ARRAY_NAME}_len = sizeof(${ARRAY_NAME});" >> "compiled/$1.h"
    rm -f "$1"
}

compile_shader()
{
    STAGE="$1"
    FILE_IN="$2"
    FILE_OUT_SPIRV="$2$3.spv"
    FILE_OUT_SMOLV="$2$3.smolv"
    FILE_OUT_METAL="$2$3.msl"
    FILE_OUT_DXIL="$2$3.dxil"

    mkdir -p "${PWD}/compiled/"

    # Discard first three parameters
    shift 3

    echo "================ ${STAGE}: ${FILE_IN} ================"
    [ $# != 0 ] && echo "glslc extra parameter(s): $*"

    rm -f "${FILE_OUT_SPIRV}" "${FILE_OUT_SMOLV}" "${FILE_OUT_METAL}" "${FILE_OUT_DXIL}"

    echo "==> $STAGE: ${FILE_IN} -> ${FILE_OUT_SPIRV}"
    glslc "$@" -fshader-stage="${STAGE}" -c "$PWD/${FILE_IN}" -o - | spirv-opt -Os  - -o "${FILE_OUT_SPIRV}"

    echo "==> $STAGE: ${FILE_OUT_SPIRV} -> ${FILE_OUT_METAL}"
    shadercross "${FILE_OUT_SPIRV}" --source SPIRV --dest MSL --entrypoint main --stage "${STAGE}" --output "${FILE_OUT_METAL}"

    echo "==> $STAGE: ${FILE_OUT_SPIRV} -> ${FILE_OUT_SMOLV}"
    "${SMOLV_CONVERTER}" spv2smolv "$PWD/${FILE_OUT_SPIRV}" "$PWD/${FILE_OUT_SMOLV}"

    # Doesn't work, besides I (Ian) don't target d3d12
    # echo "==> $STAGE: ${FILE_OUT_SPIRV} -> ${FILE_OUT_DXIL}"
    # shadercross "${FILE_OUT_SPIRV}" --source SPIRV --dest DXIL --stage "${STAGE}" --output "${FILE_OUT_DXIL}"

    make_header_binary "${FILE_OUT_SMOLV}"
    make_header_text "${FILE_OUT_METAL}"

    rm "${FILE_OUT_SPIRV}"
}

compile_shader "vertex"   "terrain.vert" ""
compile_shader "fragment" "terrain.frag" ".opaque"       -DUSE_ALPHA_TEST=0 -DDEPTH_PEELING=0
compile_shader "fragment" "terrain.frag" ".alpha_test"   -DUSE_ALPHA_TEST=1 -DDEPTH_PEELING=0
compile_shader "fragment" "terrain.frag" ".depth_peel_0" -DUSE_ALPHA_TEST=0 -DDEPTH_PEELING=1
compile_shader "fragment" "terrain.frag" ".depth_peel_n" -DUSE_ALPHA_TEST=0 -DDEPTH_PEELING=2


compile_shader "vertex"   "background.vert" ""
compile_shader "fragment" "background.frag" ""

compile_shader "vertex"   "composite.vert" ""
compile_shader "fragment" "composite.frag" ""
