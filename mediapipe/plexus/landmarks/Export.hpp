#ifndef PLEXUS_LANDMARKS_EXPORT_HPP
#define PLEXUS_LANDMARKS_EXPORT_HPP

#ifdef _WIN32
  #ifdef PLEXUS_LANDMARKS_BUILDING_DLL
    #define PLEXUS_LANDMARKS_API __declspec(dllexport)
  #else
    #define PLEXUS_LANDMARKS_API __declspec(dllimport)
  #endif
#else
  #define PLEXUS_LANDMARKS_API __attribute__((visibility("default")))
#endif

#endif
