

#ifndef BODYTRACKER_HPP
#define BODYTRACKER_HPP

#include "MediaPipeTracker.hpp"

#include <functional>
#include <string>
#include <vector>

struct TRACKER_API BodyLandmark
{
    float x, y, z;
    float visibility;
    float presence;
};

struct TRACKER_API BodyTrackingResult
{
    std::vector<BodyLandmark> landmarks;      // 33, normalized image coordinates
    std::vector<BodyLandmark> worldLandmarks; // 33, real-world meters (hip-centered)
    int64_t timestampUs;
};

class TRACKER_API BodyTracker : public MediaPipeTracker
{
public:
    using ResultCallback = std::function<void(const BodyTrackingResult &)>;

    BodyTracker();
    ~BodyTracker() override;

    bool initialize(const MediaPipeTrackerConfig &config, const std::string &landmarkStream = "pose_landmarks", const std::string &worldLandmarkStream = "pose_world_landmarks");
    void setResultCallback(ResultCallback callback);

protected:
    bool registerObservers() override;

private:
    struct BodyImpl;
    std::unique_ptr<BodyImpl> bodyImpl_;
};

#endif
