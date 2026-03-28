

#include "HandTracker.hpp"
#include "MediaPipeTrackerImpl.hpp"

#include "mediapipe/framework/formats/classification.pb.h"
#include "mediapipe/framework/formats/landmark.pb.h"

#include <map>
#include <mutex>

struct PartialResult
{
    std::vector<std::vector<HandLandmark>> hands;
    std::vector<std::vector<WorldLandmark>> worldLandmarks;
    std::vector<HandClassification> handedness;
    bool hasLandmarks = false;
    bool hasHandedness = false;
    bool hasWorldLandmarks = false;
    bool wantsWorldLandmarks = false;

    bool isComplete() const
    {
        return hasLandmarks && hasHandedness &&
               (!wantsWorldLandmarks || hasWorldLandmarks);
    }
};

struct HandTracker::HandImpl
{
    ResultCallback callback;
    std::string landmarkStream;
    std::string handednessStream;
    std::string worldLandmarkStream;
    bool wantsWorld = false;

    std::mutex pendingMutex;
    std::map<int64_t, PartialResult> pending;

    void tryEmit(int64_t timestampUs)
    {
        auto it = pending.find(timestampUs);

        if (it == pending.end() || !it->second.isComplete())
            return;

        if (callback)
        {
            HandTrackingResult result;
            result.timestampUs = timestampUs;
            result.hands = std::move(it->second.hands);
            result.handedness = std::move(it->second.handedness);
            result.worldLandmarks = std::move(it->second.worldLandmarks);
            callback(result);
        }

        pending.erase(it);

        while (pending.size() > 8)
            pending.erase(pending.begin());
    }
};

HandTracker::HandTracker() : handImpl_(std::make_unique<HandImpl>()) {}

HandTracker::~HandTracker() = default;

bool HandTracker::initialize(const MediaPipeTrackerConfig &config,
                             const std::string &landmarkStream,
                             const std::string &handednessStream,
                             const std::string &worldLandmarkStream)
{
    handImpl_->landmarkStream = landmarkStream;
    handImpl_->handednessStream = handednessStream;
    handImpl_->worldLandmarkStream = worldLandmarkStream;
    handImpl_->wantsWorld = !worldLandmarkStream.empty();

    return MediaPipeTracker::initialize(config);
}

void HandTracker::setResultCallback(ResultCallback cb)
{
    handImpl_->callback = std::move(cb);
}

bool HandTracker::registerObservers()
{
    auto &g = impl_->graph;

    // --- Observer 1: normalized landmarks ---

    auto lmStatus = g.ObserveOutputStream(
        handImpl_->landmarkStream,
        [this](const mediapipe::Packet &packet) -> absl::Status
        {
            const auto &multiHand = packet.Get<std::vector<mediapipe::NormalizedLandmarkList>>();
            int64_t ts = packet.Timestamp().Microseconds();

            std::vector<std::vector<HandLandmark>> hands;
            hands.reserve(multiHand.size());

            for (const auto &hand : multiHand)
            {
                std::vector<HandLandmark> landmarks;
                landmarks.reserve(hand.landmark_size());

                for (const auto &lm : hand.landmark())
                {
                    landmarks.push_back({lm.x(), lm.y(), lm.z(), lm.has_visibility() ? lm.visibility() : 0.0f});
                }

                hands.push_back(std::move(landmarks));
            }

            {
                std::lock_guard<std::mutex> lock(handImpl_->pendingMutex);
                auto &partial = handImpl_->pending[ts];
                partial.wantsWorldLandmarks = handImpl_->wantsWorld;
                partial.hands = std::move(hands);
                partial.hasLandmarks = true;
                handImpl_->tryEmit(ts);
            }

            return absl::OkStatus();
        });

    if (!lmStatus.ok())
    {
        fprintf(stderr, "ERROR ObserveOutputStream(%s): %s\n", handImpl_->landmarkStream.c_str(), lmStatus.ToString().c_str());
        return false;
    }

    // --- Observer 2: handedness ---

    auto hdStatus = g.ObserveOutputStream(
        handImpl_->handednessStream,
        [this](const mediapipe::Packet &packet) -> absl::Status
        {
            const auto &multiHandedness =
                packet.Get<std::vector<mediapipe::ClassificationList>>();
            int64_t ts = packet.Timestamp().Microseconds();

            std::vector<HandClassification> handedness;
            handedness.reserve(multiHandedness.size());

            for (const auto &clList : multiHandedness)
            {
                if (clList.classification_size() > 0)
                {
                    const auto &cl = clList.classification(0);
                    handedness.push_back({cl.label(), cl.score()});
                }
                else
                {
                    handedness.push_back({"Unknown", 0.0f});
                }
            }

            {
                std::lock_guard<std::mutex> lock(handImpl_->pendingMutex);
                auto &partial = handImpl_->pending[ts];
                partial.wantsWorldLandmarks = handImpl_->wantsWorld;
                partial.handedness = std::move(handedness);
                partial.hasHandedness = true;
                handImpl_->tryEmit(ts);
            }

            return absl::OkStatus();
        });

    if (!hdStatus.ok())
    {
        fprintf(stderr, "ERROR ObserveOutputStream(%s): %s\n", handImpl_->handednessStream.c_str(), hdStatus.ToString().c_str());
        return false;
    }

    // --- Observer 3: world landmarks (opcional) ---

    if (!handImpl_->wantsWorld)
        return true;

    auto wlStatus = g.ObserveOutputStream(
        handImpl_->worldLandmarkStream,
        [this](const mediapipe::Packet &packet) -> absl::Status
        {
            const auto &multiWorld = packet.Get<std::vector<mediapipe::LandmarkList>>();
            int64_t ts = packet.Timestamp().Microseconds();

            std::vector<std::vector<WorldLandmark>> worldHands;
            worldHands.reserve(multiWorld.size());

            for (const auto &hand : multiWorld)
            {
                std::vector<WorldLandmark> landmarks;
                landmarks.reserve(hand.landmark_size());

                for (const auto &lm : hand.landmark())
                {
                    landmarks.push_back({lm.x(), lm.y(), lm.z()});
                }

                worldHands.push_back(std::move(landmarks));
            }

            {
                std::lock_guard<std::mutex> lock(handImpl_->pendingMutex);
                auto &partial = handImpl_->pending[ts];
                partial.wantsWorldLandmarks = handImpl_->wantsWorld;
                partial.worldLandmarks = std::move(worldHands);
                partial.hasWorldLandmarks = true;
                handImpl_->tryEmit(ts);
            }

            return absl::OkStatus();
        });

    if (!wlStatus.ok())
    {
        fprintf(stderr, "ERROR ObserveOutputStream(%s): %s\n", handImpl_->worldLandmarkStream.c_str(), wlStatus.ToString().c_str());
        return false;
    }

    return true;
}
