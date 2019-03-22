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
//! \file     encode_hevc_vdenc_packet_g12.h
//! \brief    Defines the interface to adapt to avc vdenc encode pipeline Gen12
//!

#ifndef __CODECHAL_HEVC_VDENC_PACKET_G12_H__
#define __CODECHAL_HEVC_VDENC_PACKET_G12_H__

#include "mhw_vdbox_g12_X.h"
#include "mhw_vdbox_hcp_g12_X.h"
#include "mhw_vdbox_vdenc_g12_X.h"
#include "mhw_mi_g12_X.h"
#include "mhw_render_g12_X.h"
#include "encode_hevc_vdenc_packet.h"

namespace encode
{
#define CODECHAL_CACHELINE_SIZE                 64
#define CODECHAL_HEVC_PAK_STREAMOUT_SIZE 0x500000  //size is accounted for 4Kx4K with all 8x8 CU,based on streamout0 and streamout1 requirements

    //!
    //! \struct HucPakStitchDmemVdencG12
    //! \brief  The struct of Huc Com Dmem
    //!
    struct HucPakStitchDmemVdencG12
    {
        uint32_t     tileSizeRecordOffset[5];   // Tile Size Records, start offset  in byte, 0xffffffff means unavailable
        uint32_t     vdencStatOffset[5];        // needed for HEVC VDEnc, VP9 VDEnc, start offset  in byte, 0xffffffff means unavailable
        uint32_t     hevcPakStatOffset[5];      //needed for HEVC VDEnc, start offset  in byte, 0xffffffff means unavailable
        uint32_t     hevcStreamOutOffset[5];    //needed for HEVC VDEnc, start offset  in byte, 0xffffffff means unavailable
        uint32_t     vp9PakStatOffset[5];       //needed for VP9 VDEnc, start offset  in byte, 0xffffffff means unavailable
        uint32_t     vp9CounterBufferOffset[5]; //needed for VP9 VDEnc, start offset  in byte, 0xffffffff means unavailable
        uint32_t     lastTileBSStartInBytes;    // last tile in bitstream for region 4 and region 5
        uint32_t     sliceHeaderSizeinBits;     // needed for HEVC dual pipe BRC
        uint16_t     totalSizeInCommandBuffer;  // Total size in bytes of valid data in the command buffer
        uint16_t     offsetInCommandBuffer;     // Byte  offset of the to-be-updated Length (uint32_t) in the command buffer, 0xffff means unavailable
        uint16_t     picWidthInPixel;           // Picture width in pixel
        uint16_t     picHeightInPixel;          // Picture hieght in pixel
        uint16_t     totalNumberOfPAKs;         // [2..4]
        uint16_t     numSlices[4];              // this is number of slices from each PAK
        uint16_t     numTiles[4];               // this is number of tiles from each PAK
        uint16_t     picStateStartInBytes;      // offset for  region 7 and region 8
        uint8_t      codec;                     // 1: HEVC DP; 2: HEVC VDEnc; 3: VP9 VDEnc
        uint8_t      maxPass;                   // Max number of BRC pass >=1
        uint8_t      currentPass;               // Current BRC pass [1..MAXPass]
        uint8_t      minCUSize;                 // Minimum CU size (3: 8x8, 4:16x16), HEVC only.
        uint8_t      cabacZeroWordFlag;         // cabac zero flag, HEVC only
        uint8_t      bitdepthLuma;              // luma bitdepth, HEVC only
        uint8_t      bitdepthChroma;            // chroma bitdepth, HEVC only
        uint8_t      chromaFormatIdc;           // chroma format idc, HEVC only
        uint8_t      currFrameBRCLevel;         // Hevc dual pipe only
        uint8_t      brcUnderFlowEnable;        // Hevc dual pipe only
        uint8_t      stitchEnable;              // enable stitch cmd for Hevc dual pipe
        uint8_t      reserved1;
        uint16_t     stitchCommandOffset;       // offset in region 10 which is the second level batch buffer
        uint16_t     reserved2;
        uint32_t     bbEndforStitch;
        uint8_t      rsvd[16];
    };


    class HevcVdencPktG12 : public HevcVdencPkt
    {
    public:

        //!
        //! \brief  Constructor of class 
        //! \param  [in]*pipeline
        //!
        //! \param  [in]*task
        //!
        //! \param  [in]*hwInterface
        //!
        //!
        HevcVdencPktG12(MediaPipeline *pipeline, MediaTask *task, CodechalHwInterface *hwInterface) :
            HevcVdencPkt(pipeline, task, hwInterface) { }


        //!
        //! \brief  Constructor of class 
        //!
        virtual ~HevcVdencPktG12() {}

