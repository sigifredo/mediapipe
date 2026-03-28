

#ifndef HANDTRACKER_HPP
#define HANDTRACKER_HPP

#include "MediaPipeTracker.hpp"

#include <functional>
#include <string>
#include <vector>

struct TRACKER_API HandLandmark
{
    float x, y, z;
    float visibility;
};

struct TRACKER_API WorldLandmark
{
    float x, y, z; // metros, origen en centro geométrico de la mano
};

struct TRACKER_API HandClassification
{
    std::string label; // "Left" | "Right"
    float score;       // 0.0–1.0
};

struct TRACKER_API HandTrackingResult
{
    std::vector<std::vector<HandLandmark>> hands;
    std::vector<std::vector<WorldLandmark>> worldLandmarks; // vacío si no se pidió
    std::vector<HandClassification> handedness;
    int64_t timestampUs;
};

class TRACKER_API HandTracker : public MediaPipeTracker
{
public:
    using ResultCallback = std::function<void(const HandTrackingResult &)>;

    HandTracker();
    ~HandTracker() override;

    bool initialize(const MediaPipeTrackerConfig &config,
                    const std::string &landmarkStream = "hand_landmarks",
                    const std::string &handednessStream = "handedness",
                    const std::string &worldLandmarkStream = "");
    void setResultCallback(ResultCallback callback);

protected:
    bool registerObservers() override;

private:
    struct HandImpl;
    std::unique_ptr<HandImpl> handImpl_;
};

#endif
