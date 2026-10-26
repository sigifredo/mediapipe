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

# Fachada: no depende de MediaPipe ni de EGL. Carga libplexus_landmarks_cpu/gpu con dlopen.
# Se construye con build_plexus.sh, que genera los tres artefactos en dist/.
cc_binary(
    name = "libplexus_landmarks.so",
    srcs = [
        "BodyTracker.cpp",
        "BodyTracker.hpp",
        "BodyTypes.hpp",
        "Config.hpp",
        "EngineLoader.cpp",
        "EngineLoader.hpp",
        "Export.hpp",
        "FacadeImpl.hpp",
        "HandTracker.cpp",
        "HandTracker.hpp",
        "HandTypes.hpp",
        "LandmarkTracker.cpp",
        "LandmarkTracker.hpp",
        "Namespace.hpp",
        "LandmarksVersion.cpp",
        "LandmarksVersion.hpp",
        "backend/EngineApi.hpp",
    ] + glob(["VersionInfo.generated.hpp"]),
    defines = ["PLEXUS_LANDMARKS_BUILDING_DLL"],
    includes = ["."],
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

# Motor con MediaPipe. El mismo target se construye con GPU (config gpu) y sin ella
# (config cpu); build_plexus.sh renombra las salidas a libplexus_landmarks_gpu/cpu.so.
cc_binary(
    name = "libplexus_landmarks_engine.so",
    srcs = [
        "BodyTypes.hpp",
        "Config.hpp",
        "Export.hpp",
        "HandTypes.hpp",
        "Namespace.hpp",
        "LandmarksVersion.cpp",
        "LandmarksVersion.hpp",
        "backend/EngineApi.hpp",
        "backend/MediaPipeEngine.cpp",
    ] + glob(["VersionInfo.generated.hpp"]),
    additional_linker_inputs = select({
        "@platforms//os:macos": ["engine_exports_macos.lds"],
        "//conditions:default": [],
    }),
    # Windows: los tipos de resultado (HandTrackingResult...) son PLEXUS_LANDMARKS_API y sus miembros
    # implícitos se instancian en cada módulo. Sin esto el motor los importa (dllimport) de la
    # fachada, que no los exporta hacia él, y el enlace falla.
    defines = select({
        "@platforms//os:windows": ["PLEXUS_LANDMARKS_BUILDING_DLL"],
        "//conditions:default": [],
    }),
    includes = ["."],
    # macOS: sin esto los símbolos débiles de protobuf/abseil del motor se fusionan con los de
    # Homebrew (que llegan por OpenCV/ffmpeg) y, al diferir las versiones, el proceso se corrompe.
    linkopts = select({
        "@platforms//os:macos": ["-Wl,-exported_symbols_list,$(location engine_exports_macos.lds)"],
        "//conditions:default": [],
    }),
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
#   //mediapipe/plexus/landmarks:landmarks_bench
# RUN (desde la raíz del repositorio):
# bazel-bin/mediapipe/examples/desktop/plexus/landmarks/landmarks_bench \
#   --device=gpu --video=<video>
cc_binary(
    name = "landmarks_bench",
    srcs = ["LandmarksBench.cpp"],
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
