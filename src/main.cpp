#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <sstream>
#include <string>

extern "C" {
#include "vc.h"
}

void vc_timer(void) {
    static bool running = false;
    static std::chrono::steady_clock::time_point previousTime =
        std::chrono::steady_clock::now();

    if (!running) {
        running = true;
    } else {
        std::chrono::steady_clock::time_point currentTime =
            std::chrono::steady_clock::now();
        std::chrono::steady_clock::duration elapsedTime =
            currentTime - previousTime;

        std::chrono::duration<double> time_span =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                elapsedTime);
        double nseconds = time_span.count();

        std::cout << "Elapsed time: " << nseconds << " seconds" << std::endl;
        std::cout << "Press Enter to continue...\n";
        std::cin.get();
    }
}

#define MAX_DETECTIONS 256

typedef struct {
    int x, y, width, height;
    int xc, yc;
    int area_px;
    int perimeter_px;
    int track_id;
} Detection;

typedef struct {
    int id;
    int xc, yc;
    int missed;
    double area_mm2;
    double perimeter_mm;
    double diameter_mm;
    int size_class;
    int printed;
} Track;

static Track *tracks = NULL;
static int ntracks = 0;
static int tracks_cap = 0;
static int next_track_id = 1;

static void ensure_track_capacity(int needed) {
    if (needed <= tracks_cap)
        return;

    int new_cap = (tracks_cap == 0) ? 16 : tracks_cap * 2;
    while (new_cap < needed)
        new_cap *= 2;

    Track *new_tracks = (Track *)realloc(tracks, new_cap * sizeof(Track));
    if (new_tracks == NULL)
        return;

    tracks = new_tracks;
    tracks_cap = new_cap;
}

static int size_class_from_diameter(double diameter_mm) {
    if (diameter_mm >= 100.0)
        return 0;
    if (diameter_mm >= 87.0)
        return 1;
    if (diameter_mm >= 84.0)
        return 2;
    if (diameter_mm >= 81.0)
        return 3;
    if (diameter_mm >= 77.0)
        return 4;
    if (diameter_mm >= 73.0)
        return 5;
    if (diameter_mm >= 70.0)
        return 6;
    if (diameter_mm >= 67.0)
        return 7;
    if (diameter_mm >= 64.0)
        return 8;
    if (diameter_mm >= 62.0)
        return 9;
    if (diameter_mm >= 60.0)
        return 10;
    if (diameter_mm >= 58.0)
        return 11;
    if (diameter_mm >= 56.0)
        return 12;
    return 13;
}

static void swap_bgr_rgb(const unsigned char *src, int src_step,
                         unsigned char *dst, int dst_step, int width,
                         int height) {
    if (src == NULL || dst == NULL || width <= 0 || height <= 0)
        return;

    if (src == dst && src_step == dst_step) {
        for (int y = 0; y < height; y++) {
            unsigned char *row = dst + y * dst_step;
            for (int x = 0; x < width; x++) {
                int idx = x * 3;
                unsigned char tmp = row[idx];
                row[idx] = row[idx + 2];
                row[idx + 2] = tmp;
            }
        }
        return;
    }

    for (int y = 0; y < height; y++) {
        const unsigned char *src_row = src + y * src_step;
        unsigned char *dst_row = dst + y * dst_step;
        for (int x = 0; x < width; x++) {
            int idx = x * 3;
            dst_row[idx] = src_row[idx + 2];
            dst_row[idx + 1] = src_row[idx + 1];
            dst_row[idx + 2] = src_row[idx];
        }
    }
}

static void draw_circle_bgr(unsigned char *data, int width, int height,
                            int step, int cx, int cy, int radius,
                            unsigned char b, unsigned char g, unsigned char r,
                            int thickness) {
    if (data == NULL || width <= 0 || height <= 0 || radius <= 0)
        return;

    int r2 = radius * radius;
    int inner_r = (thickness >= 0) ? (radius - thickness) : 0;
    if (inner_r < 0)
        inner_r = 0;
    int inner2 = inner_r * inner_r;

    for (int y = cy - radius; y <= cy + radius; y++) {
        if (y < 0 || y >= height)
            continue;
        int dy = y - cy;
        int dy2 = dy * dy;
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (x < 0 || x >= width)
                continue;
            int dx = x - cx;
            int d2 = dx * dx + dy2;
            if (d2 > r2)
                continue;
            if (thickness >= 0 && d2 < inner2)
                continue;
            unsigned char *pixel = data + y * step + x * 3;
            pixel[0] = b;
            pixel[1] = g;
            pixel[2] = r;
        }
    }
}

