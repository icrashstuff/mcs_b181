#!/bin/sh
# This script requires xxd, glslc, spirv-opt, and SDL_shadercross

set -eu

# This snippet was copied from build-shaders.sh from SDL3
make_header_binary() {
    echo "// clang-format off" > "$1.h"
    xxd -i "$1" | sed \
        -e 's/^unsigned /const unsigned /g' \
        -e 's,^const,static const,' \
        >> "$1.h"
    rm -f "$1"
}
make_header_text() {
    ARRAY_NAME="$(echo "$1" | sed -e 's/\./_/g')"

    echo "// clang-format off" > "$1.h"
    echo "static const unsigned char ${ARRAY_NAME}[] = R\"(" >> "$1.h"
    cat  "$1"  >> "$1.h"
    echo ")\";"  >> "$1.h"
    echo "static const unsigned int ${ARRAY_NAME}_len = sizeof(${ARRAY_NAME});" >> "$1.h"
    rm -f "$1"
}

compile_shader()
{
    STAGE="$1"
    FILE_IN="$2"
    FILE_OUT_SPIRV="$3.spv"
    FILE_OUT_METAL="$3.msl"
    FILE_OUT_DXIL="$3.dxil"

    # Discard first three parameters
    shift 3

    echo "================ ${STAGE}: ${FILE_IN} ================"
    [ $# != 0 ] && echo "glslc extra parameter(s): $*"

    rm -f "${FILE_OUT_SPIRV}" "${FILE_OUT_METAL}" "${FILE_OUT_DXIL}"

    echo "==> $STAGE: ${FILE_IN} -> ${FILE_OUT_SPIRV}"
    glslc "$@" -fshader-stage="${STAGE}" -c "${FILE_IN}" -o - | spirv-opt -Os  - -o "${FILE_OUT_SPIRV}"

    echo "==> $STAGE: ${FILE_OUT_SPIRV} -> ${FILE_OUT_METAL}"
    shadercross "${FILE_OUT_SPIRV}" --source SPIRV --dest MSL --entrypoint main --stage "${STAGE}" --output "${FILE_OUT_METAL}"

    # Doesn't work, besides I (Ian) don't target d3d12
    # echo "==> $STAGE: ${FILE_OUT_SPIRV} -> ${FILE_OUT_DXIL}"
    # shadercross "${FILE_OUT_SPIRV}" --source SPIRV --dest DXIL --stage "${STAGE}" --output "${FILE_OUT_DXIL}"

    make_header_binary "${FILE_OUT_SPIRV}"
    make_header_text "${FILE_OUT_METAL}"
}

compile_shader "vertex"   "terrain.vert" "terrain.vert"
compile_shader "fragment" "terrain.frag" "terrain.frag.alpha_test"    -DUSE_ALPHA_TEST=1
compile_shader "fragment" "terrain.frag" "terrain.frag.no_alpha_test" -DUSE_ALPHA_TEST=0
