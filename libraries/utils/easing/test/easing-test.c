/* utils/easing/test/easing-test.c */

#include <math.h>
#include <stdio.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"

#include "utils/easing.h"

#include "test/all-tests.h"

#define EPSILON 1e-9

result_t easing_test(const char *resources)
{
  int    kind;
  int    mode;
  int    nfailures = 0;
  double lo;
  double hi;

  NOT_USED(resources);

  printf("test: endpoints, clamping and in-out midpoint\n");

  for (kind = 0; kind < easing_NKINDS; kind++)
  {
    for (mode = easing_IN; mode <= easing_IN_OUT; mode++)
    {
      lo = easing((easing_kind_t) kind, (easing_mode_t) mode, 0.0);
      hi = easing((easing_kind_t) kind, (easing_mode_t) mode, 1.0);

      if (fabs(lo) > EPSILON || fabs(hi - 1.0) > EPSILON)
      {
        printf("kind %d mode %d: f(0)=%g f(1)=%g failed!\n", kind, mode, lo, hi);
        nfailures++;
      }

      if (easing((easing_kind_t) kind, (easing_mode_t) mode, -1.0) != lo ||
          easing((easing_kind_t) kind, (easing_mode_t) mode, 2.0) != hi)
      {
        printf("kind %d mode %d: clamping failed!\n", kind, mode);
        nfailures++;
      }
    }

    lo = easing((easing_kind_t) kind, easing_IN_OUT, 0.5);

    if (fabs(lo - 0.5) > EPSILON)
    {
      printf("kind %d: in-out f(0.5)=%g failed!\n", kind, lo);
      nfailures++;
    }
  }

  printf("test: quad shape\n");

  if (fabs(easing(easing_QUAD, easing_IN, 0.5) - 0.25) > EPSILON ||
      fabs(easing(easing_QUAD, easing_OUT, 0.5) - 0.75) > EPSILON)
  {
    printf("failed!\n");
    nfailures++;
  }

  printf("easing: %d failure(s)\n", nfailures);

  return (nfailures > 0) ? result_TEST_FAILED : result_TEST_PASSED;
}
