

#ifndef HAND_TRACKER_HPP
#define HAND_TRACKER_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct HandLandmark {
    float x, y, z;
    float visibility;
};

struct HandTrackingResult {
    std::vector<std::vector<HandLandmark>> hands;
    int64_t timestamp_us;
};

using ResultCallback = std::function<void(const HandTrackingResult&)>;

class HandTracker {
public:
    HandTracker();
    ~HandTracker();

    bool Initialize(const std::string& graph_path,
                    const std::string& model_dir,
                    bool use_gpu = true);

    bool ProcessFrame(const uint8_t* rgb_data,
                      int width, int height,
                      int64_t timestamp_us);

    void SetResultCallback(ResultCallback callback);
    void Stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif

