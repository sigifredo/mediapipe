#ifndef PLEXUS_LANDMARKS_BODYTYPES_HPP
#define PLEXUS_LANDMARKS_BODYTYPES_HPP

#include "Export.hpp"

#include <cstdint>
#include <vector>

namespace plexus::landmarks
{

struct PLEXUS_LANDMARKS_API BodyLandmark
{
    float x, y, z;
    float visibility;
    float presence;
};

struct PLEXUS_LANDMARKS_API BodyTrackingResult
{
    std::vector<BodyLandmark> landmarks;      // 33, normalized image coordinates
    std::vector<BodyLandmark> worldLandmarks; // 33, real-world meters (hip-centered)
    int64_t timestampUs;
};

} // namespace plexus::landmarks

#endif