static void assign_tracks(Detection *dets, int ndets, double max_match_dist,
                          int max_missed, double mm_per_px) {
    int old_ntracks = ntracks;
    int *track_used = NULL;

    if (old_ntracks > 0) {
        track_used = (int *)calloc(old_ntracks, sizeof(int));
        if (track_used == NULL) {
            old_ntracks = 0;
        }
    }

    double max_dist_sq = max_match_dist * max_match_dist;

    for (int i = 0; i < ndets; i++) {
        int best_idx = -1;
        double best_dist_sq = max_dist_sq;

        for (int t = 0; t < old_ntracks; t++) {
            if (track_used != NULL && track_used[t])
                continue;
            double dx = (double)(dets[i].xc - tracks[t].xc);
            double dy = (double)(dets[i].yc - tracks[t].yc);
            double dist_sq = dx * dx + dy * dy;
            if (dist_sq < best_dist_sq) {
                best_dist_sq = dist_sq;
                best_idx = t;
            }
        }

        if (best_idx >= 0) {
            tracks[best_idx].xc = dets[i].xc;
            tracks[best_idx].yc = dets[i].yc;
            tracks[best_idx].missed = 0;
            if (track_used != NULL)
                track_used[best_idx] = 1;
            dets[i].track_id = tracks[best_idx].id;
        } else {
            dets[i].track_id = 0;
        }
    }

    for (int i = 0; i < ndets; i++) {
        if (dets[i].track_id == 0) {
            ensure_track_capacity(ntracks + 1);
            if (ntracks >= tracks_cap)
                continue;
            tracks[ntracks].id = next_track_id++;
            tracks[ntracks].xc = dets[i].xc;
            tracks[ntracks].yc = dets[i].yc;
            tracks[ntracks].missed = 0;
            double area_mm2 = (double)dets[i].area_px * mm_per_px * mm_per_px;
            tracks[ntracks].area_mm2 = area_mm2;
            tracks[ntracks].perimeter_mm =
                (double)dets[i].perimeter_px * mm_per_px;
            tracks[ntracks].diameter_mm =
                (area_mm2 > 0.0) ? (2.0 * std::sqrt(area_mm2 / std::acos(-1.0)))
                                 : 0.0;
            tracks[ntracks].size_class =
                size_class_from_diameter(tracks[ntracks].diameter_mm);
            tracks[ntracks].printed = 0;
            dets[i].track_id = tracks[ntracks].id;
            ntracks++;
        }
    }

    if (track_used != NULL) {
        for (int t = 0; t < old_ntracks; t++) {
            if (!track_used[t]) {
                tracks[t].missed++;
            }
        }
        free(track_used);
    } else {
        for (int t = 0; t < old_ntracks; t++) {
            tracks[t].missed++;
        }
    }

    int write = 0;
    for (int t = 0; t < ntracks; t++) {
        if (tracks[t].missed <= max_missed) {
            if (write != t) {
                tracks[write] = tracks[t];
            }
            write++;
        }
    }
    ntracks = write;
}

static Track *find_track_by_id(int id) {
    for (int i = 0; i < ntracks; i++) {
        if (tracks[i].id == id) {
            return &tracks[i];
        }
    }
    return NULL;
}

