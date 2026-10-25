#ifndef PLEXUS_LANDMARKS_HANDTRACKER_HPP
#define PLEXUS_LANDMARKS_HANDTRACKER_HPP

#include "HandTypes.hpp"
#include "LandmarkTracker.hpp"

#include <functional>
#include <string>

namespace plexus::landmarks
{

class PLEXUS_LANDMARKS_API HandTracker : public LandmarkTracker
{
public:
    using ResultCallback = std::function<void(const HandTrackingResult &)>;

    HandTracker();
    ~HandTracker() override;

    bool initialize(const TrackerConfig &config,
                    const std::string &landmarkStream = "hand_landmarks",
                    const std::string &handednessStream = "handedness",
                    const std::string &worldLandmarkStream = "");
    void setResultCallback(ResultCallback callback);

private:
    struct HandImpl;
    std::unique_ptr<HandImpl> handImpl_;
};

} // namespace plexus::landmarks

#endif
