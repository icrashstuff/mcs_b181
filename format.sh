#!/bin/bash
exec clang-format --verbose -i {client\/{,gpu/,shaders/,sys/,gui/,sound/},shared/,server/,bridge/}*.{c,h,cpp}
