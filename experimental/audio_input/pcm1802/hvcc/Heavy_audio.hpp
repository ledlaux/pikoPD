/**
 * Copyright (c) 2026 Enzien Audio, Ltd.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the phrase "powered by heavy",
 *    the heavy logo, and a hyperlink to https://enzienaudio.com, all in a visible
 *    form.
 * 
 *   2.1 If the Application is distributed in a store system (for example,
 *       the Apple "App Store" or "Google Play"), the phrase "powered by heavy"
 *       shall be included in the app description or the copyright text as well as
 *       the in the app itself. The heavy logo will shall be visible in the app
 *       itself as well.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef _HEAVY_CONTEXT_AUDIO_HPP_
#define _HEAVY_CONTEXT_AUDIO_HPP_

// object includes
#include "HeavyContext.hpp"
#include "HvControlVar.h"
#include "HvMath.h"
#include "HvTable.h"
#include "HvControlDelay.h"
#include "HvSignalTabwrite.h"
#include "HvControlTabhead.h"
#include "HvControlSystem.h"
#include "HvControlCast.h"
#include "HvControlBinop.h"
#include "HvSignalTabread.h"

class Heavy_audio : public HeavyContext {

 public:
  Heavy_audio(double sampleRate, int poolKb=10, int inQueueKb=2, int outQueueKb=0);
  ~Heavy_audio();

  const char *getName() override { return "audio"; }
  int getNumInputChannels() override { return 2; }
  int getNumOutputChannels() override { return 2; }

  int process(float **inputBuffers, float **outputBuffer, int n) override;
  int processInline(float *inputBuffers, float *outputBuffer, int n) override;
  int processInlineInterleaved(float *inputBuffers, float *outputBuffer, int n) override;

  int getParameterInfo(int index, HvParameterInfo *info) override;
  struct Parameter {
    struct In {
      enum ParameterIn : hv_uint32_t {
        TIME = 0x8E55AA5F, // time
      };
    };
  };

 private:
  HvTable *getTableForHash(hv_uint32_t tableHash) override;
  void scheduleMessageForReceiver(hv_uint32_t receiverHash, HvMessage *m) override;


  /*
  * Code for expr~ implementation
  * Write out the generic header code
  */

  // per class code

  // per object code


  // static sendMessage functions
  static void cVar_taOuagWP_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cMsg_8wuJMiYY_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cSystem_cNSDQ5bz_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cDelay_sdZptxpN_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cDelay_NI44oTfH_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cSwitchcase_JlBQOj9w_onMessage(HeavyContextInterface *, void *, int letIn, const HvMessage *const, void *);
  static void cBinop_vj60cVcI_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void hTable_753Bl4cq_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cMsg_fhwc1Inr_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cBinop_muyCCR2z_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cMsg_43ap6yPm_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cCast_MEILyWVC_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cMsg_l9lvwXNc_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cMsg_oi6zV1l5_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cTabhead_uReCLGZU_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cMsg_zLvRrYOx_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cSystem_BjitEJZH_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cVar_Lec2LUO8_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cDelay_fQeOi1P4_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cDelay_PfEXGsxx_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void sTabread_k68WdSAR_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cBinop_lnXj7yms_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cBinop_Qi8PdoeI_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cBinop_E0sUuwlY_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cSystem_b0bceCSu_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cMsg_t5R043Sh_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cMsg_LDgtVEO1_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cMsg_dDzrVm6p_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cBinop_xdG9ps3T_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cCast_beHLGRsl_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cBinop_5zZ95WTH_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cBinop_1lrhiYXY_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cCast_ORROlEd4_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cReceive_BsET403Q_sendMessage(HeavyContextInterface *, int, const HvMessage *);
  static void cReceive_6DpISZ6q_sendMessage(HeavyContextInterface *, int, const HvMessage *);

  // objects
  SignalTabwrite sTabwrite_IfbKXozw;
  SignalTabread sTabread_k68WdSAR;
  ControlVar cVar_taOuagWP;
  ControlDelay cDelay_sdZptxpN;
  ControlDelay cDelay_NI44oTfH;
  ControlBinop cBinop_vj60cVcI;
  HvTable hTable_753Bl4cq;
  ControlBinop cBinop_muyCCR2z;
  ControlTabhead cTabhead_uReCLGZU;
  ControlVar cVar_Lec2LUO8;
  ControlDelay cDelay_fQeOi1P4;
  ControlDelay cDelay_PfEXGsxx;
  ControlBinop cBinop_lnXj7yms;
  ControlBinop cBinop_Qi8PdoeI;
  ControlBinop cBinop_E0sUuwlY;
  ControlBinop cBinop_xdG9ps3T;
  ControlBinop cBinop_5zZ95WTH;
  ControlBinop cBinop_1lrhiYXY;
};

#endif // _HEAVY_CONTEXT_AUDIO_HPP_
