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

#ifndef MCS_B181__CLIENT__SOUND__MINIAUDIO_PHYSFS_HPP_INCLUDED
#define MCS_B181__CLIENT__SOUND__MINIAUDIO_PHYSFS_HPP_INCLUDED

#include "miniaudio_unifdef.h"
#include "tetra/util/physfs/physfs.h"

#define ADD_SWITCH_RETURN(CASE, VAL) \
    case CASE:                       \
        return VAL

/* Incomplete, but works well enough */
static ma_result physfs_error_to_ma(const PHYSFS_ErrorCode err_code)
{
    switch (err_code)
    {
        ADD_SWITCH_RETURN(PHYSFS_ERR_OK, MA_SUCCESS);
        ADD_SWITCH_RETURN(PHYSFS_ERR_PAST_EOF, MA_BAD_SEEK);
        ADD_SWITCH_RETURN(PHYSFS_ERR_BUSY, MA_BUSY);
        ADD_SWITCH_RETURN(PHYSFS_ERR_OUT_OF_MEMORY, MA_OUT_OF_MEMORY);
        ADD_SWITCH_RETURN(PHYSFS_ERR_NOT_FOUND, MA_DOES_NOT_EXIST);
        ADD_SWITCH_RETURN(PHYSFS_ERR_BAD_FILENAME, MA_ACCESS_DENIED);
        ADD_SWITCH_RETURN(PHYSFS_ERR_PERMISSION, MA_ACCESS_DENIED);
    default:
        return MA_ERROR;
    }
}

static ma_result vfs_callback_physfs_onOpen(ma_vfs*, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
{
    if (openMode != MA_OPEN_MODE_READ) /* mcs_b181 doesn't need MA_OPEN_MODE_WRITE */
    {
        dc_log_error("Invalid openMode: 0x%x", openMode);
        return MA_NOT_IMPLEMENTED;
    }

    dc_log_trace("%s", pFilePath);

    *pFile = static_cast<ma_vfs_file>(PHYSFS_openRead(pFilePath));

    if (!*pFile)
        return physfs_error_to_ma(PHYSFS_getLastErrorCode());
    else
        return MA_SUCCESS;
}
static ma_result vfs_callback_physfs_onClose(ma_vfs*, ma_vfs_file file)
{
    if (PHYSFS_close(static_cast<PHYSFS_File*>(file)))
        return MA_SUCCESS;
    else
        return physfs_error_to_ma(PHYSFS_getLastErrorCode());
}
static ma_result vfs_callback_physfs_onRead(ma_vfs*, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead)
{
    PHYSFS_sint64 bytes_read = PHYSFS_readBytes(static_cast<PHYSFS_File*>(file), pDst, sizeInBytes);

    if (bytes_read >= 0)
        *pBytesRead = size_t(bytes_read);
    else
        return physfs_error_to_ma(PHYSFS_getLastErrorCode());

    return MA_SUCCESS;
}
static ma_result vfs_callback_physfs_onSeek(ma_vfs*, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
{
    PHYSFS_File* fd = static_cast<PHYSFS_File*>(file);

    int err;

    switch (origin)
    {
    case ma_seek_origin_start:
        err = PHYSFS_seek(fd, offset);
        break;
    case ma_seek_origin_current:
        err = PHYSFS_seek(fd, PHYSFS_tell(fd) + offset);
        break;
    case ma_seek_origin_end:
        err = PHYSFS_seek(fd, PHYSFS_fileLength(fd) - offset);
        break;
    default:
        return MA_INVALID_ARGS;
    }
    if (err == 0)
        return physfs_error_to_ma(PHYSFS_getLastErrorCode());
    else
        return MA_SUCCESS;
}
static ma_result vfs_callback_physfs_onTell(ma_vfs*, ma_vfs_file file, ma_int64* pCursor)
{
    *pCursor = PHYSFS_tell(static_cast<PHYSFS_File*>(file));
    if (*pCursor < 0)
        return physfs_error_to_ma(PHYSFS_getLastErrorCode());
    else
        return MA_SUCCESS;
}
static ma_result vfs_callback_physfs_onInfo(ma_vfs*, ma_vfs_file file, ma_file_info* pInfo)
{
    if ((pInfo->sizeInBytes = PHYSFS_fileLength(static_cast<PHYSFS_File*>(file))) < 0)
        return physfs_error_to_ma(PHYSFS_getLastErrorCode());
    else
        return MA_SUCCESS;
}

static ma_vfs_callbacks ma_vfs_callbacks_physfs {
    .onOpen = vfs_callback_physfs_onOpen,
    .onOpenW = NULL,
    .onClose = vfs_callback_physfs_onClose,
    .onRead = vfs_callback_physfs_onRead,
    .onWrite = NULL,
    .onSeek = vfs_callback_physfs_onSeek,
    .onTell = vfs_callback_physfs_onTell,
    .onInfo = vfs_callback_physfs_onInfo,
};

#endif
