# Computer Vision — Orange Detection and Sizing

A C/C++ computer vision project that detects, tracks, counts, and estimates the size of oranges in a video. OpenCV handles video input and visualization, while a custom C library implements the image-processing operations.

**Final grade: 14/20.**

## Features

- Orange detection using HSV color segmentation and morphological opening.
- Connected-component labelling with area, perimeter, bounding box, and centroid measurements.
- Filtering of small objects and objects touching the image border.
- Centroid-based tracking with persistent IDs and a cumulative track count.
- Area and perimeter conversion to physical units, equivalent-circle diameter estimation, and size classification.
- Annotated video and a separate binary segmentation view.

## Requirements

- CMake 3.10 or newer.
- A C99 compiler and a C++14 compiler.
- OpenCV with C++ development headers, video decoding support, and GUI support.
- A graphical desktop session to display the application windows.

Install the build dependencies using your platform's package manager:

### macOS (Homebrew)

```sh
brew install cmake opencv
```

Install the Xcode Command Line Tools if a compiler is not already available:

```sh
xcode-select --install
```

### Ubuntu / Debian

```sh
sudo apt update
sudo apt install build-essential cmake libopencv-dev
```

## Build

Run these commands from the project root:

```sh
cmake -S . -B build
cmake --build build
```

The executable is generated as `build/app`.

If CMake cannot find OpenCV, provide the directory containing `OpenCVConfig.cmake` using `-DOpenCV_DIR=/absolute/path/to/opencv/cmake` when configuring the project.

## Run

From the project root:

```sh
cd build
./app
```

**Run the executable from `build/`.** The input path is currently hardcoded as `../input/video.avi` and is resolved relative to the working directory. The sample video is included in `input/`.

The application opens two windows:

| Window              | Contents                                                                                |
| ------------------- | --------------------------------------------------------------------------------------- |
| `CV - Video`        | Original video with bounding boxes, centroids, measurements, IDs, and frame statistics. |
| `CV - Segmentation` | Binary mask after HSV segmentation and morphological opening.                           |

Press **`q`** while a video window has focus to stop, or wait for the video to finish. The elapsed time is then printed in the terminal; press **Enter** there to exit.

### Output

Each new track is logged once in the terminal. Measurements are also displayed next to each detected orange:

| Label    | Meaning                                                                                     |
| -------- | ------------------------------------------------------------------------------------------- |
| `ID`     | Track identifier.                                                                           |
| `A`      | Area in mm².                                                                                |
| `P`      | Perimeter in mm.                                                                            |
| `D`      | Equivalent-circle diameter in mm, calculated as `2 × sqrt(area / π)`.                       |
| `S`      | Size class, from 0 to 13, based on the diameter thresholds in `size_class_from_diameter()`. |
| `bbox`   | Bounding box `(x, y, width, height)` in pixels; terminal output only.                       |
| `center` | Centroid `(x, y)` in pixels; terminal output only.                                          |

The total orange count is the number of track IDs created, not a guaranteed count of unique physical oranges: an object that loses its track and is detected again can be counted more than once. Measurements are stored when a track is first created and reused in subsequent frames. Results are displayed and printed, not saved to a file.

## Processing Pipeline

1. Read a frame with OpenCV and convert its BGR channel order to RGB.
2. Convert RGB to HSV using the custom C library.
3. Segment pixels within the configured hue, saturation, and value ranges.
4. Apply binary opening (erosion followed by dilation) to reduce noise.
5. Label connected components and compute their geometric properties.
6. Reject components below the minimum area or touching the image border.
7. Match detections to existing tracks by centroid distance, creating new IDs as needed.
8. Convert pixel measurements to physical units, assign a size class, and display the results.

## Configuration and Calibration

Parameters are currently defined in [`src/main.cpp`](src/main.cpp); there are no command-line options. Edit the values and rebuild to use a different video or adjust detection:

| Parameter        | Default              | Purpose                                                                |
| ---------------- | -------------------- | ---------------------------------------------------------------------- |
| `videofile`      | `../input/video.avi` | Input video path, relative to the working directory.                   |
| `hmin`, `hmax`   | 10–35°               | Hue range.                                                             |
| `smin`, `smax`   | 35–100%              | Saturation range.                                                      |
| `vmin`, `vmax`   | 10–100%              | Value (brightness) range.                                              |
| `kernel`         | 3                    | Square kernel size for morphological opening, in pixels.               |
| `mm_per_px`      | `55.0 / 280.0`       | Physical scale, approximately 0.1964 mm per pixel.                     |
| `area_min_mm2`   | 2200 mm²             | Minimum component area.                                                |
| `max_match_dist` | 50 pixels            | Maximum centroid distance for matching detections to tracks.           |
| `max_missed`     | 5 frames             | Number of consecutive missed frames tolerated before removing a track. |
| `MAX_DETECTIONS` | 256                  | Maximum detections processed per frame.                                |

The physical scale and color thresholds are specific to the supplied footage. Recalibrate them if the camera, resolution, lighting, or object distance changes. Diameter is estimated from the segmented area rather than measured directly across the fruit, so segmentation quality affects sizing accuracy.

## Project Structure

```text
.
├── CMakeLists.txt       # Build configuration
├── README.md
├── docs/
│   ├── en/              # English assignment and regulation PDFs
│   └── pt/              # Original Portuguese reference PDFs
├── input/
│   └── video.avi        # Sample input video
└── src/
    ├── main.cpp         # Video processing, tracking, sizing, and visualization
    ├── vc.c             # Custom image-processing implementation
    └── vc.h             # Image structures and public function declarations
```

## Reference Documents

- [Computer Vision assignment (English)](docs/en/assignment-computer-vision.pdf)
- [EEC Regulation No. 379/71 (English)](docs/en/regulation-EEC-379-71.pdf)
- [Original assignment (Portuguese)](docs/pt/enunciado-visao-por-computador.pdf)
- [Original regulation (Portuguese)](docs/pt/regulamento-CEE-379-71.pdf)

## Academic Context

Developed for the **Computer Vision** course, with a final grade of **14 out of 20**.
