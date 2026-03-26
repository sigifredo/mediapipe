

#ifndef MEDIAPIPE_TRACKER_IMPL_HPP
#define MEDIAPIPE_TRACKER_IMPL_HPP

#include "mediapipe_tracker.h"

#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/gpu/gl_calculator_helper.h"
#include "mediapipe/gpu/gpu_buffer.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"

struct MediaPipeTracker::Impl
{
    mediapipe::CalculatorGraph graph;
    mediapipe::GlCalculatorHelper gpuHelper;
    MediaPipeTrackerConfig config;
    bool running = false;
    bool inputClosed = false;
};

#endif
