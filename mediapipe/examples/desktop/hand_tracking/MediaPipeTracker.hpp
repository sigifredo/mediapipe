

#ifndef MEDIAPIPETRACKER_HPP
#define MEDIAPIPETRACKER_HPP

#include "TrackerExport.hpp"

#include <cstdint>
#include <memory>
#include <string>

struct TRACKER_API MediaPipeTrackerConfig
{
    std::string graphPath;
    std::string inputStream = "input_video";
    bool useGPU = false;
};

class TRACKER_API MediaPipeTracker
{
public:
    virtual ~MediaPipeTracker();

    MediaPipeTracker(const MediaPipeTracker &) = delete;
    MediaPipeTracker &operator=(const MediaPipeTracker &) = delete;

    bool closeInputStream();
    bool initialize(const MediaPipeTrackerConfig &config);
    bool isRunning() const;
    bool processFrame(const uint8_t *rgbData, int width, int height, int64_t timestampUs);
    void stop();
    bool waitUntilIdle();

protected:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    MediaPipeTracker();

    /// Llamado por initialize() después de graph.Initialize() y antes de
    /// graph.StartRun(). La subclase registra aquí sus ObserveOutputStream.
    /// Recibe referencia al Impl para acceder al graph.
    virtual bool registerObservers() = 0;
};

#endif
