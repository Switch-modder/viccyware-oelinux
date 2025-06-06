/*-------------------------------------------------------------------
Copyright (c) 2013-2017 Qualcomm Technologies, Inc. All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential
--------------------------------------------------------------------*/

#include "vtest_ComDef.h"
#include "vtest_Debug.h"
#include "vtest_Time.h"
#include "vtest_BufferManager.h"
#include "vtest_TestTranscode.h"
#include "vtest_DecoderFileSource.h"
#include "vtest_Decoder.h"
#include "vtest_Encoder.h"
#include "vtest_EncoderFileSink.h"
#ifndef NATIVE_WINDOW_SINK_DISABLE
#include "vtest_NativeWindow.h"
#endif

#undef LOG_TAG
#define LOG_TAG "VTEST_TEST_TRANSCODE"

namespace vtest {

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
TestTranscode::TestTranscode()
    : ITestCase(),
      m_pBufferManager(NULL),
      m_pCrypto(NULL) {

    m_pSources.clear();
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
TestTranscode::~TestTranscode() {}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE TestTranscode::Finish() {

    OMX_ERRORTYPE result = ITestCase::Finish();

    VTEST_MSG_HIGH("Deleting all objects");
    for (OMX_U32 i = 0; i < m_pSources.size(); i++) {
        if (m_pSources[i]) {
            delete m_pSources[i];
            m_pSources[i] = NULL;
        }
    }

    m_pSources.clear();

    if (m_pBufferManager) {
        delete m_pBufferManager;
        m_pBufferManager = NULL;
    }

    if (m_pCrypto) {
        delete m_pCrypto;
        m_pCrypto = NULL;
    }
    return result;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE TestTranscode::Execute(CodecConfigType *pConfig,
        DynamicConfigType *pDynamicConfig, OMX_S32 nTestNum) {

    (void)pDynamicConfig;
    OMX_ERRORTYPE result = OMX_ErrorNone;
    OMX_U32 i;

    pConfig->nCurrentTestNum = nTestNum;

    m_pCrypto = new Crypto();
    m_pBufferManager = new BufferManager();

    if (!m_pCrypto || !m_pBufferManager) {
        return OMX_ErrorInsufficientResources;
    }

    m_pSources.push_back(new DecoderFileSource(m_pCrypto));
    m_pSources.push_back(new Decoder(pConfig));
    m_pSources.push_back(new NativeWindow(
                pConfig->bRotateDisplay, pConfig->nOutputColorFormat));
    m_pSources.push_back(new Encoder(pConfig));
    m_pSources.push_back(new EncoderFileSink(m_pCrypto));

    if (pConfig->bSecureSession) {
        result = m_pCrypto->Init();
        FAILED1(result, "Crypto Init failed");
    }

    for (i = 0; i < m_pSources.size(); i++) {

        if (!m_pSources[i]) {
            return OMX_ErrorInsufficientResources;
        }

        ISource *prev = (i == 0 ? NULL : m_pSources[i-1]);
        ISource *next = (i == m_pSources.size() - 1 ? NULL : m_pSources[i+1]);
        VTEST_MSG_HIGH("Linking %s: %s ==> %s ==> %s",
                m_pSources[i]->Name(), (prev != NULL ? prev->Name() : "NONE"),
                m_pSources[i]->Name(), (next != NULL ? next->Name() : "NONE"));
        result = m_pSources[i]->Configure(pConfig, m_pBufferManager, prev, next);
        FAILED1(result, "Could not configure %s", m_pSources[i]->Name());
    }

    for (i = 0; i < (m_pSources.size() - 1); i++) {
        result = m_pBufferManager->SetupBufferPool(m_pSources[i], m_pSources[i+1]);
        if (result != OMX_ErrorNone) {
            VTEST_MSG_ERROR("Buffer pool alloc failed between %s %s",
                    m_pSources[i]->Name(), m_pSources[i+1]->Name());
            break;
        }
    }

    if (result != OMX_ErrorNone) {
        while (i > 0) {
            m_pBufferManager->FreeBuffers(m_pSources[i-1], PORT_INDEX_OUT);
            i--;
        }
        return result;
    }

    OMX_S32 executionResult = 0;

    /*start/stop the last node so that we can transcode all frames*/
    executionResult = m_pSources[m_pSources.size() - 1]->Start();

    executionResult |= m_pSources[m_pSources.size() - 1]->Finish();

    executionResult |= m_pSources[m_pSources.size() - 1]->Stop();
    result = (OMX_ERRORTYPE)executionResult;

    for (i = 0; i < (m_pSources.size() - 1); i++) {
        m_pBufferManager->FreeBuffers(m_pSources[i], PORT_INDEX_OUT);
    }

    if (pConfig->bSecureSession) {
        m_pCrypto->Terminate();
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
OMX_ERRORTYPE TestTranscode::ValidateAssumptions(CodecConfigType *pConfig,
        DynamicConfigType *pDynamicConfig) {

    (void)pConfig; (void)pDynamicConfig;
#if 1
    // Transcode only works with smoothstreaming due to OMX encoder bugs:
    // 1) encoder: cannot disable only InputPort
    // 2) encoder: cannot disable port on without total reallocation
    // 3) encoder: cannot set buffer-count in enabled state
    // Transcode uses shared buffers between dec-nw-enc
    pConfig->bMetaMode = OMX_TRUE;
    pConfig->eMetaBufferType = GrallocSource;
    pConfig->ePlaybackMode = QCSmoothStreaming;
    //pConfig->nFrameWidth = 1920;
    //pConfig->nFrameHeight = 1080;
#endif
    return OMX_ErrorNone;
}

} // namespace vtest
