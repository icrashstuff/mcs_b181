#!/bin/sh
# This script requires xxd, glslc, spirv-opt, and SDL_shadercross
# TODO-OPT: Metallib shaders

set -e

# This snippet was copied from build-shaders.sh from SDL3
make_header() {
    echo "// clang-format off" > "$1.h"
    xxd -i "$1" | sed \
        -e 's/^unsigned /const unsigned /g' \
        -e 's,^const,static const,' \
        >> "$1.h"
    rm -f "$1"
}

compile_shader()
{
    STAGE="$1"
    FILE_IN="$2"
    FILE_OUT_SPIRV="$2.spv"
    FILE_OUT_METAL="$2.msl"
    FILE_OUT_DXIL="$2.dxil"

    echo "================ ${STAGE} ================"

    rm -f "${FILE_OUT_SPIRV}" "${FILE_OUT_METAL}" "${FILE_OUT_DXIL}"

    echo "==> $STAGE: ${FILE_IN} -> ${FILE_OUT_SPIRV}"
    glslc -fshader-stage="${STAGE}" -c "${FILE_IN}" -o - | spirv-opt -Os  - -o "${FILE_OUT_SPIRV}"

    echo "==> $STAGE: ${FILE_OUT_SPIRV} -> ${FILE_OUT_METAL}"
    shadercross "${FILE_OUT_SPIRV}" --source SPIRV --dest MSL --entrypoint main --stage "${STAGE}" --output "${FILE_OUT_METAL}"

    # Doesn't work, besides I (Ian) don't target d3d12
    # echo "==> $STAGE: ${FILE_OUT_SPIRV} -> ${FILE_OUT_DXIL}"
    # shadercross "${FILE_OUT_SPIRV}" --source SPIRV --dest DXIL --stage "${STAGE}" --output "${FILE_OUT_DXIL}"

    make_header "${FILE_OUT_SPIRV}"
    make_header "${FILE_OUT_METAL}"
}

compile_shader "vertex" "terrain.vert"
compile_shader "fragment" "terrain.frag"
