/* test/all-tests.h */

#ifndef TESTS_ALL_TESTS_H
#define TESTS_ALL_TESTS_H

#include "base/result.h"

typedef result_t (testfn_t)(const char *resources);

/* Frames an interactive (SDL) test runs before quitting by itself; 0 waits
 * for the user to quit. Set by DPTLibTest's -autoquit option. */
extern int test_autoquit;

/* datastruct */
extern testfn_t atom_test,
                bitarr_test,
                bitfifo_test,
                bitvec_test,
                cache_test,
                hash_test,
                list_test,
                ntree_test,
                vector_test;

/* database */
extern testfn_t pickle_test,
                tagdb_test;

/* framebuf */
extern testfn_t bitmap_load_test,
                bitmap_resize_test,
                bitmap_rle_test, bitmap_convert_test, span_test,
                bmfont_test,
                bmfontcache_test,
                bmfontfamily_test,
                colourspace_test,
                composite_test,
                curve_test,
                dither_test,
                effects_test,
                octree_test,
                pattern_test,
                pixelmap_test,
                screen_test,
                tonemap_test;

/* geom */
extern testfn_t box_test,
                layout_test,
                packer_test,
                region_test,
                stack_test,
                stage_test;

/* io */
extern testfn_t dirlist_test,
                filetype_test,
                path_test,
                stream_test;

/* text */
extern testfn_t txtfmt_test,
                utf8_test;

/* utils */
extern testfn_t array_test,
                barith_test,
                bsearch_test,
                bytesex_test,
                easing_test,
                pack_test;

/* wuss */
extern testfn_t gridview_bench_test,
                paintingdoc_test,
                wuss_menu_test,
                wuss_pointer_test,
                wuss_message_test,
                wuss_drag_test,
                wuss_task_test,
                wuss_content_test,
                wuss_stack_test,
                wuss_lifecycle_test,
                wuss_invalidate_test,
                wuss_placement_test,
                wuss_focus_test,
                wuss_hidden_test,
                wuss_move_test,
                wuss_resize_test,
                wuss_scroll_test,
                wuss_toggle_test,
                wuss_furniture_test,
                wuss_iconpool_test,
                wuss_icon_test,
                wuss_icon_draw_test,
                wuss_writable_test,
                wuss_slider_test,
                wuss_hover_test,
                wuss_click_test,
                wuss_set_text_test,
                wuss_iconbar_test,
                wuss_menu_desc_test,
                wuss_gadget_test,
                wuss_component_test,
                dcs_test,
                error_test,
                colourpicker_test,
                wuss_test;

#endif /* TESTS_ALL_TESTS_H */
