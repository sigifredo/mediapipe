

/**
 * @file MediaPipeEngine.cpp
 * @brief Implementación de IEngine sobre CalculatorGraph de MediaPipe.
 *
 * Se compila dos veces con el mismo código: sin MEDIAPIPE_DISABLE_GPU
 * (libplexus_landmarks_gpu) y con él (libplexus_landmarks_cpu). Ver build_plexus.sh.
 */

// own
#include <LandmarksVersion.hpp>
#include <backend/EngineApi.hpp>

// mediapipe
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/classification.pb.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"
#if !MEDIAPIPE_DISABLE_GPU
#include "mediapipe/gpu/gpu_shared_data_internal.h"
#endif

// std
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

// system
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

ABSL_DECLARE_FLAG(std::string, resource_root_dir);

PLEXUS_LANDMARKS_NAMESPACE_BEGIN

namespace
{

    /// Sufijo del stream que recibe el ImageFrame cuando el grafo espera GpuBuffer.
    const char *const kFrameSuffix = "_frame";

    /**
     * @brief Resultado parcial de manos, a la espera de que lleguen todos los streams del timestamp.
     */
    struct PartialHandResult
    {
        std::vector<HandClassification> handedness;
        std::vector<std::vector<HandLandmark>> hands;
        bool hasHandedness = false;
        bool hasLandmarks = false;
        bool hasWorldLandmarks = false;
        bool wantsWorldLandmarks = false;
        std::vector<std::vector<HandWorldLandmark>> worldLandmarks;

        bool isComplete() const { return hasLandmarks && hasHandedness && (!wantsWorldLandmarks || hasWorldLandmarks); }
    };

    /**
     * @brief Resultado parcial de cuerpo.
     */
    struct PartialBodyResult
    {
        bool hasNormalized = false;
        bool hasWorld = false;
        std::vector<BodyLandmark> normalized;
        std::vector<BodyLandmark> world;

        bool isComplete() const { return hasNormalized && hasWorld; }
    };

    template <typename List>
    std::vector<BodyLandmark> ToBodyLandmarks(const List &list)
    {
        std::vector<BodyLandmark> landmarks;
        landmarks.reserve(list.landmark_size());

        for (const auto &lm : list.landmark())
            landmarks.push_back({lm.x(), lm.y(), lm.z(), lm.has_visibility() ? lm.visibility() : 0.0f, lm.has_presence() ? lm.presence() : 0.0f});

        return landmarks;
    }

    /// Directorio de esta biblioteca de motor.
    std::string LibraryDirectory()
    {
#ifdef _WIN32
        HMODULE module = nullptr;
        char path[MAX_PATH] = {0};

        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(&LibraryDirectory), &module) || !GetModuleFileNameA(module, path, MAX_PATH))
            return "";

        std::string full(path);
        const auto slash = full.find_last_of("\\/");

        return slash == std::string::npos ? "" : full.substr(0, slash);
#else
        Dl_info info;

        if (!dladdr(reinterpret_cast<void *>(&LibraryDirectory), &info) || !info.dli_fname)
            return "";

        std::string full(info.dli_fname);
        const auto slash = full.find_last_of('/');

        return slash == std::string::npos ? "" : full.substr(0, slash);
#endif
    }

    /**
     * @brief Fija desde dónde se resuelven los modelos (`mediapipe/modules/...`).
     *
     * Prioridad: PLEXUS_LANDMARKS_RESOURCE_DIR; el flag resource_root_dir si ya tiene
     * valor; el directorio de la biblioteca (donde build_plexus.sh deja los
     * modelos). Si nada aplica, los modelos se buscan desde el directorio de trabajo.
     */
    void ConfigureResourceRoot()
    {
        if (const char *env = std::getenv("PLEXUS_LANDMARKS_RESOURCE_DIR"))
        {
            absl::SetFlag(&FLAGS_resource_root_dir, std::string(env));
            return;
        }

        if (!absl::GetFlag(FLAGS_resource_root_dir).empty())
            return;

        const std::string dir = LibraryDirectory();

        if (!dir.empty() && std::ifstream(dir + "/mediapipe/modules/palm_detection/palm_detection_full.tflite").good())
            absl::SetFlag(&FLAGS_resource_root_dir, dir);
    }

} // namespace

/**
 * @brief Motor sobre MediaPipe. Una instancia gestiona un grafo.
 */
class MediaPipeEngine : public IEngine
{
public:
    MediaPipeEngine() = default;
    ~MediaPipeEngine() override { stop(); }

