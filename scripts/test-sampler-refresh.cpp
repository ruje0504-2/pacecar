// SPDX-License-Identifier: MIT
// Exercise the production sampler/publisher without starting threads, opening
// a hardware provider, changing power modes or creating a renderer.
#include "Sampler.h"
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

namespace pacecar::overlay {
struct SamplerPoolTestAccess {
    static void Init(Sampler& sampler, std::shared_ptr<metrics::IMetricProvider> source) {
        sampler.aggregator_ = std::make_unique<metrics::Aggregator>(2, 4, 1);
        sampler.aggregator_->AddProvider(std::move(source));
        for (unsigned i = 0; i < 4; ++i)
            sampler.framePool_.push_back(std::make_shared<metrics::DisplayFrame>());
    }
    static void Tick(Sampler& sampler) { sampler.TickOnce(); }
};
}

class ModeSource final : public pacecar::metrics::IMetricProvider {
public:
    const char* Name() const noexcept override { return "mode fixture"; }
    std::uint32_t Domains() const noexcept override { return 0; }
    std::chrono::milliseconds Cadence() const noexcept override { return std::chrono::milliseconds(0); }
    HRESULT Poll(pacecar::metrics::MetricsSnapshot& snapshot) override {
        snapshot.soc.valid = pacecar::metrics::SwitchCpu | pacecar::metrics::SwitchGpu;
        snapshot.soc.cpuKhz = cpu;
        snapshot.soc.gpuKhz = gpu;
        ++polls;
        return S_OK;
    }
    unsigned polls = 0;
    std::uint32_t cpu = 1497600, gpu = 460800;
};

int main() {
    using pacecar::overlay::Sampler;
    using pacecar::overlay::SamplerPoolTestAccess;
    Sampler sampler;
    auto source = std::make_shared<ModeSource>();
    SamplerPoolTestAccess::Init(sampler, source);
    SamplerPoolTestAccess::Tick(sampler);
    auto held = sampler.LatestFrame(); // Simulate UI retaining an old frame.
    CHECK(held && held->snapshot && held->snapshot->soc.cpuKhz == 1497600);
    for (unsigned i = 0; i < 12; ++i) SamplerPoolTestAccess::Tick(sampler);
    CHECK(sampler.LatestFrame()->snapshot->sequence == source->polls);
    CHECK(held->snapshot->sequence == 1); // A live consumer's frame was not mutated.
    source->cpu = 1581000; source->gpu = 537600;
    SamplerPoolTestAccess::Tick(sampler);
    CHECK(sampler.LatestFrame()->snapshot->soc.cpuKhz == source->cpu);
    CHECK(sampler.LatestFrame()->snapshot->soc.gpuKhz == source->gpu);
    held.reset();

    // Actual consumers may legitimately pin every publishable snapshot. The
    // sampler must resume with the latest readings once one consumer releases.
    auto first = sampler.LatestFrame();
    SamplerPoolTestAccess::Tick(sampler);
    auto second = sampler.LatestFrame();
    SamplerPoolTestAccess::Tick(sampler);
    auto third = sampler.LatestFrame();
    const auto stalled = third->snapshot->sequence;
    SamplerPoolTestAccess::Tick(sampler);
    CHECK(sampler.LatestFrame()->snapshot->sequence == stalled);
    source->cpu = 1963500; source->gpu = 844800;
    first.reset();
    SamplerPoolTestAccess::Tick(sampler);
    CHECK(sampler.LatestFrame()->snapshot->sequence > stalled);
    CHECK(sampler.LatestFrame()->snapshot->soc.cpuKhz == source->cpu);
    CHECK(sampler.LatestFrame()->snapshot->soc.gpuKhz == source->gpu);
    CHECK(second->snapshot->soc.cpuKhz == 1581000 && third->snapshot->soc.gpuKhz == 537600);
    std::puts("PASS: UI lag cannot pin idle snapshots forever; mode readbacks resume after pool pressure; retained frames stay immutable (no hardware used)");
}
