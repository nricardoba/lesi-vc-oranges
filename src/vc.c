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
#include <ctype.h>
#include <math.h>
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
// FUNÇÕES: LEITURA E ESCRITA DE IMAGENS (PBM, PGM E PPM)
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

char *netpbm_get_token(FILE *file, char *tok, int len) {
    char *t;
    int c;

    for (;;) {
        while (isspace(c = getc(file)))
            ;
        if (c != '#')
            break;
        do
            c = getc(file);
        while ((c != '\n') && (c != EOF));
        if (c == EOF)
            break;
    }

    t = tok;

    if (c != EOF) {
        do {
            *t++ = c;
            c = getc(file);
        } while ((!isspace(c)) && (c != '#') && (c != EOF) &&
                 (t - tok < len - 1));

        if (c == '#')
            ungetc(c, file);
    }

    *t = 0;

    return tok;
}

long int unsigned_char_to_bit(unsigned char *datauchar, unsigned char *databit,
                              int width, int height) {
    int x, y;
    int countbits;
    long int pos, counttotalbytes;
    unsigned char *p = databit;

    *p = 0;
    countbits = 1;
    counttotalbytes = 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos = width * y + x;

            if (countbits <= 8) {
                // Numa imagem PBM:
                // 1 = Preto
                // 0 = Branco
                //*p |= (datauchar[pos] != 0) << (8 - countbits);

                // Na nossa imagem:
                // 1 = Branco
                // 0 = Preto
                *p |= (datauchar[pos] == 0) << (8 - countbits);

                countbits++;
            }
            if ((countbits > 8) || (x == width - 1)) {
                p++;
                *p = 0;
                countbits = 1;
                counttotalbytes++;
            }
        }
    }

    return counttotalbytes;
}

void bit_to_unsigned_char(unsigned char *databit, unsigned char *datauchar,
                          int width, int height) {
    int x, y;
    int countbits;
    long int pos;
    unsigned char *p = databit;

    countbits = 1;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos = width * y + x;

            if (countbits <= 8) {
                // Numa imagem PBM:
                // 1 = Preto
                // 0 = Branco
                // datauchar[pos] = (*p & (1 << (8 - countbits))) ? 1 : 0;

                // Na nossa imagem:
                // 1 = Branco
                // 0 = Preto
                datauchar[pos] = (*p & (1 << (8 - countbits))) ? 0 : 1;

                countbits++;
            }
            if ((countbits > 8) || (x == width - 1)) {
                p++;
                countbits = 1;
            }
        }
    }
}

