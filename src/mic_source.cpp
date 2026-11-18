#include "classcaption/mic_source.h"

#include "classcaption/ring_buffer.h"

#include <miniaudio.h>
#include <span>
#include <stdexcept>

using namespace std;

namespace classcaption {

struct MicSource::Device {
    // ma_device_init fills this in; zeroing it first only gives it a defined value.
    ma_device device{}; // NOLINT(bugprone-invalid-enum-default-initialization)
    bool initialized = false;
};

namespace {

// Runs on the OS audio thread: it may only copy, never lock, allocate or wait.
void on_audio(ma_device* device, void* /*output*/, const void* input, ma_uint32 frame_count) {
    auto* const buffer = static_cast<RingBuffer*>(device->pUserData);
    buffer->push(span(static_cast<const float*>(input), frame_count));
}

} // namespace

MicSource::MicSource() : device_(make_unique<Device>()) {}

MicSource::~MicSource() {
    stop();
}

void MicSource::start(RingBuffer& out) {
    stop();
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = kSampleRate;
    config.dataCallback = on_audio;
    config.pUserData = &out;

    if (ma_device_init(nullptr, &config, &device_->device) != MA_SUCCESS) {
        throw runtime_error("could not open the microphone (check System Settings > Privacy)");
    }
    device_->initialized = true;
    if (ma_device_start(&device_->device) != MA_SUCCESS) {
        stop();
        throw runtime_error("could not start recording from the microphone");
    }
}

void MicSource::stop() {
    if (device_->initialized) {
        ma_device_uninit(&device_->device);
        device_->initialized = false;
    }
}

} // namespace classcaption