    bool closeInputStream() override
    {
        if (!running_ || inputClosed_)
            return false;

        auto status = graph_.CloseInputStream(feedStream_);

        if (!status.ok())
        {
            fprintf(stderr, "ERROR CloseInputStream: %s\n", status.ToString().c_str());
            return false;
        }

        inputClosed_ = true;
        return true;
    }

    bool hasFailed() const override { return graph_.HasError() || injectedFault_; }

    bool initializeBody(const TrackerConfig &config, const std::string &landmarkStream, const std::string &worldLandmarkStream, BodyCallback callback) override
    {
        bodyCallback_ = std::move(callback);

        return start(config, [&]()
                     { return observeBody(landmarkStream, worldLandmarkStream); });
    }

    bool initializeHand(const TrackerConfig &config, const std::string &landmarkStream, const std::string &handednessStream, const std::string &worldLandmarkStream, HandCallback callback) override
    {
        handCallback_ = std::move(callback);
        wantsWorld_ = !worldLandmarkStream.empty();

        return start(config, [&]()
                     { return observeHand(landmarkStream, handednessStream, worldLandmarkStream); });
    }

    bool isRunning() const override { return running_; }

    bool processFrame(const uint8_t *rgbData, int width, int height, int64_t timestampUs) override
    {
        if (!running_ || inputClosed_)
            return false;

#if !MEDIAPIPE_DISABLE_GPU
        // Inyección de fallos para probar el fallback a mitad de ejecución (test_landmarks.sh).
        if (faultAfterFrames_ >= 0 && ++framesSeen_ > faultAfterFrames_)
        {
            injectedFault_ = true;
            return false;
        }
#endif

        auto frame = std::make_unique<mediapipe::ImageFrame>(mediapipe::ImageFormat::SRGB, width, height, mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);

        frame->CopyPixelData(mediapipe::ImageFormat::SRGB, width, height, rgbData, mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);

        auto status = graph_.AddPacketToInputStream(feedStream_, mediapipe::Adopt(frame.release()).At(mediapipe::Timestamp(timestampUs)));

        if (!status.ok())
        {
            fprintf(stderr, "ERROR ProcessFrame: %s\n", status.ToString().c_str());
            return false;
        }

        return true;
    }

    void stop() override
    {
        if (!running_)
            return;

        if (!inputClosed_)
            graph_.CloseAllInputStreams().IgnoreError();

        graph_.WaitUntilDone().IgnoreError();
        running_ = false;
        inputClosed_ = true;
    }

    bool waitUntilIdle() override
    {
        if (!running_)
            return false;

        auto status = graph_.WaitUntilIdle();

        if (!status.ok())
        {
            fprintf(stderr, "ERROR WaitUntilIdle: %s\n", status.ToString().c_str());
            return false;
        }

        return true;
    }

private:
    BodyCallback bodyCallback_;
    std::string feedStream_;
    int faultAfterFrames_ = -1;
    int framesSeen_ = 0;
    mediapipe::CalculatorGraph graph_;
    HandCallback handCallback_;
    bool injectedFault_ = false;
    bool inputClosed_ = false;
    std::string inputStream_;
    std::mutex pendingMutex_;
    std::map<int64_t, PartialBodyResult> pendingBody_;
    std::map<int64_t, PartialHandResult> pendingHand_;
    bool running_ = false;
    bool wantsWorld_ = false;

    /**
     * @brief Registra los observers de cuerpo.
     */
    bool observeBody(const std::string &landmarkStream, const std::string &worldLandmarkStream)
    {
        auto status = graph_.ObserveOutputStream(landmarkStream, [this](const mediapipe::Packet &packet) -> absl::Status
                                                 {
            const int64_t ts = packet.Timestamp().Microseconds();
            auto landmarks = ToBodyLandmarks(packet.Get<mediapipe::NormalizedLandmarkList>());

            std::lock_guard<std::mutex> lock(pendingMutex_);
            auto &partial = pendingBody_[ts];

            partial.normalized = std::move(landmarks);
            partial.hasNormalized = true;
            emitBody(ts);

            return absl::OkStatus(); });

        if (!status.ok())
            return reportObserverError(landmarkStream, status);

        status = graph_.ObserveOutputStream(worldLandmarkStream, [this](const mediapipe::Packet &packet) -> absl::Status
                                            {
            const int64_t ts = packet.Timestamp().Microseconds();
            auto landmarks = ToBodyLandmarks(packet.Get<mediapipe::LandmarkList>());

            std::lock_guard<std::mutex> lock(pendingMutex_);
            auto &partial = pendingBody_[ts];

            partial.world = std::move(landmarks);
            partial.hasWorld = true;
            emitBody(ts);

            return absl::OkStatus(); });

        return status.ok() || reportObserverError(worldLandmarkStream, status);
    }

