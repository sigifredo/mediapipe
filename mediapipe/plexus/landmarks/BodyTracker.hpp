

#ifndef PLEXUS_LANDMARKS_BODYTRACKER_HPP
#define PLEXUS_LANDMARKS_BODYTRACKER_HPP

// own
#include <BodyTypes.hpp>
#include <LandmarkTracker.hpp>
#include <Namespace.hpp>

// std
#include <functional>
#include <string>

PLEXUS_LANDMARKS_NAMESPACE_BEGIN

class PLEXUS_LANDMARKS_API BodyTracker : public LandmarkTracker
{
public:
    using ResultCallback = std::function<void(const BodyTrackingResult &)>;

    BodyTracker();
    ~BodyTracker() override;

    bool initialize(const TrackerConfig &config, const std::string &landmarkStream = "pose_landmarks", const std::string &worldLandmarkStream = "pose_world_landmarks");
    void setResultCallback(ResultCallback callback);

private:
    struct BodyImpl;
    std::unique_ptr<BodyImpl> bodyImpl_;
};

PLEXUS_LANDMARKS_NAMESPACE_END

#endif
