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
#include "hdmi_reg_aon.h"
#include "hdmi_reg_ctrl.h"
#include "hdmi_hal_ddc.h"
#include "hdmi_hal_intf.h"
#include "hdmi_hal_ctrl.h"
#include "hdmi_product_define.h"
#include "hdmi_osal.h"

#define DDC_EDID_SALVE_ADDR         0xa0
#define DDC_HDCP_SALVE_ADDR         0x74
#define DDC_SCDC_SALVE_ADDR         0xa8

#define DDC_MAX_FIFO_SIZE           16

#define DDC_EXT_BLOCK_OFFSET        0x7e
#define DDC_MAX_EDID_EXT_NUM        (4-1)   /* (HDMI_EDID_MAX_BLOCK_NUM-1) */

typedef enum
{
    DDC_CMD_READ_SINGLE_NO_ACK  = 0x00,     /* 0'b0000: */
    DDC_CMD_READ_SINGLE_ACK     = 0x01,     /* 0'b0001: */
    DDC_CMD_READ_MUTI_NO_ACK    = 0x02,     /* 4'b0010: */
    DDC_CMD_READ_MUTI_ACK       = 0x03,     /* 4'b0011: */
    DDC_CMD_READ_SEGMENT_NO_ACK = 0x04,     /* 4'b0100: */
    DDC_CMD_READ_SEGMENT_ACK    = 0x05,     /* 4'b0101: */
    DDC_CMD_WRITE_MUTI_NO_ACK   = 0x06,     /* 4'b0110: */
    DDC_CMD_WRITE_MUTI_ACK      = 0x07,     /* 4'b0111: */
    DDC_CMD_FIFO_CLR            = 0x09,     /* 4'b1001: */
    DDC_CMD_SCL_DRV             = 0x0a,     /* 4'b1010: */

    DDC_CMD_MASTER_ABORT        = 0x0f,     /* 4'b1111: */

}DDC_ISSUE_CMD_E;


#define DDC_NULL_CHK(p) do{\
    if(HI_NULL == p)\
    {HDMI_WARN("null pointer!\n");\
    return HI_FAILURE;}\
}while(0)

#define DDC_INIT_CHK(pstMachInfo) do{\
    DDC_NULL_CHK(pstDdcInfo);\
    if(pstDdcInfo->stDdcRun.bInit != HI_TRUE)\
    {HDMI_WARN("ddc module didn't init!\n");\
    return HI_FAILURE;}\
}while(0)


/*===============  private data ================================*/

static DDC_INFO_S s_stDdcInfo[HDMI_DEVICE_ID_BUTT];

/*===============  private interface ==========================*/

static DDC_INFO_S *DdcInfoPtrGet(HDMI_DEVICE_ID_E enHdmi)
{
    if (enHdmi < HDMI_DEVICE_ID_BUTT)
    {
        return &s_stDdcInfo[enHdmi];
    }
    return HI_NULL;
}

static HI_S32  DdcAccessEnableWait(HI_U32 u32TimeOut)
{
    HI_S32      s32Ret = HI_SUCCESS;
    HI_U32      u32TmpTime = 0;

    HDMI_DDC_MST_ARB_REQ_cpu_ddc_reqSet(HI_TRUE);

    while(!HDMI_DDC_MST_ARB_ACK_cpu_ddc_req_ackGet())
    {
        HDMI_MSLEEP(1);
        u32TmpTime ++;
        if (u32TmpTime > u32TimeOut)
        {
            s32Ret = HI_FAILURE;
            break;
        }
    }

    return s32Ret;
}

static HI_S32  DdcAccessDisableWait(HI_U32 u32TimeOut)
{
    HI_S32      s32Ret = HI_SUCCESS;
    HI_U32      u32TmpTime = 0;

    HDMI_DDC_MST_ARB_REQ_cpu_ddc_reqSet(HI_FALSE);

    while(HDMI_DDC_MST_ARB_ACK_cpu_ddc_req_ackGet())
    {
        HDMI_MSLEEP(1);
        u32TmpTime += 1;
        if (u32TmpTime > u32TimeOut)
        {
            s32Ret = HI_FAILURE;
            break;
        }
    }

    return s32Ret;
}



static HI_S32 DdcSclWait(HI_U32 u32TimeOut)
{
    HI_S32      s32Ret = HI_SUCCESS;
    HI_U32      u32TmpTime = 0;

    while(!HDMI_DDC_MAN_CTRL_ddc_scl_stGet())
    {
        HDMI_MSLEEP(1);
        u32TmpTime += 1;
        if (u32TmpTime > u32TimeOut)
        {
            s32Ret = HI_FAILURE;
            break;
        }
    }

    return s32Ret;

}

