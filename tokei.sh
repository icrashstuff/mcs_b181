#!/bin/sh
# Helper script to run tokei while excluding all non mcs_b181 code
exec tokei \
    -e tokei.sh \
    -e build/ \
    -e tetra/ \
    -e sdl_net/ \
    -e client/entt \
    -e client/jzon \
    -e shared/simplex_noise/ \
    -e docs \
    -e shared/cubiomes/ \
    -e client/sound/stb_vorbis.c \
    -e client/sound/stb_vorbis.h \
    -e cmake \
    -e shared/packet_gen_def.h \
    -e client/texture_ids.h \
    -e client/gpu/smol-v/ \
    -e client/shaders/compiled/ \
    -e client/sound/miniaudio_unifdef.h \
    "$@"
