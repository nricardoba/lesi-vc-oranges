// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//           INSTITUTO POLITÉCNICO DO CÁVADO E DO AVE
//                          2025/2026
//             ENGENHARIA DE SISTEMAS INFORMÁTICOS
//                    VISÃO POR COMPUTADOR
//
//                          PROFESSOR:
//             [  DUARTE DUQUE - dduque@ipca.pt  ]
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Desabilita (no MSVC++) warnings de funções não seguras (fopen, sscanf,
// etc...)
#define _CRT_SECURE_NO_WARNINGS

#include "vc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: ALOCAR E LIBERTAR UMA IMAGEM
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Alocar memória para uma imagem
IVC *vc_image_new(int width, int height, int channels, int levels) {
    IVC *image = (IVC *)malloc(sizeof(IVC));

    if (image == NULL)
        return NULL;
    if ((levels <= 0) || (levels > 255))
        return NULL;

    image->width = width;
    image->height = height;
    image->channels = channels;
    image->levels = levels;
    image->bytesperline = image->width * image->channels;
    image->data = (unsigned char *)malloc(image->width * image->height *
                                          image->channels * sizeof(char));

    if (image->data == NULL) {
        return vc_image_free(image);
    }

    return image;
}

// Libertar memória de uma imagem
IVC *vc_image_free(IVC *image) {
    if (image != NULL) {
        if (image->data != NULL) {
            free(image->data);
            image->data = NULL;
        }

        free(image);
        image = NULL;
    }

    return image;
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: ESPAÇOS DE COR (RGB, HSV) E SEGMENTAÇÃO
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Converte de RGB para HSV
int vc_rgb_to_hsv(IVC *src, IVC *dst) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int width = src->width;
    int height = src->height;
    int x, y;
    long int pos;
    float rf, gf, bf;
    float max, min, delta;
    float hue, saturation, value;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 3) || (src->channels != dst->channels))
        return 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos = y * bytesperline + x * channels;

            // Normalizar RGB para [0, 1]
            rf = (float)datasrc[pos] / 255.0f;
            gf = (float)datasrc[pos + 1] / 255.0f;
            bf = (float)datasrc[pos + 2] / 255.0f;

            // Calcular max e min dos três canais
            max = (rf > gf) ? ((rf > bf) ? rf : bf) : ((gf > bf) ? gf : bf);
            min = (rf < gf) ? ((rf < bf) ? rf : bf) : ((gf < bf) ? gf : bf);
            delta = max - min;

            // Value = max
            value = max;

            // Saturation
            if (max == 0.0f) {
                saturation = 0.0f;
            } else {
                saturation = delta / max;
            }

            // Hue
            if (delta == 0.0f) {
                hue = 0.0f;
            } else if (max == rf) {
                hue = 60.0f * (gf - bf) / delta;
                if (hue < 0.0f) {
                    hue += 360.0f;
                }
            } else if (max == gf) {
                hue = 120.0f + 60.0f * (bf - rf) / delta;
            } else { // max == bf
                hue = 240.0f + 60.0f * (rf - gf) / delta;
            }

            // Mapear H de [0, 360) para [0, 255], S de [0, 1] para [0, 255], V
            // de [0, 1] para [0, 255]
            datadst[pos] = (unsigned char)(hue / 360.0f * 255.0f);   // H
            datadst[pos + 1] = (unsigned char)(saturation * 255.0f); // S
            datadst[pos + 2] = (unsigned char)(value * 255.0f);      // V
        }
    }

    return 1;
}