static HI_S32 DdcSdaWait(HI_U32 u32TimeOut)
{
    HI_U32 u32TmpTimeOut   = 0;
    HI_S32 s32Ret          = HI_SUCCESS;

    if(!HDMI_DDC_MAN_CTRL_ddc_sda_stGet())
    {

        HDMI_DDC_MST_CTRL_dcc_man_enSet(HI_TRUE);
        while((!HDMI_DDC_MAN_CTRL_ddc_sda_stGet()) && u32TmpTimeOut++ < u32TimeOut)
        {
            /* pull scl high */
            HDMI_DDC_MAN_CTRL_ddc_scl_oenSet(HI_TRUE);
            HDMI_UDELAY(8);
            /* pull scl low */
            HDMI_DDC_MAN_CTRL_ddc_scl_oenSet(HI_FALSE);
            HDMI_UDELAY(8);
        }

        /* STOP contition */
        if(u32TmpTimeOut < u32TimeOut && (HDMI_DDC_MAN_CTRL_ddc_sda_stGet()))
        {
            /* pull sda low */
            HDMI_DDC_MAN_CTRL_ddc_sda_oenSet(HI_FALSE);
            HDMI_UDELAY(8);
            /* pull scl high */
            HDMI_DDC_MAN_CTRL_ddc_scl_oenSet(HI_TRUE);
            HDMI_UDELAY(8);
            /* pull sda high */
            HDMI_DDC_MAN_CTRL_ddc_sda_oenSet(HI_TRUE);
            HDMI_UDELAY(8);
            HDMI_INFO("deadlock clear success\n");
            s32Ret = HI_SUCCESS;
        }
        else
        {
            HDMI_WARN("deadlock clear  fail\n");
            s32Ret = HI_FAILURE;
        }
        HDMI_DDC_MST_CTRL_dcc_man_enSet(HI_FALSE);
    }

    return s32Ret;
}

static HI_S32 DdcInProgWait(HI_U32 u32TimeOut)
{
    HI_S32      s32Ret = HI_SUCCESS;
    HI_U32      u32TmpTime = 0;

    while(HDMI_PWD_MST_STATE_pwd_i2c_in_progGet())
    {
        HDMI_MSLEEP(1);
        u32TmpTime += 1;
        if (u32TmpTime > u32TimeOut)
        {
            s32Ret = HI_FAILURE;
            break;
        }
    }

    return s32Ret;
}

static HI_S32  DdcCmdIssue(DDC_CFG_S *pstCfg, HI_BOOL bReadIssue)
{
    HI_S32                  s32Ret = HI_SUCCESS;
    HI_U32                  u32SlaveAddr = 0;
    HI_U32                  u32Segment = 0;
    HI_U32                  u32Offset = 0;
    HI_U32                  s32DataSize = 0;

    DDC_NULL_CHK(pstCfg);

    u32Segment      = pstCfg->u8Segment;
    u32Offset       = pstCfg->u8Offset;
    s32DataSize     = pstCfg->s32DataSize;

    switch(pstCfg->enFuncType)
    {
        case DDC_FUNC_TYPE_EDID:
            u32SlaveAddr = DDC_EDID_SALVE_ADDR;
            break;

        case DDC_FUNC_TYPE_HDCP:
            u32SlaveAddr = DDC_HDCP_SALVE_ADDR;
            break;

        case DDC_FUNC_TYPE_SCDC:
            u32SlaveAddr = DDC_SCDC_SALVE_ADDR;
            break;

        default:
            HDMI_WARN("un-known DDC function type,wrong slaveaddr!\n");
            break;
    }

    if (pstCfg->enMasterMode == DDC_MASTER_MODE_PWD)
    {
        HDMI_PWD_MST_CMD_pwd_mst_cmdSet(DDC_CMD_FIFO_CLR);
        HDMI_PWD_SLAVE_CFG_pwd_slave_addrSet(u32SlaveAddr);
        HDMI_PWD_SLAVE_CFG_pwd_slave_segSet(u32Segment);
        HDMI_PWD_SLAVE_CFG_pwd_slave_offsetSet(u32Offset);
        HDMI_PWD_DATA_CNT_pwd_data_out_cntSet(s32DataSize);
        HDMI_UDELAY(8);
        HDMI_PWD_MST_CMD_pwd_mst_cmdSet(pstCfg->enIssueMode);
    }
    else
    {
        HDMI_ERR("unknow enMasterMode=%u\n",pstCfg->enMasterMode);
    }

    return s32Ret;
}

