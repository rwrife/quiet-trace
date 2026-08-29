#include "quiet_trace/audio_source.hpp"
#include "synthetic_sample_source.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

namespace {

bool expect(bool condition, std::string_view description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    return false;
  }
  return true;
}

} // namespace

int main() {
  quiet_trace::test::SyntheticSampleSource source;
  std::array<std::int32_t, 4> buffer{};

  const auto first = source.read(std::span{buffer});
  bool passed = expect(first.samples_read == buffer.size(), "first frame size");
  passed &= expect(!first.overrun, "fixture has no implicit overrun");
  passed &= expect(buffer == std::array<std::int32_t, 4>{0, 1024, -1024, 2048},
                   "first deterministic frame");

  const auto second = source.read(std::span{buffer});
  passed &= expect(second.samples_read == buffer.size(), "second frame size");
  passed &= expect(buffer == std::array<std::int32_t, 4>{-2048, 512, -512, 0},
                   "second deterministic frame");

  source.reset();
  const auto after_reset = source.read(std::span{buffer});
  passed &=
      expect(after_reset.samples_read == buffer.size(), "reset frame size");
  passed &= expect(buffer == std::array<std::int32_t, 4>{0, 1024, -1024, 2048},
                   "reset reproduces first frame");

  return passed ? 0 : 1;
}
