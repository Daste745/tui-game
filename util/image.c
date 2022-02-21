#include <stdio.h>
#include <stdlib.h>
#include "image.h"

int pixelValue (Pixel pixel) {
    return (pixel.r << 16) + (pixel.g << 8) + pixel.b;
}

Image loadPPM (char* filename) {
    FILE* file = fopen(filename, "r");
    Image image;

    fscanf(file, "P3 %d %d %d", &image.width, &image.height, &image.max_val);

    image.pixels = malloc(sizeof(Pixel) * image.height);
    for (int y = 0; y < image.height; y++) {
        image.pixels[y] = malloc(sizeof(Pixel) * image.width);
    }

    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            Pixel pixel;
            fscanf(file, "%d %d %d", &pixel.r, &pixel.g, &pixel.b);
            image.pixels[y][x] = pixel;
        }
    }

    fclose(file);

    return image;
}
