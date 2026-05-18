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

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

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
// FUNÇÕES: ESPAÇOS DE COR (RGB, HSV) E SEGMENTAÇÃO
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int vc_rgb_to_hsv(IVC *src, IVC *dst);
int vc_hsv_segmentation(IVC *src, IVC *dst, int hmin, int hmax, int smin,
                        int smax, int vmin, int vmax);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: OPERADORES MORFOLÓGICOS BINÁRIOS
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int vc_binary_open(IVC *src, IVC *dst, int kernel);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// FUNÇÕES: ETIQUETAGEM DE BLOBS E CÁLCULO DE CARACTERÍSTICAS
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

OVC *vc_binary_blob_labelling(IVC *src, IVC *dst, int *nlabels);
int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs);
