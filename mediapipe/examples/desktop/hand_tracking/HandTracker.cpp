#include "HandTracker.hpp"
#include "FacadeImpl.hpp"

struct HandTracker::HandImpl
{
    ResultCallback callback;
};

HandTracker::HandTracker() : handImpl_(std::make_unique<HandImpl>()) {}

// stop() antes de destruir handImpl_: los hilos del grafo invocan el callback.
HandTracker::~HandTracker() { stop(); }

bool HandTracker::initialize(const MediaPipeTrackerConfig &config, const std::string &landmarkStream, const std::string &handednessStream, const std::string &worldLandmarkStream)
{
    // La devolución de llamada se lee en cada resultado, de modo que setResultCallback sigue funcionando tras iniciar.
    auto dispatch = [this](const HandTrackingResult &result)
    {
        if (handImpl_->callback)
            handImpl_->callback(result);
    };

    return impl_->start(config, [landmarkStream, handednessStream, worldLandmarkStream, dispatch](IBackend &backend, const MediaPipeTrackerConfig &resolved)
                        { return backend.initializeHand(resolved, landmarkStream, handednessStream, worldLandmarkStream, dispatch); });
}

void HandTracker::setResultCallback(ResultCallback callback) { handImpl_->callback = std::move(callback); }
