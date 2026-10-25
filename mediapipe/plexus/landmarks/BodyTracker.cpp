#include "BodyTracker.hpp"
#include "FacadeImpl.hpp"

namespace plexus::landmarks
{

struct BodyTracker::BodyImpl
{
    ResultCallback callback;
};

BodyTracker::BodyTracker() : bodyImpl_(std::make_unique<BodyImpl>()) {}

// stop() antes de destruir bodyImpl_: los hilos del grafo invocan el callback.
BodyTracker::~BodyTracker() { stop(); }

bool BodyTracker::initialize(const TrackerConfig &config, const std::string &landmarkStream, const std::string &worldLandmarkStream)
{
    auto dispatch = [this](const BodyTrackingResult &result)
    {
        if (bodyImpl_->callback)
            bodyImpl_->callback(result);
    };

    return impl_->start(config, [landmarkStream, worldLandmarkStream, dispatch](IEngine &engine, const TrackerConfig &resolved)
                        { return engine.initializeBody(resolved, landmarkStream, worldLandmarkStream, dispatch); });
}

void BodyTracker::setResultCallback(ResultCallback callback) { bodyImpl_->callback = std::move(callback); }

} // namespace plexus::landmarks
