

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
    std::vector<BodyLandmark> landmarks; // 33, coordenadas normalizadas
    int64_t timestampUs;
};

class TRACKER_API BodyTracker : public MediaPipeTracker
{
public:
    using ResultCallback = std::function<void(const BodyTrackingResult &)>;

    BodyTracker();
    ~BodyTracker() override;

    bool initialize(const MediaPipeTrackerConfig &config, const std::string &landmarkStream = "pose_landmarks");
    void setResultCallback(ResultCallback callback);

protected:
    bool registerObservers() override;

private:
    struct BodyImpl;
    std::unique_ptr<BodyImpl> bodyImpl_;
};

#endif