#ifndef HDMI_BUILD_IN_BOOT
static HI_S32  DdcDataPwdIssue(DDC_CFG_S *pstCfg, HI_BOOL bReadIssue)
{
    HI_U32      i = 0;
    HI_U32      u32Retry = 0;
    HI_U32      s32DataSize = 0;
    HI_U8       *pu8Data = HI_NULL;
    HI_U32      u32TimeStart = 0;
    HI_U32      u32TimeCurr = 0;

    DDC_NULL_CHK(pstCfg);
    pu8Data  = pstCfg->pu8Data;
    u32Retry = pstCfg->u32IssueTimeOut < DDC_DEFAULT_TIMEOUT_ISSUE ? DDC_DEFAULT_TIMEOUT_ISSUE : pstCfg->u32SdaTimeOut;

    /* read issue */
    if (bReadIssue)
    {
        for (s32DataSize=0;s32DataSize < pstCfg->s32DataSize;s32DataSize++,pu8Data++)
        {
            /* maybe msleep(1) cost 20ms, 30 times, total 600ms. */
            u32TimeStart = HDMI_OSAL_GetTimeInMs();
            u32TimeCurr = u32TimeStart;
            /* when read-fifo empty, every byte wait a max timeout */
            for (i = 0 ; ((i < u32Retry) && (HDMI_PWD_MST_STATE_pwd_fifo_emptyGet() || (HDMI_PWD_DATA_CNT_pwd_fifo_data_cntGet()==0))) \
                      && ((u32TimeCurr - u32TimeStart) <= DDC_DEFAULT_RETRY_TIMEOUT_ISSUE); \
                i++)
            {
                /* wait ddc status update after DDC cmd set. */
                HDMI_MSLEEP(1);

                if (HDMI_DDC_MST_STATE_ddc_i2c_no_ackGet() || HDMI_DDC_MST_STATE_ddc_i2c_bus_lowGet())
                {
                    HDMI_PWD_MST_CMD_pwd_mst_cmdSet(DDC_CMD_MASTER_ABORT);

                    HDMI_WARN("DDC status error!\n");
                    return HI_FAILURE;
                }

                u32TimeCurr = HDMI_OSAL_GetTimeInMs();
            }

            if ((i >= u32Retry) || ((u32TimeCurr - u32TimeStart) > DDC_DEFAULT_RETRY_TIMEOUT_ISSUE))
            {
                if (pstCfg->enFuncType != DDC_FUNC_TYPE_SCDC)
                {
                    HDMI_WARN("read fifo u32Retry=%u ms,size=%d,timeout:%d!\n", u32Retry, pstCfg->s32DataSize, (u32TimeCurr - u32TimeStart));
                }
                else
                {
                    HDMI_INFO("read fifo u32Retry=%u ms,size=%d,timeout:%d!\n", u32Retry, pstCfg->s32DataSize, (u32TimeCurr - u32TimeStart));
                }
                return HI_FAILURE;
            }

            if (pu8Data)
            {
                *pu8Data = HDMI_PWD_FIFO_RDATA_pwd_fifo_data_outGet();
                /* the fifo status is not refresh promptly, so re-read the fifo status and delay 1us if the fifo is empty,
                   wait the data ready. it must delay 1us(from wzg @2015.12.28) after read fifo data. */
                HDMI_UDELAY(1);
            }
            else
            {
                HDMI_ERR("edid &pu8Data[%d]=null\n",s32DataSize);
                return HI_FAILURE;
            }
        }
    }
    /* write issue */
    else
    {
        for (s32DataSize=0;s32DataSize < pstCfg->s32DataSize;s32DataSize++,pu8Data++)
        {
            /* maybe msleep(1) cost 20ms, every byte wait a max timeout 60ms. */
            u32TimeStart = HDMI_OSAL_GetTimeInMs();
            u32TimeCurr = u32TimeStart;
            /* when write-fifo full, every byte wait a max timeout and retry times */
            for (i=0 ; (((i < u32Retry) && (HDMI_PWD_DATA_CNT_pwd_fifo_data_cntGet()>=DDC_MAX_FIFO_SIZE)) \
                    && ((u32TimeCurr - u32TimeStart) <= DDC_DEFAULT_RETRY_TIMEOUT_ISSUE)); i++)
            {
                /* wait ddc status update after DDC cmd set. */
                HDMI_MSLEEP(1);

                if (HDMI_DDC_MST_STATE_ddc_i2c_no_ackGet() || HDMI_DDC_MST_STATE_ddc_i2c_bus_lowGet())
                {
                    HDMI_PWD_MST_CMD_pwd_mst_cmdSet(DDC_CMD_MASTER_ABORT);

                    HDMI_WARN("DDC status error!\n");
                    return HI_FAILURE;
                }

                u32TimeCurr = HDMI_OSAL_GetTimeInMs();
            }

            if (i >= u32Retry || ((u32TimeCurr - u32TimeStart) > DDC_DEFAULT_RETRY_TIMEOUT_ISSUE))
            {
                if (pstCfg->enFuncType != DDC_FUNC_TYPE_SCDC)
                {
                    HDMI_ERR("write fifo u32Retry=%u ms,size=%d,timeout:%d!\n", u32Retry, pstCfg->s32DataSize, (u32TimeCurr - u32TimeStart));
                }
                else
                {
                    HDMI_INFO("write fifo u32Retry=%u ms,size=%d,timeout:%d!\n", u32Retry, pstCfg->s32DataSize, (u32TimeCurr - u32TimeStart));
                }
                return HI_FAILURE;
            }

            if (pu8Data)
            {
                HDMI_PWD_FIFO_WDATA_pwd_fifo_data_inSet(*pu8Data);
                HDMI_UDELAY(1);
            }
            else
            {
                HDMI_ERR("edid &pu8Data[%d]=null\n",s32DataSize);
                return HI_FAILURE;
            }
        }

    }

    return s32DataSize;
}

