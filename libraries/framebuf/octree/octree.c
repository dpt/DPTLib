/* framebuf/octree/octree.c -- octree colour quantiser */

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/octree.h"

/* ----------------------------------------------------------------------- */

#define OCTREE_DEPTH 8 /* levels 0..7 are internal, level 8 is always a leaf */

/* Every node but the root is on the path to a leaf, and paths have at most
 * OCTREE_DEPTH nodes. Each pixel's insertion is followed by a reduction to
 * octree_MAX_COLOURS, so an insertion can see one leaf more than that. */
#define OCTREE_MAX_NODES (1 + (octree_MAX_COLOURS + 1) * OCTREE_DEPTH)

typedef struct octree_node octree_node_t;

/* Every node on an insertion path accumulates the pixel, so folding a node
 * needs no summing: its own totals already cover its children. */
struct octree_node
{
  unsigned long  r, g, b;
  unsigned long  count;
  octree_node_t *child[8];
  octree_node_t *next;  /* next reducible node at this level, or free node */
  int            leaf;
};

typedef struct octree
{
  octree_node_t *nodes;     /* block of OCTREE_MAX_NODES nodes */
  int            nused;     /* nodes taken from the block so far */
  octree_node_t *free;      /* nodes returned to the block */
  octree_node_t *root;
  octree_node_t *reducible[OCTREE_DEPTH]; /* internal nodes, per level */
  int            leaves;
}
octree_t;

/* ----------------------------------------------------------------------- */

static octree_node_t *octree_new_node(octree_t *tree, int level)
{
  octree_node_t *node;

  if (tree->free != NULL)
  {
    node       = tree->free;
    tree->free = node->next;
  }
  else
  {
    assert(tree->nused < OCTREE_MAX_NODES);
    node = &tree->nodes[tree->nused++];
  }

  memset(node, 0, sizeof(*node));

  if (level == OCTREE_DEPTH)
  {
    node->leaf = 1;
    tree->leaves++;
  }
  else
  {
    node->next = tree->reducible[level];
    tree->reducible[level] = node;
  }

  return node;
}

static void octree_free_node(octree_t *tree, octree_node_t *node)
{
  node->next = tree->free;
  tree->free = node;
}

static void octree_insert(octree_t    *tree,
                          unsigned int r,
                          unsigned int g,
                          unsigned int b)
{
  octree_node_t *node;
  int            level;
  int            shift;
  int            idx;

  node = tree->root;
  for (level = 0; ; level++)
  {
    node->r += r;
    node->g += g;
    node->b += b;
    node->count++;
    if (node->leaf)
      break;

    shift = 7 - level;
    idx   = (((r >> shift) & 1) << 2) |
            (((g >> shift) & 1) << 1) |
            ( (b >> shift) & 1);
    if (node->child[idx] == NULL)
      node->child[idx] = octree_new_node(tree, level + 1);
    node = node->child[idx];
  }
}

/* Merge leaf child `from` into leaf child `into`, then free `from`. */
static void octree_merge_children(octree_t      *tree,
                                  octree_node_t *node,
                                  int            into,
                                  int            from)
{
  octree_node_t *a;
  octree_node_t *b;

  a = node->child[into];
  b = node->child[from];

  a->r     += b->r;
  a->g     += b->g;
  a->b     += b->b;
  a->count += b->count;

  octree_free_node(tree, b);
  node->child[from] = NULL;
  tree->leaves--;
}

/* Reduce the leaf count by folding the deepest reducible node with the
 * fewest pixels into a leaf. Its children are all leaves: any internal child
 * would sit on a deeper, non-empty reducible list.
 *
 * A fold removes up to seven leaves, so it can overshoot `target`. With
 * `partial` set, an overshooting fold instead merges the node's two smallest
 * children, removing one leaf. That leaves the merged colour in a child slot
 * that later pixels would no longer reach, so only use it once every pixel
 * is inserted. */
