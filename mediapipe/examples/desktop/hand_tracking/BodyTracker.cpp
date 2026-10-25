#include "BodyTracker.hpp"
#include "FacadeImpl.hpp"

struct BodyTracker::BodyImpl
{
    ResultCallback callback;
};

BodyTracker::BodyTracker() : bodyImpl_(std::make_unique<BodyImpl>()) {}

// stop() antes de destruir bodyImpl_: los hilos del grafo invocan el callback.
BodyTracker::~BodyTracker() { stop(); }

bool BodyTracker::initialize(const MediaPipeTrackerConfig &config, const std::string &landmarkStream, const std::string &worldLandmarkStream)
{
    auto dispatch = [this](const BodyTrackingResult &result)
    {
        if (bodyImpl_->callback)
            bodyImpl_->callback(result);
    };

    return impl_->start(config, [landmarkStream, worldLandmarkStream, dispatch](IBackend &backend, const MediaPipeTrackerConfig &resolved)
                        { return backend.initializeBody(resolved, landmarkStream, worldLandmarkStream, dispatch); });
}

void BodyTracker::setResultCallback(ResultCallback callback) { bodyImpl_->callback = std::move(callback); }
