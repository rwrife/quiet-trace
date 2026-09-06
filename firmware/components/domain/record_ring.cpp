#include "quiet_trace/record_ring.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace quiet_trace {
namespace {

constexpr std::uint32_t kFnvOffset = 2166136261U;
constexpr std::uint32_t kFnvPrime = 16777619U;

template <typename T> void hash_bytes(std::uint32_t &hash, T value) noexcept {
  using Unsigned = std::make_unsigned_t<T>;
  const auto payload = static_cast<Unsigned>(value);
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    const auto byte =
        static_cast<std::uint8_t>((payload >> (index * 8U)) & 0xFFU);
    hash ^= byte;
    hash *= kFnvPrime;
  }
}

} // namespace

AggregateRingStore::AggregateRingStore(const std::size_t capacity)
    : slots_(std::max<std::size_t>(capacity, 1U)),
      slot_writes_(std::max<std::size_t>(capacity, 1U), 0U), head_(0),
      count_(0), generation_(0), last_sequence_written_(0),
      last_sequence_exported_(0) {}

std::uint32_t
AggregateRingStore::checksum_for(const StoredAggregate &record) const noexcept {
  std::uint32_t hash = kFnvOffset;
  hash_bytes(hash, record.version);
  hash_bytes(hash, record.sequence);
  hash_bytes(hash, record.monotonic_start_ms);
  hash_bytes(hash, record.level_eq_mdbfs);
  hash_bytes(hash, record.peak_125ms_mdbfs);
  for (const auto count : record.histogram_counts) {
    hash_bytes(hash, count);
  }
  hash_bytes(hash, record.quality_flags_bits);
  return hash;
}

bool AggregateRingStore::has_valid_histogram(
    const StoredAggregate &record) const noexcept {
  std::uint64_t histogram_total = 0;
  for (const auto count : record.histogram_counts) {
    histogram_total += count;
  }
  return histogram_total == 480U;
}

bool AggregateRingStore::is_valid_v1(
    const StoredAggregate &record) const noexcept {
  return record.version == kStoredAggregateVersion &&
         has_valid_histogram(record) && checksum_for(record) == record.checksum;
}

std::optional<StoredAggregate> AggregateRingStore::migrate_if_needed(
    const StoredAggregate &record) const noexcept {
  if (is_valid_v1(record)) {
    return record;
  }

  if (record.version == kLegacyStoredAggregateVersion &&
      has_valid_histogram(record) && record.checksum == 0U) {
    auto migrated = record;
    migrated.version = kStoredAggregateVersion;
    migrated.checksum = checksum_for(migrated);
    return migrated;
  }

  return std::nullopt;
}

void AggregateRingStore::append(const StoredAggregate &record,
                                const bool simulate_power_loss) noexcept {
  StoredAggregate stored = record;
  stored.version = kStoredAggregateVersion;
  stored.checksum = checksum_for(stored);
  if (simulate_power_loss) {
    stored.checksum = 0U;
  }

  std::size_t slot_index = 0;
  if (count_ == slots_.size()) {
    slot_index = head_;
    head_ = (head_ + 1U) % slots_.size();
  } else {
    slot_index = (head_ + count_) % slots_.size();
    ++count_;
  }

  slots_[slot_index] = stored;
  ++slot_writes_[slot_index];
  last_sequence_written_ = std::max(last_sequence_written_, stored.sequence);
}

bool AggregateRingStore::import_record(const StoredAggregate &record) noexcept {
  const auto migrated = migrate_if_needed(record);
  if (!migrated.has_value()) {
    return false;
  }
  append(*migrated, false);
  return true;
}

std::size_t AggregateRingStore::size() const noexcept { return count_; }

std::size_t AggregateRingStore::capacity() const noexcept {
  return slots_.size();
}

std::vector<StoredAggregate> AggregateRingStore::export_records() noexcept {
  std::vector<StoredAggregate> records;
  records.reserve(count_);
  for (std::size_t offset = 0; offset < count_; ++offset) {
    const auto &entry = slots_[(head_ + offset) % slots_.size()];
    if (!entry.has_value()) {
      continue;
    }
    const auto migrated = migrate_if_needed(*entry);
    if (migrated.has_value()) {
      records.push_back(*migrated);
    }
  }
  last_sequence_exported_ = last_sequence_written_;
  return records;
}

bool AggregateRingStore::erase_records(const bool require_export) noexcept {
  if (require_export && last_sequence_exported_ < last_sequence_written_) {
    return false;
  }

  clear_slots();
  ++generation_;
  last_sequence_exported_ = last_sequence_written_;
  return true;
}

void AggregateRingStore::factory_erase() noexcept {
  clear_slots();
  ++generation_;
  last_sequence_exported_ = last_sequence_written_;
}

std::size_t AggregateRingStore::recover() noexcept {
  std::vector<StoredAggregate> valid;
  valid.reserve(count_);

  std::size_t removed = 0;
  std::uint64_t previous_sequence = 0;
  bool has_previous_sequence = false;

  for (std::size_t offset = 0; offset < count_; ++offset) {
    const auto &entry = slots_[(head_ + offset) % slots_.size()];
    if (!entry.has_value()) {
      ++removed;
      continue;
    }

    const auto migrated = migrate_if_needed(*entry);
    if (!migrated.has_value()) {
      ++removed;
      continue;
    }

    if (has_previous_sequence && migrated->sequence <= previous_sequence) {
      ++removed;
      continue;
    }

    previous_sequence = migrated->sequence;
    has_previous_sequence = true;
    valid.push_back(*migrated);
  }

  clear_slots();
  for (const auto &entry : valid) {
    append(entry, false);
  }
  return removed;
}

std::uint64_t AggregateRingStore::generation() const noexcept {
  return generation_;
}

std::vector<std::uint32_t> AggregateRingStore::slot_write_counts() const {
  return slot_writes_;
}

void AggregateRingStore::clear_slots() noexcept {
  for (auto &slot : slots_) {
    slot.reset();
  }
  head_ = 0;
  count_ = 0;
}

} // namespace quiet_trace
