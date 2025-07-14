#include "black_frame_detector.h"

#include "decoder.h"
#include "events.h"
#include "util/log.h"
#include "util/memory.h"

#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <SDL2/SDL.h>
#include <stddef.h>

static bool
is_black_frame(const AVFrame *frame) {
    // A simple heuristic: check if the average luminance is very low.
    // This is a basic implementation and might need to be adjusted
    // for different video sources and conditions.

    if (frame->format != AV_PIX_FMT_YUV420P) {
        // This detector only supports YUV420P for now
        return false;
    }

    long long luma_sum = 0;
    for (int i = 0; i < frame->height; i++) {
        for (int j = 0; j < frame->width; j++) {
            luma_sum += frame->data[0][i * frame->linesize[0] + j];
        }
    }

    double avg_luma = (double)luma_sum / (frame->width * frame->height);
    return avg_luma < 10; // Threshold for black frame
}

static bool
sc_black_frame_detector_sink_open(struct sc_frame_sink *sink,
                                  const struct sc_video_stream_params *params) {
    (void) sink;
    (void) params;
    return true;
}

static void
sc_black_frame_detector_sink_close(struct sc_frame_sink *sink) {
    (void) sink;
}

static bool
sc_black_frame_detector_sink_push(struct sc_frame_sink *sink,
                                  const AVFrame *frame) {
    // Manual container_of calculation instead of using CONTAINER_OF macro
    struct sc_black_frame_detector *bfd = 
        (struct sc_black_frame_detector *)((char *)sink - offsetof(struct sc_black_frame_detector, frame_sink));

    if (is_black_frame(frame)) {
        bfd->black_frame_count++;
        if (bfd->black_frame_count >= 1) { // Trigger after 3 consecutive black frames
            LOGI("Black frame detected, requesting video reset");
            SDL_Event event;
            event.type = SC_EVENT_RESET_VIDEO;
            SDL_PushEvent(&event);
            bfd->black_frame_count = 0;
        }
    } else {
        bfd->black_frame_count = 0;
    }

    return true;
}

// Static instance of the ops structure instead of using FROZEN macro
static const struct sc_frame_sink_ops sc_black_frame_detector_sink_ops = {
    .open = sc_black_frame_detector_sink_open,
    .close = sc_black_frame_detector_sink_close,
    .push = sc_black_frame_detector_sink_push,
};

void
sc_black_frame_detector_init(struct sc_black_frame_detector *bfd) {
    bfd->black_frame_count = 0;
    bfd->frame_sink.ops = &sc_black_frame_detector_sink_ops;
}
