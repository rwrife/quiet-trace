#include "quiet_trace/record_ring.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {

bool expect(const bool condition, const std::string_view description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    return false;
  }
  return true;
}

quiet_trace::StoredAggregate
make_record(const std::uint64_t sequence,
            const std::uint16_t version = quiet_trace::kStoredAggregateVersion,
            const std::uint32_t checksum = 0) {
  return quiet_trace::StoredAggregate{
      .version = version,
      .sequence = sequence,
      .monotonic_start_ms = sequence * 60'000ULL,
      .level_eq_mdbfs = -35'000,
      .peak_125ms_mdbfs = -20'000,
      .histogram_counts = {0, 0, 8, 64, 192, 152, 56, 8, 0, 0},
      .quality_flags_bits = 0,
      .checksum = checksum,
  };
}

} // namespace

int main() {
  quiet_trace::AggregateRingStore store(3);

  store.append(make_record(1));
  store.append(make_record(2));
  store.append(make_record(3));
  store.append(make_record(4));
  store.append(make_record(5));

  bool passed = expect(store.size() == 3, "ring capacity retained at 3");

  auto wrapped = store.export_records();
  passed &= expect(wrapped.size() == 3, "export returns retained records");
  passed &= expect(wrapped[0].sequence == 3 && wrapped[1].sequence == 4 &&
                       wrapped[2].sequence == 5,
                   "wrap preserves newest records in sequence order");

  auto wear = store.slot_write_counts();
  const auto minmax = std::minmax_element(wear.begin(), wear.end());
  passed &= expect((*minmax.second - *minmax.first) <= 1U,
                   "round-robin writes distribute wear");

  store.append(make_record(6));
  passed &= expect(!store.erase_records(true),
                   "erase-before-export is rejected after a new write");
  static_cast<void>(store.export_records());
  passed &=
      expect(store.erase_records(true), "erase succeeds after explicit export");
  passed &=
      expect(store.generation() == 1, "erase increments store generation");
  passed &= expect(store.size() == 0, "erase clears all records");

  store.append(make_record(10));
  store.append(make_record(11), true);
  store.append(make_record(12));
  auto pre_recover = store.export_records();
  passed &= expect(pre_recover.size() == 2,
                   "power-loss write excluded from normal export");

  const auto removed = store.recover();
  passed &= expect(removed >= 1, "recovery removes corrupted entries");
  auto recovered = store.export_records();
  passed &= expect(recovered.size() == 2, "recovered ring keeps valid entries");
  passed &= expect(recovered[0].sequence == 10 && recovered[1].sequence == 12,
                   "sequence ordering remains monotonic after recovery");

  const auto legacy =
      make_record(20, quiet_trace::kLegacyStoredAggregateVersion, 0);
  passed &= expect(store.import_record(legacy),
                   "legacy version record migrates to v1");
  const auto migrated = store.export_records();
  passed &=
      expect(migrated.back().version == quiet_trace::kStoredAggregateVersion,
             "migrated record stored as current version");

  const auto bad_legacy =
      make_record(21, quiet_trace::kLegacyStoredAggregateVersion, 0xA5A5A5A5U);
  passed &= expect(!store.import_record(bad_legacy),
                   "invalid legacy checksum marker is rejected");

  store.factory_erase();
  passed &= expect(store.size() == 0, "factory erase clears records");
  passed &=
      expect(store.generation() == 2, "factory erase advances generation");

  return passed ? 0 : 1;
}