IVC *vc_read_image(char *filename) {
    FILE *file = NULL;
    IVC *image = NULL;
    unsigned char *tmp;
    char tok[20];
    long int size, sizeofbinarydata;
    int width, height, channels;
    int levels = 255;
    int v;

    // Abre o ficheiro
    if ((file = fopen(filename, "rb")) != NULL) {
        // Efectua a leitura do header
        netpbm_get_token(file, tok, sizeof(tok));

        if (strcmp(tok, "P4") == 0) {
            channels = 1;
            levels = 1;
        } // Se PBM (Binary [0,1])
        else if (strcmp(tok, "P5") == 0)
            channels = 1; // Se PGM (Gray [0,MAX(level,255)])
        else if (strcmp(tok, "P6") == 0)
            channels = 3; // Se PPM (RGB [0,MAX(level,255)])
        else {
            // clang-format off
            #ifdef VC_DEBUG
            printf("ERROR -> vc_read_image():\n\tFile is not a valid PBM, PGM "
                   "or PPM file.\n\tBad magic number!\n");
            #endif
            // clang-format on

            fclose(file);
            return NULL;
        }

        if (levels == 1) // PBM
        {
            if (sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d",
                       &width) != 1 ||
                sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d",
                       &height) != 1) {
                // clang-format off
                #ifdef VC_DEBUG
                printf("ERROR -> vc_read_image():\n\tFile is not a valid PBM "
                       "file.\n\tBad size!\n");
                #endif
                // clang-format on

                fclose(file);
                return NULL;
            }

            // Aloca memória para imagem
            image = vc_image_new(width, height, channels, levels);
            if (image == NULL)
                return NULL;

            sizeofbinarydata =
                (image->width / 8 + ((image->width % 8) ? 1 : 0)) *
                image->height;
            tmp = (unsigned char *)malloc(sizeofbinarydata);
            if (tmp == NULL)
                return 0;

            // clang-format off
            #ifdef VC_DEBUG
            printf("\nchannels=%d w=%d h=%d levels=%d\n", image->channels,
                   image->width, image->height, levels);
            #endif
            // clang-format on

            if ((v = fread(tmp, sizeof(unsigned char), sizeofbinarydata,
                           file)) != sizeofbinarydata) {
                // clang-format off
                #ifdef VC_DEBUG
                printf("ERROR -> vc_read_image():\n\tPremature EOF on file.\n");
                #endif
                // clang-format on

                vc_image_free(image);
                fclose(file);
                free(tmp);
                return NULL;
            }

            bit_to_unsigned_char(tmp, image->data, image->width, image->height);

            free(tmp);
        } else // PGM ou PPM
        {
            if (sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d",
                       &width) != 1 ||
                sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d",
                       &height) != 1 ||
                sscanf(netpbm_get_token(file, tok, sizeof(tok)), "%d",
                       &levels) != 1 ||
                levels <= 0 || levels > 255) {
                // clang-format off
                #ifdef VC_DEBUG
                printf("ERROR -> vc_read_image():\n\tFile is not a valid PGM "
                       "or PPM file.\n\tBad size!\n");
                #endif
                // clang-format on

                fclose(file);
                return NULL;
            }

            // Aloca memória para imagem
            image = vc_image_new(width, height, channels, levels);
            if (image == NULL)
                return NULL;

            // clang-format off
            #ifdef VC_DEBUG
            printf("\nchannels=%d w=%d h=%d levels=%d\n", image->channels,
                   image->width, image->height, levels);
            #endif
            // clang-format on

            size = image->width * image->height * image->channels;

            if ((v = fread(image->data, sizeof(unsigned char), size, file)) !=
                size) {
                // clang-format off
                #ifdef VC_DEBUG
                printf("ERROR -> vc_read_image():\n\tPremature EOF on file.\n");
                #endif
                // clang-format on

                vc_image_free(image);
                fclose(file);
                return NULL;
            }
        }

        fclose(file);
    } else {
        // clang-format off
        #ifdef VC_DEBUG
        printf("ERROR -> vc_read_image():\n\tFile not found.\n");
        #endif
        // clang-format on
    }

    return image;
}

int vc_write_image(char *filename, IVC *image) {
    FILE *file = NULL;
    unsigned char *tmp;
    long int totalbytes, sizeofbinarydata;

    if (image == NULL)
        return 0;

    if ((file = fopen(filename, "wb")) != NULL) {
        if (image->levels == 1) {
            sizeofbinarydata =
                (image->width / 8 + ((image->width % 8) ? 1 : 0)) *
                    image->height +
                1;
            tmp = (unsigned char *)malloc(sizeofbinarydata);
            if (tmp == NULL)
                return 0;

            fprintf(file, "%s %d %d\n", "P4", image->width, image->height);

            totalbytes = unsigned_char_to_bit(image->data, tmp, image->width,
                                              image->height);
            printf("Total = %ld\n", totalbytes);
            if (fwrite(tmp, sizeof(unsigned char), totalbytes, file) !=
                totalbytes) {
                // clang-format off
                #ifdef VC_DEBUG
                fprintf(stderr, "ERROR -> vc_read_image():\n\tError writing "
                                "PBM, PGM or PPM file.\n");
                #endif
                // clang-format on

                fclose(file);
                free(tmp);
                return 0;
            }

            free(tmp);
        } else {
            fprintf(file, "%s %d %d 255\n",
                    (image->channels == 1) ? "P5" : "P6", image->width,
                    image->height);

            if (fwrite(image->data, image->bytesperline, image->height, file) !=
                image->height) {
                // clang-format off
                #ifdef VC_DEBUG
                fprintf(stderr, "ERROR -> vc_read_image():\n\tError writing "
                                "PBM, PGM or PPM file.\n");
                #endif
                // clang-format on

                fclose(file);
                return 0;
            }
        }

        fclose(file);

        return 1;
    }

    return 0;
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: ESPAÇOS DE COR (RGB, HSV, GRAY) E SEGMENTAÇÃO
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Inverte a imagem Gray (Negativo)
int vc_gray_negative(IVC *srcdst) {
    unsigned char *data = (unsigned char *)srcdst->data;
    int width = srcdst->width;
    int height = srcdst->height;
    int bytesperline = srcdst->bytesperline;
    int channels = srcdst->channels;
    int x, y;
    long int pos;

    // Verificação de erros
    if ((srcdst->width <= 0) || (srcdst->height <= 0) || (srcdst->data == NULL))
        return 0;
    if (channels != 1)
        return 0;

    // Inverte a imagem Gray
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos = y * bytesperline + x * channels;
            data[pos] = 255 - data[pos];
        }
    }

    return 1;
}