#else
static HI_S32  DdcDataPwdIssue(DDC_CFG_S *pstCfg, HI_BOOL bReadIssue)
{
    HI_U32      i = 0;
    HI_U32      u32Retry = 0;
    HI_U32      s32DataSize = 0;
    HI_U8       *pu8Data = HI_NULL;

    DDC_NULL_CHK(pstCfg);
    pu8Data  = pstCfg->pu8Data;
    u32Retry = pstCfg->u32IssueTimeOut < DDC_DEFAULT_TIMEOUT_ISSUE ? DDC_DEFAULT_TIMEOUT_ISSUE : pstCfg->u32SdaTimeOut;

    /* read issue */
    if (bReadIssue)
    {
        for (s32DataSize=0;s32DataSize < pstCfg->s32DataSize;s32DataSize++,pu8Data++)
        {
            /* when read-fifo empty, every byte wait a max timeout */
            for (i = 0 ; ((i < u32Retry) && (HDMI_PWD_MST_STATE_pwd_fifo_emptyGet() || (HDMI_PWD_DATA_CNT_pwd_fifo_data_cntGet()==0))); i++)
            {
                /* wait ddc status update after DDC cmd set. */
                HDMI_MSLEEP(1);

                if (HDMI_DDC_MST_STATE_ddc_i2c_no_ackGet() || HDMI_DDC_MST_STATE_ddc_i2c_bus_lowGet())
                {
                    HDMI_PWD_MST_CMD_pwd_mst_cmdSet(DDC_CMD_MASTER_ABORT);

                    HDMI_WARN("DDC status error!\n");
                    return HI_FAILURE;
                }
            }

            if (i >= u32Retry)
            {
                if (pstCfg->enFuncType != DDC_FUNC_TYPE_SCDC)
                {
                    HDMI_WARN("read fifo u32Retry=%u ms,size=%d!\n", u32Retry, pstCfg->s32DataSize);
                }
                else
                {
                    HDMI_INFO("read fifo u32Retry=%u ms,size=%d\n", u32Retry, pstCfg->s32DataSize);
                }
                return HI_FAILURE;
            }

            if (pu8Data)
            {
                *pu8Data = HDMI_PWD_FIFO_RDATA_pwd_fifo_data_outGet();
                /* the fifo status is not refresh promptly, so re-read the fifo status and delay 1us if the fifo is empty,
                   wait the data ready. it must delay 1us(from wzg @2015.12.28) after read fifo data. */
                HDMI_UDELAY(1);
            }
            else
            {
                HDMI_ERR("edid &pu8Data[%d]=null\n",s32DataSize);
                return HI_FAILURE;
            }
        }
    }
    /* write issue */
    else
    {
        for (s32DataSize=0;s32DataSize < pstCfg->s32DataSize;s32DataSize++,pu8Data++)
        {
            /* when write-fifo full, every byte wait a max timeout and retry times */
            for (i=0 ; (((i < u32Retry) && (HDMI_PWD_DATA_CNT_pwd_fifo_data_cntGet()>=DDC_MAX_FIFO_SIZE))); i++)
            {
                /* wait ddc status update after DDC cmd set. */
                HDMI_MSLEEP(1);

                if (HDMI_DDC_MST_STATE_ddc_i2c_no_ackGet() || HDMI_DDC_MST_STATE_ddc_i2c_bus_lowGet())
                {
                    HDMI_PWD_MST_CMD_pwd_mst_cmdSet(DDC_CMD_MASTER_ABORT);

                    HDMI_WARN("DDC status error!\n");
                    return HI_FAILURE;
                }
            }

            if (i >= u32Retry)
            {
                if (pstCfg->enFuncType != DDC_FUNC_TYPE_SCDC)
                {
                    HDMI_ERR("write fifo u32Retry=%u ms,size=%d\n", u32Retry, pstCfg->s32DataSize);
                }
                else
                {
                    HDMI_INFO("write fifo u32Retry=%u ms,size=%d!\n", u32Retry, pstCfg->s32DataSize);
                }
                return HI_FAILURE;
            }

            if (pu8Data)
            {
                HDMI_PWD_FIFO_WDATA_pwd_fifo_data_inSet(*pu8Data);
                HDMI_UDELAY(1);
            }
            else
            {
                HDMI_ERR("edid &pu8Data[%d]=null\n",s32DataSize);
                return HI_FAILURE;
            }
        }

    }

    return s32DataSize;
}
#endif


