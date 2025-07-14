#ifndef SC_BLACK_FRAME_DETECTOR_H
#define SC_BLACK_FRAME_DETECTOR_H

#include "common.h"
#include "trait/frame_sink.h"

struct sc_black_frame_detector {
    struct sc_frame_sink frame_sink;
    int black_frame_count;
};

void
sc_black_frame_detector_init(struct sc_black_frame_detector *bfd);

#endif
