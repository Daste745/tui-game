#ifndef UTIL_IMAGE_H
#define UTIL_IMAGE_H

typedef struct Pixel {
    int r;
    int g;
    int b;
} Pixel;

typedef struct Image {
    int height;
    int width;
    int max_val;
    Pixel** pixels;
} Image;

int pixelValue (Pixel pixel);
Image loadPPM (char* filename);

#endif //UTIL_IMAGE_H
