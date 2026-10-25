# Copyright 2019 The MediaPipe Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

licenses(["notice"])

# BUILD:
# bazel build -c opt --define MEDIAPIPE_DISABLE_GPU=1 \
#   //mediapipe/examples/desktop/hand_tracking:hand_tracking_tflite
# RUN:
# bazel-bin/mediapipe/examples/desktop/hand_tracking/hand_tracking_tflite \
#   --calculator_graph_config_file=<.../hand_tracking_desktop.pbtxt> \
#   --input_side_packets=input_video_path=<input video path>,output_video_path=<output video path>
cc_binary(
    name = "hand_tracking_tflite",
    data = [
        "//mediapipe/modules/hand_landmark:hand_landmark_full.tflite",
        "//mediapipe/modules/palm_detection:palm_detection_full.tflite",
    ],
    deps = [
        "//mediapipe/examples/desktop:simple_run_graph_main",
        "//mediapipe/graphs/hand_tracking:desktop_tflite_calculators",
        "//mediapipe/graphs/pose_tracking:pose_tracking_cpu_deps",
    ],
)

# BUILD:
# bazel build -c opt --define MEDIAPIPE_DISABLE_GPU=1 \
#   //mediapipe/examples/desktop/hand_tracking:hand_tracking_cpu
# RUN:
# bazel-bin/mediapipe/examples/desktop/hand_tracking/hand_tracking_cpu \
#   --calculator_graph_config_file=<.../hand_tracking_desktop_live.pbtxt> \
#   --input_video_path=<input video path> \
#   --output_video_path=<output video path>
cc_binary(
    name = "hand_tracking_cpu",
    data = [
        "//mediapipe/modules/hand_landmark:hand_landmark_full.tflite",
        "//mediapipe/modules/palm_detection:palm_detection_full.tflite",
    ],
    deps = [
        "//mediapipe/examples/desktop:demo_run_graph_main",
        "//mediapipe/graphs/hand_tracking:desktop_tflite_calculators",
        "//mediapipe/graphs/pose_tracking:pose_tracking_cpu_deps",
    ],
)

# Linux only
cc_binary(
    name = "hand_tracking_gpu",
    data = [
        "//mediapipe/modules/hand_landmark:hand_landmark_full.tflite",
        "//mediapipe/modules/palm_detection:palm_detection_full.tflite",
    ],
    deps = [
        "//mediapipe/examples/desktop:demo_run_graph_main_gpu",
        "//mediapipe/graphs/hand_tracking:desktop_tflite_calculators",
        "//mediapipe/graphs/pose_tracking:pose_tracking_cpu_deps",
    ],
)

# Fachada: no depende de MediaPipe ni de EGL. Carga libhand_tracker_cpu/gpu con dlopen.
# Se construye con build_plexus.sh, que genera los tres artefactos en dist/.
cc_binary(
    name = "libhand_tracker.so",
    srcs = [
        "BackendLoader.cpp",
        "BackendLoader.hpp",
        "BodyTracker.cpp",
        "BodyTracker.hpp",
        "FacadeImpl.hpp",
        "HandTracker.cpp",
        "HandTracker.hpp",
        "MediaPipeTracker.cpp",
        "MediaPipeTracker.hpp",
        "TrackerExport.hpp",
        "TrackerTypes.hpp",
        "backend/BackendApi.hpp",
    ],
    defines = ["TRACKER_BUILDING_DLL"],
    linkopts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-ldl"],
    }),
    linkshared = True,
)

# Modelos que usan los grafos de manos y cuerpo. build_plexus.sh los copia a dist/.
filegroup(
    name = "models",
    srcs = [
        "//mediapipe/modules/hand_landmark:handedness.txt",
        "//mediapipe/modules/hand_landmark:hand_landmark_full.tflite",
        "//mediapipe/modules/hand_landmark:hand_landmark_lite.tflite",
        "//mediapipe/modules/palm_detection:palm_detection_full.tflite",
        "//mediapipe/modules/palm_detection:palm_detection_lite.tflite",
        "//mediapipe/modules/pose_detection:pose_detection.tflite",
        "//mediapipe/modules/pose_landmark:pose_landmark_full.tflite",
        "//mediapipe/modules/pose_landmark:pose_landmark_heavy.tflite",
        "//mediapipe/modules/pose_landmark:pose_landmark_lite.tflite",
    ],
)

# Backend con MediaPipe. El mismo target se construye con GPU (config gpu) y sin ella
# (config cpu); build_plexus.sh renombra las salidas a libhand_tracker_gpu/cpu.so.
cc_binary(
    name = "libhand_tracker_backend.so",
    srcs = [
        "TrackerExport.hpp",
        "TrackerTypes.hpp",
        "backend/BackendApi.hpp",
        "backend/Core.cpp",
    ],
    linkshared = True,
    deps = [
        "//mediapipe/framework:calculator_framework",
        "//mediapipe/framework/formats:classification_cc_proto",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:file_helpers",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",
        "//mediapipe/graphs/hand_tracking:desktop_tflite_calculators",
        "//mediapipe/graphs/pose_tracking:pose_tracking_cpu_deps",
        "//mediapipe/util:resource_util",
        "@com_google_absl//absl/flags:flag",
    ] + select({
        "//mediapipe/gpu:disable_gpu": [],
        "//conditions:default": [
            "//mediapipe/gpu:gpu_buffer",
            "//mediapipe/gpu:gpu_shared_data_internal",
            "//mediapipe/gpu:image_frame_to_gpu_buffer_calculator",
            "//mediapipe/graphs/pose_tracking:pose_tracking_gpu_deps",
            "//mediapipe/modules/hand_landmark:hand_landmark_tracking_gpu",
        ],
    }),
)

# Benchmark headless CPU/GPU. Requiere build con GPU (sin MEDIAPIPE_DISABLE_GPU).
# BUILD:
# bazel build -c opt --copt -DMESA_EGL_NO_X11_HEADERS --copt -DEGL_NO_X11 \
#   //mediapipe/examples/desktop/hand_tracking:tracker_bench
# RUN (desde la raíz del repositorio):
# bazel-bin/mediapipe/examples/desktop/hand_tracking/tracker_bench \
#   --backend=gpu --video=<video>
cc_binary(
    name = "tracker_bench",
    srcs = ["TrackerBench.cpp"],
    data = [
        "//mediapipe/modules/hand_landmark:hand_landmark_full.tflite",
        "//mediapipe/modules/palm_detection:palm_detection_full.tflite",
    ],
    deps = [
        "//mediapipe/calculators/core:constant_side_packet_calculator",
        "//mediapipe/framework:calculator_framework",
        "//mediapipe/framework/formats:image_frame",
        "//mediapipe/framework/formats:image_frame_opencv",
        "//mediapipe/framework/formats:landmark_cc_proto",
        "//mediapipe/framework/port:opencv_imgproc",
        "//mediapipe/framework/port:opencv_video",
        "//mediapipe/framework/port:parse_text_proto",
        "//mediapipe/framework/port:status",
        "//mediapipe/gpu:image_frame_to_gpu_buffer_calculator",
        "//mediapipe/modules/hand_landmark:hand_landmark_tracking_cpu",
        "//mediapipe/modules/hand_landmark:hand_landmark_tracking_gpu",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
    ],
)