static HI_S32 DdcDataIssue(DDC_CFG_S *pstCfg, HI_BOOL bReadIssue)
{
    if (pstCfg->enMasterMode==DDC_MASTER_MODE_PWD)
    {
        return DdcDataPwdIssue(pstCfg,bReadIssue);
    }
    else
    {
        HDMI_ERR("unknow enMasterMode=%u,fail!\n",pstCfg->enMasterMode);
        return HI_FAILURE;
    }

}


/*===============  public interface ==========================*/

HI_S32 HAL_HDMI_DdcInit(HDMI_DEVICE_ID_E enHdmi)
{
    DDC_INFO_S *pstDdcInfo = DdcInfoPtrGet(enHdmi);

    DDC_NULL_CHK(pstDdcInfo);

    if (!pstDdcInfo->stDdcRun.bInit)
    {
#ifndef HDMI_BUILD_IN_BOOT
        HDMI_INIT_MUTEX(pstDdcInfo->stDdcWrMutex);
        HDMI_MEMSET( pstDdcInfo->astDdcRecord , 0 ,sizeof(pstDdcInfo->astDdcRecord));
#endif
        pstDdcInfo->stDdcRun.bInit          = HI_TRUE;
        pstDdcInfo->stDdcRun.u32ElemPtr     = 0;
        pstDdcInfo->stDdcRun.u32TotalNum    = 0;
    }

    return HI_SUCCESS;
}

HI_S32 HAL_HDMI_DdcDeInit(HDMI_DEVICE_ID_E enHdmi)
{
    DDC_INFO_S *pstDdcInfo = DdcInfoPtrGet(enHdmi);

    DDC_NULL_CHK(pstDdcInfo);
    DDC_INIT_CHK(pstDdcInfo);
#ifndef HDMI_BUILD_IN_BOOT
    HAL_HDMI_DdcReset(enHdmi);
    HDMI_DESTORY_MUTEX(pstDdcInfo->stDdcWrMutex);
#endif
    pstDdcInfo->stDdcRun.bInit = HI_FALSE;

    return HI_SUCCESS;
}

#ifndef HDMI_BUILD_IN_BOOT
HI_S32 HAL_HDMI_DdcReset(HDMI_DEVICE_ID_E enHdmi)
{
    HI_U32              i = 0;
    DDC_RECORD_ELEM_S   *pstTmpElem = HI_NULL;
    DDC_INFO_S *pstDdcInfo = DdcInfoPtrGet(enHdmi);

    DDC_NULL_CHK(pstDdcInfo);
    DDC_INIT_CHK(pstDdcInfo);

    for (i=0; i < DDC_MAX_RECORD_NUM ; i++)
    {
        pstTmpElem = &pstDdcInfo->astDdcRecord[i];
        if (pstTmpElem->pu8Data)
        {
            HDMI_VFREE(pstTmpElem->pu8Data);
        }
        HDMI_MEMSET( pstTmpElem , 0 ,sizeof(DDC_RECORD_ELEM_S));
    }

    pstDdcInfo->stDdcRun.u32ElemPtr     = 0 ;
    pstDdcInfo->stDdcRun.u32TotalNum    = 0 ;

    return HAL_HDMI_DdcErrClear(enHdmi);

}
#endif

HI_S32 HAL_HDMI_DdcErrClear(HDMI_DEVICE_ID_E enHdmi)
{
    HI_S32      s32Ret = HI_SUCCESS;
    DDC_INFO_S *pstDdcInfo = DdcInfoPtrGet(enHdmi);
    DDC_NULL_CHK(pstDdcInfo);
    DDC_INIT_CHK(pstDdcInfo);

    /* In prog check */
    if (DdcInProgWait(DDC_DEFAULT_TIMEOUT_IN_PROG) != HI_SUCCESS)
    {
        HDMI_WARN("error clr, wait in prog timeout!\n");
        s32Ret = HI_FAILURE;
    }

    /* scl check */
    if (DdcSclWait(DDC_DEFAULT_TIMEOUT_SCL) != HI_SUCCESS)
    {
        HDMI_WARN("error clr,wait scl timeout!\n");
        s32Ret = HI_FAILURE;
    }

    /* sda check */
    if (DdcSdaWait(DDC_DEFAULT_TIMEOUT_SDA) != HI_SUCCESS)
    {
        HDMI_WARN("error clr,wait sda timeout!\n");
        s32Ret = HI_FAILURE;
    }


    if (s32Ret == HI_SUCCESS)
    {
        HDMI_INFO("error clr success!\n");
    }

    return s32Ret;

}

