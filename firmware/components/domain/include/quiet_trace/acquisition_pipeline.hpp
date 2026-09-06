#pragma once

#include "quiet_trace/aggregate_contract.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace quiet_trace {

struct AcquisitionPipelineConfig {
  std::uint32_t sample_rate_hz;
  std::int32_t full_scale;
  std::uint8_t framing_bits;
  double dc_block_alpha;
  double weighting_high_pass_alpha;
  bool enable_dc_block;
  bool enable_weighting_approx;
};

inline constexpr AcquisitionPipelineConfig kDefaultAcquisitionPipelineConfig{
    .sample_rate_hz = 48'000,
    .full_scale = 8'388'608,
    .framing_bits = 24,
    .dc_block_alpha = 0.995,
    .weighting_high_pass_alpha = 0.85,
    .enable_dc_block = true,
    .enable_weighting_approx = true,
};

struct CalibrationRuntimeState {
  CalibrationState state;
  double offset_db;
  bool has_offset;
};

inline constexpr CalibrationRuntimeState kDefaultCalibrationRuntimeState{
    .state = CalibrationState::uncalibrated,
    .offset_db = 0.0,
    .has_offset = false,
};

struct AggregationCounters {
  std::uint64_t clipping_samples;
  std::uint64_t framing_error_samples;
  std::uint32_t acquisition_overruns;
  std::uint32_t dropped_intervals;
  std::uint32_t clock_uncertain_intervals;
  std::uint64_t valid_samples;
  std::uint64_t dropped_samples;
};

struct MinuteAggregate {
  bool has_valid_samples;
  std::uint64_t total_samples;
  std::uint64_t valid_samples;
  double level_eq_dbfs;
  double peak_125ms_dbfs;
  double level_eq_adjusted_dbfs;
  double peak_125ms_adjusted_dbfs;
  std::array<std::int32_t, 10> histogram_counts;
  CalibrationState calibration_state;
  bool calibration_applied;
  bool clipped;
  bool framing_error;
  bool acquisition_overrun;
  bool dropped_interval;
  bool clock_adjusted;
  bool insufficient_samples;
  bool weighting_unvalidated;
};

class AcquisitionPipeline {
public:
  explicit AcquisitionPipeline(AcquisitionPipelineConfig config =
                                   kDefaultAcquisitionPipelineConfig) noexcept;

  void set_calibration(CalibrationRuntimeState calibration) noexcept;

  void ingest_block(std::span<std::int32_t> samples, bool acquisition_overrun,
                    bool dropped_interval, bool clock_uncertain,
                    bool framing_error = false) noexcept;

  [[nodiscard]] bool minute_ready() const noexcept;
  [[nodiscard]] MinuteAggregate consume_minute() noexcept;
  [[nodiscard]] AggregationCounters counters() const noexcept;
  [[nodiscard]] bool last_input_zeroized() const noexcept;

private:
  [[nodiscard]] bool sample_framing_valid(std::int32_t sample) const noexcept;
  void ingest_sample(std::int32_t sample) noexcept;
  void finalize_window() noexcept;
  void finalize_minute() noexcept;
  void reset_minute_state() noexcept;

  AcquisitionPipelineConfig config_;
  std::uint32_t expected_samples_per_minute_;
  std::uint32_t expected_window_samples_;
  std::uint32_t expected_windows_per_minute_;
  std::int32_t framing_min_;
  std::int32_t framing_max_;

  std::uint64_t minute_total_samples_;
  std::uint64_t minute_valid_samples_;
  std::uint32_t minute_window_sample_count_;
  std::uint32_t minute_complete_windows_;
  double minute_energy_sum_;
  double minute_window_energy_sum_;
  double minute_peak_dbfs_;
  std::array<std::int32_t, 10> minute_histogram_;

  double previous_input_;
  double previous_dc_output_;
  double weighting_lowpass_state_;

  CalibrationRuntimeState calibration_;

  bool minute_clipped_;
  bool minute_framing_error_;
  bool minute_overrun_;
  bool minute_dropped_;
  bool minute_clock_adjusted_;

  bool minute_ready_;
  MinuteAggregate ready_minute_;

  bool last_input_zeroized_;
  AggregationCounters counters_;
};

} // namespace quiet_trace
