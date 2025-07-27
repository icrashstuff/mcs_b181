#!/bin/sh
# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2025 Ian Hangartner <icrashstuff at outlook dot com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
# Push changes on tetra, then on the parent
#
# This is probably not a well written script, but it fits my workflow
#
# - This script is NOT meant to be added to $PATH
# - Where $SUBDIR is the working directory of tetra
# - And tetra's .git folder is located at ../tetra_git/.git/ relative to the
#   parent repository's working directory

set -eu

# Show all commands run
# set -x

# Relocate to parent repository
SCRIPT_PATH="$(readlink -f $0)"
cd "$(dirname $SCRIPT_PATH)"
cd "$(dirname $PWD)"

SUBDIR=tetra/

DIR_PARENT_WORKING="$PWD"
DIR_TETRA_WORKING="$PWD/$SUBDIR"

DIR_PARENT_GIT="$PWD/.git/"
DIR_TETRA_GIT="$PWD/../tetra_git/.git/"

PARENT_GIT() { cd "$DIR_PARENT_WORKING" ; git --git-dir="$DIR_PARENT_GIT" "$@"; }
TETRA_GIT()  { cd "$DIR_TETRA_WORKING"  ; git --git-dir="$DIR_TETRA_GIT"  "$@"; }

# Setup ssh agent
eval "$(ssh-agent -s)"

DIE()
{
    # Shutdown ssh agent
    eval "$(ssh-agent -k || true)"
    exit $1
}

# Add ssh keys
ssh-add || DIE "$?"

# Push tetra first
TETRA_GIT push || DIE "$?"
TETRA_GIT log -2

# Push mcs_b181
PARENT_GIT push || DIE "$?"
PARENT_GIT log -2

# Exit cleaning up ssh-agent
DIE 0