#ifndef HDMI_BUILD_IN_BOOT

HI_S32 HAL_HDMI_DdcIssue(HDMI_DEVICE_ID_E enHdmi,DDC_CFG_S *pstCfg)
{
    HI_S32      s32Ret = HI_SUCCESS;
    HI_BOOL     bReadIssue = HI_FALSE;
    DDC_RECORD_ELEM_S  *pstTmpElem = HI_NULL;
    DDC_INFO_S *pstDdcInfo = DdcInfoPtrGet(enHdmi);

    DDC_NULL_CHK(pstDdcInfo);
    DDC_NULL_CHK(pstCfg);
    DDC_INIT_CHK(pstDdcInfo);

    pstDdcInfo->stDdcRun.u32ElemPtr = (1+pstDdcInfo->stDdcRun.u32ElemPtr) % DDC_MAX_RECORD_NUM;
    if (pstDdcInfo->stDdcRun.u32TotalNum <= DDC_MAX_RECORD_NUM)
    {
        pstDdcInfo->stDdcRun.u32TotalNum++;
    }

    pstTmpElem = &pstDdcInfo->astDdcRecord[pstDdcInfo->stDdcRun.u32ElemPtr];

    HDMI_MEMSET( pstTmpElem , 0 ,sizeof(DDC_RECORD_ELEM_S));
    HDMI_MEMCPY( &pstTmpElem->stCfg , pstCfg ,sizeof(DDC_CFG_S));

    pstTmpElem->u64StartTime = HAL_HDMI_MachMsGet();
    HDMI_MUTEX_LOCK(pstDdcInfo->stDdcWrMutex);

    /* Access check */
    if (pstCfg->enMasterMode == DDC_MASTER_MODE_PWD)
    {
        if (DdcAccessEnableWait(pstCfg->u32AccessTimeOut) != HI_SUCCESS)
        {
            HDMI_ERR("wait access bus timeout!\n");
            pstTmpElem->unErr.u32.bzAccessWaitTimeOut = HI_TRUE;
            goto RETURN;
        }
    }

    /* scl check */
    if (DdcSclWait(pstCfg->u32SclTimeOut) != HI_SUCCESS)
    {
        HDMI_ERR("wait scl timeout!\n");
        pstTmpElem->unErr.u32.bzBusLowScl = HI_TRUE;
        goto RETURN;
    }

    /* sda check */
    if (DdcSdaWait(pstCfg->u32SdaTimeOut)!= HI_SUCCESS)
    {
        HDMI_ERR("wait sda timeout!\n");
        pstTmpElem->unErr.u32.bzBusLowSda = HI_TRUE;
        goto RETURN;
    }

    /* jude read/write issue */
    if (pstCfg->enIssueMode <= DDC_MODE_READ_SEGMENT_ACK)
    {
        bReadIssue = HI_TRUE;
    }
    else if (pstCfg->enIssueMode < DDC_MODE_BUTT)
    {
        bReadIssue      = HI_FALSE;
    }
    else
    {
        HDMI_ERR("un-known ddc issue mode!\n");
        pstTmpElem->unErr.u32.bzErrIsseuMode = HI_TRUE;
        goto RETURN;
    }

    /* Issue command */
    if (DdcCmdIssue(pstCfg,bReadIssue)!= HI_SUCCESS)
    {
        HDMI_ERR("command issue fail!\n");
        goto RETURN;
    }

    /* issue data */
    if ((s32Ret=DdcDataIssue(pstCfg,bReadIssue)) <= 0)
    {
        HDMI_INFO("data issue fail!\n");
        pstTmpElem->unErr.u32.bzIssueTimeOut = HI_TRUE;
        goto RETURN;
    }

RETURN:

    /* In prog check */
    if (DdcInProgWait(pstCfg->u32InProgTimeOut) != HI_SUCCESS)
    {
        HDMI_WARN("wait in prog timeout!\n");
        pstTmpElem->unErr.u32.bzInProgTimeOut = HI_TRUE;
    }

    if (pstTmpElem->unErr.u32Word)
    {
        HDMI_HPD_RSEN_S stHpd = {0};
        HAL_HDMI_CtrlHpdRsenGet(enHdmi,&stHpd);
        if ( !stHpd.bHpdOn )
        {
            HDMI_WARN("hpd no exist!\n");
            pstTmpElem->unErr.u32.bzHpdNoExist = HI_TRUE;
        }
        else if (HAL_HDMI_DdcErrClear(enHdmi) != HI_SUCCESS)
        {
            HDMI_WARN("error clear fail!\n");
            pstTmpElem->unErr.u32.bzClrErrFail = 1;
        }
    }

    if (pstCfg->enMasterMode == DDC_MASTER_MODE_PWD)
    {
        if (DdcAccessDisableWait(pstCfg->u32AccessTimeOut) != HI_SUCCESS)
        {
            HDMI_WARN("wait access disalbe timeout!\n");
        }
    }

    pstTmpElem->u64TimeLen = HAL_HDMI_MachMsGet() - pstTmpElem->u64StartTime ;
    HDMI_MUTEX_UNLOCK(pstDdcInfo->stDdcWrMutex);

    return s32Ret;
}
#else
HI_S32 HAL_HDMI_DdcIssue(HDMI_DEVICE_ID_E enHdmi, DDC_CFG_S *pstCfg)
{
    HI_S32      s32Ret = HI_SUCCESS;
    HI_BOOL     bReadIssue = HI_FALSE;
    DDC_INFO_S *pstDdcInfo = DdcInfoPtrGet(enHdmi);

    DDC_NULL_CHK(pstDdcInfo);
    DDC_NULL_CHK(pstCfg);
    DDC_INIT_CHK(pstDdcInfo);

    /* Access check */
    if (pstCfg->enMasterMode == DDC_MASTER_MODE_PWD)
    {
        if (DdcAccessEnableWait(pstCfg->u32AccessTimeOut) != HI_SUCCESS)
        {
            HDMI_ERR("wait access bus timeout!\n");
            goto RETURN;
        }
    }

    /* scl check */
    if (DdcSclWait(pstCfg->u32SclTimeOut) != HI_SUCCESS)
    {
        HDMI_ERR("wait scl timeout!\n");
        goto RETURN;
    }

    /* sda check */
    if (DdcSdaWait(pstCfg->u32SdaTimeOut)!= HI_SUCCESS)
    {
        HDMI_ERR("wait sda timeout!\n");
        goto RETURN;
    }

    /* jude read/write issue */
    if (pstCfg->enIssueMode <= DDC_MODE_READ_SEGMENT_ACK)
    {
        bReadIssue = HI_TRUE;
    }
    else if (pstCfg->enIssueMode < DDC_MODE_BUTT)
    {
        bReadIssue      = HI_FALSE;
    }
    else
    {
        HDMI_ERR("un-know ddc issue mode!\n");
        goto RETURN;
    }

    /*  Issue command */
    if (DdcCmdIssue(pstCfg,bReadIssue)!= HI_SUCCESS)
    {
        HDMI_ERR("command issue fail!\n");
        goto RETURN;
    }

    /* issue data */
    if ((s32Ret=DdcDataIssue(pstCfg,bReadIssue)) <= 0)
    {
        HDMI_INFO("data issue fail!\n");
        goto RETURN;
    }

RETURN:

    /* In prog check */
    if (DdcInProgWait(pstCfg->u32InProgTimeOut) != HI_SUCCESS)
    {
        HDMI_WARN("wait in prog timeout!\n");
    }

    if (pstCfg->enMasterMode == DDC_MASTER_MODE_PWD)
    {
        if (DdcAccessDisableWait(pstCfg->u32AccessTimeOut) != HI_SUCCESS)
        {
            HDMI_WARN("wait access disalbe timeout!\n");
        }
    }

    return s32Ret;
}
#endif



