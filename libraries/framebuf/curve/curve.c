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

#define SCALE (16)

static point_t scale_up(point_t p)
{
  point_t q;

  q.x = p.x * SCALE;
  q.y = p.y * SCALE;
  return q;
}

static point_t scale_down(point_t p)
{
  point_t q;

  q.x = p.x / SCALE;
  q.y = p.y / SCALE;
  return q;
}

point_t curve_bezier_point_on_quad(point_t p0,
                                   point_t p1,
                                   point_t p2,
                                   fix16_t t)
{
  point_t a, b;
  point_t ab;

  p0 = scale_up(p0);
  p1 = scale_up(p1);
  p2 = scale_up(p2);

  a = curve_point_on_line(p0, p1, t);
  b = curve_point_on_line(p1, p2, t);

  ab = curve_point_on_line(a, b, t);

  return scale_down(ab);
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

  p0 = scale_up(p0);
  p1 = scale_up(p1);
  p2 = scale_up(p2);
  p3 = scale_up(p3);

  a = curve_point_on_line(p0, p1, t);
  b = curve_point_on_line(p1, p2, t);
  c = curve_point_on_line(p2, p3, t);

  ab = curve_point_on_line(a, b, t);
  bc = curve_point_on_line(b, c, t);

  abc = curve_point_on_line(ab, bc, t);

  return scale_down(abc);
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

  p0 = scale_up(p0);
  p1 = scale_up(p1);
  p2 = scale_up(p2);
  p3 = scale_up(p3);
  p4 = scale_up(p4);

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

  return scale_down(abcd);
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

  p0 = scale_up(p0);
  p1 = scale_up(p1);
  p2 = scale_up(p2);
  p3 = scale_up(p3);
  p4 = scale_up(p4);
  p5 = scale_up(p5);

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

  return scale_down(abcde);
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

/* ----------------------------------------------------------------------- */

void curve_bezier_cubic_f(point_t  p0,
                          point_t  p1,
                          point_t  p2,
                          point_t  p3,
                          int      nsteps,
                          point_t *points)
{
  float    cx, cy;
  float    bx, by;
  float    ax, ay;
  float    h, hh, hhh;
  float    d1x, d1y;
  float    d2x, d2y;
  float    d3x, d3y;
  float    curx, cury;
  int      i;
  point_t *point;

  cx  = 3.0f * (p1.x - p0.x);
  cy  = 3.0f * (p1.y - p0.y);

  bx  = 3.0f * (p2.x - p1.x) - cx;
  by  = 3.0f * (p2.y - p1.y) - cy;

  ax  = p3.x - p0.x - cx - bx;
  ay  = p3.y - p0.y - cy - by;

  h   = 1.0f / nsteps;
  hh  = h * h;
  hhh = hh * h;

  /* first difference */
  d1x =        ax * hhh +        bx * hh + cx * h;
  d1y =        ay * hhh +        by * hh + cy * h;

  /* second difference */
  d2x = 6.0f * ax * hhh + 2.0f * bx * hh;
  d2y = 6.0f * ay * hhh + 2.0f * by * hh;

  /* third difference */
  d3x = 6.0f * ax * hhh;
  d3y = 6.0f * ay * hhh;

  point = &points[0];

  curx = p0.x;
  cury = p0.y;

  for (i = 0; ; i++)
  {
    point->x = curx;
    point->y = cury;
    point++;

    if (i == nsteps)
      break;

    curx += d1x;
    cury += d1y;

    d1x += d2x;
    d1y += d2y;

    d2x += d3x;
    d2y += d3y;
  }
}