// Segmentação de uma imagem HSV em função de intervalos de H, S e V
// hmin,hmax = [0, 360]; smin,smax = [0, 100]; vmin,vmax = [0, 100]
int vc_hsv_segmentation(IVC *src, IVC *dst, int hmin, int hmax, int smin,
                        int smax, int vmin, int vmax) {
    unsigned char *datasrc = (unsigned char *)src->data;
    int bytesperline_src = src->bytesperline;
    int channels_src = src->channels;
    unsigned char *datadst = (unsigned char *)dst->data;
    int bytesperline_dst = dst->bytesperline;
    int channels_dst = dst->channels;
    int width = src->width;
    int height = src->height;
    int x, y;
    long int pos_src, pos_dst;
    unsigned char h, s, v;
    int hmin_255, hmax_255, smin_255, smax_255, vmin_255, vmax_255;
    int in_h, in_s, in_v;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 3) || (dst->channels != 1))
        return 0;

    // Intervalos válidos: H [0,360], S/V [0,100]
    if ((hmin < 0) || (hmin > 360) || (hmax < 0) || (hmax > 360))
        return 0;
    if ((smin < 0) || (smin > 100) || (smax < 0) || (smax > 100) ||
        (smin > smax))
        return 0;
    if ((vmin < 0) || (vmin > 100) || (vmax < 0) || (vmax > 100) ||
        (vmin > vmax))
        return 0;

    hmin_255 = (hmin * 255) / 360;
    hmax_255 = (hmax * 255) / 360;
    smin_255 = (smin * 255) / 100;
    smax_255 = (smax * 255) / 100;
    vmin_255 = (vmin * 255) / 100;
    vmax_255 = (vmax * 255) / 100;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos_src = y * bytesperline_src + x * channels_src;
            pos_dst = y * bytesperline_dst + x * channels_dst;

            h = datasrc[pos_src];
            s = datasrc[pos_src + 1];
            v = datasrc[pos_src + 2];

            // H é circular: se hmin > hmax, o intervalo cruza 0º
            if (hmin_255 <= hmax_255)
                in_h = (h >= hmin_255 && h <= hmax_255);
            else // caso hmin_255 > hmax_255 (vermelho), o h tem de estar entre
                 // hmin_255 e o limite maximo ou o h tem de estar entre o
                 // hmax_255 e o limite minimo
                in_h = (h >= hmin_255 || h <= hmax_255);

            in_s = (s >= smin_255 && s <= smax_255);
            in_v = (v >= vmin_255 && v <= vmax_255);

            datadst[pos_dst] = (in_h && in_s && in_v) ? 255 : 0;
        }
    }

    return 1;
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: OPERADORES MORFOLÓGICOS BINÁRIOS
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Dilatação binária
int vc_binary_dilate(IVC *src, IVC *dst, int kernel) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, kx, ky;
    int xmin, xmax, ymin, ymax;
    long int pos, posk;
    int has_white;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    // O kernel tem de ser ímpar e maior ou igual a 3
    if (kernel < 3)
        kernel = 3;
    if ((kernel % 2) == 0)
        kernel++;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            // Calcular limites da vizinhança
            xmin = x - kernel / 2;
            xmax = x + kernel / 2;
            ymin = y - kernel / 2;
            ymax = y + kernel / 2;

            // Proteger contra as bordas da imagem
            if (xmin < 0)
                xmin = 0;
            if (ymin < 0)
                ymin = 0;
            if (xmax >= width)
                xmax = width - 1;
            if (ymax >= height)
                ymax = height - 1;

            has_white = 0;

            // Percorrer a vizinhança (kernel)
            for (ky = ymin; ky <= ymax; ky++) {
                for (kx = xmin; kx <= xmax; kx++) {
                    posk = ky * bytesperline + kx * channels;

                    if (datasrc[posk] != 0) {
                        has_white = 1;
                        break;
                    }
                }
                if (has_white) {
                    break;
                }
            }

            // Escrever no píxel central
            pos = y * bytesperline + x * channels;
            datadst[pos] = has_white ? 255 : 0;
        }
    }

    return 1;
}

// Erosão binária
int vc_binary_erode(IVC *src, IVC *dst, int kernel) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, kx, ky;
    int xmin, xmax, ymin, ymax;
    long int pos, posk;
    int has_black;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    // O kernel tem de ser ímpar e maior ou igual a 3
    if (kernel < 3)
        kernel = 3;
    if ((kernel % 2) == 0)
        kernel++;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            // Calcular limites da vizinhança
            xmin = x - kernel / 2;
            xmax = x + kernel / 2;
            ymin = y - kernel / 2;
            ymax = y + kernel / 2;

            // Proteger contra as bordas da imagem
            if (xmin < 0)
                xmin = 0;
            if (ymin < 0)
                ymin = 0;
            if (xmax >= width)
                xmax = width - 1;
            if (ymax >= height)
                ymax = height - 1;

            has_black = 0;

            // Percorrer a vizinhança (kernel)
            for (ky = ymin; ky <= ymax; ky++) {
                for (kx = xmin; kx <= xmax; kx++) {
                    posk = ky * bytesperline + kx * channels;

                    if (datasrc[posk] == 0) {
                        has_black = 1;
                        break;
                    }
                }
                if (has_black) {
                    break;
                }
            }

            // Escrever no píxel central
            pos = y * bytesperline + x * channels;
            datadst[pos] = has_black ? 0 : 255;
        }
    }

    return 1;
}

