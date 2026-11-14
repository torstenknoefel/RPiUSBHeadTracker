#pragma once
#include <cmath>

// "1 Euro filter" (Casiez, Roussel, Vogel, CHI 2012): a low-pass filter whose
// cut-off frequency rises with the speed of the signal. Holding still gives
// strong smoothing (no jitter); fast movements pass with little lag.
//
//   min_cutoff  cut-off (Hz) while still: lower = smoother, more lag
//   beta        how fast the cut-off rises with speed: higher = less lag
//   d_cutoff    cut-off (Hz) for the speed estimate
class OneEuroFilter
{
public:
  OneEuroFilter(double min_cutoff = 1.0, double beta = 0.05, double d_cutoff = 1.0)
    : min_cutoff_(min_cutoff), beta_(beta), d_cutoff_(d_cutoff) {}

  void set_parameters(double min_cutoff, double beta, double d_cutoff = 1.0)
  {
    min_cutoff_ = min_cutoff;
    beta_ = beta;
    d_cutoff_ = d_cutoff;
  }

  // Filters 'value' measured at time 't' (seconds, increasing).
  double filter(double value, double t)
  {
    if (!initialized_) {
      initialized_ = true;
      x_ = value;
      dx_ = 0;
      t_ = t;
      return x_;
    }
    const double dt = t - t_;
    if (dt <= 0)
      return x_;
    t_ = t;

    const double dx = (value - x_) / dt;
    dx_ += alpha(d_cutoff_, dt) * (dx - dx_);
    const double cutoff = min_cutoff_ + beta_ * std::fabs(dx_);
    x_ += alpha(cutoff, dt) * (value - x_);
    return x_;
  }

  void reset() { initialized_ = false; }
  bool initialized() const { return initialized_; }
  double value() const { return x_; }

private:
  static double alpha(double cutoff, double dt)
  {
    const double tau = 1.0 / (2.0 * 3.14159265358979323846 * cutoff);
    return 1.0 / (1.0 + tau / dt);
  }

  double min_cutoff_, beta_, d_cutoff_;
  bool initialized_ = false;
  double x_ = 0, dx_ = 0, t_ = 0;
};