// Inverte a imagem RGB (Negativo)
int vc_rgb_negative(IVC *srcdst) {
    unsigned char *data = (unsigned char *)srcdst->data;
    int width = srcdst->width;
    int height = srcdst->height;
    int bytesperline = srcdst->bytesperline;
    int channels = srcdst->channels;
    int x, y;
    long int pos;

    // Verificação de erros
    if ((srcdst->width <= 0) || (srcdst->height <= 0) || (srcdst->data == NULL))
        return 0;
    if (channels != 3)
        return 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos = y * bytesperline + x * channels;

            data[pos] = 255 - data[pos];         // Inverte o canal R
            data[pos + 1] = 255 - data[pos + 1]; // Inverte o canal G
            data[pos + 2] = 255 - data[pos + 2]; // Inverte o canal B
        }
    }
    return 1;
}

// Converter de RGB para Gray
int vc_rgb_to_gray(IVC *src, IVC *dst) {
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
    float rf, gf, bf;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 3) || (dst->channels != 1))
        return 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos_src = y * bytesperline_src + x * channels_src;
            pos_dst = y * bytesperline_dst + x * channels_dst;

            rf = (float)datasrc[pos_src];
            gf = (float)datasrc[pos_src + 1];
            bf = (float)datasrc[pos_src + 2];

            datadst[pos_dst] =
                (unsigned char)((rf * 0.299) + (gf * 0.587) + (bf * 0.114));
        }
    }

    return 1;
}

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

// Escala de cinzentos para palete de cores (Azul -> Ciano -> Verde -> Amarelo
// -> Vermelho)
int vc_scale_gray_to_color_palette(IVC *src, IVC *dst) {
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
    int gray;
    int r, g, b;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (dst->channels != 3))
        return 0;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos_src = y * bytesperline_src + x * channels_src;
            pos_dst = y * bytesperline_dst + x * channels_dst;

            gray = datasrc[pos_src];

            // Palete de cores baseada no gráfico (Azul -> Ciano -> Verde ->
            // Amarelo -> Vermelho)
            if (gray < 64) {
                // 1º Quartil (0 - 64): Azul -> Ciano
                // B fixo em 255. G sobe de 0 a 255. R fixo em 0.
                b = 255;
                g = gray * 4;
                r = 0;
            } else if (gray < 128) {
                // 2º Quartil (64 - 128): Ciano -> Verde
                // B desce de 255 a 0. G fixo em 255. R fixo em 0.
                b = 255 - ((gray - 64) * 4);
                g = 255;
                r = 0;
            } else if (gray < 192) {
                // 3º Quartil (128 - 192): Verde -> Amarelo
                // B fixo em 0. G fixo em 255. R sobe de 0 a 255.
                b = 0;
                g = 255;
                r = (gray - 128) * 4;
            } else {
                // 4º Quartil (192 - 255): Amarelo -> Vermelho
                // B fixo em 0. G desce de 255 a 0. R fixo em 255.
                b = 0;
                g = 255 - ((gray - 192) * 4);
                r = 255;
            }

            // Guardar em RGB
            datadst[pos_dst] = (unsigned char)r;
            datadst[pos_dst + 1] = (unsigned char)g;
            datadst[pos_dst + 2] = (unsigned char)b;
        }
    }

    return 1;
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: BINARIZAÇÃO (THRESHOLDING)
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Binarização Simples (Thresholding)
int vc_gray_to_binary(IVC *src, IVC *dst, int threshold) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y;
    long int pos;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    if (threshold < 0)
        threshold = 0;
    if (threshold > 255)
        threshold = 255;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos = y * bytesperline + x * channels;

            datadst[pos] = (datasrc[pos] > threshold) ? 255 : 0;
        }
    }

    return 1;
}

