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
# Make a commit on tetra, then on the parent repository
#
# This is not a well written script, but it fits my workflow
#
# - This script is meant to be run from the parent repository's working directory
# - Where $SUBDIR is the working directory of tetra
# - And tetra's .git folder is located at ../tetra_git/.git/ relative to the
#   parent repository's working directory

set -eu

# Display all commands
# set -x

SUBDIR=tetra/

DIR_PARENT_WORKING="$PWD"
DIR_TETRA_WORKING="$PWD/$SUBDIR"

DIR_PARENT_GIT="$PWD/.git/"
DIR_TETRA_GIT="$PWD/../tetra_git/.git/"

FILE_PARENT_CMT_MSG=$(mktemp)
FILE_PARENT_PATCH=$(mktemp)

PARENT_GIT() { cd "$DIR_PARENT_WORKING" ; git --git-dir="$DIR_PARENT_GIT" "$@"; }
TETRA_GIT()  { cd "$DIR_TETRA_WORKING"  ; git --git-dir="$DIR_TETRA_GIT"  "$@"; }


DIE_AND_POP_PARENT()
{
    PARENT_GIT clean -f -d tetra/ || echo "WARNING: Failed to clean ${SUBDIR}, restoration may fail"
    PARENT_GIT stash pop || echo "ERROR: Failed to restore parent repository, manual intervention is required"
    rm -f "$FILE_PARENT_PATCH" "$FILE_PARENT_CMT_MSG"
    exit "$1"
}

DIE()
{
    rm -f "$FILE_PARENT_PATCH" "$FILE_PARENT_CMT_MSG"
    exit "$1"
}

# Create tetra commit
TETRA_GIT commit "$@" || DIE "$?"

# Create patch file
TETRA_GIT diff --src-prefix="a/$SUBDIR" --dst-prefix="b/$SUBDIR" --output="$FILE_PARENT_PATCH" HEAD~1 HEAD || DIE "$?"

# Create log message for parent repo
CMT_ID_TETRA=$(TETRA_GIT rev-parse HEAD)
printf "Tetra: "                                      > "$FILE_PARENT_CMT_MSG"
TETRA_GIT log -1 --format=%B                         >> "$FILE_PARENT_CMT_MSG"
echo   ""                                            >> "$FILE_PARENT_CMT_MSG"
echo   "- This brings $SUBDIR inline with commit:"   >> "$FILE_PARENT_CMT_MSG"
echo   "  ${CMT_ID_TETRA}"                           >> "$FILE_PARENT_CMT_MSG"

# Save parent
PARENT_GIT add "$SUBDIR" || DIE "$?"
PARENT_GIT stash push -m "TEMP: For $0" "$SUBDIR" || DIE "$?"

# Alternative to git am
PARENT_GIT apply "$FILE_PARENT_PATCH" || DIE_AND_POP_PARENT "$?"
PARENT_GIT add "$SUBDIR"
PARENT_GIT commit --file "$FILE_PARENT_CMT_MSG" || DIE_AND_POP_PARENT "$?"

# Restore parent
PARENT_GIT stash pop || DIE "$?"
PARENT_GIT restore --staged "$SUBDIR"

rm -f "$FILE_PARENT_PATCH" "$FILE_PARENT_CMT_MSG"
