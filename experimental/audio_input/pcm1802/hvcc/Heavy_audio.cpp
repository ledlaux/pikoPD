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

#include "Heavy_audio.hpp"

#include <new>

#define Context(_c) static_cast<Heavy_audio *>(_c)


/*
 * C Functions
 */

extern "C" {
  HV_EXPORT HeavyContextInterface *hv_audio_new(double sampleRate) {
    // allocate aligned memory
    void *ptr = hv_malloc(sizeof(Heavy_audio));
    // ensure non-null
    if (!ptr) return nullptr;
    // call constructor
    new(ptr) Heavy_audio(sampleRate);
    return Context(ptr);
  }

  HV_EXPORT HeavyContextInterface *hv_audio_new_with_options(double sampleRate,
      int poolKb, int inQueueKb, int outQueueKb) {
    // allocate aligned memory
    void *ptr = hv_malloc(sizeof(Heavy_audio));
    // ensure non-null
    if (!ptr) return nullptr;
    // call constructor
    new(ptr) Heavy_audio(sampleRate, poolKb, inQueueKb, outQueueKb);
    return Context(ptr);
  }

  HV_EXPORT void hv_audio_free(HeavyContextInterface *instance) {
    // call destructor
    Context(instance)->~Heavy_audio();
    // free memory
    hv_free(instance);
  }
} // extern "C"







/*
 * Class Functions
 */

Heavy_audio::Heavy_audio(double sampleRate, int poolKb, int inQueueKb, int outQueueKb)
    : HeavyContext(sampleRate, poolKb, inQueueKb, outQueueKb) {
  numBytes += sTabwrite_init(&sTabwrite_IfbKXozw, &hTable_753Bl4cq);
  numBytes += sTabread_init(&sTabread_k68WdSAR, &hTable_753Bl4cq, true);
  numBytes += cVar_init_f(&cVar_taOuagWP, 0.0f);
  numBytes += cDelay_init(this, &cDelay_sdZptxpN, 0.0f);
  numBytes += cDelay_init(this, &cDelay_NI44oTfH, 0.0f);
  numBytes += hTable_init(&hTable_753Bl4cq, 256);
  numBytes += cTabhead_init(&cTabhead_uReCLGZU, &hTable_753Bl4cq);
  numBytes += cVar_init_s(&cVar_Lec2LUO8, "del-buf");
  numBytes += cDelay_init(this, &cDelay_fQeOi1P4, 250.0f);
  numBytes += cDelay_init(this, &cDelay_PfEXGsxx, 0.0f);
  numBytes += cBinop_init(&cBinop_lnXj7yms, 250.0f); // __mul
  numBytes += cBinop_init(&cBinop_E0sUuwlY, 0.0f); // __sub
  numBytes += cBinop_init(&cBinop_xdG9ps3T, 0.0f); // __max
  numBytes += cBinop_init(&cBinop_1lrhiYXY, 0.0f); // __sub
  
  // schedule a message to trigger all loadbangs via the __hv_init receiver
  scheduleMessageForReceiver(0xCE5CC65B, msg_initWithBang(HV_MESSAGE_ON_STACK(1), 0));
}

Heavy_audio::~Heavy_audio() {
  hTable_free(&hTable_753Bl4cq);
}

HvTable *Heavy_audio::getTableForHash(hv_uint32_t tableHash) {switch (tableHash) {
    case 0x12A436CA: return &hTable_753Bl4cq; // del-buf
    default: return nullptr;
  }
}

void Heavy_audio::scheduleMessageForReceiver(hv_uint32_t receiverHash, HvMessage *m) {
  switch (receiverHash) {
    case 0xCE5CC65B: { // __hv_init
      mq_addMessageByTimestamp(&mq, m, 0, &cReceive_BsET403Q_sendMessage);
      break;
    }
    case 0x8E55AA5F: { // time
      mq_addMessageByTimestamp(&mq, m, 0, &cReceive_6DpISZ6q_sendMessage);
      break;
    }
    default: return;
  }
}

