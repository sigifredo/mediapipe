

#include "BodyTracker.hpp"
#include "MediaPipeTrackerImpl.hpp"

#include "mediapipe/framework/formats/landmark.pb.h"

struct BodyTracker::BodyImpl
{
    ResultCallback callback;
    std::string landmarkStream;
};

BodyTracker::BodyTracker() : bodyImpl_(std::make_unique<BodyImpl>()) {}

BodyTracker::~BodyTracker() = default;

bool BodyTracker::initialize(const MediaPipeTrackerConfig &config, const std::string &landmarkStream)
{
    bodyImpl_->landmarkStream = landmarkStream;

    return MediaPipeTracker::initialize(config);
}

void BodyTracker::setResultCallback(ResultCallback cb)
{
    bodyImpl_->callback = std::move(cb);
}

bool BodyTracker::registerObservers()
{
    auto &g = impl_->graph;

    auto status = g.ObserveOutputStream(
        bodyImpl_->landmarkStream,
        [this](const mediapipe::Packet &packet) -> absl::Status
        {
            const auto &pose = packet.Get<mediapipe::NormalizedLandmarkList>();
            int64_t ts = packet.Timestamp().Microseconds();

            std::vector<BodyLandmark> landmarks;
            landmarks.reserve(pose.landmark_size());

            for (const auto &lm : pose.landmark())
            {
                landmarks.push_back({lm.x(),
                                     lm.y(),
                                     lm.z(),
                                     lm.has_visibility() ? lm.visibility() : 0.0f,
                                     lm.has_presence() ? lm.presence() : 0.0f});
            }

            if (bodyImpl_->callback)
            {
                BodyTrackingResult result;
                result.timestampUs = ts;
                result.landmarks = std::move(landmarks);
                bodyImpl_->callback(result);
            }

            return absl::OkStatus();
        });

    if (!status.ok())
    {
        fprintf(stderr, "ERROR ObserveOutputStream(%s): %s\n", bodyImpl_->landmarkStream.c_str(), status.ToString().c_str());
        return false;
    }

    return true;
}
