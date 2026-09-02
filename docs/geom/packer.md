# [DPTLib](https://github.com/dpt/DPTLib) > geom > packer

"packer" is a sub-library of DPTLib for automatic box packing and layout management. It provides a 2D bin packing solution that can efficiently place rectangular areas within a defined container while maintaining proper spacing and margins.

The packer is useful for GUI layout management, document layout systems, or any scenario requiring automatic positioning of rectangular elements.

- It supports four positioning strategies: top-left, top-right, bottom-left, and bottom-right.
- It maintains margin spacing between packed elements.
- It can track consumed areas and provide layout feedback.
- Its packing algorithm should be reasonably efficient for most use cases.

This is a reimplementation of a technique that I was introduced to by Paul Gardiner.

## Setup

#### Create a packer:

`packer_create()` to create a new packer with specified dimensions, returning a packer handle.

#### Set margins (optional):

`packer_set_margins()` to define spacing between packed elements.

## Measuring

Use `packer_next_width()` to query available space before placing elements:

```C
int packer_next_width(packer_t     *packer,
                      packer_loc_t  loc);
```

It requires a packer handle and a location strategy. It returns the width of the next available area in pixels.

Location strategies are:

- `packer_LOC_TOP_LEFT` - Pack from top-left corner
- `packer_LOC_TOP_RIGHT` - Pack from top-right corner
- `packer_LOC_BOTTOM_LEFT` - Pack from bottom-left corner
- `packer_LOC_BOTTOM_RIGHT` - Pack from bottom-right corner

## Placing Elements

Use `packer_place_by()` to automatically place elements:

```C
result_t packer_place_by(packer_t      *packer,
                         packer_loc_t   loc,
                         int            w,
                         int            h,
                         const box_t  **pos);
```

It requires a packer handle, location strategy, width and height of the element to place. It returns the actual position where the element was placed (optional

- pass `NULL` if not required).

For absolute positioning, use `packer_place_at()`:

```C
result_t packer_place_at(packer_t    *packer,
                         const box_t *area);
```

This places an element at a specific location, ignoring margins.

## Releasing Elements

Use `packer_release()` to hand a previously placed area back to the free pool:

```C
result_t packer_release(packer_t    *packer,
                        const box_t *area);
```

Pass the same box that `packer_place_at()` placed, or the area consumed by `packer_place_by()` (the returned footprint plus its gutter strip). The packer matches it against its record of placed boxes and rebuilds the free list from the container minus every box still live, so repeated place/release cycles reclaim the whole area exactly rather than fragmenting it into unusable slivers. `area` is clipped to the margins and copied.

A release whose box matches no recorded placement (for example after `packer_clear()`, which drops the placement record) simply adds the area to the free pool without a rebuild. `packer_get_consumed_area()` is not narrowed by a release.

## Layout Management

Use `packer_clear()` to move past consumed areas:

```C
result_t packer_clear(packer_t           *packer,
                      packer_cleardir_t   clear);
```

Clear directions are:

- `packer_CLEAR_LEFT` - Clear to left boundary
- `packer_CLEAR_RIGHT` - Clear to right boundary
- `packer_CLEAR_BOTH` - Clear to both boundaries

## Inspection

Use `packer_get_consumed_area()` to get the bounding box of all placed elements:

```C
const box_t *packer_get_consumed_area(const packer_t *packer);
```

Use `packer_map()` to iterate over all placed areas:

```C
result_t packer_map(packer_t        *packer,
                    packer_map_fn_t *fn,
                    void            *opaque);
```

The callback function signature is:

```C
typedef result_t (packer_map_fn_t)(const box_t *area, void *opaque);
```

## Error Handling

The packer can return these specific result codes:

- `result_PACKER_DIDNT_FIT` - Element couldn't be placed in available space
- `result_PACKER_EMPTY` - No elements have been placed yet, or a released area lay entirely outside the margins

## Cleanup

Use `packer_destroy()` to free resources when finished:

```C
void packer_destroy(packer_t *doomed);
```

## Limitations

- The packer uses a simple bin packing algorithm - more sophisticated algorithms might achieve better space utilisation.
- No support for element rotation to improve fit.
- No built-in support for element priorities or weights.
