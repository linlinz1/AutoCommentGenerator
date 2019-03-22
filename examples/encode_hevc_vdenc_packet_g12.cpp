/*
* Copyright (c) 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/
//!
//! \file     encode_hevc_vdenc_packet_g12.cpp
//! \brief    Defines the interface for hevc encode vdenc packet of G12
//!
#include "encode_hevc_vdenc_packet_g12.h"
#include "encode_utils.h"
#include "encode_hevc_pipeline.h"
#include "encode_hevc_basic_feature.h"
#include "encode_status_report_defs.h"
#include "codechal_huc_cmd_initializer_ext.h"
#include "mos_solo_generic.h"
#include "encode_status_report_defs.h"
#include "encode_tile.h"
#include "encode_hevc_brc.h"

namespace encode
{
    MOS_STATUS HevcVdencPktG12::AllocateResources()
    {
        ENCODE_FUNC_CALL();

        MOS_STATUS eStatus = MOS_STATUS_SUCCESS;
        HevcVdencPkt::AllocateResources();
        MHW_VDBOX_HCP_BUFFER_SIZE_PARAMS hcpBufSizeParam;
        MOS_ZeroMemory(&hcpBufSizeParam, sizeof(hcpBufSizeParam));

        hcpBufSizeParam.ucMaxBitDepth = m_basicFeature->m_bitDepth;
        hcpBufSizeParam.ucChromaFormat = m_basicFeature->m_chromaFormat;
        // We should move the buffer allocation to picture level if the size is dependent on LCU size
        hcpBufSizeParam.dwCtbLog2SizeY = 6; //assume Max LCU size
        hcpBufSizeParam.dwPicWidth = MOS_ALIGN_CEIL(m_basicFeature->m_frameWidth, m_basicFeature->m_maxLCUSize);
        hcpBufSizeParam.dwPicHeight = MOS_ALIGN_CEIL(m_basicFeature->m_frameHeight, m_basicFeature->m_maxLCUSize);

        MOS_ALLOC_GFXRES_PARAMS allocParamsForBufferLinear;
        MOS_ZeroMemory(&allocParamsForBufferLinear, sizeof(MOS_ALLOC_GFXRES_PARAMS));
        allocParamsForBufferLinear.Type = MOS_GFXRES_BUFFER;
        allocParamsForBufferLinear.TileType = MOS_TILE_LINEAR;
        allocParamsForBufferLinear.Format = Format_Buffer;

        // PAK stream-out buffer
        allocParamsForBufferLinear.dwBytes = CODECHAL_HEVC_PAK_STREAMOUT_SIZE;
        allocParamsForBufferLinear.pBufName = "Pak StreamOut Buffer";
        m_resStreamOutBuffer[0] = m_allocator->AllocateResource(allocParamsForBufferLinear,false);

        // Metadata Line buffer
        eStatus = (MOS_STATUS)m_hcpInterface->GetHevcBufferSize(
            MHW_VDBOX_HCP_INTERNAL_BUFFER_META_LINE,
            &hcpBufSizeParam);
        if (eStatus != MOS_STATUS_SUCCESS)
        {
            ENCODE_ASSERTMESSAGE("Failed to get the size for Metadata Line Buffer.");
            return eStatus;
        }
        allocParamsForBufferLinear.dwBytes = hcpBufSizeParam.dwBufferSize;
        allocParamsForBufferLinear.pBufName = "MetadataLineBuffer";
        m_resMetadataLineBuffer = m_allocator->AllocateResource(allocParamsForBufferLinear,false);

        // Metadata Tile Line buffer
        eStatus = (MOS_STATUS)m_hcpInterface->GetHevcBufferSize(
            MHW_VDBOX_HCP_INTERNAL_BUFFER_META_TILE_LINE,
            &hcpBufSizeParam);
        if (eStatus != MOS_STATUS_SUCCESS)
        {
            ENCODE_ASSERTMESSAGE("Failed to get the size for Metadata Tile Line Buffer.");
            return eStatus;
        }
        allocParamsForBufferLinear.dwBytes = hcpBufSizeParam.dwBufferSize;
        allocParamsForBufferLinear.pBufName = "MetadataTileLineBuffer";
        m_resMetadataTileLineBuffer = m_allocator->AllocateResource(allocParamsForBufferLinear,false);

        // Metadata Tile Column buffer
        eStatus = (MOS_STATUS)m_hcpInterface->GetHevcBufferSize(
            MHW_VDBOX_HCP_INTERNAL_BUFFER_META_TILE_COL,
            &hcpBufSizeParam);
        if (eStatus != MOS_STATUS_SUCCESS)
        {
            ENCODE_ASSERTMESSAGE("Failed to get the size for Metadata Tile Column Buffer.");
            return eStatus;
        }
        allocParamsForBufferLinear.dwBytes = hcpBufSizeParam.dwBufferSize;
        allocParamsForBufferLinear.pBufName = "MetadataTileColumnBuffer";
        m_resMetadataTileColumnBuffer = m_allocator->AllocateResource(allocParamsForBufferLinear,false);

        // Lcu ILDB StreamOut buffer
        // TODO: Allocate the buffer size according to B-spec
        // This is not enabled with HCP_PIPE_MODE_SELECT yet, placeholder here
        allocParamsForBufferLinear.dwBytes = CODECHAL_CACHELINE_SIZE;
        allocParamsForBufferLinear.pBufName = "LcuILDBStreamOutBuffer";
        m_resLCUIldbStreamOutBuffer = m_allocator->AllocateResource(allocParamsForBufferLinear,false);

        // Allocate SSE Source Pixel Row Store Buffer
        uint32_t maxTileColumns    = MOS_ROUNDUP_DIVIDE(m_basicFeature->m_frameWidth, CODECHAL_HEVC_MIN_TILE_SIZE);
        allocParamsForBufferLinear.dwBytes  = 2 * m_basicFeature->m_sizeOfSseSrcPixelRowStoreBufferPerLcu * (m_basicFeature->m_widthAlignedMaxLCU + 3 * maxTileColumns);
        allocParamsForBufferLinear.pBufName = "SseSrcPixelRowStoreBuffer";
        m_resSSESrcPixelRowStoreBuffer = m_allocator->AllocateResource(allocParamsForBufferLinear,false);

        uint32_t frameWidthInCus = CODECHAL_GET_WIDTH_IN_BLOCKS(m_basicFeature->m_frameWidth, CODECHAL_HEVC_MIN_CU_SIZE);
        uint32_t frameHeightInCus = CODECHAL_GET_WIDTH_IN_BLOCKS(m_basicFeature->m_frameHeight, CODECHAL_HEVC_MIN_CU_SIZE);
        uint32_t frameWidthInLCUs = CODECHAL_GET_WIDTH_IN_BLOCKS(m_basicFeature->m_frameWidth, CODECHAL_HEVC_MAX_LCU_SIZE_G10);
        uint32_t frameHeightInLCUs = CODECHAL_GET_WIDTH_IN_BLOCKS(m_basicFeature->m_frameHeight, CODECHAL_HEVC_MAX_LCU_SIZE_G10);
        // PAK CU Level Streamout Data:   DW57-59 in HCP pipe buffer address command
        // One CU has 16-byte. But, each tile needs to be aliged to the cache line
        auto size = MOS_ALIGN_CEIL(frameWidthInCus * frameHeightInCus * 16, CODECHAL_CACHELINE_SIZE);
        allocParamsForBufferLinear.dwBytes = size;
        allocParamsForBufferLinear.pBufName = "PAK CU Level Streamout Data";
        m_resPakcuLevelStreamOutData = m_allocator->AllocateResource(allocParamsForBufferLinear,false);


        //TODO:Check nullptr for all the allocated resources.
        return eStatus;

    }

    MOS_STATUS HevcVdencPktG12::Prepare()
    {
        ENCODE_FUNC_CALL();

        HevcVdencPkt::Prepare();

        //ENCODE_CHK_STATUS_RETURN(m_trackedBuf->AllocateForCurrFrame());
        //m_resMbCodeSurface = *m_trackedBuf->GetCurrMbCodeBuffer();
        //if (m_trackedBuf->GetCurrMvDataBuffer())
        //{
        //    m_resMvDataSurface = *m_trackedBuf->GetCurrMvDataBuffer();
        //}

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::Submit(
        MOS_COMMAND_BUFFER* commandBuffer,
        uint8_t packetPhase)
    {
        ENCODE_FUNC_CALL();

        MOS_COMMAND_BUFFER &cmdBuffer = *commandBuffer;
        ENCODE_CHK_STATUS_RETURN(Mos_Solo_PreProcessEncode(m_osInterface, &m_basicFeature->m_resBitstreamBuffer, &m_basicFeature->m_reconSurface));

        ENCODE_CHK_STATUS_RETURN(PatchPictureLevelCommands(packetPhase, cmdBuffer));

        bool tileEnabled = false;
        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, IsEnabled, tileEnabled);
        if (!tileEnabled)
        {
            ENCODE_CHK_STATUS_RETURN(PatchSliceLevelCommands(cmdBuffer, packetPhase));
        }
        else
        {
            ENCODE_CHK_STATUS_RETURN(PatchTileLevelCommands(cmdBuffer, packetPhase));
        }

        ENCODE_CHK_STATUS_RETURN(Mos_Solo_PreProcessEncode(m_osInterface, &m_basicFeature->m_resBitstreamBuffer, &m_basicFeature->m_reconSurface));

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::PatchPictureLevelCommands(const uint8_t &packetPhase, MOS_COMMAND_BUFFER  &cmdBuffer)
    {
        ENCODE_FUNC_CALL();

        ENCODE_CHK_STATUS_RETURN(m_miInterface->SetWatchdogTimerThreshold(m_basicFeature->m_frameWidth, m_basicFeature->m_frameHeight));

        SetPerfTag(CODECHAL_ENCODE_PERFTAG_CALL_PAK_ENGINE, (uint16_t)m_basicFeature->m_mode, m_basicFeature->m_pictureCodingType);

        auto feature = static_cast<HEVCEncodeBRC*>(m_featureManager->GetFeature(FeatureIDs::hevcBrcFeature));
        ENCODE_CHK_NULL_RETURN(feature);

        // Reset multi-pipe sync semaphores
        auto scalability = m_pipeline->GetMediaScalability();
        ENCODE_CHK_STATUS_RETURN(scalability->ResetSemaphore(syncOnePipeWaitOthers, 0, &cmdBuffer));

        if ((m_pipeline->IsFirstPass() && !feature->IsACQPEnabled()))
        {
            ENCODE_CHK_STATUS_RETURN(AddForceWakeup(cmdBuffer));

            // Send command buffer header at the beginning (OS dependent)
            ENCODE_CHK_STATUS_RETURN(SendPrologCmds(cmdBuffer));
        }

        if (m_pipeline->IsFirstPipe())
        {
            ENCODE_CHK_STATUS_RETURN(StartStatusReport(statusReportMfx, &cmdBuffer));
        }

        ENCODE_CHK_STATUS_RETURN(AddPictureHcpCommands(cmdBuffer));

        ENCODE_CHK_STATUS_RETURN(AddPictureVdencCommands(cmdBuffer));

        ENCODE_CHK_STATUS_RETURN(AddPicStateWithNoTile(cmdBuffer));
        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::PatchSliceLevelCommands(MOS_COMMAND_BUFFER  &cmdBuffer, uint8_t packetPhase)
    {
        ENCODE_FUNC_CALL();

        if (m_hevcPicParams->tiles_enabled_flag)
        {
            return MOS_STATUS_SUCCESS;
        }
        MHW_VDBOX_HEVC_SLICE_STATE_G12 sliceStateParams;
        SetHcpSliceStateCommonParams(sliceStateParams);

        auto feature = static_cast<HEVCEncodeBRC*>(m_featureManager->GetFeature(FeatureIDs::hevcBrcFeature));
        ENCODE_CHK_NULL_RETURN(feature);
        auto vdenc2ndLevelBatchBuffer = feature->GetVdenc2ndLevelBatchBuffer(m_pipeline->m_currRecycledBufIdx);

        // starting location for executing slice level cmds
        vdenc2ndLevelBatchBuffer->dwOffset = m_hwInterface->m_vdencBatchBuffer1stGroupSize + m_hwInterface->m_vdencBatchBuffer2ndGroupSize;

        PCODEC_ENCODER_SLCDATA slcData = m_basicFeature->m_slcData;
        for (uint32_t startLcu = 0, slcCount = 0; slcCount < m_basicFeature->m_numSlices; slcCount++)
        {
            if (m_pipeline->IsFirstPass())
            {
                slcData[slcCount].CmdOffset = startLcu * (m_hcpInterface->GetHcpPakObjSize()) * sizeof(uint32_t);
            }
            //TODO:combine below 2 functions
            SetHcpSliceStateParams(sliceStateParams, slcData, slcCount);

            ENCODE_CHK_STATUS_RETURN(SendHwSliceEncodeCommand(sliceStateParams, cmdBuffer));

            startLcu += m_hevcSliceParams[slcCount].NumLCUsInSlice;

            m_batchBufferForPakSlicesStartOffset = (uint32_t)m_batchBufferForPakSlices[m_basicFeature->m_currPakSliceIdx].iCurrent;
            if (feature->IsACQPEnabled() || feature->IsBRCEnabled())
            {
                // save offset for next 2nd level batch buffer usage
                // This is because we don't know how many times HCP_WEIGHTOFFSET_STATE & HCP_PAK_INSERT_OBJECT will be inserted for each slice
                // dwVdencBatchBufferPerSliceConstSize: constant size for each slice
                // m_vdencBatchBufferPerSliceVarSize:   variable size for each slice
                vdenc2ndLevelBatchBuffer->dwOffset += m_hwInterface->m_vdencBatchBufferPerSliceConstSize + m_basicFeature->m_vdencBatchBufferPerSliceVarSize[slcCount];
            }

            ENCODE_CHK_STATUS_RETURN(WaitVdencDone(cmdBuffer));
        }

        if (m_useBatchBufferForPakSlices)
        {
            ENCODE_CHK_STATUS_RETURN(Mhw_UnlockBb(
                m_osInterface,
                &m_batchBufferForPakSlices[m_basicFeature->m_currPakSliceIdx],
                m_lastTaskInPhase));
        }

        // Insert end of sequence/stream if set
        if (m_basicFeature->m_lastPicInSeq || m_basicFeature->m_lastPicInStream)
        {
            ENCODE_CHK_STATUS_RETURN(InsertSeqStreamEnd(cmdBuffer));
        }
        //TODO: combine below 3 functions
        ENCODE_CHK_STATUS_RETURN(EnsureAllCommandsExecuted(cmdBuffer));

        ENCODE_CHK_STATUS_RETURN(WaitHevcDone(cmdBuffer));

        ENCODE_CHK_STATUS_RETURN(EnsureAllCommandsExecuted(cmdBuffer));

        ENCODE_CHK_STATUS_RETURN(ReadSseStatistics(cmdBuffer));
        ENCODE_CHK_STATUS_RETURN(ReadSliceSize(cmdBuffer));

        ENCODE_CHK_STATUS_RETURN(EndStatusReport(statusReportMfx, &cmdBuffer));

        if (m_pipeline->IsLastPass() && m_pipeline->IsFirstPipe())
        {
            ENCODE_CHK_STATUS_RETURN(UpdateStatusReport(statusReportGlobalCount, &cmdBuffer));
        }

        // Reset parameters for next PAK execution
        if (false == m_pipeline->IsFrameTrackingEnabled() && m_pipeline->IsLastPass() && m_pipeline->IsLastPipe())
        {
            UpdateParameters();
        }

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::Construct3rdLevelBatch()
    {
        ENCODE_FUNC_CALL();

        MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

        MHW_VDBOX_HEVC_PIC_STATE_G12 picStateParams;
        SetHcpPicStateParams(picStateParams);

        // Begin patching 3rd level batch cmds
        MOS_COMMAND_BUFFER constructedCmdBuf;
        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, BeginPatch3rdLevelBatch, constructedCmdBuf);

        ENCODE_CHK_STATUS_RETURN(AddVdencCmd1Cmd(&constructedCmdBuf, true, m_basicFeature->m_ref.IsLowDelay()));

        ENCODE_CHK_STATUS_RETURN(m_hcpInterface->AddHcpPicStateCmd(&constructedCmdBuf, &picStateParams));

        ENCODE_CHK_STATUS_RETURN(AddVdencCmd2Cmd(&constructedCmdBuf, true, m_basicFeature->m_ref.IsLowDelay()));

        // set MI_BATCH_BUFFER_END command
        ENCODE_CHK_STATUS_RETURN(m_miInterface->AddMiBatchBufferEnd(&constructedCmdBuf, nullptr));

        // End patching 3rd level batch cmds
        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, EndPatch3rdLevelBatch);

        return eStatus;
    }

    MOS_STATUS HevcVdencPktG12::AddSlicesCommandsInTile(
        MOS_COMMAND_BUFFER &cmdBuffer)
    {
        ENCODE_FUNC_CALL();

        PCODEC_ENCODER_SLCDATA         slcData = m_basicFeature->m_slcData;
        MHW_VDBOX_HEVC_SLICE_STATE_G12 sliceState;
        SetHcpSliceStateCommonParams(sliceState);

        uint32_t slcCount, sliceNumInTile = 0;
        for (slcCount = 0; slcCount < m_basicFeature->m_numSlices; slcCount++)
        {
            bool sliceInTile  = false;
            m_lastSliceInTile = false;

            EncodeTileData curTileData = {};
            RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, GetCurrentTile, curTileData);
            RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, IsSliceInTile, 
                slcCount, &curTileData, &sliceInTile, &m_lastSliceInTile);

            if (!sliceInTile)
            {
                continue;
            }

            SetHcpSliceStateParams(sliceState, slcData, (uint16_t)slcCount);
            ENCODE_CHK_STATUS_RETURN(SendHwSliceEncodeCommand(sliceState, cmdBuffer));

            // Send VD_PIPELINE_FLUSH command  for each slice
            ENCODE_CHK_STATUS_RETURN(WaitHevcVdencDone(cmdBuffer));

            sliceNumInTile++;
        }  // end of slice

        if (0 == sliceNumInTile)
        {
            // One tile must have at least one slice
            ENCODE_ASSERT(false);
            return MOS_STATUS_INVALID_PARAMETER;
        }
        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::AddOneTileCommands(
        MOS_COMMAND_BUFFER &cmdBuffer,
        uint32_t tileRow,
        uint32_t tileCol,
        uint32_t tileRowPass)
    {
        ENCODE_FUNC_CALL();
        auto eStatus = MOS_STATUS_SUCCESS;

        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, SetCurrentTile, tileRow, tileCol, m_pipeline);

        if ((m_pipeline->GetPipeNum() > 1) && (tileCol != m_pipeline->GetCurrentPipe()))
        {
            return MOS_STATUS_SUCCESS;
        }

        // Begin patching tile level batch cmds
        MOS_COMMAND_BUFFER constructTileBatchBuf = {};
        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, BeginPatchTileLevelBatch,
            tileRowPass, constructTileBatchBuf);

        // Add batch buffer start for tile
        PMHW_BATCH_BUFFER tileLevelBatchBuffer = nullptr;
        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, GetTileLevelBatchBuffer, 
            tileLevelBatchBuffer);
        ENCODE_CHK_STATUS_RETURN(m_miInterface->AddMiBatchBufferStartCmd(&cmdBuffer, tileLevelBatchBuffer));

        // HCP Lock for multiple pipe mode
        if (m_pipeline->GetNumPipes() > 1)
        {
            MHW_MI_VD_CONTROL_STATE_PARAMS vdControlStateParams;
            MOS_ZeroMemory(&vdControlStateParams, sizeof(MHW_MI_VD_CONTROL_STATE_PARAMS));
            vdControlStateParams.scalableModePipeLock = true;
            MhwMiInterfaceG12 *miInterface = dynamic_cast<MhwMiInterfaceG12*>(m_miInterface);
            ENCODE_CHK_NULL_RETURN(miInterface);
            ENCODE_CHK_STATUS_RETURN(miInterface->AddMiVdControlStateCmd(
                &constructTileBatchBuf, &vdControlStateParams));
        }

        ENCODE_CHK_STATUS_RETURN(VdencPipeModeSelect(m_pipeModeSelectParams, constructTileBatchBuf));

        ENCODE_CHK_STATUS_RETURN(m_hcpInterface->AddHcpPipeModeSelectCmd(&constructTileBatchBuf, &m_pipeModeSelectParams));

        ENCODE_CHK_STATUS_RETURN(AddPicStateWithTile(constructTileBatchBuf));

        MHW_VDBOX_HCP_TILE_CODING_PARAMS_G12 curTileCodingParams = {};
        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, SetHcpTileCodingParams, m_pipeline->GetNumPipes(), curTileCodingParams);
        
        MhwVdboxHcpInterfaceG12 *hcpInterface = dynamic_cast<MhwVdboxHcpInterfaceG12 *>(m_hcpInterface);
        ENCODE_CHK_NULL_RETURN(hcpInterface);
        ENCODE_CHK_STATUS_RETURN(hcpInterface->AddHcpTileCodingCmd(&constructTileBatchBuf, &curTileCodingParams));

        ENCODE_CHK_STATUS_RETURN(AddSlicesCommandsInTile(constructTileBatchBuf));

        //HCP unLock for multiple pipe mode
        if (m_pipeline->GetNumPipes() > 1)
        {
            MHW_MI_VD_CONTROL_STATE_PARAMS vdControlStateParams;
            MOS_ZeroMemory(&vdControlStateParams, sizeof(MHW_MI_VD_CONTROL_STATE_PARAMS));
            vdControlStateParams.scalableModePipeUnlock = true;
            MhwMiInterfaceG12 *miInterface = dynamic_cast<MhwMiInterfaceG12*>(m_miInterface);
            ENCODE_CHK_NULL_RETURN(miInterface);
            ENCODE_CHK_STATUS_RETURN(miInterface->AddMiVdControlStateCmd(
                &constructTileBatchBuf, &vdControlStateParams));
        }

        ENCODE_CHK_STATUS_RETURN(WaitHevcDone(constructTileBatchBuf));

        ENCODE_CHK_STATUS_RETURN(EnsureAllCommandsExecuted(constructTileBatchBuf));

        // Add batch buffer end at the end of each tile batch, 2nd level batch buffer
        ENCODE_CHK_STATUS_RETURN(m_miInterface->AddMiBatchBufferEnd(&constructTileBatchBuf, nullptr));

        // End patching tile level batch cmds
        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, EndPatchTileLevelBatch);

        return eStatus;
    }

    MOS_STATUS HevcVdencPktG12::PatchTileLevelCommands(MOS_COMMAND_BUFFER &cmdBuffer, uint8_t packetPhase)
    {
        ENCODE_FUNC_CALL();
        auto eStatus = MOS_STATUS_SUCCESS;

        if (!m_hevcPicParams->tiles_enabled_flag)
        {
            return MOS_STATUS_INVALID_PARAMETER;
        }

        ENCODE_CHK_STATUS_RETURN(Construct3rdLevelBatch());

        SetHcpPipeModeSelectParams(m_pipeModeSelectParams);

        uint8_t numTileColumns = 1;
        uint8_t numTileRows    = 1;
        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, GetTileRowColumns, numTileRows, numTileColumns);

        for (uint32_t tileRow = 0; tileRow < numTileRows; tileRow++)
        {
            for (uint32_t tileRowPass = 0; tileRowPass < m_NumPassesForTileReplay; tileRowPass++)
            {
                for (uint32_t tileCol = 0; tileCol < numTileColumns; tileCol++)
                {
                    ENCODE_CHK_STATUS_RETURN(AddOneTileCommands(
                        cmdBuffer,
                        tileRow,
                        tileCol,
                        tileRowPass));
                }
            }
        }

        // Insert end of sequence/stream if set
        if ((m_basicFeature->m_lastPicInSeq || m_basicFeature->m_lastPicInStream) && m_pipeline->IsLastPipe())
        {
            ENCODE_CHK_STATUS_RETURN(InsertSeqStreamEnd(cmdBuffer));
        }

        // Send VD_CONTROL_STATE (Memory Implict Flush)
        MHW_MI_VD_CONTROL_STATE_PARAMS vdControlStateParams;
        MOS_ZeroMemory(&vdControlStateParams, sizeof(MHW_MI_VD_CONTROL_STATE_PARAMS));
        vdControlStateParams.memoryImplicitFlush = true;
        MhwMiInterfaceG12 *miInterface = dynamic_cast<MhwMiInterfaceG12*>(m_miInterface);
        ENCODE_CHK_NULL_RETURN(miInterface);
        ENCODE_CHK_STATUS_RETURN(miInterface->AddMiVdControlStateCmd(&cmdBuffer, &vdControlStateParams));

        ENCODE_CHK_STATUS_RETURN(WaitHevcDone(cmdBuffer));

        ENCODE_CHK_STATUS_RETURN(EnsureAllCommandsExecuted(cmdBuffer));

        // Wait all pipe cmds done for the packet
        auto scalability = m_pipeline->GetMediaScalability();
        ENCODE_CHK_STATUS_RETURN(scalability->SyncPipe(syncOnePipeWaitOthers, 0, &cmdBuffer));

        // post-operations are done by pak integrate pkt

        return MOS_STATUS_SUCCESS;
    }

    void HevcVdencPktG12::SetHcpPicStateParams(MHW_VDBOX_HEVC_PIC_STATE& picStateParams)
    {
        ENCODE_FUNC_CALL();

        HevcVdencPkt::SetHcpPicStateParams(picStateParams);

        static_cast<MHW_VDBOX_HEVC_PIC_STATE_G12&>(picStateParams).ucRecNotFilteredID  = m_slotForRecNotFiltered;
        // To fix the possible BSpec issue later
        static_cast<MHW_VDBOX_HEVC_PIC_STATE_G12&>(picStateParams).IBCControl = m_enableLBCOnly ? 0x2 : 0x3;
        static_cast<MHW_VDBOX_HEVC_PIC_STATE_G12&>(picStateParams).PartialFrameUpdateEnable = m_enablePartialFrameUpdate & (picStateParams.pHevcEncPicParams->CodingType != I_TYPE);
    }

    MOS_STATUS HevcVdencPktG12::AddVdencCmd1Cmd(PMOS_COMMAND_BUFFER cmdBuffer, bool addToBatchBufferHuCBRC, bool isLowDelayB)
    {
        ENCODE_FUNC_CALL();

        void *cmdParams = nullptr;
        // Send VDENC_COSTS_STATE command
        MHW_VDBOX_VDENC_CMD1_PARAMS  vdencCostsStateParams;
        MOS_ZeroMemory(&vdencCostsStateParams, sizeof(vdencCostsStateParams));
        vdencCostsStateParams.Mode = CODECHAL_ENCODE_MODE_HEVC;
        vdencCostsStateParams.pHevcEncPicParams = (PCODEC_HEVC_ENCODE_PICTURE_PARAMS)m_basicFeature->m_hevcPicParams;
        vdencCostsStateParams.pHevcEncSlcParams = (PCODEC_HEVC_ENCODE_SLICE_PARAMS)m_basicFeature->m_hevcSliceParams;
        vdencCostsStateParams.pInputParams      = cmdParams;

        ENCODE_CHK_STATUS_RETURN(m_vdencInterface->AddVdencCmd1Cmd(cmdBuffer, nullptr, &vdencCostsStateParams));
        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::AddVdencCmd2Cmd(PMOS_COMMAND_BUFFER cmdBuffer, bool addToBatchBufferHuCBRC, bool isLowDelayB)
    {
        ENCODE_FUNC_CALL();

        void *cmdParams = nullptr;
        PMHW_VDBOX_VDENC_CMD2_STATE_EXT hevcImgStateParams = MOS_New(MHW_VDBOX_VDENC_CMD2_STATE_EXT);
        ENCODE_CHK_NULL_RETURN(hevcImgStateParams);

        bool panicEnabled = false;//(m_brcEnabled) && (m_panicEnable) && (GetCurrentPass() == 1) && !m_pakOnlyPass;

        // set VDENC_HEVC_VP9_IMG_STATE command
        hevcImgStateParams->Mode = CODECHAL_ENCODE_MODE_HEVC;
        hevcImgStateParams->pHevcEncSeqParams = (PCODEC_HEVC_ENCODE_SEQUENCE_PARAMS)m_basicFeature->m_hevcSeqParams;
        hevcImgStateParams->pHevcEncPicParams = (PCODEC_HEVC_ENCODE_PICTURE_PARAMS)m_basicFeature->m_hevcPicParams;
        hevcImgStateParams->pHevcEncSlcParams = (PCODEC_HEVC_ENCODE_SLICE_PARAMS)m_basicFeature->m_hevcSliceParams;
        hevcImgStateParams->bRoundingEnabled = true;// TODO: m_basicFeature->m_hevcVdencRoundingEnabled;
        hevcImgStateParams->bPakOnlyMultipassEnable = m_pakOnlyPass;
        hevcImgStateParams->bUseDefaultQpDeltas = false;//TODO (m_basicFeature->m_hevcVdencAcqpEnabled && hevcImgStateParams->pHevcEncSeqParams->QpAdjustment) ||
        //TODO:(m_brcEnabled && hevcImgStateParams->pHevcEncSeqParams->MBBRC != 2/*mbBrcDisabled*/);
        hevcImgStateParams->bPanicEnabled           = panicEnabled;
        hevcImgStateParams->bStreamInEnabled        = m_streamInEnabled;
        RUN_FEATURE_INTERFACE(HevcVdencRoi, FeatureIDs::hevcVdencRoiFeature, SetVdencCmd2Cmd, hevcImgStateParams);
        hevcImgStateParams->bTileReplayEnable       = false;//m_tileReplayEnabled;
        hevcImgStateParams->bIsLowDelayB            = isLowDelayB;
        hevcImgStateParams->bCaptureModeEnable      = false;//m_captureModeEnable;
        hevcImgStateParams->m_WirelessSessionID     = 0;
        hevcImgStateParams->pRefIdxMapping          = m_basicFeature->m_ref.GetRefIdxMapping();
        hevcImgStateParams->pInputParams            = cmdParams;

        ENCODE_CHK_STATUS_RETURN(m_vdencInterface->AddVdencCmd2Cmd(cmdBuffer, nullptr, hevcImgStateParams));
        MOS_Delete(hevcImgStateParams);
        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::AddPicStateWithNoTile(
        MOS_COMMAND_BUFFER &cmdBuffer)
    {
        ENCODE_FUNC_CALL();

        bool tileEnabled = false;
        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, IsEnabled, tileEnabled);
        if (tileEnabled)
        {
            return MOS_STATUS_SUCCESS;
        }

        MHW_VDBOX_HEVC_PIC_STATE_G12 picStateParams;
        SetHcpPicStateParams(picStateParams);
        auto brcFeature = static_cast<HEVCEncodeBRC*>(m_featureManager->GetFeature(FeatureIDs::hevcBrcFeature));
        ENCODE_CHK_NULL_RETURN(brcFeature);
        auto vdenc2ndLevelBatchBuffer = brcFeature->GetVdenc2ndLevelBatchBuffer(m_pipeline->m_currRecycledBufIdx);

        if (brcFeature->IsBRCUpdateRequired())
        {
            ENCODE_CHK_STATUS_RETURN(m_miInterface->AddMiBatchBufferStartCmd(&cmdBuffer, vdenc2ndLevelBatchBuffer));
        }
        // When tile is enabled, below commands are needed for each tile instead of each picture
        else
        {
            //TODO: should be m_lowDelay
            AddVdencCmd1Cmd(&cmdBuffer, true, m_basicFeature->m_ref.IsLowDelay());

            ENCODE_CHK_STATUS_RETURN(m_hcpInterface->AddHcpPicStateCmd(&cmdBuffer, &picStateParams));

            //TODO: should be m_lowDelay
            AddVdencCmd2Cmd(&cmdBuffer, true, m_basicFeature->m_ref.IsLowDelay());
        }

        // Send HEVC_VP9_RDOQ_STATE command
        ENCODE_CHK_STATUS_RETURN(AddHcpHevcVp9RdoqStateCmd(cmdBuffer, &picStateParams));

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::AddPicStateWithTile(
        MOS_COMMAND_BUFFER &cmdBuffer)
    {
        ENCODE_FUNC_CALL();

        bool tileEnabled = false;
        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, IsEnabled, tileEnabled);
        if (!tileEnabled)
        {
            return MOS_STATUS_SUCCESS;
        }

        MHW_VDBOX_HEVC_PIC_STATE_G12 picStateParams;
        SetHcpPicStateParams(picStateParams);
        auto brcFeature = static_cast<HEVCEncodeBRC *>(m_featureManager->GetFeature(FeatureIDs::hevcBrcFeature));
        ENCODE_CHK_NULL_RETURN(brcFeature);
        auto vdenc2ndLevelBatchBuffer = brcFeature->GetVdenc2ndLevelBatchBuffer(m_pipeline->m_currRecycledBufIdx);

        if (brcFeature->IsBRCUpdateRequired())
        {
            ENCODE_CHK_STATUS_RETURN(m_miInterface->AddMiBatchBufferStartCmd(&cmdBuffer, vdenc2ndLevelBatchBuffer));
        }
        // When tile is enabled, below commands are needed for each tile instead of each picture
        else
        {
            // 3nd level batch buffer start
            PMHW_BATCH_BUFFER thirdLevelBatchBuffer = nullptr;
            RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, GetThirdLevelBatchBuffer, thirdLevelBatchBuffer);
            ENCODE_CHK_STATUS_RETURN(m_miInterface->AddMiBatchBufferStartCmd(&cmdBuffer, thirdLevelBatchBuffer));
        }

        // Send HEVC_VP9_RDOQ_STATE command
        ENCODE_CHK_STATUS_RETURN(AddHcpHevcVp9RdoqStateCmd(cmdBuffer, &picStateParams));

        return MOS_STATUS_SUCCESS;
    }

    void HevcVdencPktG12::UpdateParameters()
    {
        ENCODE_FUNC_CALL();

        if (!m_pipeline->IsSingleTaskPhaseSupported())
        {
            m_osInterface->pfnResetPerfBufferID(m_osInterface);
        }

        m_basicFeature->m_currPakSliceIdx = (m_basicFeature->m_currPakSliceIdx + 1) % m_basicFeature->m_codecHalHevcNumPakSliceBatchBuffers;
    }

    MOS_STATUS HevcVdencPktG12::EnsureAllCommandsExecuted(MOS_COMMAND_BUFFER &cmdBuffer)
    {
        ENCODE_FUNC_CALL();

        // Send MI_FLUSH command
        MHW_MI_FLUSH_DW_PARAMS flushDwParams;
        MOS_ZeroMemory(&flushDwParams, sizeof(flushDwParams));
        flushDwParams.bVideoPipelineCacheInvalidate = true;
        ENCODE_CHK_STATUS_RETURN(m_miInterface->AddMiFlushDwCmd(&cmdBuffer, &flushDwParams));

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::InsertSeqStreamEnd(MOS_COMMAND_BUFFER &cmdBuffer)
    {
        ENCODE_FUNC_CALL();

        MHW_VDBOX_PAK_INSERT_PARAMS pakInsertObjectParams;
        MOS_ZeroMemory(&pakInsertObjectParams, sizeof(pakInsertObjectParams));
        pakInsertObjectParams.bLastPicInSeq = m_basicFeature->m_lastPicInSeq;
        pakInsertObjectParams.bLastPicInStream = m_basicFeature->m_lastPicInStream;
        ENCODE_CHK_STATUS_RETURN(m_hcpInterface->AddHcpPakInsertObject(&cmdBuffer, &pakInsertObjectParams));

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::AddVdencWalkerStateCmd(
        MHW_VDBOX_HEVC_SLICE_STATE &params,
        MOS_COMMAND_BUFFER &cmdBuffer)
    {
        MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

        ENCODE_FUNC_CALL();

        MHW_VDBOX_VDENC_WALKER_STATE_PARAMS_G12 vdencWalkerStateParams;
        vdencWalkerStateParams.Mode = CODECHAL_ENCODE_MODE_HEVC;
        vdencWalkerStateParams.pHevcEncSeqParams = params.pEncodeHevcSeqParams;
        vdencWalkerStateParams.pHevcEncPicParams = params.pEncodeHevcPicParams;
        vdencWalkerStateParams.pEncodeHevcSliceParams = params.pEncodeHevcSliceParams;

        switch (m_pipeline->GetPipeNum())
        {
        case 0:
        case 1:
            vdencWalkerStateParams.dwNumberOfPipes = VDENC_PIPE_SINGLE_PIPE;
            break;
        case 2:
            vdencWalkerStateParams.dwNumberOfPipes = VDENC_PIPE_TWO_PIPE;
            break;
        case 4:
            vdencWalkerStateParams.dwNumberOfPipes = VDENC_PIPE_FOUR_PIPE;
            break;
        default:
            vdencWalkerStateParams.dwNumberOfPipes = VDENC_PIPE_INVALID;
            ENCODE_ASSERT(false);
            break;
        }
        
        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, SetVdencWalkerStateParams, vdencWalkerStateParams);

        ENCODE_CHK_STATUS_RETURN(m_vdencInterface->AddVdencWalkerStateCmd(&cmdBuffer, &vdencWalkerStateParams));

        return eStatus;
    }

    void HevcVdencPktG12::SetHcpPipeBufAddrParams(MHW_VDBOX_PIPE_BUF_ADDR_PARAMS& pipeBufAddrParams)
    {
        ENCODE_FUNC_CALL();

        HevcVdencPkt::SetHcpPipeBufAddrParams(pipeBufAddrParams);

        if (m_pipeline->GetNumPipes() > 1)
        {
            RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, SetHcpPipeBufAddrParams, pipeBufAddrParams);
        }

        // Set up the recon not filtered surface for IBC
        if (m_enableSCC &&
            m_hevcPicParams->pps_curr_pic_ref_enabled_flag)
        {
            // I frame is much simpler
            if (m_basicFeature->m_pictureCodingType == I_TYPE)
            {
                //pipeBufAddrParams.presReferences[0] = &m_vdencRecNotFilteredBuffer;
                m_slotForRecNotFiltered = 0;
            }
            // B frame
            else
            {
                unsigned int i;

                // Find one available slot
                for (i = 0; i < CODECHAL_MAX_CUR_NUM_REF_FRAME_HEVC; i++)
                {
                    if (pipeBufAddrParams.presReferences[i] == nullptr)
                    {
                        break;
                    }
                }

                ENCODE_ASSERT(i < CODECHAL_MAX_CUR_NUM_REF_FRAME_HEVC);

                //record the slot for HCP_REF_IDX_STATE
                m_slotForRecNotFiltered = (unsigned char)i;
                //pipeBufAddrParams.presReferences[i] = &m_vdencRecNotFilteredBuffer;
            }
        }
    }

    MOS_STATUS HevcVdencPktG12::AddPictureVdencCommands(MOS_COMMAND_BUFFER & cmdBuffer)
    {
        ENCODE_FUNC_CALL();

        MHW_VDBOX_PIPE_BUF_ADDR_PARAMS_G12 pipeBufAddrParams;

        ENCODE_CHK_STATUS_RETURN(VdencPipeModeSelect(m_pipeModeSelectParams, cmdBuffer));
        ENCODE_CHK_STATUS_RETURN(SetVdencSurfaces(cmdBuffer));
        ENCODE_CHK_STATUS_RETURN(AddVdencPipeBufAddrCmd(pipeBufAddrParams, cmdBuffer));
        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::SetVdencPipeBufAddrParams(
        MHW_VDBOX_PIPE_BUF_ADDR_PARAMS& pipeBufAddrParams)
   {
        ENCODE_FUNC_CALL();

        HevcVdencPkt::SetVdencPipeBufAddrParams(pipeBufAddrParams);

        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, SetVdencPipeBufAddrParams, pipeBufAddrParams);
        
//TODO:  comment out 
        /*
        // Set up the recon not filtered surface for IBC
        if (m_enableSCC &&
            m_hevcPicParams->pps_curr_pic_ref_enabled_flag)
        {
            // I frame is much simpler
            if (m_pictureCodingType == I_TYPE)
            {
                pipeBufAddrParams.presVdencReferences[0] = &m_vdencRecNotFilteredBuffer;
            }
            // B frame
            else
            {
                unsigned int i;

                // Find one available slot
                for (i = 0; i < CODECHAL_MAX_CUR_NUM_REF_FRAME_HEVC; i++)
                {
                    if (pipeBufAddrParams.presVdencReferences[i] == nullptr)
                    {
                        break;
                    }
                }

                ENCODE_ASSERT(i < CODECHAL_MAX_CUR_NUM_REF_FRAME_HEVC);
                if (i != 0)
                {
                    pipeBufAddrParams.dwNumRefIdxL0ActiveMinus1 += 1;
                }
                pipeBufAddrParams.presVdencReferences[i] = &m_vdencRecNotFilteredBuffer;
            }
        }
        */
    //pipeBufAddrParams.presVdencTileRowStoreBuffer = &m_vdencTileRowStoreBuffer;
        return MOS_STATUS_SUCCESS;
   }

    MOS_STATUS HevcVdencPktG12::AddHcpPipeBufAddrCmd(
        MOS_COMMAND_BUFFER  &cmdBuffer)
    {
        ENCODE_FUNC_CALL();

        MHW_VDBOX_PIPE_BUF_ADDR_PARAMS_G12 pipeBufAddrParams = {};
        SetHcpPipeBufAddrParams(pipeBufAddrParams);
#ifdef _MMC_SUPPORTED
        //m_mmcState->SetPipeBufAddr(m_pipeBufAddrParams);
#endif

        ENCODE_CHK_STATUS_RETURN(m_hcpInterface->AddHcpPipeBufAddrCmd(&cmdBuffer, &pipeBufAddrParams));

        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::AddPictureHcpCommands(
        MOS_COMMAND_BUFFER & cmdBuffer)
    {
        ENCODE_FUNC_CALL();

        ENCODE_CHK_STATUS_RETURN(AddHcpPipeModeSelect(cmdBuffer));

        ENCODE_CHK_STATUS_RETURN(AddHcpSurfaces(cmdBuffer));

        ENCODE_CHK_STATUS_RETURN(AddHcpPipeBufAddrCmd(cmdBuffer));

        ENCODE_CHK_STATUS_RETURN(AddHcpIndObjBaseAddrCmd(cmdBuffer));

        ENCODE_CHK_STATUS_RETURN(AddHcpQmStateCmd(cmdBuffer));
        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::AddHcpPipeModeSelect(
        MOS_COMMAND_BUFFER &cmdBuffer)
    {
        ENCODE_FUNC_CALL();

        MHW_VDBOX_VDENC_CONTROL_STATE_PARAMS  vdencControlStateParams;
        MHW_MI_VD_CONTROL_STATE_PARAMS        vdControlStateParams;

        //set up VDENC_CONTROL_STATE command
        MOS_ZeroMemory(&vdencControlStateParams, sizeof(MHW_VDBOX_VDENC_CONTROL_STATE_PARAMS));
        vdencControlStateParams.bVdencInitialization = true;
        ENCODE_CHK_STATUS_RETURN(
            static_cast<MhwVdboxVdencInterfaceG12X*>(m_vdencInterface)->AddVdencControlStateCmd(&cmdBuffer, &vdencControlStateParams));

        //set up VD_CONTROL_STATE command
        MOS_ZeroMemory(&vdControlStateParams, sizeof(MHW_MI_VD_CONTROL_STATE_PARAMS));
        vdControlStateParams.initialization = true;
        MhwMiInterfaceG12 *miInterface = dynamic_cast<MhwMiInterfaceG12 *>(m_miInterface);
        ENCODE_CHK_NULL_RETURN(miInterface);
        ENCODE_CHK_STATUS_RETURN(miInterface->AddMiVdControlStateCmd(&cmdBuffer, &vdControlStateParams));

        SetHcpPipeModeSelectParams(m_pipeModeSelectParams);

        ENCODE_CHK_STATUS_RETURN(m_hcpInterface->AddHcpPipeModeSelectCmd(&cmdBuffer, &m_pipeModeSelectParams));

        return MOS_STATUS_SUCCESS;
    }

    void HevcVdencPktG12::SetHcpPipeModeSelectParams(MHW_VDBOX_PIPE_MODE_SELECT_PARAMS& vdboxPipeModeSelectParams)
    {
        ENCODE_FUNC_CALL();

        HevcVdencPkt::SetHcpPipeModeSelectParams(vdboxPipeModeSelectParams);

        MHW_VDBOX_PIPE_MODE_SELECT_PARAMS_G12& pipeModeSelectParams = static_cast<MHW_VDBOX_PIPE_MODE_SELECT_PARAMS_G12&>(vdboxPipeModeSelectParams);

        if (m_pipeline->GetPipeNum() > 1)
        {
            // Running in the multiple VDBOX mode
            if (m_pipeline->IsFirstPipe())
            {
                pipeModeSelectParams.MultiEngineMode = MHW_VDBOX_HCP_MULTI_ENGINE_MODE_LEFT;
            }
            else if (m_pipeline->IsLastPipe())
            {
                pipeModeSelectParams.MultiEngineMode = MHW_VDBOX_HCP_MULTI_ENGINE_MODE_RIGHT;
            }
            else
            {
                pipeModeSelectParams.MultiEngineMode = MHW_VDBOX_HCP_MULTI_ENGINE_MODE_MIDDLE;
            }
            pipeModeSelectParams.PipeWorkMode = MHW_VDBOX_HCP_PIPE_WORK_MODE_CODEC_BE;
        }
        else
        {
            pipeModeSelectParams.MultiEngineMode = MHW_VDBOX_HCP_MULTI_ENGINE_MODE_FE_LEGACY;
            pipeModeSelectParams.PipeWorkMode = MHW_VDBOX_HCP_PIPE_WORK_MODE_LEGACY;
        }

        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, SetHcpPipeModeSelectParams, pipeModeSelectParams);
        // In single pipe mode, if TileBasedReplayMode is enabled, the bit stream for each tile will not be continuous
        if (m_hevcPicParams->tiles_enabled_flag)
        {
            pipeModeSelectParams.bTileBasedReplayMode = m_basicFeature->m_enableTileReplay;
        }
        else
        {
            pipeModeSelectParams.bTileBasedReplayMode = 0;
        }

        if (m_hevcSeqParams->EnableStreamingBufferLLC || m_hevcSeqParams->EnableStreamingBufferDDR)
        {
            pipeModeSelectParams.bStreamingBufferEnabled = true;
        }

        RUN_FEATURE_INTERFACE(HEVCEncodeBRC, FeatureIDs::hevcBrcFeature,
            SetHcpPipeModeSelectParams, pipeModeSelectParams);
    }

    MOS_STATUS HevcVdencPktG12::AddHcpSurfaces(MOS_COMMAND_BUFFER &cmdBuffer)
    {
        ENCODE_FUNC_CALL();

        ENCODE_CHK_STATUS_RETURN(HevcVdencPkt::AddHcpSurfaces(cmdBuffer));

        // Add the surface state for reference picture, GEN12 HW change (GEN:HAS:1209978040)
        MHW_VDBOX_SURFACE_PARAMS reconSurfaceParams;
        SetHcpReconSurfaceParams(reconSurfaceParams);
        reconSurfaceParams.ucSurfaceStateId = CODECHAL_HCP_REF_SURFACE_ID;
        ENCODE_CHK_STATUS_RETURN(m_hcpInterface->AddHcpSurfaceCmd(&cmdBuffer, &reconSurfaceParams));
        return MOS_STATUS_SUCCESS;
    }

    void HevcVdencPktG12::SetVdencPipeModeSelectParams(MHW_VDBOX_PIPE_MODE_SELECT_PARAMS& vdboxPipeModeSelectParams)
    {
        ENCODE_FUNC_CALL();

        HevcVdencPkt::SetVdencPipeModeSelectParams(vdboxPipeModeSelectParams);

        MHW_VDBOX_PIPE_MODE_SELECT_PARAMS_G12& pipeModeSelectParams = static_cast<MHW_VDBOX_PIPE_MODE_SELECT_PARAMS_G12&>(vdboxPipeModeSelectParams);

        // Enable RGB encoding
        pipeModeSelectParams.bRGBEncodingMode  = m_rgbEncodingEnable;

        // Capture mode enable
        pipeModeSelectParams.bWirelessEncodeEnabled = m_captureModeEnable;
        pipeModeSelectParams.ucWirelessSessionId    = 0;

        // Set random access flag
        pipeModeSelectParams.bIsRandomAccess        = !m_basicFeature->m_ref.IsLowDelay();

        if (m_enableSCC && (m_hevcPicParams->pps_curr_pic_ref_enabled_flag || m_hevcSeqParams->palette_mode_enabled_flag))
        {
            pipeModeSelectParams.bVdencPakObjCmdStreamOutEnable = false;
        }
    }

    void HevcVdencPktG12::SetHcpSliceStateCommonParams(MHW_VDBOX_HEVC_SLICE_STATE& sliceStateParams)
    {
        ENCODE_FUNC_CALL();

        MOS_ZeroMemory(&sliceStateParams, sizeof(sliceStateParams));

        sliceStateParams.presDataBuffer = m_basicFeature->m_resMbCodeBuffer;
        sliceStateParams.pHevcPicIdx = nullptr;
        sliceStateParams.pEncodeHevcSeqParams = (CODEC_HEVC_ENCODE_SEQUENCE_PARAMS *)m_hevcSeqParams;
        sliceStateParams.pEncodeHevcPicParams = (CODEC_HEVC_ENCODE_PICTURE_PARAMS *)m_hevcPicParams;
        sliceStateParams.pBsBuffer = &(m_basicFeature->m_bsBuffer);
        sliceStateParams.ppNalUnitParams = (CODECHAL_NAL_UNIT_PARAMS **)m_nalUnitParams;
        sliceStateParams.dwHeaderBytesInserted = 0;
        sliceStateParams.dwHeaderDummyBytes = 0;
        sliceStateParams.pRefIdxMapping = m_basicFeature->m_ref.GetRefIdxMapping();
        sliceStateParams.bIsLowDelay = m_basicFeature->m_ref.IsLowDelay();
        sliceStateParams.RoundingIntra = m_roundingIntra;
        sliceStateParams.RoundingInter = m_roundingInter;

        sliceStateParams.bVdencInUse        = true;

        // This bit disables Top intra Reference pixel fetch in VDENC mode.
        // In PAK only second pass, this bit should be set to one.
        // "IntraRefFetchDisable" in HCP SLICE STATE should be set to 0 in first pass and 1 in subsequent passes.
        // For dynamic slice, 2nd pass is still VDEnc + PAK pass, not PAK only pass.
        sliceStateParams.bIntraRefFetchDisable = m_pakOnlyPass;

        RUN_FEATURE_INTERFACE(HEVCEncodeBRC, FeatureIDs::hevcBrcFeature,
            SetHcpSliceStateCommonParams, sliceStateParams, m_pipeline->m_currRecycledBufIdx);
    }

    MOS_STATUS HevcVdencPktG12::SetHcpSliceStateParams(
        MHW_VDBOX_HEVC_SLICE_STATE  &sliceStateParams,
        PCODEC_ENCODER_SLCDATA      slcData,
        uint32_t                    currSlcIdx)
    {
        ENCODE_FUNC_CALL();

        HevcVdencPkt::SetHcpSliceStateParams(sliceStateParams, slcData, currSlcIdx);

        RUN_FEATURE_INTERFACE(EncodeTile, FeatureIDs::encodeTile, SetHcpSliceStateParams, sliceStateParams, m_lastSliceInTile);
        return MOS_STATUS_SUCCESS;
    }

    MOS_STATUS HevcVdencPktG12::AddHcpRefIdxCmd(
        PMOS_COMMAND_BUFFER         cmdBuffer,
        PMHW_BATCH_BUFFER           batchBuffer,
        PMHW_VDBOX_HEVC_SLICE_STATE params)
    {
        MOS_STATUS eStatus = MOS_STATUS_SUCCESS;

        ENCODE_FUNC_CALL();

        ENCODE_CHK_NULL_RETURN(params);
        ENCODE_CHK_NULL_RETURN(params->pEncodeHevcSliceParams);
        ENCODE_CHK_NULL_RETURN(params->pEncodeHevcPicParams);

        if (cmdBuffer == nullptr && batchBuffer == nullptr)
        {
            ENCODE_ASSERTMESSAGE("There was no valid buffer to add the HW command to.");
            return MOS_STATUS_NULL_POINTER;
        }

        PCODEC_HEVC_ENCODE_PICTURE_PARAMS hevcPicParams = params->pEncodeHevcPicParams;
        PCODEC_HEVC_ENCODE_SLICE_PARAMS   hevcSlcParams = params->pEncodeHevcSliceParams;

        if (hevcSlcParams->slice_type != encodeHevcISlice)
        {
            MHW_VDBOX_HEVC_REF_IDX_PARAMS_G12 refIdxParams = {};

            refIdxParams.CurrPic         = hevcPicParams->CurrReconstructedPic;
            refIdxParams.isEncode        = true;
            refIdxParams.ucList          = LIST_0;
            refIdxParams.ucNumRefForList = hevcSlcParams->num_ref_idx_l0_active_minus1 + 1;
            eStatus                      = MOS_SecureMemcpy(&refIdxParams.RefPicList, sizeof(refIdxParams.RefPicList), &hevcSlcParams->RefPicList, sizeof(hevcSlcParams->RefPicList));
            if (eStatus != MOS_STATUS_SUCCESS)
            {
                ENCODE_ASSERTMESSAGE("Failed to copy memory.");
                return eStatus;
            }

            refIdxParams.hevcRefList  = (void **)m_basicFeature->m_ref.GetRefList();
            refIdxParams.poc_curr_pic = hevcPicParams->CurrPicOrderCnt;
            for (auto i = 0; i < CODEC_MAX_NUM_REF_FRAME_HEVC; i++)
            {
                refIdxParams.poc_list[i] = hevcPicParams->RefFramePOCList[i];
            }

            refIdxParams.pRefIdxMapping     = params->pRefIdxMapping;
            refIdxParams.RefFieldPicFlag    = 0;  // there is no interlaced support in encoder
            refIdxParams.RefBottomFieldFlag = 0;  // there is no interlaced support in encoder

            ENCODE_CHK_STATUS_RETURN(m_hcpInterface->AddHcpRefIdxStateCmd(cmdBuffer, batchBuffer, &refIdxParams));

            if (hevcSlcParams->slice_type == encodeHevcBSlice)
            {
                refIdxParams.ucList          = LIST_1;
                refIdxParams.ucNumRefForList = hevcSlcParams->num_ref_idx_l1_active_minus1 + 1;
                ENCODE_CHK_STATUS_RETURN(m_hcpInterface->AddHcpRefIdxStateCmd(cmdBuffer, batchBuffer, &refIdxParams));
            }
        }

        return eStatus;
    }

    MOS_STATUS HevcVdencPktG12::CalculatePictureStateCommandSize()
    {
        ENCODE_FUNC_CALL();

        MHW_VDBOX_STATE_CMDSIZE_PARAMS_G12 stateCmdSizeParams;
        ENCODE_CHK_STATUS_RETURN(
            m_hwInterface->GetHxxStateCommandSize(
                CODECHAL_ENCODE_MODE_HEVC,
                &m_defaultPictureStatesSize,
                &m_defaultPicturePatchListSize,
                &stateCmdSizeParams));

        return MOS_STATUS_SUCCESS;
    }
}

