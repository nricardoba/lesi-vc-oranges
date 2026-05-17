// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//           INSTITUTO POLITÉCNICO DO CÁVADO E DO AVE
//                          2025/2026
//             ENGENHARIA DE SISTEMAS INFORMÁTICOS
//                    VISÃO POR COMPUTADOR
//
//                          PROFESSOR:
//             [  DUARTE DUQUE - dduque@ipca.pt  ]
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#define VC_DEBUG

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// MACROS
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ESTRUTURA DE UMA IMAGEM
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

typedef struct {
    unsigned char *data;
    int width, height;
    int channels;     // Binário/Cinzentos=1; RGB=3
    int levels;       // Binário=1; Cinzentos [1,255]; RGB [1,255]
    int bytesperline; // width * channels
} IVC;

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ESTRUTURA DE UM BLOB (OBJECTO)
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

typedef struct {
    int x, y, width, height; // Caixa Delimitadora (Bounding Box)
    int area;                // Área
    int xc, yc;              // Centro-de-massa
    int perimeter;           // Perímetro
    int label;               // Etiqueta
} OVC;

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: ALOCAR E LIBERTAR UMA IMAGEM
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

IVC *vc_image_new(int width, int height, int channels, int levels);
IVC *vc_image_free(IVC *image);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: LEITURA E ESCRITA DE IMAGENS (PBM, PGM E PPM)
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

IVC *vc_read_image(char *filename);
int vc_write_image(char *filename, IVC *image);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: ESPAÇOS DE COR (RGB, HSV, GRAY) E SEGMENTAÇÃO
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int vc_gray_negative(IVC *srcdst);
int vc_rgb_negative(IVC *srcdst);
int vc_rgb_to_gray(IVC *src, IVC *dst);
int vc_rgb_to_hsv(IVC *src, IVC *dst);
int vc_hsv_segmentation(IVC *src, IVC *dst, int hmin, int hmax, int smin,
                        int smax, int vmin, int vmax);
int vc_scale_gray_to_color_palette(IVC *src, IVC *dst);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: BINARIZAÇÃO (THRESHOLDING)
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int vc_gray_to_binary(IVC *src, IVC *dst, int threshold);
int vc_gray_to_binary_global_mean(IVC *src, IVC *dst);
int vc_gray_to_binary_midpoint(IVC *src, IVC *dst, int kernel);
int vc_gray_to_binary_bernsen(IVC *src, IVC *dst, int kernel, int cmin);
int vc_gray_to_binary_niblack(IVC *src, IVC *dst, int kernel, float k);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: OPERADORES MORFOLÓGICOS BINÁRIOS
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int vc_binary_dilate(IVC *src, IVC *dst, int kernel);
int vc_binary_erode(IVC *src, IVC *dst, int kernel);
int vc_binary_open(IVC *src, IVC *dst, int kernel);
int vc_binary_close(IVC *src, IVC *dst, int kernel);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: ETIQUETAGEM DE BLOBS E CÁLCULO DE CARACTERÍSTICAS
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

OVC *vc_binary_blob_labelling(IVC *src, IVC *dst, int *nlabels);
int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: HISTOGRAMAS E EQUALIZAÇÃO
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int vc_gray_histogram_show(IVC *src, IVC *dst);
int vc_gray_histogram_equalization(IVC *src, IVC *dst);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: DETEÇÃO DE CONTORNOS (EDGE DETECTION)
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int vc_gray_edge_prewitt(IVC *src, IVC *dst, float th);
int vc_gray_edge_sobel(IVC *src, IVC *dst, float th);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: FILTROS ESPACIAIS E DE REDUÇÃO DE RUÍDO
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int vc_gray_lowpass_mean_filter(IVC *src, IVC *dst, int kernelsize);
int vc_gray_lowpass_median_filter(IVC *src, IVC *dst, int kernelsize);
int vc_gray_lowpass_gaussian_filter(IVC *src, IVC *dst);
int vc_gray_highpass_filter(IVC *src, IVC *dst);
int vc_gray_highpass_filter_enhance(IVC *src, IVC *dst, int gain);