// Binarização pela Média Global
int vc_gray_to_binary_global_mean(IVC *src, IVC *dst) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y;
    long int pos;
    long int sum = 0;
    int threshold;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    // Calcular threshold automático pela média global
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos = y * bytesperline + x * channels;
            sum += datasrc[pos];
        }
    }

    threshold = (int)(sum / (width * height));

    // Binarização usando o threshold calculado
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos = y * bytesperline + x * channels;
            datadst[pos] = (datasrc[pos] > threshold) ? 255 : 0;
        }
    }

    return 1;
}

// Binarização pelo Midpoint (Ponto Médio) da Vizinhança
int vc_gray_to_binary_midpoint(IVC *src, IVC *dst, int kernel) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, kx, ky;
    int xmin, xmax, ymin, ymax;
    int min, max, threshold;
    long int pos, posk;

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

            min = 255;
            max = 0;

            // Percorrer a vizinhança (kernel)
            for (ky = ymin; ky <= ymax; ky++) {
                for (kx = xmin; kx <= xmax; kx++) {
                    posk = ky * bytesperline + kx * channels;

                    if (datasrc[posk] < min)
                        min = datasrc[posk];
                    if (datasrc[posk] > max)
                        max = datasrc[posk];
                }
            }

            // Calcular o threshold pelo Midpoint (Ponto Médio)
            threshold = (min + max) / 2;

            // Escrever no píxel central
            pos = y * bytesperline + x * channels;
            datadst[pos] = (datasrc[pos] > threshold) ? 255 : 0;
        }
    }

    return 1;
}

// Binarização adaptativa: Método de Bernsen
int vc_gray_to_binary_bernsen(IVC *src, IVC *dst, int kernel, int cmin) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, kx, ky;
    int xmin, xmax, ymin, ymax;
    int min, max, threshold;
    long int pos, posk;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (dst->channels != 1))
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

            min = 255;
            max = 0;

            // Percorrer a vizinhança (kernel)
            for (ky = ymin; ky <= ymax; ky++) {
                for (kx = xmin; kx <= xmax; kx++) {
                    posk = ky * bytesperline + kx * channels;

                    if (datasrc[posk] < min)
                        min = datasrc[posk];
                    if (datasrc[posk] > max)
                        max = datasrc[posk];
                }
            }

            pos = y * bytesperline + x * channels;

            // Método de Bernsen: Calcular Contraste Local
            if ((max - min) < cmin) {
                // Região homogénea (contraste baixo)
                datadst[pos] = (datasrc[pos] > 128) ? 255 : 0;
            } else {
                // Região com contraste alto: aplica o threshold normal do ponto
                // médio
                threshold = (min + max) / 2;
                datadst[pos] = (datasrc[pos] > threshold) ? 255 : 0;
            }
        }
    }

    return 1;
}

// Binarização adaptativa: Método de Niblack
int vc_gray_to_binary_niblack(IVC *src, IVC *dst, int kernel, float k) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, kx, ky;
    int xmin, xmax, ymin, ymax;
    long int pos, posk;
    float mean, std_dev, sum, sum_sq;
    int num_pixels, threshold;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (dst->channels != 1))
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

            sum = 0.0f;
            sum_sq = 0.0f;
            num_pixels = 0;

            // Percorrer a vizinhança (kernel)
            for (ky = ymin; ky <= ymax; ky++) {
                for (kx = xmin; kx <= xmax; kx++) {
                    posk = ky * bytesperline + kx * channels;

                    float val = (float)datasrc[posk];
                    sum += val;
                    sum_sq += val * val;
                    num_pixels++;
                }
            }

            // Calcular média
            mean = sum / num_pixels;

            // Calcular variância: (Soma dos quadrados / N) - (média^2)
            float variance = (sum_sq / num_pixels) - (mean * mean);

            // Garantir que a variância não é negativa (erros minúsculos de
            // float)
            if (variance < 0.0f)
                variance = 0.0f;

            std_dev = sqrt(variance);

            // Calcular o threshold de Niblack
            threshold = (int)(mean + k * std_dev);

            // Escrever no píxel central
            pos = y * bytesperline + x * channels;
            datadst[pos] = (datasrc[pos] > threshold) ? 255 : 0;
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

