

#ifndef HAND_TRACKER_HPP
#define HAND_TRACKER_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct HandLandmark
{
    float x, y, z;
    float visibility;
};

struct HandClassification
{
    std::string label; // "Left" | "Right"
    float score;       // 0.0–1.0
};

struct HandTrackingResult
{
    std::vector<std::vector<HandLandmark>> hands;
    std::vector<HandClassification> handedness;
    int64_t timestamp_us;
};

using ResultCallback = std::function<void(const HandTrackingResult &)>;

struct HandTrackerConfig
{
    std::string graph_path;
    std::string input_stream = "input_video";
    std::string landmark_stream = "hand_landmarks";
    std::string handedness_stream = "handedness";
    bool use_gpu = false;
};

class HandTracker
{
public:
    HandTracker();
    ~HandTracker();

    HandTracker(const HandTracker &) = delete;
    HandTracker &operator=(const HandTracker &) = delete;

    bool Initialize(const HandTrackerConfig &config);

    bool ProcessFrame(const uint8_t *rgb_data, int width, int height, int64_t timestamp_us);

    /// Bloquea hasta que el grafo termine de procesar todos los packets en cola.
    bool WaitUntilIdle();

    /// Cierra el input stream — necesario para liberar PreviousLoopbackCalculator
    /// en modo single-image. Después de esto, no se pueden enviar más frames.
    bool CloseInputStream();

    void SetResultCallback(ResultCallback callback);
    void Stop();

    bool is_running() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
