/* utils/easing/easing.c -- easing curves */

#include <math.h>

#include "base/utils.h"

#include "utils/easing.h"

#define PI 3.1415926535897932384626433832795

/* ----------------------------------------------------------------------- */

/* Each function is the "out" form of its curve. The "in" and "in-out" forms
 * are derived by reflection in easing(). */

static double out_linear(double t)
{
  return t;
}

static double out_quad(double t)
{
  return 1.0 - pow(1.0 - t, 2.0);
}

static double out_cubic(double t)
{
  return 1.0 - pow(1.0 - t, 3.0);
}

static double out_quart(double t)
{
  return 1.0 - pow(1.0 - t, 4.0);
}

static double out_quint(double t)
{
  return 1.0 - pow(1.0 - t, 5.0);
}

static double out_sine(double t)
{
  return sin(t * PI / 2.0);
}

static double out_expo(double t)
{
  return (t >= 1.0) ? 1.0 : 1.0 - pow(2.0, -10.0 * t);
}

static double out_circ(double t)
{
  return sqrt(1.0 - (t - 1.0) * (t - 1.0));
}

static double out_back(double t)
{
  static const double c1 = 1.70158;
  static const double c3 = 2.70158;

  double u;

  u = t - 1.0;

  return 1.0 + c3 * u * u * u + c1 * u * u;
}

static double out_elastic(double t)
{
  if (t <= 0.0 || t >= 1.0)
  {
    return t;
  }

  return pow(2.0, -10.0 * t) * sin((t * 10.0 - 0.75) * (2.0 * PI / 3.0)) + 1.0;
}

static double out_bounce(double t)
{
  static const double n1 = 7.5625;
  static const double d1 = 2.75;

  if (t < 1.0 / d1)
  {
    return n1 * t * t;
  }
  else if (t < 2.0 / d1)
  {
    t -= 1.5 / d1;
    return n1 * t * t + 0.75;
  }
  else if (t < 2.5 / d1)
  {
    t -= 2.25 / d1;
    return n1 * t * t + 0.9375;
  }

  t -= 2.625 / d1;
  return n1 * t * t + 0.984375;
}

/* ----------------------------------------------------------------------- */

double easing(easing_kind_t kind, easing_mode_t mode, double t)
{
  static double (*const out[easing_NKINDS])(double) =
  {
    out_linear,
    out_quad,
    out_cubic,
    out_quart,
    out_quint,
    out_sine,
    out_expo,
    out_circ,
    out_back,
    out_elastic,
    out_bounce
  };

  t = CLAMP(t, 0.0, 1.0);

  switch (mode)
  {
  case easing_IN:
    return 1.0 - out[kind](1.0 - t);

  case easing_OUT:
    return out[kind](t);

  case easing_IN_OUT:
    if (t < 0.5)
    {
      return (1.0 - out[kind](1.0 - 2.0 * t)) / 2.0;
    }

    return (1.0 + out[kind](2.0 * t - 1.0)) / 2.0;
  }

  return t;
}
