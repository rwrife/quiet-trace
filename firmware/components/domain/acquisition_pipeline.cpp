#include "quiet_trace/acquisition_pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace quiet_trace {
namespace {

constexpr double kEpsilon = 1e-20;

std::int32_t histogram_index(const double value_dbfs) noexcept {
  if (value_dbfs < -100.0) {
    return 0;
  }
  if (value_dbfs >= 0.0) {
    return 9;
  }

  const auto index =
      static_cast<std::int32_t>(std::floor((value_dbfs + 100.0) / 10.0));
  return std::clamp(index, std::int32_t{0}, std::int32_t{9});
}

} // namespace

AcquisitionPipeline::AcquisitionPipeline(
    AcquisitionPipelineConfig config) noexcept
    : config_(config), expected_samples_per_minute_(0),
      expected_window_samples_(0), expected_windows_per_minute_(0),
      framing_min_(0), framing_max_(0), minute_total_samples_(0),
      minute_valid_samples_(0), minute_window_sample_count_(0),
      minute_complete_windows_(0), minute_energy_sum_(0.0),
      minute_window_energy_sum_(0.0), minute_peak_dbfs_(-200.0),
      minute_histogram_{}, previous_input_(0.0), previous_dc_output_(0.0),
      weighting_lowpass_state_(0.0),
      calibration_(kDefaultCalibrationRuntimeState), minute_clipped_(false),
      minute_framing_error_(false), minute_overrun_(false),
      minute_dropped_(false), minute_clock_adjusted_(false),
      minute_ready_(false), ready_minute_{}, last_input_zeroized_(true),
      counters_{} {
  if (config_.sample_rate_hz == 0U) {
    config_.sample_rate_hz = kDefaultAcquisitionPipelineConfig.sample_rate_hz;
  }
  if (config_.full_scale <= 0) {
    config_.full_scale = kDefaultAcquisitionPipelineConfig.full_scale;
  }
  if (config_.framing_bits < 8U || config_.framing_bits > 31U) {
    config_.framing_bits = kDefaultAcquisitionPipelineConfig.framing_bits;
  }
  if (config_.dc_block_alpha <= 0.0 || config_.dc_block_alpha >= 1.0) {
    config_.dc_block_alpha = kDefaultAcquisitionPipelineConfig.dc_block_alpha;
  }
  if (config_.weighting_high_pass_alpha <= 0.0 ||
      config_.weighting_high_pass_alpha >= 1.0) {
    config_.weighting_high_pass_alpha =
        kDefaultAcquisitionPipelineConfig.weighting_high_pass_alpha;
  }

  expected_samples_per_minute_ = config_.sample_rate_hz * 60U;
  expected_window_samples_ =
      std::max<std::uint32_t>(1U, config_.sample_rate_hz / 8U);
  expected_windows_per_minute_ =
      expected_samples_per_minute_ / expected_window_samples_;

  const auto frame_peak =
      static_cast<std::int64_t>(1)
      << static_cast<std::int64_t>(config_.framing_bits - 1U);
  framing_min_ = static_cast<std::int32_t>(-frame_peak);
  framing_max_ = static_cast<std::int32_t>(frame_peak - 1);
}

void AcquisitionPipeline::set_calibration(
    const CalibrationRuntimeState calibration) noexcept {
  calibration_ = calibration;
}

bool AcquisitionPipeline::sample_framing_valid(
    const std::int32_t sample) const noexcept {
  return sample >= framing_min_ && sample <= framing_max_;
}

void AcquisitionPipeline::ingest_sample(const std::int32_t sample) noexcept {
  const double normalized = std::clamp(
      static_cast<double>(sample) / static_cast<double>(config_.full_scale),
      -1.0, 1.0);

  if (sample >= config_.full_scale - 1 || sample <= -config_.full_scale) {
    minute_clipped_ = true;
    ++counters_.clipping_samples;
  }

  double dc_removed = normalized;
  if (config_.enable_dc_block) {
    dc_removed = normalized - previous_input_ +
                 config_.dc_block_alpha * previous_dc_output_;
    previous_input_ = normalized;
    previous_dc_output_ = dc_removed;
  }

  double weighted = dc_removed;
  if (config_.enable_weighting_approx) {
    weighting_lowpass_state_ += (1.0 - config_.weighting_high_pass_alpha) *
                                (dc_removed - weighting_lowpass_state_);
    weighted = dc_removed - weighting_lowpass_state_;
  }

  const double sample_energy = weighted * weighted;
  minute_energy_sum_ += sample_energy;
  minute_window_energy_sum_ += sample_energy;
  ++minute_valid_samples_;
  ++counters_.valid_samples;

  ++minute_window_sample_count_;
  if (minute_window_sample_count_ >= expected_window_samples_) {
    finalize_window();
  }
}

void AcquisitionPipeline::finalize_window() noexcept {
  if (minute_window_sample_count_ == 0U) {
    return;
  }

  const double window_energy = minute_window_energy_sum_ /
                               static_cast<double>(minute_window_sample_count_);
  const double window_dbfs =
      10.0 * std::log10(std::max(window_energy, kEpsilon));
  minute_peak_dbfs_ = std::max(minute_peak_dbfs_, window_dbfs);
  ++minute_histogram_[histogram_index(window_dbfs)];
  ++minute_complete_windows_;

  minute_window_energy_sum_ = 0.0;
  minute_window_sample_count_ = 0U;
}

