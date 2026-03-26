

#include "HandTracker.hpp"
#include "MediaPipeTrackerImpl.hpp"

#include "mediapipe/framework/formats/classification.pb.h"
#include "mediapipe/framework/formats/landmark.pb.h"

#include <map>
#include <mutex>

struct PartialResult
{
    std::vector<std::vector<HandLandmark>> hands;
    std::vector<HandClassification> handedness;
    bool hasLandmarks = false;
    bool hasHandedness = false;

    bool isComplete() const { return hasLandmarks && hasHandedness; }
};

struct HandTracker::HandImpl
{
    ResultCallback callback;
    std::string landmarkStream;
    std::string handednessStream;

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
            callback(result);
        }

        pending.erase(it);

        while (pending.size() > 8)
            pending.erase(pending.begin());
    }
};

HandTracker::HandTracker() : handImpl_(std::make_unique<HandImpl>()) {}

HandTracker::~HandTracker() = default;

bool HandTracker::initialize(const MediaPipeTrackerConfig &config, const std::string &landmarkStream, const std::string &handednessStream)
{
    handImpl_->landmarkStream = landmarkStream;
    handImpl_->handednessStream = handednessStream;

    return MediaPipeTracker::initialize(config);
}

void HandTracker::setResultCallback(ResultCallback cb)
{
    handImpl_->callback = std::move(cb);
}

bool HandTracker::registerObservers()
{
    auto &g = impl_->graph;

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

    return true;
}
