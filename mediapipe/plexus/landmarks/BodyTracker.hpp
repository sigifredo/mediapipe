#ifndef PLEXUS_LANDMARKS_BODYTRACKER_HPP
#define PLEXUS_LANDMARKS_BODYTRACKER_HPP

#include "BodyTypes.hpp"
#include "LandmarkTracker.hpp"

#include <functional>
#include <string>

namespace plexus::landmarks
{

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

} // namespace plexus::landmarks

#endif
