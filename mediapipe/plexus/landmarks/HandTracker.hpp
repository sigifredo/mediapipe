

#ifndef PLEXUS_LANDMARKS_HANDTRACKER_HPP
#define PLEXUS_LANDMARKS_HANDTRACKER_HPP

// own
#include <HandTypes.hpp>
#include <LandmarkTracker.hpp>
#include <Namespace.hpp>

// std
#include <functional>
#include <string>

PLEXUS_LANDMARKS_NAMESPACE_BEGIN

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

PLEXUS_LANDMARKS_NAMESPACE_END

#endif
