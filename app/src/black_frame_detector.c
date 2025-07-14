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
    // A more robust heuristic: check if the average luminance is very low.
    // Support multiple pixel formats and use a more reasonable threshold.

    if (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_NV12 || frame->format == AV_PIX_FMT_NV21) {
        // For YUV formats, check the Y (luminance) plane
        long long luma_sum = 0;
        int total_pixels = frame->width * frame->height;
        
        for (int i = 0; i < frame->height; i++) {
            for (int j = 0; j < frame->width; j++) {
                luma_sum += frame->data[0][i * frame->linesize[0] + j];
            }
        }
        
        double avg_luma = (double)luma_sum / total_pixels;
        LOGD("Frame luminance: %.2f (threshold: 30)", avg_luma);
        return avg_luma < 30; // Increased threshold for better detection
    } else if (frame->format == AV_PIX_FMT_RGB24 || frame->format == AV_PIX_FMT_BGR24) {
        // For RGB formats, check average brightness across all channels
        long long total_sum = 0;
        int total_samples = frame->width * frame->height * 3;
        
        for (int i = 0; i < frame->height; i++) {
            for (int j = 0; j < frame->width * 3; j++) {
                total_sum += frame->data[0][i * frame->linesize[0] + j];
            }
        }
        
        double avg_brightness = (double)total_sum / total_samples;
        LOGD("Frame brightness: %.2f (threshold: 30)", avg_brightness);
        return avg_brightness < 30;
    }
    
    // For unsupported formats, log and return false
    LOGD("Unsupported pixel format for black frame detection: %d", frame->format);
    return false;
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

    bfd->total_frames_processed++;
    bfd->frames_since_last_black_episode++;
    
    LOGD("Processing frame %d: %dx%d, format: %d", 
         bfd->total_frames_processed, frame->width, frame->height, frame->format);
    
    if (is_black_frame(frame)) {
        bfd->black_frame_count++;
        LOGI("Black frame detected! Count: %d in current episode", bfd->black_frame_count);
        
        // If we see too many consecutive black frames (>10), it's likely screen off, not blinking
        if (bfd->black_frame_count > 10) {
            LOGD("Too many consecutive black frames (%d), likely screen off - ignoring", bfd->black_frame_count);
            bfd->black_frame_count = 0; // Reset to avoid overflow
            bfd->recent_black_episodes = 0; // Reset episode tracking
            return true;
        }
    } else {
        if (bfd->black_frame_count > 0) {
            LOGD("Normal frame detected, had %d black frames in this episode", bfd->black_frame_count);
            
            // If we had a short burst of black frames (2-5), count it as a blinking episode
            if (bfd->black_frame_count >= 2 && bfd->black_frame_count <= 5) {
                bfd->recent_black_episodes++;
                bfd->frames_since_last_black_episode = 0;
                LOGI("Blinking episode detected! Episode count: %d", bfd->recent_black_episodes);
                
                // If we see multiple blinking episodes in a short time, trigger reset
                if (bfd->recent_black_episodes >= 3) {
                    LOGI("Multiple blinking episodes detected (%d), requesting video reset", bfd->recent_black_episodes);
                    SDL_Event event;
                    event.type = SC_EVENT_RESET_VIDEO;
                    SDL_PushEvent(&event);
                    bfd->recent_black_episodes = 0; // Reset after triggering
                }
            }
            
            bfd->black_frame_count = 0;
        }
    }
    
    // Reset episode counter if too much time has passed without black episodes
    if (bfd->frames_since_last_black_episode > 300) { // ~10 seconds at 30fps
        if (bfd->recent_black_episodes > 0) {
            LOGD("Resetting black episode count due to timeout");
        }
        bfd->recent_black_episodes = 0;
        bfd->frames_since_last_black_episode = 0;
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
    bfd->total_frames_processed = 0;
    bfd->recent_black_episodes = 0;
    bfd->frames_since_last_black_episode = 0;
    bfd->frame_sink.ops = &sc_black_frame_detector_sink_ops;
}
