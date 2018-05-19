#include "blit.hpp"

#include <stdlib.h>

Canvas create_canvas(int width, int height, int margin) {
    //NOTE: we allocate extra buffer margin around the explicitly used canvas area
    //      so that operations can safely read from slightly outside the used area
    //      of the canvas. this simplifies and speeds up some operations because it
    //      eliminates the need for bounds checks and explicit handling of edge cases
    int canvasBytes =
        (width + 2 * margin) * (height + 2 * margin) * sizeof(Pixel);
    Pixel * canvasData = (Pixel *) malloc(canvasBytes);
    Canvas canv = {
        canvasData + margin * (width + 2 * margin) + margin,
        width, height, width + 2 * margin
    };
    Canvas * canvas = &canv;
    //fill whole canvas (including margins) with solid black
    for (int y = 0; y < canvas->height; ++y) {
        Pixel * row = canvas->pixels + y * canvas->pitch;
        for (int x = 0; x < canvas->width; ++x) {
            row[x] = { 255, 0, 255, 255 };
        }
    }

    return canv;
}

void fast_scaled_blit(SDL_Surface * surface, Canvas * canvas, int scale) {
    if (scale == 1) {
        for (int y = 0; y < canvas->height; ++y) {
            Pixel * src = canvas->pixels + y * canvas->pitch;
            Pixel * dest = (Pixel *) ((u8 *) surface->pixels + y * surface->pitch);
            for (int x = 0; x < canvas->width; ++x) {
                dest[x] = src[x];
            }
        }
    } else if (scale == 2) {
        for (int y = 0; y < canvas->height; ++y) {
            Pixel * src = canvas->pixels + y * canvas->pitch;
            Pixel * dest1 = (Pixel *) ((u8 *) surface->pixels + (y * 2 + 0) * surface->pitch);
            Pixel * dest2 = (Pixel *) ((u8 *) surface->pixels + (y * 2 + 1) * surface->pitch);
            for (int x = 0; x < canvas->width; ++x) {
                dest1[x * 2 + 0] = src[x];
                dest1[x * 2 + 1] = src[x];
                dest2[x * 2 + 0] = src[x];
                dest2[x * 2 + 1] = src[x];
            }
        }
    } else if (scale == 3) {
        for (int y = 0; y < canvas->height; ++y) {
            Pixel * src = canvas->pixels + y * canvas->pitch;
            Pixel * dest1 = (Pixel *) ((u8 *) surface->pixels + (y * 3 + 0) * surface->pitch);
            Pixel * dest2 = (Pixel *) ((u8 *) surface->pixels + (y * 3 + 1) * surface->pitch);
            Pixel * dest3 = (Pixel *) ((u8 *) surface->pixels + (y * 3 + 2) * surface->pitch);
            for (int x = 0; x < canvas->width; ++x) {
                dest1[x * 3 + 0] = src[x];
                dest1[x * 3 + 1] = src[x];
                dest1[x * 3 + 2] = src[x];
                dest2[x * 3 + 0] = src[x];
                dest2[x * 3 + 1] = src[x];
                dest2[x * 3 + 2] = src[x];
                dest3[x * 3 + 0] = src[x];
                dest3[x * 3 + 1] = src[x];
                dest3[x * 3 + 2] = src[x];
            }
        }
    } else if (scale == 4) {
        for (int y = 0; y < canvas->height; ++y) {
            Pixel * src = canvas->pixels + y * canvas->pitch;
            Pixel * dest1 = (Pixel *) ((u8 *) surface->pixels + (y * 4 + 0) * surface->pitch);
            Pixel * dest2 = (Pixel *) ((u8 *) surface->pixels + (y * 4 + 1) * surface->pitch);
            Pixel * dest3 = (Pixel *) ((u8 *) surface->pixels + (y * 4 + 2) * surface->pitch);
            Pixel * dest4 = (Pixel *) ((u8 *) surface->pixels + (y * 4 + 3) * surface->pitch);
            for (int x = 0; x < canvas->width; ++x) {
                dest1[x * 4 + 0] = src[x];
                dest1[x * 4 + 1] = src[x];
                dest1[x * 4 + 2] = src[x];
                dest1[x * 4 + 3] = src[x];
                dest2[x * 4 + 0] = src[x];
                dest2[x * 4 + 1] = src[x];
                dest2[x * 4 + 2] = src[x];
                dest2[x * 4 + 3] = src[x];
                dest3[x * 4 + 0] = src[x];
                dest3[x * 4 + 1] = src[x];
                dest3[x * 4 + 2] = src[x];
                dest3[x * 4 + 3] = src[x];
                dest4[x * 4 + 0] = src[x];
                dest4[x * 4 + 1] = src[x];
                dest4[x * 4 + 2] = src[x];
                dest4[x * 4 + 3] = src[x];
            }
        }
    } else {
        for (int y = 0; y < canvas->height; ++y) {
            Pixel * row = canvas->pixels + y * canvas->pitch;
            for (int x = 0; x < canvas->width; ++x) {
                //write into dest pixels
                for (int yy = 0; yy < scale; ++yy) {
                    Pixel * dest =
                        (Pixel *)((u8 *)surface->pixels + (y * scale + yy) * surface->pitch);
                    for (int xx = 0; xx < scale; ++xx) {
                        dest[x * scale + xx] = row[x];
                    }
                }
            }
        }
    }
}