static void octree_reduce(octree_t *tree, int target, int partial)
{
  octree_node_t **prev;
  octree_node_t **bestprev;
  octree_node_t  *node;
  int             level;
  int             nchildren;
  int             first, second;
  int             i;

  for (level = OCTREE_DEPTH - 1; level >= 0; level--)
    if (tree->reducible[level])
      break;

  if (level < 0)
    return;

  bestprev = &tree->reducible[level];
  for (prev = &tree->reducible[level]; *prev; prev = &(*prev)->next)
    if ((*prev)->count < (*bestprev)->count)
      bestprev = prev;

  node = *bestprev;

  nchildren = 0;
  for (i = 0; i < 8; i++)
    if (node->child[i])
      nchildren++;

  if (partial && nchildren > 1 && tree->leaves - nchildren + 1 < target)
  {
    first = second = -1;
    for (i = 0; i < 8; i++)
    {
      if (node->child[i] == NULL)
        continue;

      if (first < 0 || node->child[i]->count < node->child[first]->count)
      {
        second = first;
        first  = i;
      }
      else if (second < 0 ||
               node->child[i]->count < node->child[second]->count)
      {
        second = i;
      }
    }
    octree_merge_children(tree, node, second, first);
    return;
  }

  *bestprev = node->next;

  for (i = 0; i < 8; i++)
  {
    if (node->child[i])
    {
      octree_free_node(tree, node->child[i]);
      node->child[i] = NULL;
      tree->leaves--;
    }
  }

  node->leaf = 1;
  tree->leaves++;
}

static void octree_collect(const octree_node_t *node,
                           colour_t            *palette,
                           int                 *n)
{
  unsigned long half;
  int           i;

  if (node == NULL)
    return;

  if (node->leaf)
  {
    half = node->count / 2;
    palette[(*n)++] = colour_rgb((unsigned int) ((node->r + half) / node->count),
                                 (unsigned int) ((node->g + half) / node->count),
                                 (unsigned int) ((node->b + half) / node->count));
    return;
  }

  for (i = 0; i < 8; i++)
    octree_collect(node->child[i], palette, n);
}

/* ----------------------------------------------------------------------- */

result_t octree_make_palette(const bitmap_t *src,
                             int             ncolours,
                             colour_t       *palette,
                             int            *nfound)
{
  octree_t                   tree;
  const unsigned char       *row;
  const pixelfmt_rgbx8888_t *px;
  int                        x, y;

  *nfound = 0;

  if (src->format != pixelfmt_rgbx8888 && src->format != pixelfmt_rgba8888)
    return result_BAD_ARG;
  if (ncolours < octree_MIN_COLOURS || ncolours > octree_MAX_COLOURS)
    return result_BAD_ARG;

  /* the root sums every pixel's channels */
  if ((unsigned long) src->size.w * src->size.h > ULONG_MAX / 255)
    return result_TOO_BIG;

  memset(&tree, 0, sizeof(tree));

  tree.nodes = malloc(OCTREE_MAX_NODES * sizeof(*tree.nodes));
  if (tree.nodes == NULL)
    return result_OOM;

  tree.root = octree_new_node(&tree, 0);

  row = src->base;
  for (y = 0; y < src->size.h; y++)
  {
    px = (const pixelfmt_rgbx8888_t *) row;
    for (x = 0; x < src->size.w; x++)
    {
      octree_insert(&tree,
                    PIXELFMT_Rxxx8888(px[x]),
                    PIXELFMT_xGxx8888(px[x]),
                    PIXELFMT_xxBx8888(px[x]));

      /* bound the working set; the partial reduction below gets the
       * final count exact */
      while (tree.leaves > octree_MAX_COLOURS)
        octree_reduce(&tree, octree_MAX_COLOURS, 0);
    }
    row += src->rowbytes;
  }

  while (tree.leaves > ncolours)
    octree_reduce(&tree, ncolours, 1);

  octree_collect(tree.root, palette, nfound);

  free(tree.nodes);

  return result_OK;
}