        //!
        //! \brief  Prepare the parameters for command submission
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        virtual MOS_STATUS Prepare() override;

        //!
        //! \brief  Add the command sequence into the commandBuffer and
        //!         and return to the caller task
        //! \param  [in] commandBuffer
        //!         Pointer to the command buffer which is allocated by caller
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        virtual MOS_STATUS Submit(
            MOS_COMMAND_BUFFER* commandBuffer,
            uint8_t packetPhase = otherPacket) override;


        //!
        //! \brief
        //! \param  [in]&picStateParams
        //!
        //! \return void
        //!
        //!
        void SetHcpPicStateParams(MHW_VDBOX_HEVC_PIC_STATE& picStateParams);

        //!
        //! \brief
        //! \param  [in]cmdBuffer
        //!
        //! \param  [in]addToBatchBufferHuCBRC
        //!
        //! \param  [in]isLowDelayB
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS AddVdencCmd1Cmd(PMOS_COMMAND_BUFFER cmdBuffer, bool addToBatchBufferHuCBRC, bool isLowDelayB);

        //!
        //! \brief
        //! \param  [in]cmdBuffer
        //!
        //! \param  [in]addToBatchBufferHuCBRC
        //!
        //! \param  [in]isLowDelayB
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS AddVdencCmd2Cmd(PMOS_COMMAND_BUFFER cmdBuffer, bool addToBatchBufferHuCBRC, bool isLowDelayB);


        //!
        //! \brief
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        virtual MOS_STATUS CalculatePictureStateCommandSize() override;

    protected:

        //!
        //! \brief
        //! \param  [in]&cmdBuffer
        //!
        //! \param  [in]packetPhase
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS PatchSliceLevelCommands(MOS_COMMAND_BUFFER &cmdBuffer, uint8_t packetPhase);

        //!
        //! \brief
        //! \param  [in]&cmdBuffer
        //!
        //! \param  [in]packetPhase
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS PatchTileLevelCommands(MOS_COMMAND_BUFFER &cmdBuffer, uint8_t packetPhase);

        //!
        //! \brief
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS AddOneTileCommands(
            MOS_COMMAND_BUFFER  &cmdBuffer,
            uint32_t            tileRow,
            uint32_t            tileCol,
            uint32_t            tileRowPass);


        //!
        //! \brief
        //! \param  [in]&cmdBuffer
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS AddSlicesCommandsInTile(
            MOS_COMMAND_BUFFER &cmdBuffer);


        //!
        //! \brief
        //! \return void
        //!
        //!
        void UpdateParameters();


        //!
        //! \brief
        //! \param  [in]&cmdBuffer
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS AddPicStateWithNoTile(
            MOS_COMMAND_BUFFER &cmdBuffer);


        //!
        //! \brief
        //! \param  [in]&cmdBuffer
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS AddPicStateWithTile(
            MOS_COMMAND_BUFFER &cmdBuffer);

        /******************************************************************
        Picture Level related
        *******************************************************************/

        //!
        //! \brief
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS AddHcpPipeBufAddrCmd(
            MOS_COMMAND_BUFFER  &cmdBuffer);


        //!
        //! \brief
        //! \param  [in]&cmdBuffer
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS AddPictureHcpCommands(MOS_COMMAND_BUFFER &cmdBuffer);


        //!
        //! \brief
        //! \param  [in]&params
        //!
        //! \param  [in]&cmdBuffer
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS AddVdencWalkerStateCmd(
            MHW_VDBOX_HEVC_SLICE_STATE &params,
            MOS_COMMAND_BUFFER &cmdBuffer);


        //!
        //! \brief
        //! \param  [in]&pipeBufAddrParams
        //!
        //! \return void
        //!
        //!
        void SetHcpPipeBufAddrParams(MHW_VDBOX_PIPE_BUF_ADDR_PARAMS& pipeBufAddrParams);


        //!
        //! \brief
        //! \param  [in]&cmdBuffer
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS AddPictureVdencCommands(MOS_COMMAND_BUFFER &cmdBuffer);


        //!
        //! \brief
        //! \param  [in]&pipeBufAddrParams
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS SetVdencPipeBufAddrParams(
            MHW_VDBOX_PIPE_BUF_ADDR_PARAMS& pipeBufAddrParams);


        //!
        //! \brief
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS PatchPictureLevelCommands(const uint8_t &packetPhase, MOS_COMMAND_BUFFER  &cmdBuffer);


        //!
        //! \brief
        //! \param  [in]&cmdBuffer
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS InsertSeqStreamEnd(MOS_COMMAND_BUFFER &cmdBuffer);


