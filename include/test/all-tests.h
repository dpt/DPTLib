/* all-tests.h */

#ifndef TESTS_ALL_TESTS_H
#define TESTS_ALL_TESTS_H

#include "base/result.h"

typedef result_t (testfn_t)(const char *resources);

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
extern testfn_t bmfont_test,
                composite_test,
                curve_test;

/* geom */
extern testfn_t box_test,
                layout_test,
                packer_test;

/* io */
extern testfn_t stream_test;

/* utils */
extern testfn_t array_test,
                bsearch_test;

#endif /* TESTS_ALL_TESTS_H */