int Heavy_audio::getParameterInfo(int index, HvParameterInfo *info) {
  if (info != nullptr) {
    switch (index) {
      case 0: {
        info->name = "time";
        info->hash = 0x8E55AA5F;
        info->type = HvParameterType::HV_PARAM_TYPE_PARAMETER_IN;
        info->minVal = 0.0f;
        info->maxVal = 499.0f;
        info->defaultVal = 250.0f;
        break;
      }
      default: {
        info->name = "invalid parameter index";
        info->hash = 0;
        info->type = HvParameterType::HV_PARAM_TYPE_PARAMETER_IN;
        info->minVal = 0.0f;
        info->maxVal = 0.0f;
        info->defaultVal = 0.0f;
        break;
      }
    }
  }
  return 1;
}



/*
 * Send Function Implementations
 */


void Heavy_audio::cVar_taOuagWP_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cBinop_onMessage(_c, &Context(_c)->cBinop_lnXj7yms, HV_BINOP_MULTIPLY, 1, m, &cBinop_lnXj7yms_sendMessage);
  cCast_onMessage(_c, HV_CAST_BANG, 0, m, &cCast_ORROlEd4_sendMessage);
}

void Heavy_audio::cMsg_8wuJMiYY_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const n) {
  HvMessage *m = nullptr;
  m = HV_MESSAGE_ON_STACK(1);
  msg_init(m, 1, msg_getTimestamp(n));
  msg_setSymbol(m, 0, "samplerate");
  cSystem_onMessage(_c, NULL, 0, m, &cSystem_cNSDQ5bz_sendMessage);
}

void Heavy_audio::cSystem_cNSDQ5bz_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cBinop_k_onMessage(_c, NULL, HV_BINOP_DIVIDE, 1000.0f, 0, m, &cBinop_muyCCR2z_sendMessage);
}

void Heavy_audio::cDelay_sdZptxpN_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const m) {
  cDelay_clearExecutingMessage(&Context(_c)->cDelay_sdZptxpN, m);
  cDelay_onMessage(_c, &Context(_c)->cDelay_NI44oTfH, 0, m, &cDelay_NI44oTfH_sendMessage);
  cDelay_onMessage(_c, &Context(_c)->cDelay_sdZptxpN, 0, m, &cDelay_sdZptxpN_sendMessage);
  sTabwrite_onMessage(_c, &Context(_c)->sTabwrite_IfbKXozw, 1, m, NULL);
}

void Heavy_audio::cDelay_NI44oTfH_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const m) {
  cDelay_clearExecutingMessage(&Context(_c)->cDelay_NI44oTfH, m);
  cMsg_43ap6yPm_sendMessage(_c, 0, m);
}

void Heavy_audio::cSwitchcase_JlBQOj9w_onMessage(HeavyContextInterface *_c, void *o, int letIn, const HvMessage *const m, void *sendMessage) {
  int msgIndex = 0;
  switch (msg_getHash(m, msgIndex)) {
    case 0x6D60E6E: { // "symbol"
      msgIndex = 1;
      break;
    }
  }
  switch (msg_getHash(m, msgIndex)) {
    case 0x47BE8354: { // "clear"
      cMsg_oi6zV1l5_sendMessage(_c, 0, m);
      break;
    }
    default: {
      break;
    }
  }
}

void Heavy_audio::cBinop_vj60cVcI_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cMsg_fhwc1Inr_sendMessage(_c, 0, m);
}

void Heavy_audio::hTable_753Bl4cq_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cMsg_l9lvwXNc_sendMessage(_c, 0, m);
  cDelay_onMessage(_c, &Context(_c)->cDelay_sdZptxpN, 2, m, &cDelay_sdZptxpN_sendMessage);
  cCast_onMessage(_c, HV_CAST_BANG, 0, m, &cCast_MEILyWVC_sendMessage);
}

void Heavy_audio::cMsg_fhwc1Inr_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const n) {
  HvMessage *m = nullptr;
  m = HV_MESSAGE_ON_STACK(2);
  msg_init(m, 2, msg_getTimestamp(n));
  msg_setSymbol(m, 0, "resize");
  msg_setElementToFrom(m, 1, n, 0);
  hTable_onMessage(_c, &Context(_c)->hTable_753Bl4cq, 0, m, &hTable_753Bl4cq_sendMessage);
}

