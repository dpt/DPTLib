/* framebuf/curve.h -- bezier calculations */

#ifndef DPTLIB_CURVE_H
#define DPTLIB_CURVE_H

#include "base/result.h"
#include "geom/point.h"
#include "framebuf/screen.h"
#include "utils/fxp.h"

/**
 * Return the point on the line defined by points `p0` and `p1` at time `t`.
 *
 * This is a straight linear interpolation between the two points: there is no
 * curvature.
 *
 * \param[in] p0 Start point.
 * \param[in] p1 End point.
 * \param[in] t  Time (0.0 to 1.0 as fixed-point).
 * \return Point on the line.
 */
point_t curve_point_on_line(point_t p0,
                            point_t p1,
                            fix16_t t);

/**
 * Return the point on the curve defined by points `p0` to `p2` at time `t`.
 *
 * This is a quadratic Bézier curve: it has a single control point.
 *
 * \param[in] p0 Start point.
 * \param[in] p1 Control point.
 * \param[in] p2 End point.
 * \param[in] t  Time (0.0 to 1.0 as fixed-point).
 * \return Point on the curve.
 */
point_t curve_bezier_point_on_quad(point_t p0,
                                   point_t p1,
                                   point_t p2,
                                   fix16_t t);

/**
 * Return the point on the curve defined by `p0` to `p3` at time `t`.
*
 * This is a cubic Bézier curve: it has two control points.
 *
 * \param[in] p0 Start point.
 * \param[in] p1 Control point 1.
 * \param[in] p2 Control point 2.
 * \param[in] p3 End point.
 * \param[in] t  Time (0.0 to 1.0 as fixed-point).
 * \return Point on the curve.
 */
point_t curve_bezier_point_on_cubic(point_t p0,
                                    point_t p1,
                                    point_t p2,
                                    point_t p3,
                                    fix16_t t);

/**
 * Return the point on the curve defined by points `p0` to `p4` at time `t`.
 *
 * This is a quartic Bézier curve: it has three control points.
 *
 * \param[in] p0 Start point.
 * \param[in] p1 Control point 1.
 * \param[in] p2 Control point 2.
 * \param[in] p3 Control point 3.
 * \param[in] p4 End point.
 * \param[in] t  Time.
 * \return Point on the curve.
 */
point_t curve_bezier_point_on_quartic(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      point_t p4,
                                      fix16_t t);

/**
 * Return the point on the curve defined by points `p0` to `p5` at time `t`.
 *
 * This is a quintic Bézier curve: it has four control points.
 *
 * \param[in] p0 Start point.
 * \param[in] p1 Control point 1.
 * \param[in] p2 Control point 2.
 * \param[in] p3 Control point 3.
 * \param[in] p4 Control point 4.
 * \param[in] p5 End point.
 * \param[in] t  Time.
 * \return Point on the curve.
 */
point_t curve_bezier_point_on_quintic(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      point_t p4,
                                      point_t p5,
                                      fix16_t t);

/**
 * As for \ref curve_bezier_point_on_cubic but is written in terms of quads.
 *
 * \param[in] p0 Start point.
 * \param[in] p1 Control point 1.
 * \param[in] p2 Control point 2.
 * \param[in] p3 Control point 3.
 * \param[in] t  Time (0.0 to 1.0 as fixed-point).
 * \return Point on the curve.
 */
point_t curve_bezier_point_on_cubic_r(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      fix16_t t);

/**
 * As for \ref curve_bezier_point_on_quartic but is written in terms of cubics
 * (and in turn of quads).
 *
 * \param[in] p0 Start point.
 * \param[in] p1 Control point 1.
 * \param[in] p2 Control point 2.
 * \param[in] p3 Control point 3.
 * \param[in] p4 End point.
 * \param[in] t  Time (0.0 to 1.0 as fixed-point).
 * \return Point on the curve.
 */
point_t curve_bezier_point_on_quartic_r(point_t p0,
                                        point_t p1,
                                        point_t p2,
                                        point_t p3,
                                        point_t p4,
                                        fix16_t t);

/**
 * As for \ref curve_bezier_point_on_quintic but is written in terms of quartics
 * (and in turn of cubics, etc.).
 *
 * \param[in] p0 Start point.
 * \param[in] p1 Control point 1.
 * \param[in] p2 Control point 2.
 * \param[in] p3 Control point 3.
 * \param[in] p4 End point.
 * \param[in] p5 End point.
 * \param[in] t  Time (0.0 to 1.0 as fixed-point).
 * \return Point on the curve.
 */
point_t curve_bezier_point_on_quintic_r(point_t p0,
                                        point_t p1,
                                        point_t p2,
                                        point_t p3,
                                        point_t p4,
                                        point_t p5,
                                        fix16_t t);

/**
 * As for \ref curve_bezier_cubic but uses forward differencing (float version).
 *
 * \param[in]  p0     Start point.
 * \param[in]  p1     Control point 1.
 * \param[in]  p2     Control point 2.
 * \param[in]  p3     End point.
 * \param[in]  nsteps Number of points to generate.
 * \param[out] points Array of points.
 */
void curve_bezier_cubic_f(point_t  p0,
                          point_t  p1,
                          point_t  p2,
                          point_t  p3,
                          int      nsteps,
                          point_t *points);

/**
 * As for \ref curve_bezier_cubic but uses forward differencing (fixed-point
 * version).
 *
 * \warning This suffers from drift (the end point is not guaranteed to be
 *          reached) if `nsteps` isn't a power of 2.
 *
 * \param[in]  p0     Start point.
 * \param[in]  p1     Control point 1.
 * \param[in]  p2     Control point 2.
 * \param[in]  p3     End point.
 * \param[in]  nsteps Number of points to generate.
 * \param[out] points Array of points.
 */
void curve_bezier_cubic(point_t  p0,
                        point_t  p1,
                        point_t  p2,
                        point_t  p3,
                        int      nsteps,
                        point_t *points);

#endif /* DPTLIB_CURVE_H */
