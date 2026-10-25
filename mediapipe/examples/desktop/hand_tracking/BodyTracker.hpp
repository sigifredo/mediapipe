#ifndef BODYTRACKER_HPP
#define BODYTRACKER_HPP

#include "MediaPipeTracker.hpp"

#include <functional>
#include <string>

class TRACKER_API BodyTracker : public MediaPipeTracker
{
public:
    using ResultCallback = std::function<void(const BodyTrackingResult &)>;

    BodyTracker();
    ~BodyTracker() override;

    bool initialize(const MediaPipeTrackerConfig &config, const std::string &landmarkStream = "pose_landmarks", const std::string &worldLandmarkStream = "pose_world_landmarks");
    void setResultCallback(ResultCallback callback);

private:
    struct BodyImpl;
    std::unique_ptr<BodyImpl> bodyImpl_;
};

#endif
