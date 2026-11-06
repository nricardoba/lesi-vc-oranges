// Image containers and public operations for the custom vision library.
#define VC_DEBUG

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

// Row-major image with interleaved 8-bit channels and no row padding.
typedef struct {
    unsigned char *data; // Pixel buffer owned by the image.
    int width, height;   // Image dimensions in pixels.
    int channels;        // One for binary/label images, three for RGB or HSV.
    int levels;          // Maximum sample value, in the range 1..255.
    int bytesperline;    // Row stride in bytes: width * channels.
} IVC;

// Geometry of a connected component in a single-channel label image.
typedef struct {
    int x, y, width, height; // Bounding box origin and dimensions in pixels.
    int area;                // Number of pixels belonging to the component.
    int xc, yc;              // Integer centroid of the component's pixels.
    int perimeter;           // Boundary-pixel count using four axial neighbors.
    int label;               // Nonzero pixel value identifying the component.
} OVC;

// Allocate an image and an uninitialized pixel buffer; return NULL on failure.
IVC *vc_image_new(int width, int height, int channels, int levels);
// Release the image and its buffer; accept NULL and always return NULL.
IVC *vc_image_free(IVC *image);

// Processing operations below return 1 on success or 0 on failure.
// Image arguments must point to allocated IVC structures.

// Convert RGB to HSV, encoding each channel in 0..255, including hue.
int vc_rgb_to_hsv(IVC *src, IVC *dst);
// Threshold byte-encoded HSV into a single-channel mask (0 or 255).
// Bounds are inclusive: H uses degrees (0..360), S and V use percent (0..100).
// A hue minimum greater than its maximum selects a range crossing zero degrees.
int vc_hsv_segmentation(IVC *src, IVC *dst, int hmin, int hmax, int smin,
                        int smax, int vmin, int vmax);

// Erode then dilate using a square kernel, adjusted to an odd size of at
// least 3.
int vc_binary_open(IVC *src, IVC *dst, int kernel);

// Label 8-connected foreground components, clearing the destination image
// border. Store the component count in nlabels; label values need not be
// consecutive. Return a zero-initialized descriptor array owned by the caller
// (release with free), or NULL if no components are found or an error occurs.
OVC *vc_binary_blob_labelling(IVC *src, IVC *dst, int *nlabels);
// Fill descriptors from a label image; perimeter fields must initially be zero.
// Return 1 on success or 0 on failure.
int vc_binary_blob_info(IVC *src, OVC *blobs, int nblobs);