// Fecho binário (Dilatação seguida de Erosão)
int vc_binary_close(IVC *src, IVC *dst, int kernel) {
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

    ret = vc_binary_dilate(src, tmp, kernel);
    if (ret) {
        ret = vc_binary_erode(tmp, dst, kernel);
    }

    vc_image_free(tmp);

    return ret;
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: ETIQUETAGEM DE BLOBS E CÁLCULO DE CARACTERÍSTICAS
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Etiquetagem de blobs
// src		: Imagem binária de entrada
// dst		: Imagem grayscale (irá conter as etiquetas)
// nlabels	: Endereço de memória de uma variável, onde será armazenado o
// número de etiquetas encontradas.
// OVC*		: Retorna um array de estruturas de blobs (objectos),
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

    // printf("\nMax Label = %d\n", label);

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

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: HISTOGRAMAS E EQUALIZAÇÃO
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Mostrar Histograma de Imagem em Tons de Cinzento
int vc_gray_histogram_show(IVC *src, IVC *dst) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int n = width * height;
    int ni[256] = {0};
    float pdf[256];
    float pdfmax = 0.0f;
    float pdfnorm[256];
    int i, x, y;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((dst->width != 256) || (dst->height != 256))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    // 1. Contar número de pixeis para cada valor de brilho
    for (i = 0; i < n; i++) {
        ni[datasrc[i]]++;
    }

    // 2. Calcular a função densidade de probabilidade (PDF)
    for (i = 0; i < 256; i++) {
        pdf[i] = (float)ni[i] / (float)n;
    }

    // 3. Calcular o PDF máximo
    for (i = 0; i < 256; i++) {
        if (pdf[i] > pdfmax) {
            pdfmax = pdf[i];
        }
    }

    // 4. Normalizar PDF entre 0 e 1
    for (i = 0; i < 256; i++) {
        if (pdfmax == 0) {
            pdfnorm[i] = 0.0f;
        } else {
            pdfnorm[i] = pdf[i] / pdfmax;
        }
    }

    // 5. Gerar o gráfico na imagem
    for (i = 0; i < 256 * 256; i++) {
        datadst[i] = 0;
    }

    for (x = 0; x < 256; x++) {
        for (y = (256 - 1); y >= (int)((256 - 1) - pdfnorm[x] * 255.0f); y--) {
            datadst[y * 256 + x] = 255;
        }
    }

    return 1;
}

// Equalização de Histograma em Tons de Cinzento
int vc_gray_histogram_equalization(IVC *src, IVC *dst) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int n = width * height;
    int ni[256] = {0};
    float pdf[256];
    float cdf[256];
    float cdfmin = -1.0f;
    int i, x, y;
    long int pos;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    // 1. Contar número de pixeis para cada valor de brilho
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos = y * bytesperline + x * channels;
            ni[datasrc[pos]]++;
        }
    }

    // 2. Calcular a função densidade de probabilidade (PDF)
    for (i = 0; i < 256; i++) {
        pdf[i] = (float)ni[i] / (float)n;
    }

    // 3. Calcular a função distribuição cumulativa (CDF)
    for (i = 0; i < 256; i++) {
        if (i == 0) {
            cdf[i] = pdf[i];
        } else {
            cdf[i] = cdf[i - 1] + pdf[i];
        }
    }

    // 4. Encontrar o cdfmin (primeiro valor não nulo)
    for (i = 0; i < 256; i++) {
        if (cdf[i] != 0.0f) {
            cdfmin = cdf[i];
            break;
        }
    }

    // 5. Calcular os novos valores dos pixeis
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pos = y * bytesperline + x * channels;

            if (cdfmin >= 0.0f && cdfmin < 1.0f) {
                datadst[pos] = (unsigned char)(((cdf[datasrc[pos]] - cdfmin) /
                                                (1.0f - cdfmin)) *
                                               255.0f);
            } else {
                datadst[pos] = datasrc[pos];
            }
        }
    }

    return 1;
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: DETEÇÃO DE CONTORNOS E FILTROS ESPACIAIS
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Deteção de Contornos - Operador de Prewitt
int vc_gray_edge_prewitt(IVC *src, IVC *dst, float th) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y;
    long int pos, posA, posB, posC, posD, posE, posF, posG, posH;
    float histX, histY, hist;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            pos = y * bytesperline + x * channels;
            posA = (y - 1) * bytesperline + (x - 1) * channels;
            posB = (y - 1) * bytesperline + x * channels;
            posC = (y - 1) * bytesperline + (x + 1) * channels;
            posD = y * bytesperline + (x - 1) * channels;
            posE = y * bytesperline + (x + 1) * channels;
            posF = (y + 1) * bytesperline + (x - 1) * channels;
            posG = (y + 1) * bytesperline + x * channels;
            posH = (y + 1) * bytesperline + (x + 1) * channels;

            histX = (float)(-datasrc[posA] + datasrc[posC] - datasrc[posD] +
                            datasrc[posE] - datasrc[posF] + datasrc[posH]);
            histY = (float)(-datasrc[posA] - datasrc[posB] - datasrc[posC] +
                            datasrc[posF] + datasrc[posG] + datasrc[posH]);

            hist = sqrt(histX * histX + histY * histY);

            if (hist > th) {
                datadst[pos] = 255;
            } else {
                datadst[pos] = 0;
            }
        }
    }

    return 1;
}

