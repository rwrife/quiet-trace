#pragma once

#include "quiet_trace/audio_source.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace quiet_trace::test {

class SyntheticSampleSource final : public AudioSource {
public:
  [[nodiscard]] AudioReadResult
  read(std::span<std::int32_t> destination) noexcept override {
    const auto available = kSamples.size() - cursor_;
    const auto count = std::min(destination.size(), available);
    std::copy_n(kSamples.begin() + static_cast<std::ptrdiff_t>(cursor_), count,
                destination.begin());
    cursor_ += count;
    return AudioReadResult{
        .samples_read = count,
        .overrun = false,
        .framing_error = false,
    };
  }

  void reset() noexcept { cursor_ = 0; }

private:
  inline static constexpr std::array<std::int32_t, 8> kSamples{
      0, 1024, -1024, 2048, -2048, 512, -512, 0,
  };
  std::size_t cursor_{0};
};

} // namespace quiet_trace::test
