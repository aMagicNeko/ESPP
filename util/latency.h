#pragma once
#include "util/common.h"
class LatencyWrapper {
public:
    LatencyWrapper(bvar::LatencyRecorder& recorder) : _recorder(recorder) {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        _time_stamp = now_ms;
    }
    ~LatencyWrapper() {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        uint64_t time_stamp = now_ms;
        _recorder << time_stamp - _time_stamp;
    }
private:
    bvar::LatencyRecorder& _recorder;
    uint64_t _time_stamp;
};