    /**
     * @brief Registra los observers de manos (el de world landmarks es opcional).
     */
    bool observeHand(const std::string &landmarkStream, const std::string &handednessStream, const std::string &worldLandmarkStream)
    {
        auto status = graph_.ObserveOutputStream(landmarkStream, [this](const mediapipe::Packet &packet) -> absl::Status
                                                 {
            const int64_t ts = packet.Timestamp().Microseconds();
            const auto &multiHand = packet.Get<std::vector<mediapipe::NormalizedLandmarkList>>();

            std::vector<std::vector<HandLandmark>> hands;
            hands.reserve(multiHand.size());

            for (const auto &hand : multiHand)
            {
                std::vector<HandLandmark> landmarks;
                landmarks.reserve(hand.landmark_size());

                for (const auto &lm : hand.landmark())
                    landmarks.push_back({lm.x(), lm.y(), lm.z(), lm.has_visibility() ? lm.visibility() : 0.0f});

                hands.push_back(std::move(landmarks));
            }

            std::lock_guard<std::mutex> lock(pendingMutex_);
            auto &partial = pendingHand_[ts];

            partial.wantsWorldLandmarks = wantsWorld_;
            partial.hands = std::move(hands);
            partial.hasLandmarks = true;
            emitHand(ts);

            return absl::OkStatus(); });

        if (!status.ok())
            return reportObserverError(landmarkStream, status);

        status = graph_.ObserveOutputStream(handednessStream, [this](const mediapipe::Packet &packet) -> absl::Status
                                            {
            const int64_t ts = packet.Timestamp().Microseconds();
            const auto &multiHandedness = packet.Get<std::vector<mediapipe::ClassificationList>>();

            std::vector<HandClassification> handedness;
            handedness.reserve(multiHandedness.size());

            for (const auto &list : multiHandedness)
            {
                if (list.classification_size() > 0)
                    handedness.push_back({list.classification(0).label(), list.classification(0).score()});
                else
                    handedness.push_back({"Unknown", 0.0f});
            }

            std::lock_guard<std::mutex> lock(pendingMutex_);
            auto &partial = pendingHand_[ts];

            partial.wantsWorldLandmarks = wantsWorld_;
            partial.handedness = std::move(handedness);
            partial.hasHandedness = true;
            emitHand(ts);

            return absl::OkStatus(); });

        if (!status.ok())
            return reportObserverError(handednessStream, status);

        if (!wantsWorld_)
            return true;

        status = graph_.ObserveOutputStream(worldLandmarkStream, [this](const mediapipe::Packet &packet) -> absl::Status
                                            {
            const int64_t ts = packet.Timestamp().Microseconds();
            const auto &multiWorld = packet.Get<std::vector<mediapipe::LandmarkList>>();

            std::vector<std::vector<HandWorldLandmark>> worldHands;
            worldHands.reserve(multiWorld.size());

            for (const auto &hand : multiWorld)
            {
                std::vector<HandWorldLandmark> landmarks;
                landmarks.reserve(hand.landmark_size());

                for (const auto &lm : hand.landmark())
                    landmarks.push_back({lm.x(), lm.y(), lm.z()});

                worldHands.push_back(std::move(landmarks));
            }

            std::lock_guard<std::mutex> lock(pendingMutex_);
            auto &partial = pendingHand_[ts];

            partial.wantsWorldLandmarks = wantsWorld_;
            partial.worldLandmarks = std::move(worldHands);
            partial.hasWorldLandmarks = true;
            emitHand(ts);

            return absl::OkStatus(); });

        return status.ok() || reportObserverError(worldLandmarkStream, status);
    }

    /// Emite el resultado de cuerpo si ya llegaron ambos streams. Requiere pendingMutex_.
    void emitBody(int64_t timestampUs)
    {
        auto it = pendingBody_.find(timestampUs);

        if (it == pendingBody_.end() || !it->second.isComplete())
            return;

        if (bodyCallback_)
        {
            BodyTrackingResult result;

            result.timestampUs = timestampUs;
            result.landmarks = std::move(it->second.normalized);
            result.worldLandmarks = std::move(it->second.world);
            bodyCallback_(result);
        }

        pendingBody_.erase(it);

        while (pendingBody_.size() > 8)
            pendingBody_.erase(pendingBody_.begin());
    }

    /// Emite el resultado de manos si ya llegaron todos los streams. Requiere pendingMutex_.
    void emitHand(int64_t timestampUs)
    {
        auto it = pendingHand_.find(timestampUs);

        if (it == pendingHand_.end() || !it->second.isComplete())
            return;

        if (handCallback_)
        {
            HandTrackingResult result;

            result.timestampUs = timestampUs;
            result.hands = std::move(it->second.hands);
            result.handedness = std::move(it->second.handedness);
            result.worldLandmarks = std::move(it->second.worldLandmarks);
            handCallback_(result);
        }

        pendingHand_.erase(it);

        while (pendingHand_.size() > 8)
            pendingHand_.erase(pendingHand_.begin());
    }

