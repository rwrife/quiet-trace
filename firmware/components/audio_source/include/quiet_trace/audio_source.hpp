#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace quiet_trace {

struct AudioReadResult {
  std::size_t samples_read;
  bool overrun;
  bool framing_error;
};

class AudioSource {
public:
  virtual ~AudioSource() = default;
  [[nodiscard]] virtual AudioReadResult
  read(std::span<std::int32_t> destination) noexcept = 0;
};

} // namespace quiet_trace
