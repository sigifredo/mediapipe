

#include "BodyTracker.hpp"
#include "MediaPipeTrackerImpl.hpp"

#include "mediapipe/framework/formats/landmark.pb.h"

#include <map>
#include <mutex>

struct PartialBodyResult
{
    std::vector<BodyLandmark> normalized;
    std::vector<BodyLandmark> world;
    bool hasNormalized = false;
    bool hasWorld = false;

    bool isComplete() const { return hasNormalized && hasWorld; }
};

struct BodyTracker::BodyImpl
{
    ResultCallback callback;
    std::string landmarkStream;
    std::string worldLandmarkStream;

    std::mutex pendingMutex;
    std::map<int64_t, PartialBodyResult> pending;

    void tryEmit(int64_t timestampUs)
    {
        auto it = pending.find(timestampUs);

        if (it == pending.end() || !it->second.isComplete())
            return;

        if (callback)
        {
            BodyTrackingResult result;
            result.timestampUs = timestampUs;
            result.landmarks = std::move(it->second.normalized);
            result.worldLandmarks = std::move(it->second.world);
            callback(result);
        }

        pending.erase(it);

        while (pending.size() > 8)
            pending.erase(pending.begin());
    }
};

BodyTracker::BodyTracker() : bodyImpl_(std::make_unique<BodyImpl>()) {}

BodyTracker::~BodyTracker() = default;

bool BodyTracker::initialize(const MediaPipeTrackerConfig &config, const std::string &landmarkStream, const std::string &worldLandmarkStream)
{
    bodyImpl_->landmarkStream = landmarkStream;
    bodyImpl_->worldLandmarkStream = worldLandmarkStream;

    return MediaPipeTracker::initialize(config);
}

void BodyTracker::setResultCallback(ResultCallback cb)
{
    bodyImpl_->callback = std::move(cb);
}

bool BodyTracker::registerObservers()
{
    auto &g = impl_->graph;

    // --- Normalized landmarks ---

    auto nlStatus = g.ObserveOutputStream(
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

            {
                std::lock_guard<std::mutex> lock(bodyImpl_->pendingMutex);
                auto &partial = bodyImpl_->pending[ts];
                partial.normalized = std::move(landmarks);
                partial.hasNormalized = true;
                bodyImpl_->tryEmit(ts);
            }

            return absl::OkStatus();
        });

    if (!nlStatus.ok())
    {
        fprintf(stderr, "ERROR ObserveOutputStream(%s): %s\n", bodyImpl_->landmarkStream.c_str(), nlStatus.ToString().c_str());
        return false;
    }

    // --- World landmarks ---

    auto wlStatus = g.ObserveOutputStream(
        bodyImpl_->worldLandmarkStream,
        [this](const mediapipe::Packet &packet) -> absl::Status
        {
            const auto &pose = packet.Get<mediapipe::LandmarkList>();
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

            {
                std::lock_guard<std::mutex> lock(bodyImpl_->pendingMutex);
                auto &partial = bodyImpl_->pending[ts];
                partial.world = std::move(landmarks);
                partial.hasWorld = true;
                bodyImpl_->tryEmit(ts);
            }

            return absl::OkStatus();
        });

    if (!wlStatus.ok())
    {
        fprintf(stderr, "ERROR ObserveOutputStream(%s): %s\n", bodyImpl_->worldLandmarkStream.c_str(), wlStatus.ToString().c_str());
        return false;
    }

    return true;
}