// Abertura binária (Erosão seguida de Dilatação)
int vc_binary_open(IVC *src, IVC *dst, int kernel) {
    IVC *tmp;
    int ret = 1;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    tmp = vc_image_new(src->width, src->height, src->channels, src->levels);
    if (tmp == NULL)
        return 0;

    ret = vc_binary_erode(src, tmp, kernel);
    if (ret) {
        ret = vc_binary_dilate(tmp, dst, kernel);
    }

    vc_image_free(tmp);

    return ret;
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: ETIQUETAGEM DE BLOBS E CÁLCULO DE CARACTERÍSTICAS
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Etiquetagem de blobs
// src        : Imagem binária de entrada
// dst        : Imagem grayscale (irá conter as etiquetas)
// nlabels    : Endereço de memória de uma variável, onde será armazenado o
// número de etiquetas encontradas.
// OVC*       : Retorna um array de estruturas de blobs (objectos),
// com respectivas etiquetas. É necessário libertar posteriormente esta memória.
OVC *vc_binary_blob_labelling(IVC *src, IVC *dst, int *nlabels) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, a, b;
    long int i, size;
    long int posX, posA, posB, posC, posD;
    int labeltable[256] = {0};
    int labelarea[256] = {0};
    int label = 1; // Etiqueta inicial.
    int num, tmplabel;
    OVC *blobs; // Apontador para array de blobs (objectos) que será retornado
                // desta função.

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height) ||
        (src->channels != dst->channels))
        return NULL;
    if (channels != 1)
        return NULL;

    // Copia dados da imagem binária para imagem grayscale
    memcpy(datadst, datasrc, bytesperline * height);

    // Todos os pixéis de plano de fundo devem obrigatóriamente ter valor 0
    // Todos os pixéis de primeiro plano devem obrigatóriamente ter valor 255
    // Serão atribuídas etiquetas no intervalo [1,254]
    // Este algoritmo está assim limitado a 254 labels
    for (i = 0, size = bytesperline * height; i < size; i++) {
        if (datadst[i] != 0)
            datadst[i] = 255;
    }

    // Limpa os rebordos da imagem binária
    for (y = 0; y < height; y++) {
        datadst[y * bytesperline + 0 * channels] = 0;
        datadst[y * bytesperline + (width - 1) * channels] = 0;
    }
    for (x = 0; x < width; x++) {
        datadst[0 * bytesperline + x * channels] = 0;
        datadst[(height - 1) * bytesperline + x * channels] = 0;
    }

    // Efectua a etiquetagem
    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            // Kernel:
            // A B C
            // D X

            posA = (y - 1) * bytesperline + (x - 1) * channels; // A
            posB = (y - 1) * bytesperline + x * channels;       // B
            posC = (y - 1) * bytesperline + (x + 1) * channels; // C
            posD = y * bytesperline + (x - 1) * channels;       // D
            posX = y * bytesperline + x * channels;             // X

            // Se o pixel foi marcado
            if (datadst[posX] != 0) {
                if ((datadst[posA] == 0) && (datadst[posB] == 0) &&
                    (datadst[posC] == 0) && (datadst[posD] == 0)) {
                    datadst[posX] = label;
                    labeltable[label] = label;
                    label++;
                } else {
                    num = 255;

                    // Se A está marcado
                    if (datadst[posA] != 0)
                        num = labeltable[datadst[posA]];
                    // Se B está marcado, e é menor que a etiqueta "num"
                    if ((datadst[posB] != 0) &&
                        (labeltable[datadst[posB]] < num))
                        num = labeltable[datadst[posB]];
                    // Se C está marcado, e é menor que a etiqueta "num"
                    if ((datadst[posC] != 0) &&
                        (labeltable[datadst[posC]] < num))
                        num = labeltable[datadst[posC]];
                    // Se D está marcado, e é menor que a etiqueta "num"
                    if ((datadst[posD] != 0) &&
                        (labeltable[datadst[posD]] < num))
                        num = labeltable[datadst[posD]];

                    // Atribui a etiqueta ao pixel
                    datadst[posX] = num;
                    labeltable[num] = num;

                    // Actualiza a tabela de etiquetas
                    if (datadst[posA] != 0) {
                        if (labeltable[datadst[posA]] != num) {
                            for (tmplabel = labeltable[datadst[posA]], a = 1;
                                 a < label; a++) {
                                if (labeltable[a] == tmplabel) {
                                    labeltable[a] = num;
                                }
                            }
                        }
                    }
                    if (datadst[posB] != 0) {
                        if (labeltable[datadst[posB]] != num) {
                            for (tmplabel = labeltable[datadst[posB]], a = 1;
                                 a < label; a++) {
                                if (labeltable[a] == tmplabel) {
                                    labeltable[a] = num;
                                }
                            }
                        }
                    }
                    if (datadst[posC] != 0) {
                        if (labeltable[datadst[posC]] != num) {
                            for (tmplabel = labeltable[datadst[posC]], a = 1;
                                 a < label; a++) {
                                if (labeltable[a] == tmplabel) {
                                    labeltable[a] = num;
                                }
                            }
                        }
                    }
                    if (datadst[posD] != 0) {
                        if (labeltable[datadst[posD]] != num) {
                            for (tmplabel = labeltable[datadst[posD]], a = 1;
                                 a < label; a++) {
                                if (labeltable[a] == tmplabel) {
                                    labeltable[a] = num;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Volta a etiquetar a imagem
    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            posX = y * bytesperline + x * channels; // X

            if (datadst[posX] != 0) {
                datadst[posX] = labeltable[datadst[posX]];
            }
        }
    }

    // Contagem do número de blobs
    // Passo 1: Eliminar, da tabela, etiquetas repetidas
    for (a = 1; a < label - 1; a++) {
        for (b = a + 1; b < label; b++) {
            if (labeltable[a] == labeltable[b])
                labeltable[b] = 0;
        }
    }
    // Passo 2: Conta etiquetas e organiza a tabela de etiquetas, para que não
    // hajam valores vazios (zero) entre etiquetas
    *nlabels = 0;
    for (a = 1; a < label; a++) {
        if (labeltable[a] != 0) {
            labeltable[*nlabels] =
                labeltable[a]; // Organiza tabela de etiquetas
            (*nlabels)++;      // Conta etiquetas
        }
    }

    // Se não há blobs
    if (*nlabels == 0)
        return NULL;

    // Cria lista de blobs (objectos) e preenche a etiqueta
    blobs = (OVC *)calloc((*nlabels), sizeof(OVC));
    if (blobs != NULL) {
        for (a = 0; a < (*nlabels); a++)
            blobs[a].label = labeltable[a];
    } else
        return NULL;

    return blobs;
}

int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs) {
    unsigned char *data = (unsigned char *)src->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, i;
    long int pos;
    int xmin, ymin, xmax, ymax;
    long int sumx, sumy;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (channels != 1)
        return 0;

    // Conta área de cada blob
    for (i = 0; i < nblobs; i++) {
        xmin = width - 1;
        ymin = height - 1;
        xmax = 0;
        ymax = 0;

        sumx = 0;
        sumy = 0;

        blobs[i].area = 0;

        for (y = 1; y < height - 1; y++) {
            for (x = 1; x < width - 1; x++) {
                pos = y * bytesperline + x * channels;

                if (data[pos] == blobs[i].label) {
                    // Área
                    blobs[i].area++;

                    // Centro de Gravidade
                    sumx += x;
                    sumy += y;

                    // Bounding Box
                    if (xmin > x)
                        xmin = x;
                    if (ymin > y)
                        ymin = y;
                    if (xmax < x)
                        xmax = x;
                    if (ymax < y)
                        ymax = y;

                    // Perímetro
                    // Se pelo menos um dos quatro vizinhos não pertence ao
                    // mesmo label, então é um pixel de contorno
                    if ((data[pos - 1] != blobs[i].label) ||
                        (data[pos + 1] != blobs[i].label) ||
                        (data[pos - bytesperline] != blobs[i].label) ||
                        (data[pos + bytesperline] != blobs[i].label)) {
                        blobs[i].perimeter++;
                    }
                }
            }
        }

        // Bounding Box
        blobs[i].x = xmin;
        blobs[i].y = ymin;
        blobs[i].width = (xmax - xmin) + 1;
        blobs[i].height = (ymax - ymin) + 1;

        // Centro de Gravidade
        // blobs[i].xc = (xmax - xmin) / 2;
        // blobs[i].yc = (ymax - ymin) / 2;
        blobs[i].xc = sumx / MAX(blobs[i].area, 1);
        blobs[i].yc = sumy / MAX(blobs[i].area, 1);
    }

    return 1;
}
