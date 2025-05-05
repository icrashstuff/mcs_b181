#!/bin/bash
exec clang-format --verbose -i {client\/{,gpu/,shaders/,sys/,gui/,sound/sound_,lang/},shared/,server/,bridge/}*.{c,h,cpp}
