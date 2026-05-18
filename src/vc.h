#define VC_DEBUG

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

typedef struct {
    unsigned char *data;
    int width, height;
    int channels;
    int levels;
    int bytesperline;
} IVC;

typedef struct {
    int x, y, width, height;
    int area;
    int xc, yc;
    int perimeter;
    int label;
} OVC;

IVC *vc_image_new(int width, int height, int channels, int levels);
IVC *vc_image_free(IVC *image);

int vc_rgb_to_hsv(IVC *src, IVC *dst);
int vc_hsv_segmentation(IVC *src, IVC *dst, int hmin, int hmax, int smin,
                        int smax, int vmin, int vmax);

int vc_binary_open(IVC *src, IVC *dst, int kernel);

OVC *vc_binary_blob_labelling(IVC *src, IVC *dst, int *nlabels);
int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs);
