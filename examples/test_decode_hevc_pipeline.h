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
//! \file     examples/decode_hevc_pipeline.h 
//! \brief
//!
#ifndef __DECODE_HEVC_PIPELINE_H__
#define __DECODE_HEVC_PIPELINE_H__
#include "decode_pipeline.h"

namespace decode {

class HevcPipeline : public DecodePipeline
{
public:


    //!
    //! \brief  Constructor of class 
    //! \param  [in]*debugInterface
    //!
    //!
    HevcPipeline(
        CodechalHwInterface *   hwInterface,
        CodechalDebugInterface *debugInterface);


    //!
    //! \brief  Constructor of class 
    //!
    virtual ~HevcPipeline() {};

protected:

    //!
    //! \brief
    //! \param  [in]*settings
    //!
    //! \return MOS_STATUS
    //!         MOS_STATUS_SUCCESS if success, else fail reason
    //!
    virtual MOS_STATUS Initialize(void *settings) override;

    //!
    //! \brief
    //! \return MOS_STATUS
    //!         MOS_STATUS_SUCCESS if success, else fail reason
    //!
    virtual MOS_STATUS Uninitialize() override;

    //!
    //! \brief
    //! \return MOS_STATUS
    //!         MOS_STATUS_SUCCESS if success, else fail reason
    //!
    virtual MOS_STATUS CreateBufferTracker() override;

    //!
    //! \brief
    //! \return MOS_STATUS
    //!         MOS_STATUS_SUCCESS if success, else fail reason
    //!
    virtual MOS_STATUS CreateStatusReport() override;
    bool m_shortFormatInUse;                                 //!< Indicate it is Short Format
    CODEC_PICTURE   m_currPic ={};                           //!< Current Picture Structs
};

}
#endif // !__DECODE_HEVC_PIPELINE_H__
