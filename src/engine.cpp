// SPDX-License-Identifier: Apache-2.0
#include "engine.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_resource_variable.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/experimental/microfrontend/lib/frontend.h>
#include <tensorflow/lite/experimental/microfrontend/lib/frontend_util.h>

namespace {
#if CONFIG_FUURIN_AUDIO_INTERNAL_ARENA_SIZE > 0
alignas(16) uint8_t internal_arena[CONFIG_FUURIN_AUDIO_INTERNAL_ARENA_SIZE];
#endif
alignas(16) uint8_t variable_arena[2048];
FrontendState frontend;
tflite::MicroInterpreter *interpreter;
unsigned slice;
unsigned stride;
alignas(tflite::MicroInterpreter) unsigned char interpreter_storage[sizeof(tflite::MicroInterpreter)];
bool resolver_registered;

}
static bool register_ops(tflite::MicroMutableOpResolver<20> &r) {
#define ADD(op) if (r.Add##op() != kTfLiteOk) return false
    ADD(CallOnce); ADD(VarHandle); ADD(Reshape); ADD(ReadVariable);
    ADD(StridedSlice); ADD(Concatenation); ADD(AssignVariable); ADD(Conv2D);
    ADD(Mul); ADD(Add); ADD(Mean); ADD(FullyConnected); ADD(Logistic);
    ADD(Quantize); ADD(DepthwiseConv2D); ADD(AveragePool2D); ADD(MaxPool2D);
    ADD(Pad); ADD(Pack); ADD(SplitV);
#undef ADD
    return true;
}

static bool init_frontend() {
    FrontendConfig cfg;
    FrontendFillConfigWithDefaults(&cfg);
    cfg.window.size_ms = 30;
    cfg.window.step_size_ms = 10;
    cfg.filterbank.num_channels = 40;
    cfg.filterbank.lower_band_limit = 125.0f;
    cfg.filterbank.upper_band_limit = 7500.0f;
    cfg.noise_reduction.smoothing_bits = 10;
    cfg.noise_reduction.even_smoothing = 0.025f;
    cfg.noise_reduction.odd_smoothing = 0.06f;
    cfg.noise_reduction.min_signal_remaining = 0.05f;
    cfg.pcan_gain_control.enable_pcan = 1;
    cfg.pcan_gain_control.strength = 0.95f;
    cfg.pcan_gain_control.offset = 80.0f;
    cfg.pcan_gain_control.gain_bits = 21;
    cfg.log_scale.enable_log = 1;
    cfg.log_scale.scale_shift = 6;
    return FrontendPopulateState(&cfg, &frontend, 16000);
}


void mww_deinit() {
    if (interpreter) {
        interpreter->~MicroInterpreter();
        interpreter = nullptr;
    }
    FrontendFreeStateContents(&frontend);
    memset(&frontend, 0, sizeof(frontend));
    slice = 0;
}

bool mww_init(const unsigned char *data, size_t size, void *arena, size_t arena_size) {
#if CONFIG_FUURIN_AUDIO_INTERNAL_ARENA_SIZE > 0
    if (!arena) { arena = internal_arena; arena_size = sizeof(internal_arena); }
#endif
    if (!data || size < 8 || uintptr_t(data) % 16 || !arena ||
        uintptr_t(arena) % 16 || arena_size < 1024) return false;
    flatbuffers::Verifier verifier(data, size);
    if (!tflite::VerifyModelBuffer(verifier)) return false;
    memset(arena, 0, arena_size);
    memset(variable_arena, 0, sizeof(variable_arena));
    const auto *model = tflite::GetModel(data);
    if (model->version() != TFLITE_SCHEMA_VERSION) return false;
    static tflite::MicroMutableOpResolver<20> resolver;
    if (!resolver_registered) {
        if (!register_ops(resolver)) return false;
        resolver_registered = true;
    }
    auto *allocator = tflite::MicroAllocator::Create(variable_arena, sizeof(variable_arena));
    if (!allocator) return false;
    auto *variables = tflite::MicroResourceVariables::Create(allocator, 20);
    if (!variables) return false;
    interpreter = new (interpreter_storage) tflite::MicroInterpreter(
        model, resolver, static_cast<uint8_t *>(arena), arena_size, variables);
    if (interpreter->AllocateTensors() != kTfLiteOk) return false;
    if (interpreter->inputs_size() != 1 || interpreter->outputs_size() != 1) return false;
    auto *in = interpreter->input(0);
    auto *out = interpreter->output(0);
    if (in->type != kTfLiteInt8 || in->dims->size != 3 ||
        in->dims->data[0] != 1 || in->dims->data[1] < 1 || in->dims->data[2] != 40 ||
        in->bytes != unsigned(in->dims->data[1]) * 40 || in->params.scale <= 0 ||
        out->type != kTfLiteUInt8 || out->bytes != 1 || out->params.scale <= 0) return false;
    stride = in->dims->data[1];
    slice = 0;
    return init_frontend();
}
unsigned mww_stride() { return stride; }

bool mww_reset() {
    slice = 0;
    FrontendReset(&frontend);
    return interpreter->Reset() == kTfLiteOk;
}

unsigned mww_arena_used() { return interpreter->arena_used_bytes(); }

bool mww_process(const int16_t *pcm, size_t count, MwwScoreCallback callback, void *context) {
    while (count) {
        size_t used = 0;
        auto features = FrontendProcessSamples(&frontend, pcm, count, &used);
        if (!used || used > count) return false;
        pcm += used;
        count -= used;
        if (!features.size) continue;
        if (features.size != 40) return false;
        auto *input = interpreter->input(0);
        for (unsigned i = 0; i < 40; ++i) {
            // Match hey_pico_runtime.py: raw microfrontend /25.6, then
            // model-specific quantization with NumPy's round-to-nearest-even.
            float q = nearbyintf((float(features.values[i]) * 0.0390625f) /
                                input->params.scale + input->params.zero_point);
            input->data.int8[slice * 40 + i] = int8_t(std::clamp(q, -128.0f, 127.0f));
        }
        if (++slice == stride) {
            slice = 0;
            if (interpreter->Invoke() != kTfLiteOk) return false;
            auto *out = interpreter->output(0);
            callback((int(out->data.uint8[0]) - out->params.zero_point) *
                     out->params.scale, context);
        }
    }
    return true;
}