void Heavy_audio::cBinop_muyCCR2z_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cBinop_k_onMessage(_c, NULL, HV_BINOP_MULTIPLY, 500.0f, 0, m, &cBinop_vj60cVcI_sendMessage);
}

void Heavy_audio::cMsg_43ap6yPm_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const n) {
  HvMessage *m = nullptr;
  m = HV_MESSAGE_ON_STACK(1);
  msg_init(m, 1, msg_getTimestamp(n));
  msg_setSymbol(m, 0, "mirror");
  hTable_onMessage(_c, &Context(_c)->hTable_753Bl4cq, 0, m, &hTable_753Bl4cq_sendMessage);
}

void Heavy_audio::cCast_MEILyWVC_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cDelay_onMessage(_c, &Context(_c)->cDelay_sdZptxpN, 0, m, &cDelay_sdZptxpN_sendMessage);
}

void Heavy_audio::cMsg_l9lvwXNc_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const n) {
  HvMessage *m = nullptr;
  m = HV_MESSAGE_ON_STACK(1);
  msg_init(m, 1, msg_getTimestamp(n));
  msg_setFloat(m, 0,  static_cast<float>(HV_N_SIMD));
  cDelay_onMessage(_c, &Context(_c)->cDelay_NI44oTfH, 2, m, &cDelay_NI44oTfH_sendMessage);
}

void Heavy_audio::cMsg_oi6zV1l5_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const n) {
  HvMessage *m = nullptr;
  m = HV_MESSAGE_ON_STACK(1);
  msg_init(m, 1, msg_getTimestamp(n));
  msg_setSymbol(m, 0, "clear");
  sTabwrite_onMessage(_c, &Context(_c)->sTabwrite_IfbKXozw, 1, m, NULL);
}

void Heavy_audio::cTabhead_uReCLGZU_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cBinop_onMessage(_c, &Context(_c)->cBinop_E0sUuwlY, HV_BINOP_SUBTRACT, 0, m, &cBinop_E0sUuwlY_sendMessage);
}

void Heavy_audio::cMsg_zLvRrYOx_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const n) {
  HvMessage *m = nullptr;
  m = HV_MESSAGE_ON_STACK(1);
  msg_init(m, 1, msg_getTimestamp(n));
  msg_setSymbol(m, 0, "samplerate");
  cSystem_onMessage(_c, NULL, 0, m, &cSystem_BjitEJZH_sendMessage);
}

void Heavy_audio::cSystem_BjitEJZH_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cBinop_k_onMessage(_c, NULL, HV_BINOP_DIVIDE, 1000.0f, 0, m, &cBinop_Qi8PdoeI_sendMessage);
}

void Heavy_audio::cVar_Lec2LUO8_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cMsg_t5R043Sh_sendMessage(_c, 0, m);
}

void Heavy_audio::cDelay_fQeOi1P4_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const m) {
  cDelay_clearExecutingMessage(&Context(_c)->cDelay_fQeOi1P4, m);
  cDelay_onMessage(_c, &Context(_c)->cDelay_PfEXGsxx, 0, m, &cDelay_PfEXGsxx_sendMessage);
  sTabread_onMessage(_c, &Context(_c)->sTabread_k68WdSAR, 0, m, &sTabread_k68WdSAR_sendMessage);
}

void Heavy_audio::cDelay_PfEXGsxx_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const m) {
  cDelay_clearExecutingMessage(&Context(_c)->cDelay_PfEXGsxx, m);
  sTabread_onMessage(_c, &Context(_c)->sTabread_k68WdSAR, 0, m, &sTabread_k68WdSAR_sendMessage);
  cDelay_onMessage(_c, &Context(_c)->cDelay_PfEXGsxx, 0, m, &cDelay_PfEXGsxx_sendMessage);
}

void Heavy_audio::sTabread_k68WdSAR_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  switch (letIn) {
    case 0: {
      break;
    }
    case 1: {
      cBinop_onMessage(_c, &Context(_c)->cBinop_1lrhiYXY, HV_BINOP_SUBTRACT, 0, m, &cBinop_1lrhiYXY_sendMessage);
      break;
    }
    default: return;
  }
}

