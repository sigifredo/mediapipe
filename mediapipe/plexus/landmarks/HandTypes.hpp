

#ifndef PLEXUS_LANDMARKS_HANDTYPES_HPP
#define PLEXUS_LANDMARKS_HANDTYPES_HPP

// own
#include <Export.hpp>
#include <Namespace.hpp>

// std
#include <cstdint>
#include <string>
#include <vector>

PLEXUS_LANDMARKS_NAMESPACE_BEGIN

struct PLEXUS_LANDMARKS_API HandLandmark
{
    float x, y, z;
    float visibility;
};

struct PLEXUS_LANDMARKS_API HandWorldLandmark
{
    float x, y, z; // metros, origen en centro geométrico de la mano
};

struct PLEXUS_LANDMARKS_API HandClassification
{
    std::string label; // "Left" | "Right"
    float score;       // 0.0–1.0
};

struct PLEXUS_LANDMARKS_API HandTrackingResult
{
    std::vector<std::vector<HandLandmark>> hands;
    std::vector<std::vector<HandWorldLandmark>> worldLandmarks; // vacío si no se pidió
    std::vector<HandClassification> handedness;
    int64_t timestampUs;
};

PLEXUS_LANDMARKS_NAMESPACE_END

#endif
