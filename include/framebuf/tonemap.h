/* framebuf/tonemap.h -- per-channel tone maps */

/**
 * \file tonemap.h
 *
 * A tone map holds one 256-entry correction table for each of the red,
 * green, blue and (optionally) alpha channels. Each table is derived from a
 * \ref tonemap_spec_t: gamma, brightness, contrast, midpoint, bias and gain,
 * plus optional inversion and reflection. Tables are built lazily and cached
 * until the next change.
 */

#ifndef FRAMEBUF_TONEMAP_H
#define FRAMEBUF_TONEMAP_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/* ----------------------------------------------------------------------- */

/** An opaque tone map. */
typedef struct tonemap tonemap_t;

/** A 256-entry channel lookup table: input level to output level. */
typedef struct tonemap_table
{
  uint8_t level[256];
}
tonemap_table_t;

/** Bitmask selecting channels of a tone map. */
typedef unsigned int tonemap_channels_t;

#define tonemap_CHANNEL_RED   (1u << 0)
#define tonemap_CHANNEL_GREEN (1u << 1)
#define tonemap_CHANNEL_BLUE  (1u << 2)
#define tonemap_CHANNEL_ALPHA (1u << 3)

#define tonemap_CHANNEL_RGB   (0x7u << 0)
#define tonemap_CHANNEL_RGBA  (0xfu << 0)

/** Modifiers applied after the table has been computed. */
typedef unsigned int tonemap_flags_t;

/** Swap the two halves of the table. */
#define tonemap_FLAG_INVERT   (1u << 0)

/** Mirror the lower half of the table onto the upper. */
#define tonemap_FLAG_REFLECT  (1u << 1)

/**
 * Parameters for one channel. Typical ranges:
 *
 * - gamma      10..290 (100 is neutral) - brightness  0..200 (100 is
 * neutral) - contrast    0..200 (100 is neutral) - midd        0..100 (50 is
 * neutral) - bias        0..100 (50 is neutral) - gain        0..100 (50 is
 * neutral)
 */
typedef struct tonemap_spec
{
  tonemap_flags_t flags;
  int             gamma;
  int             brightness;
  int             contrast;
  int             midd;
  int             bias;
  int             gain;
}
tonemap_spec_t;

/* ----------------------------------------------------------------------- */

/**
 * Create a tone map with neutral red, green and blue tables.
 *
 * \return The new tone map, or NULL on allocation failure.
 */
tonemap_t *tonemap_create(void);

/**
 * Destroy a tone map.
 *
 * \param[in] map Tone map to destroy, or NULL.
 */
void tonemap_destroy(tonemap_t *map);

/**
 * Reset every channel of a tone map to neutral settings.
 *
 * \param[in] map Tone map to reset.
 */
void tonemap_reset(tonemap_t *map);

/**
 * Duplicate a tone map.
 *
 * \param[in] map Tone map to copy.
 * \return The copy, or NULL on allocation failure.
 */
tonemap_t *tonemap_copy(const tonemap_t *map);

/**
 * Apply the given parameters to the selected channels.
 *
 * Selecting the alpha channel for the first time enables it.
 *
 * \param[in] map      Tone map to modify.
 * \param[in] channels Channels to change.
 * \param[in] spec     New parameters.
 */
void tonemap_set(tonemap_t            *map,
                 tonemap_channels_t    channels,
                 const tonemap_spec_t *spec);

/**
 * Read back the parameters of a single channel.
 *
 * \param[in]  map      Tone map to query.
 * \param[in]  channels One channel; \ref tonemap_CHANNEL_RGB reads red.
 * \param[out] spec     Receives the parameters.
 */
void tonemap_get_values(const tonemap_t   *map,
                        tonemap_channels_t channels,
                        tonemap_spec_t    *spec);

/**
 * Fetch the correction tables, rebuilding them if stale.
 *
 * The tables remain valid until the tone map is next changed or destroyed. A
 * tone map without an alpha channel yields a linear alpha table.
 *
 * \param[in]  map   Tone map to query.
 * \param[out] red   Receives the red table, or NULL to skip.
 * \param[out] green Receives the green table, or NULL to skip.
 * \param[out] blue  Receives the blue table, or NULL to skip.
 * \param[out] alpha Receives the alpha table, or NULL to skip.
 */
void tonemap_get_tables(tonemap_t              *map,
                        const tonemap_table_t **red,
                        const tonemap_table_t **green,
                        const tonemap_table_t **blue,
                        const tonemap_table_t **alpha);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* FRAMEBUF_TONEMAP_H */
