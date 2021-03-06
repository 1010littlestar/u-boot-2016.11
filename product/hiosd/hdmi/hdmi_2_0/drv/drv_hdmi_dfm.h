/*
* Copyright (c) 2018 HiSilicon Technologies Co., Ltd.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
*/
#ifndef __DRV_HDMI_DFM__
#define __DRV_HDMI_DFM__

#ifdef __cplusplus
    extern "C" {
#endif

#include "hi_type.h"

#define MAX(a, b) (((a) < (b)) ?  (b) : (a))
#define MIN(a, b) (((a) > (b)) ?  (b) : (a))

typedef struct
{
    HI_BOOL bAudioSupport;
    HI_BOOL bVideoSupport;
    HI_BOOL bUncompressSupport;
    HI_BOOL bIsExtraMode;
    HI_BOOL bCanbeTrans;
    HI_S32 s32Htotal;
    HI_S32 s32Vtotal;
    HI_S32 s32Bpp;
    HI_S32 s32CFrlSB;
    HI_S32 s32OverheadSB;
    HI_S32 s32OverheadRS;
    HI_S32 s32OverheadMap;
    HI_S32 s32OverheadMin;
    HI_S32 s32OverheadM;
    HI_S32 s32OverheadMax;
    HI_S32 s32MinTimeLine;
    HI_S32 s32MaxTimeLine;
    HI_S32 s32MinFrlCharsPerLine;
    HI_S32 s32MaxFrlCharsPerLine;
    HI_S32 s32AudioAP;
    HI_S32 s32AudioRAP;
    HI_S32 s32AvgAudioPackets;
    HI_S32 s32AudioPackets;
    HI_S32 s32BlankAudioMin;
    HI_S32 s32CFrlFree;
    HI_S32 s32CFrlRCMargin;
    HI_S32 s32CFrlRCSavings;
    HI_S32 s32ActiveBytesPerLine;
    HI_S32 s32ActiveTBPerLine;
    HI_S32 s32BlankTBPerLine;
    HI_S32 s32ActiveTimeRef;
    HI_S32 s32BlankTimeRef;
    HI_S32 s32ActiveTimeMin;
    HI_S32 s32BlankTimeMin;
    HI_S32 s32TBorrow;
    HI_S32 s32TBBorrow;
    HI_S32 s32CFrlActualPayload;
    HI_S32 s32Utilization;
    HI_S32 s32Margin;
    HI_S64 s64AvgTBRate;
    HI_S64 s64PixelClk;
    HI_S64 s64MaxPixelClk;
    HI_S64 s64MinPixelClk;
    HI_S64 s64MaxBitRate;
    HI_S64 s64MinBitRate;
    HI_S64 s64MaxFrlCharRate;
    HI_S64 s64MinFrlCharRate;
}DFM_INFO_S;

typedef struct
{
    HI_S32 s32Hactive;
    HI_S32 s32Vactive;
    HI_S32 s32Hblank;
    HI_S32 s32Vblank;
    HI_S32 s32Hsync;
    HI_S32 s32Hback;
    HI_S32 s32Hfront;
    HI_S32 s32Vsync;
    HI_S32 s32Vback;
    HI_S32 s32Vfront;
    HI_S32 s32VFreq;
    HI_S32 s32ColorDepth;
    HI_S32 s32PixelFormat; /* 420 :1 ; 422 :2 ; RGB/444 :3 */
    HI_S32 s32LaneNum;
    HI_S32 s32BitRate;
    HI_S32 s32AudioRate;
    HI_S32 s32PacketType;
    HI_S32 s32Layout;
    HI_S32 s32ACAT;
}DFM_IN_S;

HI_BOOL DRV_HDMI_DFMFormatCanbeSupport(DFM_IN_S *pstDFMIn);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_HDMI_DFM__ */

