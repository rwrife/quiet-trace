#include "quiet_trace/acquisition_pipeline.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <string_view>
#include <vector>

namespace {

bool expect(const bool condition, const std::string_view description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    return false;
  }
  return true;
}

bool expect_near(const double actual, const double expected,
                 const double tolerance, const std::string_view description) {
  const auto delta = std::fabs(actual - expected);
  if (delta > tolerance) {
    std::cerr << "FAIL: " << description << " actual=" << actual
              << " expected=" << expected << " tolerance=" << tolerance
              << " delta=" << delta << '\n';
    return false;
  }
  return true;
}

std::vector<std::int32_t> make_sine_minute(const std::uint32_t sample_rate_hz,
                                           const std::int32_t full_scale,
                                           const double amplitude_ratio,
                                           const double frequency_hz) {
  const std::size_t sample_count =
      static_cast<std::size_t>(sample_rate_hz) * static_cast<std::size_t>(60U);

  std::vector<std::int32_t> values(sample_count);
  for (std::size_t index = 0; index < sample_count; ++index) {
    const double t =
        static_cast<double>(index) / static_cast<double>(sample_rate_hz);
    const double normalized =
        amplitude_ratio * std::sin(2.0 * std::numbers::pi * frequency_hz * t);
    values[index] = static_cast<std::int32_t>(
        std::llround(normalized * static_cast<double>(full_scale)));
  }
  return values;
}

} // namespace

int main() {
  quiet_trace::AcquisitionPipelineConfig config{
      .sample_rate_hz = 800,
      .full_scale = 8'388'608,
      .framing_bits = 24,
      .dc_block_alpha = 0.995,
      .weighting_high_pass_alpha = 0.85,
      .enable_dc_block = false,
      .enable_weighting_approx = false,
  };

  quiet_trace::AcquisitionPipeline pipeline(config);
  pipeline.set_calibration(quiet_trace::CalibrationRuntimeState{
      .state = quiet_trace::CalibrationState::reference_adjusted,
      .offset_db = 3.0,
      .has_offset = true,
  });

  auto minute =
      make_sine_minute(config.sample_rate_hz, config.full_scale, 0.5, 8.0);
  pipeline.ingest_block(minute, false, false, false);

  bool passed = expect(pipeline.last_input_zeroized(),
                       "raw input block zeroized after ingest");
  passed &= expect(pipeline.minute_ready(), "minute should be ready");

  const auto aggregate = pipeline.consume_minute();
  passed &= expect(aggregate.has_valid_samples, "aggregate has valid samples");
  passed &=
      expect(aggregate.valid_samples ==
                 static_cast<std::uint64_t>(config.sample_rate_hz) * 60ULL,
             "minute sample count");
  passed &= expect(!aggregate.insufficient_samples,
                   "full-minute aggregate is complete");

  constexpr double kExpectedEqDbfs = -9.030899869;
  passed &= expect_near(aggregate.level_eq_dbfs, kExpectedEqDbfs, 0.15,
                        "equivalent level matches deterministic sine");
  passed &= expect_near(aggregate.peak_125ms_dbfs, kExpectedEqDbfs, 0.2,
                        "125ms peak matches deterministic windows");

  std::int64_t histogram_total = 0;
  for (const auto count : aggregate.histogram_counts) {
    histogram_total += count;
  }
  passed &= expect(histogram_total == 480,
                   "histogram totals 480 complete 125ms windows");
  passed &= expect(aggregate.histogram_counts[9] == 480,
                   "-9 dBFS windows land in final bin");
  passed &= expect(aggregate.calibration_state ==
                       quiet_trace::CalibrationState::reference_adjusted,
                   "calibration state propagated");
  passed &= expect(aggregate.calibration_applied,
                   "calibration offset applied to aggregate outputs");
  passed &= expect_near(aggregate.level_eq_adjusted_dbfs,
                        aggregate.level_eq_dbfs + 3.0, 1e-9,
                        "calibration offset applied to equivalent output");

  const auto counters = pipeline.counters();
  passed &= expect(counters.clipping_samples == 0,
                   "deterministic sine should not clip");
  passed &=
      expect(counters.valid_samples ==
                 static_cast<std::uint64_t>(config.sample_rate_hz) * 60ULL,
             "counters valid sample count");

  quiet_trace::AcquisitionPipeline error_pipeline(config);
  std::array<std::int32_t, 8> malformed_block{
      0,
      config.full_scale * 2,
      200,
      -200,
      config.full_scale - 1,
      -config.full_scale,
      0,
      0,
  };
  error_pipeline.ingest_block(malformed_block, true, true, true);
  passed &= expect(error_pipeline.last_input_zeroized(),
                   "malformed input still zeroized");
  const auto error_counters = error_pipeline.counters();
  passed &= expect(error_counters.acquisition_overruns == 1,
                   "overrun counter increments");
  passed &= expect(error_counters.dropped_intervals == 1,
                   "dropped interval counter increments");
  passed &= expect(error_counters.clock_uncertain_intervals == 1,
                   "clock uncertainty counter increments");
  passed &= expect(error_counters.dropped_samples == malformed_block.size(),
                   "dropped interval counts dropped samples");

  std::array<std::int32_t, 6> framing_samples{
      0, 1, 2, config.full_scale * 4, -config.full_scale * 3, 5,
  };
  error_pipeline.ingest_block(framing_samples, false, false, false);
  const auto framing_counters = error_pipeline.counters();
  passed &= expect(framing_counters.framing_error_samples == 2,
                   "invalid framing samples are rejected");

  return passed ? 0 : 1;
}
