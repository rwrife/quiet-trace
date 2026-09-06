#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace quiet_trace {

inline constexpr std::uint16_t kStoredAggregateVersion = 1;
inline constexpr std::uint16_t kLegacyStoredAggregateVersion = 0;

struct StoredAggregate {
  std::uint16_t version;
  std::uint64_t sequence;
  std::uint64_t monotonic_start_ms;
  std::int32_t level_eq_mdbfs;
  std::int32_t peak_125ms_mdbfs;
  std::array<std::uint16_t, 10> histogram_counts;
  std::uint16_t quality_flags_bits;
  std::uint32_t checksum;
};

class AggregateRingStore {
public:
  explicit AggregateRingStore(std::size_t capacity);

  void append(const StoredAggregate &record,
              bool simulate_power_loss = false) noexcept;
  [[nodiscard]] bool import_record(const StoredAggregate &record) noexcept;

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t capacity() const noexcept;
  [[nodiscard]] std::vector<StoredAggregate> export_records() noexcept;
  [[nodiscard]] bool erase_records(bool require_export = true) noexcept;
  void factory_erase() noexcept;

  [[nodiscard]] std::size_t recover() noexcept;
  [[nodiscard]] std::uint64_t generation() const noexcept;
  [[nodiscard]] std::vector<std::uint32_t> slot_write_counts() const;

private:
  [[nodiscard]] std::uint32_t
  checksum_for(const StoredAggregate &record) const noexcept;
  [[nodiscard]] bool is_valid_v1(const StoredAggregate &record) const noexcept;
  [[nodiscard]] bool
  has_valid_histogram(const StoredAggregate &record) const noexcept;
  [[nodiscard]] std::optional<StoredAggregate>
  migrate_if_needed(const StoredAggregate &record) const noexcept;
  void clear_slots() noexcept;

  std::vector<std::optional<StoredAggregate>> slots_;
  std::vector<std::uint32_t> slot_writes_;
  std::size_t head_;
  std::size_t count_;
  std::uint64_t generation_;
  std::uint64_t last_sequence_written_;
  std::uint64_t last_sequence_exported_;
};

} // namespace quiet_trace