void AcquisitionPipeline::reset_minute_state() noexcept {
  minute_total_samples_ = 0;
  minute_valid_samples_ = 0;
  minute_window_sample_count_ = 0;
  minute_complete_windows_ = 0;
  minute_energy_sum_ = 0.0;
  minute_window_energy_sum_ = 0.0;
  minute_peak_dbfs_ = -200.0;
  minute_histogram_.fill(0);

  minute_clipped_ = false;
  minute_framing_error_ = false;
  minute_overrun_ = false;
  minute_dropped_ = false;
  minute_clock_adjusted_ = false;
}

void AcquisitionPipeline::finalize_minute() noexcept {
  if (minute_window_sample_count_ > 0U) {
    counters_.dropped_samples += minute_window_sample_count_;
    minute_dropped_ = true;
    ++counters_.dropped_intervals;
    minute_window_sample_count_ = 0U;
    minute_window_energy_sum_ = 0.0;
  }

  ready_minute_.has_valid_samples = minute_valid_samples_ > 0U;
  ready_minute_.total_samples = minute_total_samples_;
  ready_minute_.valid_samples = minute_valid_samples_;
  ready_minute_.histogram_counts = minute_histogram_;
  ready_minute_.calibration_state = calibration_.state;
  ready_minute_.calibration_applied = false;
  ready_minute_.clipped = minute_clipped_;
  ready_minute_.framing_error = minute_framing_error_;
  ready_minute_.acquisition_overrun = minute_overrun_;
  ready_minute_.dropped_interval = minute_dropped_;
  ready_minute_.clock_adjusted = minute_clock_adjusted_;
  ready_minute_.weighting_unvalidated = true;

  if (minute_valid_samples_ == 0U) {
    ready_minute_.level_eq_dbfs = -200.0;
    ready_minute_.peak_125ms_dbfs = -200.0;
    ready_minute_.insufficient_samples = true;
  } else {
    const double energy =
        minute_energy_sum_ / static_cast<double>(minute_valid_samples_);
    ready_minute_.level_eq_dbfs = 10.0 * std::log10(std::max(energy, kEpsilon));
    ready_minute_.peak_125ms_dbfs = minute_complete_windows_ == 0U
                                        ? ready_minute_.level_eq_dbfs
                                        : minute_peak_dbfs_;
    ready_minute_.insufficient_samples =
        minute_valid_samples_ < expected_samples_per_minute_ ||
        minute_complete_windows_ != expected_windows_per_minute_;
  }

  ready_minute_.level_eq_adjusted_dbfs = ready_minute_.level_eq_dbfs;
  ready_minute_.peak_125ms_adjusted_dbfs = ready_minute_.peak_125ms_dbfs;
  if (calibration_.state == CalibrationState::reference_adjusted &&
      calibration_.has_offset && std::isfinite(calibration_.offset_db)) {
    ready_minute_.level_eq_adjusted_dbfs += calibration_.offset_db;
    ready_minute_.peak_125ms_adjusted_dbfs += calibration_.offset_db;
    ready_minute_.calibration_applied = true;
  }

  minute_ready_ = true;
  reset_minute_state();
}

void AcquisitionPipeline::ingest_block(std::span<std::int32_t> samples,
                                       const bool acquisition_overrun,
                                       const bool dropped_interval,
                                       const bool clock_uncertain,
                                       const bool framing_error) noexcept {
  if (acquisition_overrun) {
    minute_overrun_ = true;
    ++counters_.acquisition_overruns;
  }
  if (dropped_interval) {
    minute_dropped_ = true;
    ++counters_.dropped_intervals;
  }
  if (clock_uncertain) {
    minute_clock_adjusted_ = true;
    ++counters_.clock_uncertain_intervals;
  }
  if (framing_error) {
    minute_framing_error_ = true;
  }

  std::size_t index = 0;
  while (index < samples.size()) {
    const auto remaining_minute = static_cast<std::size_t>(
        expected_samples_per_minute_ - minute_total_samples_);
    const auto slice_count = std::min(remaining_minute, samples.size() - index);

    if (!dropped_interval) {
      for (std::size_t offset = 0; offset < slice_count; ++offset) {
        const auto sample = samples[index + offset];
        if (framing_error || !sample_framing_valid(sample)) {
          minute_framing_error_ = true;
          ++counters_.framing_error_samples;
          ++counters_.dropped_samples;
          minute_dropped_ = true;
          continue;
        }
        ingest_sample(sample);
      }
    } else {
      counters_.dropped_samples += slice_count;
    }

    minute_total_samples_ += slice_count;
    index += slice_count;

    if (minute_total_samples_ >= expected_samples_per_minute_) {
      finalize_minute();
    }
  }

  std::fill(samples.begin(), samples.end(), 0);
  last_input_zeroized_ =
      std::all_of(samples.begin(), samples.end(),
                  [](const std::int32_t sample) { return sample == 0; });
}

bool AcquisitionPipeline::minute_ready() const noexcept {
  return minute_ready_;
}

MinuteAggregate AcquisitionPipeline::consume_minute() noexcept {
  minute_ready_ = false;
  const auto minute = ready_minute_;
  ready_minute_ = MinuteAggregate{};
  return minute;
}

AggregationCounters AcquisitionPipeline::counters() const noexcept {
  return counters_;
}

bool AcquisitionPipeline::last_input_zeroized() const noexcept {
  return last_input_zeroized_;
}

} // namespace quiet_trace