void Heavy_audio::cBinop_lnXj7yms_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cBinop_onMessage(_c, &Context(_c)->cBinop_xdG9ps3T, HV_BINOP_MAX, 0, m, &cBinop_xdG9ps3T_sendMessage);
}

void Heavy_audio::cBinop_Qi8PdoeI_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cBinop_onMessage(_c, &Context(_c)->cBinop_lnXj7yms, HV_BINOP_MULTIPLY, 0, m, &cBinop_lnXj7yms_sendMessage);
}

void Heavy_audio::cBinop_E0sUuwlY_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cMsg_LDgtVEO1_sendMessage(_c, 0, m);
  sTabread_onMessage(_c, &Context(_c)->sTabread_k68WdSAR, 0, m, &sTabread_k68WdSAR_sendMessage);
  cCast_onMessage(_c, HV_CAST_BANG, 0, m, &cCast_beHLGRsl_sendMessage);
}

void Heavy_audio::cSystem_b0bceCSu_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cBinop_onMessage(_c, &Context(_c)->cBinop_1lrhiYXY, HV_BINOP_SUBTRACT, 1, m, &cBinop_1lrhiYXY_sendMessage);
  cDelay_onMessage(_c, &Context(_c)->cDelay_PfEXGsxx, 2, m, &cDelay_PfEXGsxx_sendMessage);
}

void Heavy_audio::cMsg_t5R043Sh_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const n) {
  HvMessage *m = nullptr;
  m = HV_MESSAGE_ON_STACK(3);
  msg_init(m, 3, msg_getTimestamp(n));
  msg_setSymbol(m, 0, "table");
  msg_setElementToFrom(m, 1, n, 0);
  msg_setSymbol(m, 2, "size");
  cSystem_onMessage(_c, NULL, 0, m, &cSystem_b0bceCSu_sendMessage);
}

void Heavy_audio::cMsg_LDgtVEO1_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const n) {
  HvMessage *m = nullptr;
  m = HV_MESSAGE_ON_STACK(1);
  msg_init(m, 1, msg_getTimestamp(n));
  msg_setSymbol(m, 0, "clear");
  cDelay_onMessage(_c, &Context(_c)->cDelay_fQeOi1P4, 0, m, &cDelay_fQeOi1P4_sendMessage);
  cDelay_onMessage(_c, &Context(_c)->cDelay_PfEXGsxx, 0, m, &cDelay_PfEXGsxx_sendMessage);
}

void Heavy_audio::cMsg_dDzrVm6p_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *const n) {
  HvMessage *m = nullptr;
  m = HV_MESSAGE_ON_STACK(1);
  msg_init(m, 1, msg_getTimestamp(n));
  msg_setFloat(m, 0,  static_cast<float>(HV_N_SIMD));
  cBinop_onMessage(_c, &Context(_c)->cBinop_xdG9ps3T, HV_BINOP_MAX, 1, m, &cBinop_xdG9ps3T_sendMessage);
}

void Heavy_audio::cBinop_xdG9ps3T_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cBinop_onMessage(_c, &Context(_c)->cBinop_E0sUuwlY, HV_BINOP_SUBTRACT, 1, m, &cBinop_E0sUuwlY_sendMessage);
}

void Heavy_audio::cCast_beHLGRsl_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cDelay_onMessage(_c, &Context(_c)->cDelay_fQeOi1P4, 0, m, &cDelay_fQeOi1P4_sendMessage);
}

void Heavy_audio::cBinop_5zZ95WTH_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cDelay_onMessage(_c, &Context(_c)->cDelay_fQeOi1P4, 2, m, &cDelay_fQeOi1P4_sendMessage);
}

void Heavy_audio::cBinop_1lrhiYXY_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cBinop_k_onMessage(_c, NULL, HV_BINOP_MULTIPLY, -1.0f, 0, m, &cBinop_5zZ95WTH_sendMessage);
}

void Heavy_audio::cCast_ORROlEd4_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cVar_onMessage(_c, &Context(_c)->cVar_Lec2LUO8, 0, m, &cVar_Lec2LUO8_sendMessage);
  cMsg_zLvRrYOx_sendMessage(_c, 0, m);
  cTabhead_onMessage(_c, &Context(_c)->cTabhead_uReCLGZU, 0, m, &cTabhead_uReCLGZU_sendMessage);
}