HI_S32 HAL_HDMI_DdcDefaultCfgGet(HDMI_DEVICE_ID_E enHdmi,DDC_CFG_S *pstCfg)
{
    DDC_INFO_S      *pstDdcInfo = DdcInfoPtrGet(enHdmi);

    DDC_NULL_CHK(pstDdcInfo);
    DDC_NULL_CHK(pstCfg);
    DDC_INIT_CHK(pstDdcInfo);

    pstCfg->u8Segment           = 0;
    pstCfg->u8Offset            = 0;
    pstCfg->enFuncType          = DDC_FUNC_TYPE_EDID;
    pstCfg->enIssueMode         = DDC_MODE_READ_MUTIL_NO_ACK;
    pstCfg->u32Speed            = 0;          /* TO DO */
    pstCfg->enMasterMode        = DDC_MASTER_MODE_PWD; /* DDC_MASTER_MODE_AON; */

    pstCfg->u32AccessTimeOut    = DDC_DEFAULT_TIMEOUT_ACCESS;
    pstCfg->u32HpdTimeOut       = DDC_DEFAULT_TIMEOUT_HPD;
    pstCfg->u32InProgTimeOut    = DDC_DEFAULT_TIMEOUT_IN_PROG;
    pstCfg->u32SclTimeOut       = DDC_DEFAULT_TIMEOUT_SCL;
    pstCfg->u32SdaTimeOut       = DDC_DEFAULT_TIMEOUT_SDA;
    pstCfg->u32IssueTimeOut     = DDC_DEFAULT_TIMEOUT_ISSUE;

    pstCfg->s32DataSize         = 0;
    pstCfg->pu8Data             = HI_NULL;

    return HI_SUCCESS;

}