// Deteção de Contornos - Operador de Sobel
int vc_gray_edge_sobel(IVC *src, IVC *dst, float th) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y;
    long int pos, posA, posB, posC, posD, posE, posF, posG, posH;
    float histX, histY, hist;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            pos = y * bytesperline + x * channels;
            posA = (y - 1) * bytesperline + (x - 1) * channels;
            posB = (y - 1) * bytesperline + x * channels;
            posC = (y - 1) * bytesperline + (x + 1) * channels;
            posD = y * bytesperline + (x - 1) * channels;
            posE = y * bytesperline + (x + 1) * channels;
            posF = (y + 1) * bytesperline + (x - 1) * channels;
            posG = (y + 1) * bytesperline + x * channels;
            posH = (y + 1) * bytesperline + (x + 1) * channels;

            histX = -datasrc[posA] + datasrc[posC] - 2 * datasrc[posD] +
                    2 * datasrc[posE] - datasrc[posF] + datasrc[posH];
            histY = -datasrc[posA] - 2 * datasrc[posB] - datasrc[posC] +
                    datasrc[posF] + 2 * datasrc[posG] + datasrc[posH];

            hist = sqrt(histX * histX + histY * histY);

            if (hist > th) {
                datadst[pos] = 255;
            } else {
                datadst[pos] = 0;
            }
        }
    }

    return 1;
}

// Filtro Passa-Baixo (Média)
int vc_gray_lowpass_mean_filter(IVC *src, IVC *dst, int kernel) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, kx, ky;
    int xmin, xmax, ymin, ymax;
    long int pos, posk;
    int sum, count;

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

            sum = 0;
            count = 0;

            // Percorrer a vizinhança (kernel)
            for (ky = ymin; ky <= ymax; ky++) {
                for (kx = xmin; kx <= xmax; kx++) {
                    posk = ky * bytesperline + kx * channels;

                    sum += datasrc[posk];
                    count++;
                }
            }

            // Escrever no píxel central
            pos = y * bytesperline + x * channels;
            datadst[pos] = sum / count;
        }
    }

    return 1;
}

// Filtro Passa-Baixo (Mediana)
int vc_gray_lowpass_median_filter(IVC *src, IVC *dst, int kernel) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, kx, ky, i, j;
    int xmin, xmax, ymin, ymax;
    long int pos, posk;
    int count;
    unsigned char tmp;
    unsigned char *window;

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

    window = (unsigned char *)malloc(kernel * kernel * sizeof(unsigned char));
    if (window == NULL)
        return 0;

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

            count = 0;

            // Percorrer a vizinhança (kernel)
            for (ky = ymin; ky <= ymax; ky++) {
                for (kx = xmin; kx <= xmax; kx++) {
                    posk = ky * bytesperline + kx * channels;

                    window[count++] = datasrc[posk];
                }
            }

            // Ordenação (Insertion Sort)
            for (i = 1; i < count; i++) {
                tmp = window[i];
                for (j = i; j > 0 && tmp < window[j - 1]; j--) {
                    window[j] = window[j - 1];
                }
                window[j] = tmp;
            }

            // Escrever no píxel central
            pos = y * bytesperline + x * channels;
            datadst[pos] = window[count / 2];
        }
    }

    free(window);
    return 1;
}

