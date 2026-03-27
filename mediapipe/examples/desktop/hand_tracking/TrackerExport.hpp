

#ifndef TRACKER_EXPORT_H
#define TRACKER_EXPORT_H

#ifdef _WIN32
  #ifdef TRACKER_BUILDING_DLL
    #define TRACKER_API __declspec(dllexport)
  #else
    #define TRACKER_API __declspec(dllimport)
  #endif
#else
  #define TRACKER_API __attribute__((visibility("default")))
#endif

#endif