        //!
        //! \brief
        //! \param  [in]&cmdBuffer
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS EnsureAllCommandsExecuted(MOS_COMMAND_BUFFER &cmdBuffer);

        // virtual functions

        //!
        //! \brief
        //! \param  [in]&vdboxPipeModeSelectParams
        //!
        //! \return void
        //!
        //!
        virtual void SetHcpPipeModeSelectParams(
            MHW_VDBOX_PIPE_MODE_SELECT_PARAMS& vdboxPipeModeSelectParams) override;


        //!
        //! \brief
        //! \param  [in]params
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        virtual MOS_STATUS AddHcpRefIdxCmd(
            PMOS_COMMAND_BUFFER         cmdBuffer,
            PMHW_BATCH_BUFFER           batchBuffer,
            PMHW_VDBOX_HEVC_SLICE_STATE params);


        //!
        //! \brief
        //! \param  [in]&cmdBuffer
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        virtual MOS_STATUS AddHcpPipeModeSelect(
            MOS_COMMAND_BUFFER &cmdBuffer) override;


        //!
        //! \brief
        //! \param  [in]&cmdBuffer
        //!
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        virtual MOS_STATUS AddHcpSurfaces(MOS_COMMAND_BUFFER &cmdBuffer) override;


        //!
        //! \brief
        //! \param  [in]&vdboxPipeModeSelectParams
        //!
        //! \return void
        //!
        //!
        void SetVdencPipeModeSelectParams(MHW_VDBOX_PIPE_MODE_SELECT_PARAMS& vdboxPipeModeSelectParams)override;


        //!
        //! \brief
        //! \param  [in]&sliceStateParams
        //!
        //! \return void
        //!
        //!
        void SetHcpSliceStateCommonParams(MHW_VDBOX_HEVC_SLICE_STATE& sliceStateParams);


        //!
        //! \brief
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        virtual MOS_STATUS SetHcpSliceStateParams(
            MHW_VDBOX_HEVC_SLICE_STATE  &sliceStateParams,
            PCODEC_ENCODER_SLCDATA      slcData,
            uint32_t                    currSlcIdx) override;


        //!
        //! \brief
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        MOS_STATUS Construct3rdLevelBatch();


        //!
        //! \brief
        //! \return MOS_STATUS
        //!         MOS_STATUS_SUCCESS if success, else fail reason
        //!
        virtual MOS_STATUS AllocateResources();

        static constexpr uint32_t m_VdboxVDENCRegBase[4] = M_VDBOX_VDENC_REG_BASE;
        static constexpr uint32_t m_NumPassesForTileReplay = 1; // todo: Change when enabling tile replay 

        MHW_VDBOX_PIPE_MODE_SELECT_PARAMS_G12 m_pipeModeSelectParams = {};

        // SCC related
        bool                        m_enableSCC = false;                   //!< Flag to indicate if HEVC SCC is enabled.
        unsigned char               m_slotForRecNotFiltered = 0;           //!< Slot for not filtered reconstructed surface
        //MOS_RESOURCE                m_vdencRecNotFilteredBuffer;         //!< Rec but not filtered surface for IBC

        bool                        m_rgbEncodingEnable = false;           //!< Enable RGB encoding
        bool                        m_captureModeEnable = false;           //!< Enable Capture mode with display

        // VDENC Display interface related
        bool                        m_enableLBCOnly = false;               //!< Enable LBC only for IBC
        bool                        m_enablePartialFrameUpdate = false;    //!< Enable Parital Frame Update
        
	// GEN12 specific resources
        //MOS_RESOURCE                m_vdencTileRowStoreBuffer;           //!< Tile row store buffer
        //MOS_RESOURCE                m_vdencPaletteModeStreamOutBuffer;   //!< Palette mode stream out buffer
        //MOS_RESOURCE                m_resHwCountTileReplay;              //!< Tile based HW Counter buffer

        // Tile level batch buffer
        //uint32_t                    m_tileLevelBatchSize = 0;            //!< Size of the 2rd level batch buffer for each tile
        //uint32_t                    m_numTileBatchAllocated = 0;         //!< The number of allocated batch buffer for tiles
        //PMHW_BATCH_BUFFER           m_tileLevelBatchBuffer[VDENC_BRC_NUM_OF_PASSES];   //!< Tile level batch buffer for each tile

        // 3rd Level Batch buffer
        uint32_t                    m_thirdLBSize = 0;                     //!< Size of the 3rd level batch buffer
        MHW_BATCH_BUFFER            m_thirdLevelBatchBuffer;               //!< 3rd level batch buffer
    };

}

#endif