void Heavy_audio::cReceive_BsET403Q_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cMsg_8wuJMiYY_sendMessage(_c, 0, m);
  cMsg_dDzrVm6p_sendMessage(_c, 0, m);
  cVar_onMessage(_c, &Context(_c)->cVar_Lec2LUO8, 0, m, &cVar_Lec2LUO8_sendMessage);
  cMsg_zLvRrYOx_sendMessage(_c, 0, m);
  cTabhead_onMessage(_c, &Context(_c)->cTabhead_uReCLGZU, 0, m, &cTabhead_uReCLGZU_sendMessage);
}

void Heavy_audio::cReceive_6DpISZ6q_sendMessage(HeavyContextInterface *_c, int letIn, const HvMessage *m) {
  cVar_onMessage(_c, &Context(_c)->cVar_taOuagWP, 0, m, &cVar_taOuagWP_sendMessage);
}



/*
 * Code for expr~ implementation
 * Write out the generic implementation code
 */

 // per class code

 // per object code


/*
 * Context Process Implementation
 */

int Heavy_audio::process(float **inputBuffers, float **outputBuffers, int n) {
  while (hLp_hasData(&inQueue)) {
    hv_uint32_t numBytes = 0;
    ReceiverMessagePair *p = reinterpret_cast<ReceiverMessagePair *>(hLp_getReadBuffer(&inQueue, &numBytes));
    hv_assert(numBytes >= sizeof(ReceiverMessagePair));
    scheduleMessageForReceiver(p->receiverHash, &p->msg);
    hLp_consume(&inQueue);
  }

  sendBangToReceiver(0xDD21C0EB); // send to __hv_bang~ on next cycle
  const int n4 = n & ~HV_N_SIMD_MASK; // ensure that the block size is a multiple of HV_N_SIMD

  // temporary signal vars
  hv_bufferf_t Bf0, Bf1;

  // input and output vars
  hv_bufferf_t O0, O1;
  hv_bufferf_t I0, I1;

  // declare and init the zero buffer
  hv_bufferf_t ZERO; __hv_zero_f(VOf(ZERO));

  hv_uint32_t nextBlock = blockStartTimestamp;
  for (int n = 0; n < n4; n += HV_N_SIMD) {

    // process all of the messages for this block
    nextBlock += HV_N_SIMD;
    while (mq_hasMessageBefore(&mq, nextBlock)) {
      MessageNode *const node = mq_peek(&mq);
      node->sendMessage(this, node->let, node->m);
      mq_pop(&mq);
    }

    // load input buffers
    __hv_load_f(inputBuffers[0]+n, VOf(I0));
    __hv_load_f(inputBuffers[1]+n, VOf(I1));

    // zero output buffers
    __hv_zero_f(VOf(O0));
    __hv_zero_f(VOf(O1));

    // process all signal functions
    __hv_add_f(VIf(I0), VIf(I1), VOf(Bf0));
    __hv_tabwrite_f(&sTabwrite_IfbKXozw, VIf(Bf0));
    __hv_tabread_f(&sTabread_k68WdSAR, VOf(Bf0));
    __hv_add_f(VIf(Bf0), VIf(I1), VOf(Bf1));
    __hv_add_f(VIf(Bf1), VIf(O1), VOf(O1));
    __hv_add_f(VIf(Bf0), VIf(I0), VOf(Bf0));
    __hv_add_f(VIf(Bf0), VIf(O0), VOf(O0));

    // save output vars to output buffer
    __hv_store_f(outputBuffers[0]+n, VIf(O0));
    __hv_store_f(outputBuffers[1]+n, VIf(O1));
  }

  blockStartTimestamp = nextBlock;

  return n4; // return the number of frames processed

}

