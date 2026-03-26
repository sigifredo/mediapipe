

#ifndef HANDTRACKER_HPP
#define HANDTRACKER_HPP

#include "MediaPipeTracker.hpp"

#include <functional>
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
    int64_t timestampUs;
};

class HandTracker : public MediaPipeTracker
{
public:
    using ResultCallback = std::function<void(const HandTrackingResult &)>;

    HandTracker();
    ~HandTracker() override;

    bool initialize(const MediaPipeTrackerConfig &config, const std::string &landmarkStream = "hand_landmarks", const std::string &handednessStream = "handedness");
    void setResultCallback(ResultCallback callback);

protected:
    bool registerObservers() override;

private:
    struct HandImpl;
    std::unique_ptr<HandImpl> handImpl_;
};

#endif
