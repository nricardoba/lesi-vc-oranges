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
    const double area_extra_max_mm2 = 4500.0;
    const double area_cat1_max_mm2 = 6500.0;
    const double area_cat2_max_mm2 = 8500.0;

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

    auto classify_blob = [&](double area_mm2) -> const char * {
        if (area_mm2 < area_extra_max_mm2)
            return "EXTRA";
        if (area_mm2 < area_cat1_max_mm2)
            return "CATEGORIA 1";
        if (area_mm2 < area_cat2_max_mm2)
            return "CATEGORIA 2";
        return "CATEGORIA 3";
    };

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

        // Desenha bounding boxes e centros de gravidade dos blobs
        if (blobs != NULL) {
            int category_counts[4] = {0, 0, 0, 0};
            for (int i = 0; i < nlabels; i++) {
                double area_mm2 =
                    static_cast<double>(blobs[i].area) * mm_per_px * mm_per_px;
                if (area_mm2 < area_min_mm2) {
                    continue;
                }
                // Ignora blobs que tocam nas bordas da imagem
                if (label_on_border[blobs[i].label]) {
                    continue;
                }

                int category = 3;
                if (area_mm2 < area_extra_max_mm2)
                    category = 0;
                else if (area_mm2 < area_cat1_max_mm2)
                    category = 1;
                else if (area_mm2 < area_cat2_max_mm2)
                    category = 2;
                category_counts[category]++;

                const char *category_name = classify_blob(area_mm2);
                cv::rectangle(frame,
                              cv::Rect(blobs[i].x, blobs[i].y, blobs[i].width,
                                       blobs[i].height),
                              cv::Scalar(0, 255, 0), 2);
                cv::circle(frame, cv::Point(blobs[i].xc, blobs[i].yc), 3,
                           cv::Scalar(0, 0, 255), -1);
                double perimeter_mm =
                    static_cast<double>(blobs[i].perimeter) * mm_per_px;
                std::ostringstream info_stream;
                info_stream << std::fixed << std::setprecision(1)
                            << category_name << " "
                            << "A:" << area_mm2 << "mm2 "
                            << "P:" << perimeter_mm << "mm";
                std::string blob_info = info_stream.str();
                int text_y = blobs[i].y - 5;
                if (text_y < 10) {
                    text_y = blobs[i].y + 15;
                }
                cv::putText(frame, blob_info, cv::Point(blobs[i].x, text_y),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0),
                            1);
            }

            std::string counts_text =
                "EXTRA: " + std::to_string(category_counts[0]) +
                "  C1: " + std::to_string(category_counts[1]) +
                "  C2: " + std::to_string(category_counts[2]) +
                "  C3: " + std::to_string(category_counts[3]);
            cv::putText(frame, counts_text, cv::Point(20, 130),
                        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 0), 2);
            cv::putText(frame, counts_text, cv::Point(20, 130),
                        cv::FONT_HERSHEY_SIMPLEX, 0.8,
                        cv::Scalar(255, 255, 255), 1);

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

    return 0;
}
