
#include <stdio.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/suppress.h"
#include "utils/array.h"

#include "datastruct/list.h"

typedef struct node
{
  list_t ll;
  char   string[32];
}
node_t;

static int printelement(list_t *e, void *opaque)
{
  node_t *n;

  NOT_USED(opaque);

  n = (node_t *) e;

  printf("%p: %s\n", (void *) e, n->string);

  return 0;
}

result_t list_test(void)
{
  node_t data[] =
  {
    { { NULL }, "deckard"   },
    { { NULL }, "batty"     },
    { { NULL }, "rachael"   },
    { { NULL }, "gaff"      },
    { { NULL }, "bryant"    },
    { { NULL }, "pris"      },
    { { NULL }, "sebastian" },
  };

  list_t anchor = { NULL };

  int i;

  printf("test: add to head\n");

  for (i = 0; i < NELEMS(data); i++)
  {
    printf("adding '%s'...\n", data[i].string);
    list_add_to_head(&anchor, &data[i].ll);
  }

  printf("test: iterate\n");

  list_walk(&anchor, printelement, NULL);

  return result_TEST_PASSED;
}
