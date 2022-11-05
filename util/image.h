#pragma once

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

int pixelValue(Pixel pixel);
Image loadPPM(char* filename);

