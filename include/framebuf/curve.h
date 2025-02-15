/* curve.h -- bezier calculations */

#ifndef DPTLIB_CURVE_H
#define DPTLIB_CURVE_H

#include "base/result.h"
#include "geom/point.h"
#include "framebuf/screen.h"
#include "utils/fxp.h"

/**
 * Return the point on the line defined by `p0` and `p1` at parameter `t`.
 *
 * \param[in] p0 Start point.
 * \param[in] p1 End point.
 * \param[in] t Parameter.
 * \return Point on the line.
 */
point_t curve_point_on_line(point_t p0,
                            point_t p1,
                            fix16_t t);

/**
 * Return the point on the quadratic Bézier curve defined by `p0`, `p1`, and `p2` at parameter `t`.
 *
 * \param[in] p0 Start point.
 * \param[in] p1 Control point.
 * \param[in] p2 End point.
 * \param[in] t Parameter.
 * \return Point on the curve.
 */
point_t curve_bezier_point_on_quad(point_t p0,
                                   point_t p1,
                                   point_t p2,
                                   fix16_t t);

/**
 * Return the point on the cubic Bézier curve defined by `p0`, `p1`, `p2`, and `p3` at parameter `t`.
 *
 * \param[in] p0 Start point.
 * \param[in] p1 Control point 1.
 * \param[in] p2 Control point 2.
 * \param[in] p3 End point.
 * \param[in] t Parameter.
 * \return Point on the curve.
 */
point_t curve_bezier_point_on_cubic(point_t p0,
                                    point_t p1,
                                    point_t p2,
                                    point_t p3,
                                    fix16_t t);

/**
 * Return the point on the quartic Bézier curve defined by `p0`, `p1`, `p2`, `p3`, and `p4` at parameter `t`.
 *
 * \param[in] p0 Start point.
 * \param[in] p1 Control point 1.
 * \param[in] p2 Control point 2.
 * \param[in] p3 Control point 3.
 * \param[in] p4 End point.
 * \param[in] t Parameter.
 * \return Point on the curve.
 */
point_t curve_bezier_point_on_quartic(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      point_t p4,
                                      fix16_t t);

/**
 * Return the point on the quintic Bézier curve defined by `p0`, `p1`, `p2`, `p3`, `p4`, and `p5` at parameter `t`.
 *
 * \param[in] p0 Start point.
 * \param[in] p1 Control point 1.
 * \param[in] p2 Control point 2.
 * \param[in] p3 Control point 3.
 * \param[in] p4 End point.
 * \param[in] p5 End point.
 * \param[in] t Parameter.
 * \return Point on the curve.
 */
point_t curve_bezier_point_on_quintic(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      point_t p4,
                                      point_t p5,
                                      fix16_t t);

// As for curve_bezier_point_on_cubic but written in terms of quads.
point_t curve_bezier_point_on_cubic_r(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      fix16_t t);

// As for curve_bezier_point_on_quartic but written in terms of cubics (and in turn of quads).
point_t curve_bezier_point_on_quartic_r(point_t p0,
                                        point_t p1,
                                        point_t p2,
                                        point_t p3,
                                        point_t p4,
                                        fix16_t t);

// As for curve_bezier_point_on_quintic but written in terms of quartics (and in turn of cubics, etc.).
point_t curve_bezier_point_on_quintic_r(point_t p0,
                                        point_t p1,
                                        point_t p2,
                                        point_t p3,
                                        point_t p4,
                                        point_t p5,
                                        fix16_t t);

// Uses forward differencing (float version).
void curve_bezier_cubic_f(point_t  p0,
                          point_t  p1,
                          point_t  p2,
                          point_t  p3,
                          int      nsteps,
                          point_t *points);

// Uses forward differencing (fixed-point version).
void curve_bezier_cubic(point_t  p0,
                        point_t  p1,
                        point_t  p2,
                        point_t  p3,
                        int      nsteps,
                        point_t *points);

#endif /* DPTLIB_CURVE_H */