// Filtro Passa-Baixo (Gaussiano)
int vc_gray_lowpass_gaussian_filter(IVC *src, IVC *dst) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y;
    int sum;
    long int pos, posA, posB, posC, posD, posE, posF, posG, posH, posI;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            pos = y * bytesperline + x * channels;
            posA = (y - 1) * bytesperline + (x - 1) * channels;
            posB = (y - 1) * bytesperline + x * channels;
            posC = (y - 1) * bytesperline + (x + 1) * channels;
            posD = y * bytesperline + (x - 1) * channels;
            posE = y * bytesperline + x * channels;
            posF = y * bytesperline + (x + 1) * channels;
            posG = (y + 1) * bytesperline + (x - 1) * channels;
            posH = (y + 1) * bytesperline + x * channels;
            posI = (y + 1) * bytesperline + (x + 1) * channels;

            sum = datasrc[posA] * 1 + datasrc[posB] * 2 + datasrc[posC] * 1 +
                  datasrc[posD] * 2 + datasrc[posE] * 4 + datasrc[posF] * 2 +
                  datasrc[posG] * 1 + datasrc[posH] * 2 + datasrc[posI] * 1;

            datadst[pos] = sum / 16;
        }
    }

    return 1;
}

// Filtro Passa-Alto
int vc_gray_highpass_filter(IVC *src, IVC *dst) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y;
    int sum;
    long int pos, posA, posB, posC, posD, posE, posF, posG, posH, posI;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            pos = y * bytesperline + x * channels;
            posA = (y - 1) * bytesperline + (x - 1) * channels;
            posB = (y - 1) * bytesperline + x * channels;
            posC = (y - 1) * bytesperline + (x + 1) * channels;
            posD = y * bytesperline + (x - 1) * channels;
            posE = y * bytesperline + x * channels;
            posF = y * bytesperline + (x + 1) * channels;
            posG = (y + 1) * bytesperline + (x - 1) * channels;
            posH = (y + 1) * bytesperline + x * channels;
            posI = (y + 1) * bytesperline + (x + 1) * channels;

            sum = 8 * datasrc[posE] - datasrc[posA] - datasrc[posB] -
                  datasrc[posC] - datasrc[posD] - datasrc[posF] -
                  datasrc[posG] - datasrc[posH] - datasrc[posI];
            sum = sum / 9;

            if (sum < 0)
                sum = 0;
            if (sum > 255)
                sum = 255;

            datadst[pos] = (unsigned char)sum;
        }
    }

    return 1;
}

// Filtro Passa-Alto (Enhance)
int vc_gray_highpass_filter_enhance(IVC *src, IVC *dst, int gain) {
    unsigned char *datasrc = (unsigned char *)src->data;
    unsigned char *datadst = (unsigned char *)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y;
    int sum;
    long int pos, posA, posB, posC, posD, posE, posF, posG, posH, posI;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
        return 0;
    if (dst->data == NULL)
        return 0;
    if ((src->width != dst->width) || (src->height != dst->height))
        return 0;
    if ((src->channels != 1) || (src->channels != dst->channels))
        return 0;

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            pos = y * bytesperline + x * channels;
            posA = (y - 1) * bytesperline + (x - 1) * channels;
            posB = (y - 1) * bytesperline + x * channels;
            posC = (y - 1) * bytesperline + (x + 1) * channels;
            posD = y * bytesperline + (x - 1) * channels;
            posE = y * bytesperline + x * channels;
            posF = y * bytesperline + (x + 1) * channels;
            posG = (y + 1) * bytesperline + (x - 1) * channels;
            posH = (y + 1) * bytesperline + x * channels;
            posI = (y + 1) * bytesperline + (x + 1) * channels;

            sum = 8 * datasrc[posE] - datasrc[posA] - datasrc[posB] -
                  datasrc[posC] - datasrc[posD] - datasrc[posF] -
                  datasrc[posG] - datasrc[posH] - datasrc[posI];
            sum = sum / 9;

            sum = datasrc[posE] + gain * sum;

            if (sum < 0)
                sum = 0;
            if (sum > 255)
                sum = 255;

            datadst[pos] = (unsigned char)sum;
        }
    }

    return 1;
}
