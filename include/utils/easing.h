/* utils/easing.h -- easing curves */

/**
 * \file easing.h
 * \brief Easing curves for animation.
 *
 * Each curve maps a progress value t in [0,1] to an eased value that is 0 at
 * t=0 and 1 at t=1. Back and elastic curves overshoot [0,1] in between.
 */

#ifndef UTILS_EASING_H
#define UTILS_EASING_H

#ifdef __cplusplus
extern "C"
{
#endif

/** The shape of an easing curve. */
typedef enum
{
  easing_LINEAR,
  easing_QUAD,
  easing_CUBIC,
  easing_QUART,
  easing_QUINT,
  easing_SINE,
  easing_EXPO,
  easing_CIRC,
  easing_BACK,
  easing_ELASTIC,
  easing_BOUNCE,

  easing_NKINDS
}
easing_kind_t;

/** Where the curve accelerates. */
typedef enum
{
  easing_IN,     /**< Starts slowly. */
  easing_OUT,    /**< Ends slowly. */
  easing_IN_OUT  /**< Starts and ends slowly. */
}
easing_mode_t;

/**
 * Evaluate an easing curve.
 *
 * \param kind Curve shape.
 * \param mode Where the curve accelerates.
 * \param t    Progress, clamped to [0,1].
 * \return     Eased value; 0 at t=0 and 1 at t=1.
 */
double easing(easing_kind_t kind, easing_mode_t mode, double t);

#ifdef __cplusplus
}
#endif

#endif /* UTILS_EASING_H */
