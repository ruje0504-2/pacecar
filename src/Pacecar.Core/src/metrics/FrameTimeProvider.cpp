#include "pacecar/metrics/FrameTimeProvider.h"

#include <algorithm>
#include <cstdio>

namespace pacecar::metrics
{
namespace
{
constexpr std::uint16_t kPresentEventKind = 1;
constexpr std::uint16_t kGdiUpdateKind = 2;
constexpr std::uint64_t kPresentTimeoutMs = 2000;
constexpr double kLiveWindowMs = 1000.0;

std::uint64_t SteadyNowMs() noexcept
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
} // namespace

FrameTimeProvider::FrameTimeProvider(std::shared_ptr<SensorHelperClient> client)
    : client_(std::move(client)), clock_(&SteadyNowMs)
{
    eventScratch_.reserve(ipc::kMaxFrameEvents);
}

FrameTimeProvider::FrameTimeProvider(std::shared_ptr<SensorHelperClient> client, Clock clock)
    : client_(std::move(client)), clock_(std::move(clock))
{
    if (!clock_)
    {
        clock_ = &SteadyNowMs;
    }
    eventScratch_.reserve(ipc::kMaxFrameEvents);
}

const char* FrameTimeProvider::Name() const noexcept
{
    return "frame-time";
}

std::uint32_t FrameTimeProvider::Domains() const noexcept
{
    return static_cast<std::uint32_t>(MetricDomain::FrameTime);
}

std::chrono::milliseconds FrameTimeProvider::Cadence() const noexcept
{
    return std::chrono::milliseconds(1000);
}

HRESULT FrameTimeProvider::Poll(MetricsSnapshot& snapshot)
{
    if (!client_)
    {
        return E_FAIL;
    }

    const auto now = clock_();
    static_cast<void>(client_->Pump(now));

    FrameCaptureState state = FrameCaptureState::NotCapturing;
    std::uint32_t targetPid = 0;
    std::uint64_t qpcFrequency = 0;
    std::uint64_t timestampMs = 0;
    std::uint64_t eventsLost = 0;
    std::uint64_t queueDrops = 0;
    client_->TakeFrameData(eventScratch_, state, targetPid, qpcFrequency, timestampMs, eventsLost,
                           queueDrops);

    const bool active = state == FrameCaptureState::Capturing && targetPid != 0;
    if (!active || targetPid != sampledPid_)
    {
        processor_.Reset();
        havePresent_ = false;
        sampledKind_ = 0;
        std::lock_guard<std::mutex> lock(mutex_);
        degraded_ = false;
        healthObserved_ = false;
    }
    sampledPid_ = targetPid;
    snapshot.frame.captureState = static_cast<std::uint32_t>(state);
    snapshot.frame.targetPid = active ? targetPid : 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = state;
    }

    if (active)
    {
        UpdateHealth(eventsLost, queueDrops);
    }

    const auto containsKind = [&](std::uint16_t kind) {
        return std::any_of(eventScratch_.begin(), eventScratch_.end(),
                           [&](const auto& e) { return e.pid == targetPid && e.kind == kind; });
    };
    const bool expired = havePresent_ && now >= lastPresentMs_ &&
                         now - lastPresentMs_ > kPresentTimeoutMs;
    std::uint16_t kind = sampledKind_;
    if (active && containsKind(kPresentEventKind))
    {
        kind = kPresentEventKind;
    }
    else if (active && (sampledKind_ != kPresentEventKind || expired) && containsKind(kGdiUpdateKind))
    {
        kind = kGdiUpdateKind;
    }
    if (kind != sampledKind_ || (expired && !containsKind(kind)))
    {
        processor_.Reset();
        havePresent_ = false;
    }
    sampledKind_ = kind;
    processor_.SetClockFrequency(qpcFrequency);
    for (const ipc::FrameEventPayload& event : eventScratch_)
    {
        if (!active || kind == 0 || event.kind != kind || event.pid != targetPid)
        {
            continue;
        }
        PresentEvent present{};
        present.qpcTicks = event.qpcTicks;
        present.cpuTicks = event.cpuTicks;
        present.gpuTicks = event.gpuTicks;
        present.swapChain = event.swapChain;
        present.pid = event.pid;
        static_cast<void>(processor_.AddPresent(present));
        havePresent_ = true;
        lastPresentMs_ = now;
    }

    FrameTimeStats stats = processor_.ComputeRecent(kLiveWindowMs);
    snapshot.frame.eventKind = stats.valid ? kind : 0;
    snapshot.frame.lastPresentQpc = stats.lastPresentQpc;
    snapshot.frame.surface = stats.surface;
    if (!stats.valid)
    {
        snapshot.frame.fps = 0;
        snapshot.frame.frameTimeMs = 0;
        snapshot.frame.cpuTimeMs = 0;
        snapshot.frame.gpuTimeMs = 0;
        std::lock_guard<std::mutex> lock(mutex_);
        lastFps_ = 0;
        snapshot.frame.degraded = degraded_;
        return E_FAIL;
    }

    snapshot.frame.fps = stats.fps;
    snapshot.frame.frameTimeMs = stats.frameTimeMs;
    snapshot.frame.cpuTimeMs = stats.cpuTimeMs;
    snapshot.frame.gpuTimeMs = stats.gpuTimeMs;
    snapshot.frame.degraded = Degraded();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        lastFps_ = stats.fps;
    }
    return S_OK;
}

void FrameTimeProvider::UpdateHealth(std::uint64_t eventsLost, std::uint64_t queueDrops) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!healthObserved_)
    {
        lastEventsLost_ = eventsLost;
        lastQueueDrops_ = queueDrops;
        healthObserved_ = true;
        return;
    }
    if (eventsLost < lastEventsLost_ || queueDrops < lastQueueDrops_)
    {
        // Counters went backwards: the helper started a fresh capture session.
        lastEventsLost_ = eventsLost;
        lastQueueDrops_ = queueDrops;
        degraded_ = false;
        return;
    }
    if (eventsLost > lastEventsLost_ || queueDrops > lastQueueDrops_)
    {
        degraded_ = true;
        lastEventsLost_ = eventsLost;
        lastQueueDrops_ = queueDrops;
    }
}

bool FrameTimeProvider::Degraded() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return degraded_;
}

void FrameTimeProvider::Reset() noexcept
{
    processor_.Reset();
    sampledPid_ = 0;
    sampledKind_ = 0;
    havePresent_ = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = FrameCaptureState::NotCapturing;
        lastFps_ = 0.0;
        degraded_ = false;
        healthObserved_ = false;
        lastEventsLost_ = 0;
        lastQueueDrops_ = 0;
    }
    if (client_)
    {
        client_->SetCaptureTarget(0, false);
    }
}

FrameCaptureState FrameTimeProvider::CaptureState() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

double FrameTimeProvider::LastFps() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return lastFps_;
}

std::wstring FrameTimeProvider::StatusLine() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::wstring text = FrameCaptureStatusText(state_);
    if (state_ == FrameCaptureState::Capturing && lastFps_ > 0.0)
    {
        wchar_t buffer[64] = {};
        swprintf_s(buffer, L"%.1f FPS", lastFps_);
        text = buffer;
    }
    if (degraded_)
    {
        text += L" (degraded: events lost/dropped)";
    }
    return text;
}
} // namespace pacecar::metrics
