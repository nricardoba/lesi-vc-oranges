#include <chrono>
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

        // Tempo em segundos.
        std::chrono::duration<double> time_span =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                elapsedTime);
        double nseconds = time_span.count();

        std::cout << "Tempo decorrido: " << nseconds << "segundos" << std::endl;
        std::cout << "Pressione qualquer tecla para continuar...\n";
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

    // Match detections to existing tracks
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

    // Create new tracks for unmatched detections
    for (int i = 0; i < ndets; i++) {
        if (dets[i].track_id == 0) {
            ensure_track_capacity(ntracks + 1);
            if (ntracks >= tracks_cap)
                continue;
            tracks[ntracks].id = next_track_id++;
            tracks[ntracks].xc = dets[i].xc;
            tracks[ntracks].yc = dets[i].yc;
            tracks[ntracks].missed = 0;
            tracks[ntracks].area_mm2 =
                (double)dets[i].area_px * mm_per_px * mm_per_px;
            tracks[ntracks].perimeter_mm =
                (double)dets[i].perimeter_px * mm_per_px;
            dets[i].track_id = tracks[ntracks].id;
            ntracks++;
        }
    }

    // Update missed counter
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

    // Remove tracks that have been missing for too long
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
    // Vídeo
    char videofile[20] = "../input/video.avi";
    cv::VideoCapture capture;
    struct {
        int width, height;
        int ntotalframes;
        int fps;
        int nframe;
    } video;
    // Outros
    std::string str;
    int key = 0;

    // Parâmetros de segmentação HSV (ajustar conforme necessário)
    int hmin = 10, hmax = 35;
    int smin = 35, smax = 100;
    int vmin = 10, vmax = 100;

    // Parâmetros de fecho/abertura binária
    int kernel = 3;

    // Escala: 280 píxeis = 55 mm
    const double mm_per_px = 55.0 / 280.0;

    // Filtro por área mínima do blob em mm² (ajustar conforme necessário)
    const double area_min_mm2 = 3000.0;
    const int area_min_px = (int)(area_min_mm2 / (mm_per_px * mm_per_px) + 0.5);

    // Parâmetros de tracking
    const double max_match_dist = 50.0; // em píxeis
    const int max_missed = 5;

    /* Leitura de vídeo de um ficheiro */
    /* NOTA IMPORTANTE:
    O ficheiro video.avi deverá estar localizado no mesmo directório que o
    ficheiro de código fonte.
    */
    capture.open(videofile);

    /* Em alternativa, abrir captura de vídeo pela Webcam #0 */
    // capture.open(0, cv::CAP_DSHOW); // Pode-se utilizar apenas
    // capture.open(0);

    /* Verifica se foi possível abrir o ficheiro de vídeo */
    if (!capture.isOpened()) {
        std::cerr << "Erro ao abrir o ficheiro de vídeo!\n";
        return 1;
    }

    /* Número total de frames no vídeo */
    video.ntotalframes = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);
    /* Frame rate do vídeo */
    video.fps = (int)capture.get(cv::CAP_PROP_FPS);
    /* Resolução do vídeo */
    video.width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
    video.height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    /* Cria uma janela para exibir o vídeo */
    cv::namedWindow("VC - VIDEO", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("VC - VIDEO AUX", cv::WINDOW_AUTOSIZE);

    /* Inicia o timer */
    vc_timer();

    cv::Mat frame;
    while (key != 'q') {
        /* Leitura de uma frame do vídeo */
        capture.read(frame);

        /* Verifica se conseguiu ler a frame */
        if (frame.empty())
            break;

        /* Número da frame a processar */
        video.nframe = (int)capture.get(cv::CAP_PROP_POS_FRAMES);

        // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        // Faça o seu código aqui...
        // Converte a frame de BGR (OpenCV) para RGB
        cv::Mat frame_rgb;
        cv::cvtColor(frame, frame_rgb, cv::COLOR_BGR2RGB);

        // Cria imagens IVC
        IVC *image = vc_image_new(video.width, video.height, 3, 255);
        IVC *image_hsv = vc_image_new(video.width, video.height, 3, 255);
        IVC *image_seg = vc_image_new(video.width, video.height, 1, 255);
        IVC *image_close = vc_image_new(video.width, video.height, 1, 255);
        IVC *image_labels = vc_image_new(video.width, video.height, 1, 255);

        // Copia dados de imagem da estrutura cv::Mat para uma estrutura IVC
        memcpy(image->data, frame_rgb.data, video.width * video.height * 3);

        // Converte RGB para HSV (cópia)
        vc_rgb_to_hsv(image, image_hsv);

        // Segmentação em HSV
        vc_hsv_segmentation(image_hsv, image_seg, hmin, hmax, smin, smax, vmin,
                            vmax);

        // Fecho binário (dilatação seguida de erosão)
        vc_binary_close(image_seg, image_close, kernel);

        // Etiquetagem de blobs após o fecho
        int nlabels = 0;
        OVC *blobs =
            vc_binary_blob_labelling(image_close, image_labels, &nlabels);
        bool label_on_border[256] = {false};
        if (blobs != NULL) {
            vc_binary_blob_info(image_labels, blobs, nlabels);

            // Marcar labels que tocam nas bordas (usando a imagem binária
            // original)
            unsigned char *data_close = (unsigned char *)image_close->data;
            unsigned char *data_labels = (unsigned char *)image_labels->data;
            int width = image_close->width;
            int height = image_close->height;
            int bytesperline = image_close->bytesperline;

            auto mark_label = [&](int x, int y) {
                if (x < 0 || x >= width || y < 0 || y >= height)
                    return;
                unsigned char label = data_labels[y * bytesperline + x];
                if (label != 0)
                    label_on_border[label] = true;
            };

            if (width >= 2 && height >= 2) {
                // Borda superior (y = 0) -> vizinhança interna (y = 1)
                for (int x = 0; x < width; x++) {
                    if (data_close[0 * bytesperline + x] != 0) {
                        mark_label(x - 1, 1);
                        mark_label(x, 1);
                        mark_label(x + 1, 1);
                    }
                }
                // Borda inferior (y = height - 1) -> vizinhança interna (y =
                // height - 2)
                for (int x = 0; x < width; x++) {
                    if (data_close[(height - 1) * bytesperline + x] != 0) {
                        mark_label(x - 1, height - 2);
                        mark_label(x, height - 2);
                        mark_label(x + 1, height - 2);
                    }
                }
                // Borda esquerda (x = 0) -> vizinhança interna (x = 1)
                for (int y = 0; y < height; y++) {
                    if (data_close[y * bytesperline + 0] != 0) {
                        mark_label(1, y - 1);
                        mark_label(1, y);
                        mark_label(1, y + 1);
                    }
                }
                // Borda direita (x = width - 1) -> vizinhança interna (x =
                // width - 2)
                for (int y = 0; y < height; y++) {
                    if (data_close[y * bytesperline + (width - 1)] != 0) {
                        mark_label(width - 2, y - 1);
                        mark_label(width - 2, y);
                        mark_label(width - 2, y + 1);
                    }
                }
            }
        }

        // Copia etiquetagem (IVC) para cv::Mat para visualização
        cv::Mat frame_aux(video.height, video.width, CV_8UC1);
        memcpy(frame_aux.data, image_close->data,
               video.width * video.height * 1);

        // Copia dados de imagem da estrutura IVC para uma estrutura cv::Mat
        memcpy(frame_rgb.data, image->data, video.width * video.height * 3);

        // Converte de RGB de volta para BGR para visualização no OpenCV
        cv::cvtColor(frame_rgb, frame, cv::COLOR_RGB2BGR);

        // Preparar deteções válidas
        Detection detections[MAX_DETECTIONS];
        int ndets = 0;
        if (blobs != NULL) {
            for (int i = 0; i < nlabels; i++) {
                if (blobs[i].area < area_min_px) {
                    continue;
                }
                // Ignora blobs que tocam nas bordas da imagem
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

        // Associar deteções a tracks (persistência entre frames)
        assign_tracks(detections, ndets, max_match_dist, max_missed, mm_per_px);

        // Desenha bounding boxes, centros e info dos blobs com ID persistente
        for (int i = 0; i < ndets; i++) {
            Detection *det = &detections[i];
            Track *tr = find_track_by_id(det->track_id);
            double area_mm2 = 0.0;
            double perimeter_mm = 0.0;
            if (tr != NULL) {
                area_mm2 = tr->area_mm2;
                perimeter_mm = tr->perimeter_mm;
            } else {
                area_mm2 = (double)det->area_px * mm_per_px * mm_per_px;
                perimeter_mm = (double)det->perimeter_px * mm_per_px;
            }
            cv::rectangle(frame,
                          cv::Rect(det->x, det->y, det->width, det->height),
                          cv::Scalar(0, 255, 0), 2);
            cv::circle(frame, cv::Point(det->xc, det->yc), 3,
                       cv::Scalar(0, 0, 255), -1);
            std::ostringstream info_stream;
            info_stream << std::fixed << std::setprecision(1)
                        << "ID:" << det->track_id << " A:" << area_mm2 << "mm2 "
                        << "P:" << perimeter_mm << "mm";
            std::string blob_info = info_stream.str();
            int text_y = det->y - 5;
            if (text_y < 10) {
                text_y = det->y + 15;
            }
            cv::putText(frame, blob_info, cv::Point(det->x, text_y),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
        }

        if (blobs != NULL) {
            free(blobs);
        }

        // Liberta a memória das imagens IVC que haviam sido criadas
        vc_image_free(image);
        vc_image_free(image_hsv);
        vc_image_free(image_seg);
        vc_image_free(image_close);
        vc_image_free(image_labels);

        // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        /* Exemplo de inserção texto na frame */
        str = std::string("RESOLUCAO: ")
                  .append(std::to_string(video.width))
                  .append("x")
                  .append(std::to_string(video.height));
        cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(255, 255, 255), 1);
        str = std::string("TOTAL DE FRAMES: ")
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
        str = std::string("N. DA FRAME: ").append(std::to_string(video.nframe));
        cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(255, 255, 255), 1);
        str = std::string("TOTAL DE LARANJAS: ")
                  .append(std::to_string(next_track_id - 1));
        cv::putText(frame, str, cv::Point(20, 125), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 125), cv::FONT_HERSHEY_SIMPLEX,
                    1.0, cv::Scalar(255, 255, 255), 1);

        /* Define manualmente o tamanho das janelas (via redimensionamento da
         * imagem) */
        int window_width = 640;
        int window_height = 360;

        /* Exibe a frame */
        cv::Mat frame_display;
        cv::Mat frame_aux_display;
        cv::resize(frame, frame_display, cv::Size(window_width, window_height));
        cv::resize(frame_aux, frame_aux_display,
                   cv::Size(window_width, window_height), 0, 0,
                   cv::INTER_NEAREST);
        cv::imshow("VC - VIDEO", frame_display);
        cv::imshow("VC - VIDEO AUX", frame_aux_display);

        /* Sai da aplicação, se o utilizador premir a tecla 'q' */
        key = cv::waitKey(1);
    }

    /* Para o timer e exibe o tempo decorrido */
    vc_timer();

    /* Fecha a janela */
    cv::destroyWindow("VC - VIDEO");
    cv::destroyWindow("VC - VIDEO AUX");

    /* Fecha o ficheiro de vídeo */
    capture.release();

    /* Limpa tracks */
    if (tracks != NULL) {
        free(tracks);
        tracks = NULL;
    }
    ntracks = 0;
    tracks_cap = 0;
    next_track_id = 1;

    return 0;
}