int Heavy_audio::processInline(float *inputBuffers, float *outputBuffers, int n4) {
  hv_assert(!(n4 & HV_N_SIMD_MASK)); // ensure that n4 is a multiple of HV_N_SIMD

  // define the heavy input buffer for 2 channel(s)
  float **const bIn = reinterpret_cast<float **>(hv_alloca(2*sizeof(float *)));
  bIn[0] = inputBuffers+(0*n4);
  bIn[1] = inputBuffers+(1*n4);

  // define the heavy output buffer for 2 channel(s)
  float **const bOut = reinterpret_cast<float **>(hv_alloca(2*sizeof(float *)));
  bOut[0] = outputBuffers+(0*n4);
  bOut[1] = outputBuffers+(1*n4);

  int n = process(bIn, bOut, n4);
  return n;
}

int Heavy_audio::processInlineInterleaved(float *inputBuffers, float *outputBuffers, int n4) {
  hv_assert(n4 & ~HV_N_SIMD_MASK); // ensure that n4 is a multiple of HV_N_SIMD

  // define the heavy input buffer for 2 channel(s), uninterleave
  float *const bIn = reinterpret_cast<float *>(hv_alloca(2*n4*sizeof(float)));
  #if HV_SIMD_SSE || HV_SIMD_AVX
  for (int i = 0, j = 0; j < n4; j += 4, i += 8) {
    __m128 a = _mm_load_ps(inputBuffers+i);                // LRLR
    __m128 b = _mm_load_ps(inputBuffers+4+i);              // LRLR
    __m128 x = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2,0,2,0)); // LLLL
    __m128 y = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3,1,3,1)); // RRRR
    _mm_store_ps(bIn+j, x);
    _mm_store_ps(bIn+n4+j, y);
  }
  #elif HV_SIMD_NEON
  for (int i = 0, j = 0; j < n4; j += 4, i += 8) {
    float32x4x2_t a = vld2q_f32(inputBuffers+i); // load and uninterleave
    vst1q_f32(bIn+j, a.val[0]);
    vst1q_f32(bIn+n4+j, a.val[1]);
  }
  #else // HV_SIMD_NONE
  for (int j = 0; j < n4; ++j) {
    bIn[0*n4+j] = inputBuffers[0+2*j];
    bIn[1*n4+j] = inputBuffers[1+2*j];
  }
  #endif

  // define the heavy output buffer for 2 channel(s)
  float *const bOut = reinterpret_cast<float *>(hv_alloca(2*n4*sizeof(float)));

  int n = processInline(bIn, bOut, n4);

  // interleave the heavy output into the output buffer
  #if HV_SIMD_AVX
  for (int i = 0, j = 0; j < n4; j += 8, i += 16) {
    __m256 x = _mm256_load_ps(bOut+j);    // LLLLLLLL
    __m256 y = _mm256_load_ps(bOut+n4+j); // RRRRRRRR
    __m256 a = _mm256_unpacklo_ps(x, y);  // LRLRLRLR
    __m256 b = _mm256_unpackhi_ps(x, y);  // LRLRLRLR
    _mm256_store_ps(outputBuffers+i, a);
    _mm256_store_ps(outputBuffers+8+i, b);
  }
  #elif HV_SIMD_SSE
  for (int i = 0, j = 0; j < n4; j += 4, i += 8) {
    __m128 x = _mm_load_ps(bOut+j);    // LLLL
    __m128 y = _mm_load_ps(bOut+n4+j); // RRRR
    __m128 a = _mm_unpacklo_ps(x, y);  // LRLR
    __m128 b = _mm_unpackhi_ps(x, y);  // LRLR
    _mm_store_ps(outputBuffers+i, a);
    _mm_store_ps(outputBuffers+4+i, b);
  }
  #elif HV_SIMD_NEON
  // https://community.arm.com/groups/processors/blog/2012/03/13/coding-for-neon--part-5-rearranging-vectors
  for (int i = 0, j = 0; j < n4; j += 4, i += 8) {
    float32x4_t x = vld1q_f32(bOut+j);
    float32x4_t y = vld1q_f32(bOut+n4+j);
    float32x4x2_t z = {x, y};
    vst2q_f32(outputBuffers+i, z); // interleave and store
  }
  #else // HV_SIMD_NONE
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < n4; ++j) {
      outputBuffers[i+2*j] = bOut[i*n4+j];
    }
  }
  #endif

  return n;
}
