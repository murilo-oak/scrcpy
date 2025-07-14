#ifndef SC_BLACK_FRAME_DETECTOR_H
#define SC_BLACK_FRAME_DETECTOR_H

#include "common.h"
#include "trait/frame_sink.h"

struct sc_black_frame_detector {
    struct sc_frame_sink frame_sink;
    int black_frame_count;
    int total_frames_processed;
    int recent_black_episodes;  // Count of recent black frame episodes
    int frames_since_last_black_episode;
};

void
sc_black_frame_detector_init(struct sc_black_frame_detector *bfd);

#endif