int main(void) {
    char videofile[20] = "../input/video.avi";
    cv::VideoCapture capture;
    struct {
        int width, height;
        int ntotalframes;
        int fps;
        int nframe;
    } video;
    std::string str;
    int key = 0;

    int hmin = 10, hmax = 35;
    int smin = 35, smax = 100;
    int vmin = 10, vmax = 100;

    int kernel = 3;

    const double mm_per_px = 55.0 / 280.0;

    const double area_min_mm2 = 2200.0;
    const int area_min_px = (int)(area_min_mm2 / (mm_per_px * mm_per_px) + 0.5);

    const double max_match_dist = 50.0;
    const int max_missed = 5;

    capture.open(videofile);

    if (!capture.isOpened()) {
        std::cerr << "Failed to open the video file!\n";
        return 1;
    }

    video.ntotalframes = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);
    video.fps = (int)capture.get(cv::CAP_PROP_FPS);
    video.width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
    video.height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    cv::namedWindow("CV - Video", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("CV - Segmentation", cv::WINDOW_AUTOSIZE);

    vc_timer();

    cv::Mat frame;
    while (key != 'q') {
        capture.read(frame);

        if (frame.empty())
            break;

        video.nframe = (int)capture.get(cv::CAP_PROP_POS_FRAMES);

        cv::Mat frame_rgb(video.height, video.width, CV_8UC3);
        swap_bgr_rgb(frame.data, (int)frame.step, frame_rgb.data,
                     (int)frame_rgb.step, video.width, video.height);

        IVC *image = vc_image_new(video.width, video.height, 3, 255);
        IVC *image_hsv = vc_image_new(video.width, video.height, 3, 255);
        IVC *image_seg = vc_image_new(video.width, video.height, 1, 255);
        IVC *image_open = vc_image_new(video.width, video.height, 1, 255);
        IVC *image_labels = vc_image_new(video.width, video.height, 1, 255);

        memcpy(image->data, frame_rgb.data, video.width * video.height * 3);

        vc_rgb_to_hsv(image, image_hsv);

        vc_hsv_segmentation(image_hsv, image_seg, hmin, hmax, smin, smax, vmin,
                            vmax);

        vc_binary_open(image_seg, image_open, kernel);

        int nlabels = 0;
        OVC *blobs =
            vc_binary_blob_labelling(image_open, image_labels, &nlabels);
        bool label_on_border[256] = {false};
        if (blobs != NULL) {
            vc_binary_blob_info(image_labels, blobs, nlabels);

            unsigned char *data_open = (unsigned char *)image_open->data;
            unsigned char *data_labels = (unsigned char *)image_labels->data;
            int width = image_open->width;
            int height = image_open->height;
            int bytesperline = image_open->bytesperline;

            auto mark_label = [&](int x, int y) {
                if (x < 0 || x >= width || y < 0 || y >= height)
                    return;
                unsigned char label = data_labels[y * bytesperline + x];
                if (label != 0)
                    label_on_border[label] = true;
            };

            if (width >= 2 && height >= 2) {
                for (int x = 0; x < width; x++) {
                    if (data_open[0 * bytesperline + x] != 0) {
                        mark_label(x - 1, 1);
                        mark_label(x, 1);
                        mark_label(x + 1, 1);
                    }
                }
                for (int x = 0; x < width; x++) {
                    if (data_open[(height - 1) * bytesperline + x] != 0) {
                        mark_label(x - 1, height - 2);
                        mark_label(x, height - 2);
                        mark_label(x + 1, height - 2);
                    }
                }
                for (int y = 0; y < height; y++) {
                    if (data_open[y * bytesperline + 0] != 0) {
                        mark_label(1, y - 1);
                        mark_label(1, y);
                        mark_label(1, y + 1);
                    }
                }
                for (int y = 0; y < height; y++) {
                    if (data_open[y * bytesperline + (width - 1)] != 0) {
                        mark_label(width - 2, y - 1);
                        mark_label(width - 2, y);
                        mark_label(width - 2, y + 1);
                    }
                }
            }
        }

        cv::Mat frame_aux(video.height, video.width, CV_8UC1);
        memcpy(frame_aux.data, image_open->data,
               video.width * video.height * 1);

        memcpy(frame_rgb.data, image->data, video.width * video.height * 3);

        swap_bgr_rgb(frame_rgb.data, (int)frame_rgb.step, frame.data,
                     (int)frame.step, video.width, video.height);

        Detection detections[MAX_DETECTIONS];
        int ndets = 0;
        if (blobs != NULL) {
            for (int i = 0; i < nlabels; i++) {
                if (blobs[i].area < area_min_px) {
                    continue;
                }
                if (label_on_border[blobs[i].label]) {
                    continue;
                }
                if (ndets < MAX_DETECTIONS) {
                    Detection det;
                    det.x = blobs[i].x;
                    det.y = blobs[i].y;
                    det.width = blobs[i].width;
                    det.height = blobs[i].height;
                    det.xc = blobs[i].xc;
                    det.yc = blobs[i].yc;
                    det.area_px = blobs[i].area;
                    det.perimeter_px = blobs[i].perimeter;
                    det.track_id = 0;
                    detections[ndets] = det;
                    ndets++;
                }
            }
        }

        assign_tracks(detections, ndets, max_match_dist, max_missed, mm_per_px);

        for (int i = 0; i < ndets; i++) {
            Detection *det = &detections[i];
            Track *tr = find_track_by_id(det->track_id);
            double area_mm2 = 0.0;
            double perimeter_mm = 0.0;
            double diameter_mm = 0.0;
            int size_class = 0;
            if (tr != NULL) {
                area_mm2 = tr->area_mm2;
                perimeter_mm = tr->perimeter_mm;
                diameter_mm = tr->diameter_mm;
                size_class = tr->size_class;
            } else {
                area_mm2 = (double)det->area_px * mm_per_px * mm_per_px;
                perimeter_mm = (double)det->perimeter_px * mm_per_px;
                diameter_mm =
                    (area_mm2 > 0.0)
                        ? (2.0 * std::sqrt(area_mm2 / std::acos(-1.0)))
                        : 0.0;
                size_class = size_class_from_diameter(diameter_mm);
            }
            cv::rectangle(frame, cv::Point(det->x, det->y),
                          cv::Point(det->x + det->width, det->y + det->height),
                          cv::Scalar(0, 255, 0), 2);
            draw_circle_bgr(frame.data, frame.cols, frame.rows, (int)frame.step,
                            det->xc, det->yc, 3, 0, 0, 255, -1);
            std::ostringstream info_stream;
            info_stream << std::fixed << std::setprecision(1)
                        << "ID:" << det->track_id << " A:" << area_mm2 << "mm2 "
                        << "P:" << perimeter_mm << "mm "
                        << "D:" << diameter_mm << "mm "
                        << "S:" << size_class;
            std::string blob_info = info_stream.str();
            int text_y = det->y - 5;
            if (text_y < 10) {
                text_y = det->y + 15;
            }
            cv::putText(frame, blob_info, cv::Point(det->x, text_y),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);

            if (tr != NULL && tr->printed == 0) {
                std::ostringstream term_stream;
                term_stream << std::fixed << std::setprecision(1)
                            << "ID:" << det->track_id << " A:" << area_mm2
                            << "mm2 P:" << perimeter_mm
                            << "mm D:" << diameter_mm << "mm S:" << size_class
                            << " bbox: (" << det->x << "," << det->y << ","
                            << det->width << "," << det->height << ") center: ("
                            << det->xc << "," << det->yc << ")";
                std::cout << term_stream.str() << "\n";
                tr->printed = 1;
            }
        }

        if (blobs != NULL) {
            free(blobs);
        }

        vc_image_free(image);
        vc_image_free(image_hsv);
        vc_image_free(image_seg);
        vc_image_free(image_open);
        vc_image_free(image_labels);

        str = std::string("RESOLUTION: ")
                  .append(std::to_string(video.width))
                  .append("x")
                  .append(std::to_string(video.height));
        cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(255, 255, 255), 1);
        str = std::string("TOTAL FRAMES: ")
                  .append(std::to_string(video.ntotalframes));
        cv::putText(frame, str, cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(255, 255, 255), 1);
        str = std::string("FRAME RATE: ").append(std::to_string(video.fps));
        cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(255, 255, 255), 1);
        str =
            std::string("FRAME NUMBER: ").append(std::to_string(video.nframe));
        cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(255, 255, 255), 1);
        str = std::string("TOTAL ORANGES: ")
                  .append(std::to_string(next_track_id - 1));
        cv::putText(frame, str, cv::Point(20, 125), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 125), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(255, 255, 255), 1);
        str = std::string("ORANGES IN FRAME: ").append(std::to_string(ndets));
        cv::putText(frame, str, cv::Point(20, 150), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 150), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(255, 255, 255), 1);

        cv::imshow("CV - Video", frame);
        cv::imshow("CV - Segmentation", frame_aux);

        key = cv::waitKey(1);
    }

    vc_timer();

    cv::destroyWindow("CV - Video");
    cv::destroyWindow("CV - Segmentation");

    capture.release();

    if (tracks != NULL) {
        free(tracks);
        tracks = NULL;
    }
    ntracks = 0;
    tracks_cap = 0;
    next_track_id = 1;

    return 0;
}
