/* geom/stage/test/stage-test.c */

#include <stdio.h>
#include <string.h>

#include "base/types.h"
#include "base/utils.h"
#include "geom/stage.h"

#include "test/all-tests.h"

#define CHECK(cond_) \
  do { if (!(cond_)) { printf("stage-test: %s:%d: %s\n", \
                              __FILE__, __LINE__, #cond_); \
                       return result_TEST_FAILED; } } while (0)

#define GRID_W 700
#define GRID_H 600

/* Boxes must cover the work area with no gaps. Same-kind boxes may overlap,
 * e.g. at the corners of the stroked border. */
static result_t check_coverage(const stageconfig_t *config,
                               int                  wa_w,
                               int                  wa_h,
                               int                  content_w,
                               int                  content_h)
{
  static unsigned char covered[GRID_H][GRID_W];

  stagebox_t           boxes[stage_MAX_BOXES];
  size_t               nboxes;
  int                  min_w;
  int                  min_h;
  size_t               i;
  int                  x;
  int                  y;

  stage_generate(config, wa_w, wa_h, content_w, content_h,
                 &min_w, &min_h, boxes, &nboxes);

  CHECK(min_w <= GRID_W && min_h <= GRID_H);
  CHECK(nboxes > 0 && nboxes <= stage_MAX_BOXES);
  CHECK(boxes[0].kind == stageboxkind_CONTENT);
  CHECK(boxes[0].box.x1 - boxes[0].box.x0 ==
        (content_w & ~((1 << config->log2_pixel_x) - 1)));
  CHECK(boxes[0].box.y1 - boxes[0].box.y0 ==
        (content_h & ~((1 << config->log2_pixel_y) - 1)));

  memset(covered, 0, sizeof(covered));
  for (i = 0; i < nboxes; i++)
  {
    CHECK(!box_is_empty(&boxes[i].box));
    CHECK(boxes[i].box.x0 >= 0 && boxes[i].box.x1 <= min_w);
    CHECK(boxes[i].box.y0 >= 0 && boxes[i].box.y1 <= min_h);

    for (y = boxes[i].box.y0; y < boxes[i].box.y1; y++)
      for (x = boxes[i].box.x0; x < boxes[i].box.x1; x++)
        covered[y][x] = 1;
  }

  for (y = 0; y < min_h; y++)
    for (x = 0; x < min_w; x++)
      CHECK(covered[y][x]);

  return result_TEST_PASSED;
}

static result_t test_tiling(void)
{
  stageconfig_t config;
  result_t      rc;

  stageconfig_init(&config);

  rc = check_coverage(&config, 640, 480, 200, 100); /* shadow, roomy */
  if (rc != result_TEST_PASSED)
    return rc;

  rc = check_coverage(&config, 10, 10, 200, 100);   /* work area too small */
  if (rc != result_TEST_PASSED)
    return rc;

  config.flags = 0;
  rc = check_coverage(&config, 640, 480, 200, 100); /* no shadow */
  if (rc != result_TEST_PASSED)
    return rc;

  config.log2_pixel_x = 1; /* 2 units per pixel, like RISC OS */
  config.log2_pixel_y = 1;
  return check_coverage(&config, 641, 481, 201, 101);
}

static result_t test_min_size(void)
{
  stageconfig_t config;
  stagebox_t    boxes[stage_MAX_BOXES];
  size_t        nboxes;
  int           min_w;
  int           min_h;
  int           fixed_w;
  int           fixed_h;

  stageconfig_init(&config);
  stage_get_fixed(&config, &fixed_w, &fixed_h);
  CHECK(fixed_w == 32 && fixed_h == 32);

  stage_generate(&config, 0, 0, 100, 50, &min_w, &min_h, boxes, &nboxes);
  CHECK(min_w == 100 + 2 * 8 + 16);
  CHECK(min_h == 50 + 2 * 8 + 16);

  return result_TEST_PASSED;
}

result_t stage_test(const char *resources)
{
  typedef result_t (*stagetestfn)(void);

  static const stagetestfn tests[] =
  {
    test_tiling,
    test_min_size
  };

  size_t i;
  int    nfailures;

  NOT_USED(resources);

  nfailures = 0;
  for (i = 0; i < NELEMS(tests); i++)
    if (tests[i]() != result_TEST_PASSED)
      nfailures++;

  return (nfailures == 0) ? result_TEST_PASSED : result_TEST_FAILED;
}
