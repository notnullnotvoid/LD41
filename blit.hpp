// HEAVYWEIGHT CANVAS OPERATIONS DEFINED HERE

#ifndef BLIT_HPP
#define BLIT_HPP

#include <SDL.h>

#include "types.hpp"

//match window surface's byte order for faster blit
typedef struct Pixel {
    u8 b, g, r, a;
} Color;

struct Canvas {
    Pixel * pixels;
    int width;
    int height;
    int pitch; //number of pixels, NOT number of bytes!
};

Canvas create_canvas(int width, int height, int margin);
void fast_scaled_blit(SDL_Surface * surface, Canvas * canvas, int scale);

#endif // BLIT_HPP