    bool reportObserverError(const std::string &stream, const absl::Status &status)
    {
        fprintf(stderr, "ERROR ObserveOutputStream(%s): %s\n", stream.c_str(), status.ToString().c_str());
        return false;
    }

    /**
     * @brief Carga el grafo, prepara la GPU si corresponde, registra observers y arranca.
     * @param registerObservers Registra los observers del tipo de tracker.
     */
    template <typename F>
    bool start(const TrackerConfig &config, F registerObservers)
    {
        inputStream_ = config.inputStream;
        feedStream_ = config.inputStream;
        ConfigureResourceRoot();

#if !MEDIAPIPE_DISABLE_GPU
        if (const char *fault = std::getenv("PLEXUS_LANDMARKS_FAULT_AFTER_FRAMES"))
            faultAfterFrames_ = std::atoi(fault);
#endif

        std::ifstream file(config.graphPath);

        if (!file.is_open())
        {
            fprintf(stderr, "ERROR: no se puede abrir graph: %s\n", config.graphPath.c_str());
            return false;
        }

        std::stringstream text;
        text << file.rdbuf();

        auto graphConfig = mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(text.str());

#if !MEDIAPIPE_DISABLE_GPU
        // Se crea el contexto EGL/GLES antes que el grafo para fallar pronto y con un mensaje claro.
        auto gpuResources = mediapipe::GpuResources::Create();

        if (!gpuResources.ok())
        {
            fprintf(stderr, "ERROR GpuResources::Create: %s\n", gpuResources.status().ToString().c_str());
            return false;
        }

        if (!insertImageFrameConversion(graphConfig))
            return false;
#endif

        auto status = graph_.Initialize(graphConfig);

        if (!status.ok())
        {
            fprintf(stderr, "ERROR graph.Initialize: %s\n", status.ToString().c_str());
            return false;
        }

#if !MEDIAPIPE_DISABLE_GPU
        status = graph_.SetGpuResources(std::move(gpuResources).value());

        if (!status.ok())
        {
            fprintf(stderr, "ERROR SetGpuResources: %s\n", status.ToString().c_str());
            return false;
        }
#endif

        if (!registerObservers())
            return false;

        status = graph_.StartRun({});

        if (!status.ok())
        {
            fprintf(stderr, "ERROR graph.StartRun: %s\n", status.ToString().c_str());
            return false;
        }

        running_ = true;
        inputClosed_ = false;

        return true;
    }

#if !MEDIAPIPE_DISABLE_GPU
    /**
     * @brief Hace que el grafo de GPU reciba ImageFrame.
     *
     * El grafo de GPU declara `input_stream` como GpuBuffer. Se renombra ese
     * stream de entrada a `<nombre>_frame` y se añade un nodo
     * ImageFrameToGpuBufferCalculator que produce el stream original, de modo
     * que el grafo no necesita conocer la conversión.
     */
    bool insertImageFrameConversion(mediapipe::CalculatorGraphConfig &graphConfig)
    {
        bool found = false;

        for (auto &name : *graphConfig.mutable_input_stream())
        {
            if (name == inputStream_)
            {
                name = inputStream_ + kFrameSuffix;
                found = true;
            }
        }

        if (!found)
        {
            fprintf(stderr, "ERROR: el grafo no declara el input_stream '%s'\n", inputStream_.c_str());
            return false;
        }

        auto *node = graphConfig.add_node();

        node->set_calculator("ImageFrameToGpuBufferCalculator");
        node->add_input_stream(inputStream_ + kFrameSuffix);
        node->add_output_stream(inputStream_);
        feedStream_ = inputStream_ + kFrameSuffix;

        return true;
    }
#endif
};

PLEXUS_LANDMARKS_NAMESPACE_END

extern "C"
{
    PLEXUS_LANDMARKS_ENGINE_EXPORT plexus::landmarks::IEngine *plexusLandmarksCreateEngine() { return new plexus::landmarks::MediaPipeEngine(); }

    PLEXUS_LANDMARKS_ENGINE_EXPORT void plexusLandmarksDestroyEngine(plexus::landmarks::IEngine *engine) { delete engine; }

    /// Identificación del motor (`plexus::landmarks::describe()`); la fachada la compara con la suya al cargarlo.
    PLEXUS_LANDMARKS_ENGINE_EXPORT const char *plexusLandmarksEngineVersion() { return plexus::landmarks::describe(); }
}
