#ifndef HANDTRACKER_HPP
#define HANDTRACKER_HPP

#include "MediaPipeTracker.hpp"

#include <functional>
#include <string>

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

private:
    struct HandImpl;
    std::unique_ptr<HandImpl> handImpl_;
};

#endif