#ifndef HDMI_BUILD_IN_BOOT

HI_S32 HAL_HDMI_DdcEdidRawGet(HDMI_DEVICE_ID_E enHdmi,HI_S32 s32Size,HI_U8 *pu8Data)
{
    HI_U8           u8ExtBlockNum = 0;
    HI_S32          s32Ret = 0;
    DDC_CFG_S       stCfg = {0};

    DDC_NULL_CHK(pu8Data);

    HAL_HDMI_DdcDefaultCfgGet(enHdmi,&stCfg);
    stCfg.u8Segment         = 0;
    stCfg.enFuncType        = DDC_FUNC_TYPE_EDID;
    stCfg.enIssueMode       = DDC_MODE_READ_MUTIL_NO_ACK;
    stCfg.s32DataSize       = HDMI_EDID_BLOCK_SIZE;
    stCfg.pu8Data           = &pu8Data[0];

    /* block 0 */
    if( 0 < (s32Ret +=HAL_HDMI_DdcIssue(enHdmi,&stCfg)) )
    {
        HDMI_INFO("EDID block 0 read success!\n");
        /* extension number */
        u8ExtBlockNum = pu8Data[DDC_EXT_BLOCK_OFFSET];
        if(u8ExtBlockNum > DDC_MAX_EDID_EXT_NUM)
        {
            HDMI_WARN("sink edid blocks num=%u ,but we only support %u\n",u8ExtBlockNum,DDC_MAX_EDID_EXT_NUM);
            u8ExtBlockNum = DDC_MAX_EDID_EXT_NUM;
        }

        if (u8ExtBlockNum)
        {
            /* block 1 */
            stCfg.pu8Data += stCfg.s32DataSize;
            stCfg.u8Offset = HDMI_EDID_BLOCK_SIZE;
            if( 0 < (s32Ret +=HAL_HDMI_DdcIssue(enHdmi,&stCfg)) )
            {
                HDMI_INFO("EDID EXT-block 1 read success!\n");
                u8ExtBlockNum--;
                if (u8ExtBlockNum)
                {
                    /* block 2&3 */
                    stCfg.pu8Data += stCfg.s32DataSize;
                    stCfg.u8Offset = 0;
                    stCfg.enIssueMode       = DDC_MODE_READ_SEGMENT_NO_ACK;
                    stCfg.u8Segment         = 1;
                    stCfg.s32DataSize       = HDMI_EDID_BLOCK_SIZE*u8ExtBlockNum;
                    if( 0  < (s32Ret +=HAL_HDMI_DdcIssue(enHdmi,&stCfg)) )
                    {
                        HDMI_INFO("EDID read EXT-block 2~%u success!\n",u8ExtBlockNum+1);
                    }
                    else
                    {
                        HDMI_WARN("EDID read EXT-block 2~%u fail!\n",u8ExtBlockNum+1);
                    }
                }

            }
            else
            {
                HDMI_WARN("EDID EXT-block 1 read fail!\n");
            }

        }

    }
    else
    {
        HDMI_WARN("edid block 0 read fail!\n");
    }

    return s32Ret ;
}


HI_S32 HAL_HDMI_DdcStatusGet(HDMI_DEVICE_ID_E enHdmi,DDC_RUN_S *pstDdcRun)
{
    DDC_INFO_S      *pstDdcInfo = DdcInfoPtrGet(enHdmi);

    DDC_NULL_CHK(pstDdcInfo);
    DDC_NULL_CHK(pstDdcRun);
    DDC_INIT_CHK(pstDdcInfo);

    HDMI_MEMCPY(pstDdcRun , &pstDdcInfo->stDdcRun , sizeof(DDC_RUN_S));

    return HI_SUCCESS;
}

HI_S32 HAL_HDMI_DdcElemGet(HDMI_DEVICE_ID_E enHdmi,HI_U32 u32Idx,DDC_RECORD_ELEM_S *pstElem)
{
    DDC_INFO_S      *pstDdcInfo = DdcInfoPtrGet(enHdmi);

    DDC_NULL_CHK(pstDdcInfo);
    DDC_NULL_CHK(pstElem);
    DDC_INIT_CHK(pstDdcInfo);

    if (u32Idx >= pstDdcInfo->stDdcRun.u32TotalNum || u32Idx >= DDC_MAX_RECORD_NUM)
    {
        HDMI_WARN("wrong u32Idx,Range [0,u32TotalNum-1]=[0,%u]\n",
            pstDdcInfo->stDdcRun.u32TotalNum>0 ? pstDdcInfo->stDdcRun.u32TotalNum : 0);
        return HI_FAILURE;
    }

    HDMI_MEMCPY(pstElem,&pstDdcInfo->astDdcRecord[u32Idx],sizeof(DDC_RECORD_ELEM_S));

    return HI_SUCCESS;
}

#endif


