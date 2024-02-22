/* curve.h -- bezier calculations */

#ifndef DPTLIB_CURVE_H
#define DPTLIB_CURVE_H

#include "base/result.h"
#include "geom/point.h"
#include "framebuf/screen.h"
#include "utils/fxp.h"

point_t curve_point_on_line(point_t p0,
                            point_t p1,
                            fix16_t t);

point_t curve_bezier_point_on_quad(point_t p0,
                                   point_t p1,
                                   point_t p2,
                                   fix16_t t);

point_t curve_bezier_point_on_cubic(point_t p0,
                                    point_t p1,
                                    point_t p2,
                                    point_t p3,
                                    fix16_t t);

point_t curve_bezier_point_on_quartic(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      point_t p4,
                                      fix16_t t);

point_t curve_bezier_point_on_quintic(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      point_t p4,
                                      point_t p5,
                                      fix16_t t);

// written in terms of quads
point_t curve_bezier_point_on_cubic_r(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      fix16_t t);

// written in terms of cubics (and in turn of quads)
point_t curve_bezier_point_on_quartic_r(point_t p0,
                                        point_t p1,
                                        point_t p2,
                                        point_t p3,
                                        point_t p4,
                                        fix16_t t);

// written in terms of quartics (and in turn of cubics, etc.)
point_t curve_bezier_point_on_quintic_r(point_t p0,
                                        point_t p1,
                                        point_t p2,
                                        point_t p3,
                                        point_t p4,
                                        point_t p5,
                                        fix16_t t);

// uses forward differencing (float version)
void curve_bezier_cubic_f(point_t  p0,
                          point_t  p1,
                          point_t  p2,
                          point_t  p3,
                          int      nsteps,
                          point_t *points);

void curve_bezier_cubic(point_t  p0,
                        point_t  p1,
                        point_t  p2,
                        point_t  p3,
                        int      nsteps,
                        point_t *points);

#endif /* DPTLIB_CURVE_H */
