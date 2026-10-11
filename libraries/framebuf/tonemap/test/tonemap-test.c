/* framebuf/tonemap/test/tonemap-test.c */

#include <stdio.h>

#include "base/result.h"

#include "framebuf/tonemap.h"

#include "test/all-tests.h"

result_t tonemap_test(const char *resources)
{
  tonemap_t             *map;
  tonemap_t             *copy;
  tonemap_spec_t         spec;
  tonemap_spec_t         got;
  const tonemap_table_t *red;
  const tonemap_table_t *green;
  const tonemap_table_t *alpha;
  int                    before;
  int                    i;

  (void) resources;

  map = tonemap_create();
  if (map == NULL)
    return result_TEST_FAILED;

  /* 1. neutral map: identity, R/G/B tables identical, alpha linear */
  tonemap_get_tables(map, &red, &green, NULL, &alpha);

  for (i = 0; i < 256; i++)
  {
    if (red->level[i] != i)
    {
      printf("tonemap: neutral red[%d] = %d\n", i, red->level[i]);
      goto failure;
    }
  }

  for (i = 0; i < 256; i++)
  {
    if (red->level[i] != green->level[i] || alpha->level[i] != i)
    {
      printf("tonemap: neutral tables differ at %d\n", i);
      goto failure;
    }
  }

  /* 2. gamma > 100 brightens the midtones */
  before = red->level[64];

  tonemap_get_values(map, tonemap_CHANNEL_RGB, &spec);
  spec.gamma = 200;
  tonemap_set(map, tonemap_CHANNEL_RGB, &spec);

  tonemap_get_tables(map, &red, NULL, NULL, NULL);
  if (red->level[64] <= before)
  {
    printf("tonemap: gamma 200 did not brighten (%d -> %d)\n",
           before, red->level[64]);
    goto failure;
  }

  /* 3. a copy is independent of the original */
  copy = tonemap_copy(map);
  if (copy == NULL)
    goto failure;

  tonemap_reset(copy);
  tonemap_get_values(map, tonemap_CHANNEL_RED, &got);
  tonemap_destroy(copy);

  if (got.gamma != 200)
  {
    printf("tonemap: copy aliased original\n");
    goto failure;
  }

  /* 4. enabling alpha uses its own table */
  spec.gamma = 100;
  spec.flags = tonemap_FLAG_INVERT;
  tonemap_set(map, tonemap_CHANNEL_ALPHA, &spec);

  tonemap_get_tables(map, NULL, NULL, NULL, &alpha);
  if (alpha->level[0] == 0)
  {
    printf("tonemap: inverted alpha table unchanged\n");
    goto failure;
  }

  tonemap_destroy(map);

  return result_TEST_PASSED;

failure:
  tonemap_destroy(map);

  return result_TEST_FAILED;
}
