

#ifndef MEDIAPIPETRACKER_IMPL_HPP
#define MEDIAPIPETRACKER_IMPL_HPP

#include "MediaPipeTracker.hpp"

#include "mediapipe/framework/calculator_framework.h"
#if !MEDIAPIPE_DISABLE_GPU
#include "mediapipe/gpu/gl_calculator_helper.h"
#include "mediapipe/gpu/gpu_buffer.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"
#endif

struct MediaPipeTracker::Impl
{
    mediapipe::CalculatorGraph graph;
#if !MEDIAPIPE_DISABLE_GPU
    mediapipe::GlCalculatorHelper gpuHelper;
#endif
    MediaPipeTrackerConfig config;
    bool running = false;
    bool inputClosed = false;
};

#endif
