/* bezier.c */

// - Eliminate structure passing, or at least analyse some code with it inlined to see if it helps
// - Does these need to round to nearest?
// - Consider overflow too

#include "base/utils.h"

#include "geom/point.h"

#include "framebuf/curve.h"

point_t curve_point_on_line(point_t p0, point_t p1, fix16_t t)
{
  point_t c;

  c.x = p0.x + (p1.x - p0.x) * t / 65536;
  c.y = p0.y + (p1.y - p0.y) * t / 65536;

  return c;
}

point_t curve_bezier_point_on_quad(point_t p0,
                                   point_t p1,
                                   point_t p2,
                                   fix16_t t)
{
  point_t a, b;
  point_t ab;

  a = curve_point_on_line(p0, p1, t);
  b = curve_point_on_line(p1, p2, t);

  ab = curve_point_on_line(a, b, t);

  return ab;
}

point_t curve_bezier_point_on_cubic(point_t p0,
                                    point_t p1,
                                    point_t p2,
                                    point_t p3,
                                    fix16_t t)
{
  point_t a, b, c;
  point_t ab, bc;
  point_t abc;

  a = curve_point_on_line(p0, p1, t);
  b = curve_point_on_line(p1, p2, t);
  c = curve_point_on_line(p2, p3, t);

  ab = curve_point_on_line(a, b, t);
  bc = curve_point_on_line(b, c, t);

  abc = curve_point_on_line(ab, bc, t);

  return abc;
}

point_t curve_bezier_point_on_quartic(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      point_t p4,
                                      fix16_t t)
{
  point_t a, b, c, d;
  point_t ab, bc, cd;
  point_t abc, bcd;
  point_t abcd;

  a = curve_point_on_line(p0, p1, t);
  b = curve_point_on_line(p1, p2, t);
  c = curve_point_on_line(p2, p3, t);
  d = curve_point_on_line(p3, p4, t);

  ab = curve_point_on_line(a, b, t);
  bc = curve_point_on_line(b, c, t);
  cd = curve_point_on_line(c, d, t);

  abc = curve_point_on_line(ab, bc, t);
  bcd = curve_point_on_line(bc, cd, t);

  abcd = curve_point_on_line(abc, bcd, t);

  return abcd;
}

point_t curve_bezier_point_on_quintic(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      point_t p4,
                                      point_t p5,
                                      fix16_t t)
{
  point_t a, b, c, d, e;
  point_t ab, bc, cd, de;
  point_t abc, bcd, cde;
  point_t abcd, bcde;
  point_t abcde;

  a = curve_point_on_line(p0, p1, t);
  b = curve_point_on_line(p1, p2, t);
  c = curve_point_on_line(p2, p3, t);
  d = curve_point_on_line(p3, p4, t);
  e = curve_point_on_line(p4, p5, t);

  ab = curve_point_on_line(a, b, t);
  bc = curve_point_on_line(b, c, t);
  cd = curve_point_on_line(c, d, t);
  de = curve_point_on_line(d, e, t);

  abc = curve_point_on_line(ab, bc, t);
  bcd = curve_point_on_line(bc, cd, t);
  cde = curve_point_on_line(cd, de, t);

  abcd = curve_point_on_line(abc, bcd, t);
  bcde = curve_point_on_line(bcd, cde, t);

  abcde = curve_point_on_line(abcd, bcde, t);

  return abcde;
}

/* ----------------------------------------------------------------------- */

point_t curve_bezier_point_on_cubic_r(point_t p0,
                                      point_t p1,
                                      point_t p2,
                                      point_t p3,
                                      fix16_t t)
{
  point_t ab, bc;
  point_t abc;

  ab = curve_bezier_point_on_quad(p0, p1, p2, t);
  bc = curve_bezier_point_on_quad(p1, p2, p3, t);

  abc = curve_point_on_line(ab, bc, t);

  return abc;
}

point_t curve_bezier_point_on_quartic_r(point_t p0,
                                        point_t p1,
                                        point_t p2,
                                        point_t p3,
                                        point_t p4,
                                        fix16_t t)
{
  point_t abc, bcd;
  point_t abcd;

  abc = curve_bezier_point_on_cubic_r(p0, p1, p2, p3, t);
  bcd = curve_bezier_point_on_cubic_r(p1, p2, p3, p4, t);

  abcd = curve_point_on_line(abc, bcd, t);

  return abcd;
}

point_t curve_bezier_point_on_quintic_r(point_t p0,
                                        point_t p1,
                                        point_t p2,
                                        point_t p3,
                                        point_t p4,
                                        point_t p5,
                                        fix16_t t)
{
  point_t abcd, bcde;
  point_t abcde;

  abcd = curve_bezier_point_on_quartic_r(p0, p1, p2, p3, p4, t);
  bcde = curve_bezier_point_on_quartic_r(p1, p2, p3, p4, p5, t);

  abcde = curve_point_on_line(abcd, bcde, t);

  return abcde;
}
