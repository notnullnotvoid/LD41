//NOTES
//TODO: print frame timings

//reject ideas
    //sokoban stealth game (you push blocks around to push guards around to not get caught)

//editor feature
    //1 bit : direction of triangulation
    //4 bit : slope of first triangle
    //4 bit : slope of second triangle
//cheap-ass backward compatibility of file format
    //reserve a few kilbytes (maybe 4k?) struct space for info and relative pointers
    //append-only (deprecate by prepending underscores to the field name)
//actions
    //copy tile under cursor (right click)
    //left click + drag to paste
    //expand/contract one of the four edges
    //move view (arrow keys or right click drag?)

//(we ended up just using Tiled as a map editor and importing the levels as .csv)



//TODO: write filled (float-precision) circle drawing routine?
//TODO: float-precision lines?
    //(see if I can find this algorithm in early blockade runner commits
    //  or that one processing sketch...)

//TODO: add visual indicator for when you can and can't start a putt



//options
    //music volume
    //sfx volume
    //overlay?
    //reset progress (button)



//TODO: constrain follow cam to level bounds
//TODO: pause menu
    //volume controls
    //restart button?
    //???

//TODO: better font rendering?
//TODO: fix collision bounds for bumper tiles
//TODO: fix collision bugs (think on this)
//TODO: overlay toggle?
//TODO: save/load progress?
//TODO: background sky effects?
//TODO: gamepad support?

//eventual
//TODO: code cleanup
//TODO: optimizations! yay!
    //faster renderer
        //front-to-back rendering
        //sse optimization
    //audio streaming
//controller support

//figure out where to go from here (if we want to make a larger game out of this)

////////////////////////////////////////////////////////////////////////////////
/// INCLUDES                                                                 ///
////////////////////////////////////////////////////////////////////////////////

#include <SDL.h>

#include "types.hpp"
#include "math.hpp"
#include "blit.hpp"
#include "ArrayList.hpp"
#include "lib/stb_image.h"

#include "soloud.h"
#include "soloud_wav.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
/// DYNAMIC LIBRARY LOADING                                                  ///
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

Uint64 (* PSDL_GetPerformanceCounter) ();
Uint64 (* PSDL_GetPerformanceFrequency) ();
int (* PSDL_Init) (Uint32 flags);
const char * (* PSDL_GetError) ();
SDL_Window * (* PSDL_CreateWindow) (const char* title, int x, int y, int w, int h, Uint32 flags);
// SDL_Surface * (* PSDL_CreateRGBSurface) (Uint32 flags, int width, int height, int depth,
//                                          Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
int (* PSDL_PollEvent) (SDL_Event * event);
SDL_Surface * (* PSDL_GetWindowSurface) (SDL_Window * window);
int (* PSDL_UpdateWindowSurface) (SDL_Window * window);

#include <windows.h>

template <typename TYPE>
bool load_function(HMODULE handle, TYPE * func, const char * name) {
    *func = (TYPE) GetProcAddress(handle, name);
    if (*func == nullptr) {
        printf("FAILED TO LOAD FUNCTION %s\n", name);
        printf("Error code: %d\n", GetLastError());
        return false;
    }
    return true;
}

//we don't care to unload the DLL before application exit,
//so it's fine that we just throw away the handle inside these functions
//instead of returning it like we're "supposed" to
bool load_sdl_functions(const char * filepath) {
    HMODULE handle = LoadLibraryA(filepath);
    if (!handle) {
        printf("FAILED TO LOAD DYNAMIC LIBRARY: %s\n", filepath);
        printf("Error code: %d\n", GetLastError());
        return false;
    }

    if (!load_function(handle, &PSDL_GetPerformanceCounter, "SDL_GetPerformanceCounter"))
        return false;
    if (!load_function(handle, &PSDL_GetPerformanceFrequency, "SDL_GetPerformanceFrequency"))
        return false;
    if (!load_function(handle, &PSDL_Init, "SDL_Init"))
        return false;
    if (!load_function(handle, &PSDL_GetError, "SDL_GetError"))
        return false;
    if (!load_function(handle, &PSDL_CreateWindow, "SDL_CreateWindow"))
        return false;
    // if (!load_function(handle, &PSDL_CreateRGBSurface, "SDL_CreateRGBSurface"))
        // return false;
    if (!load_function(handle, &PSDL_PollEvent, "SDL_PollEvent"))
        return false;
    if (!load_function(handle, &PSDL_GetWindowSurface, "SDL_GetWindowSurface"))
        return false;
    if (!load_function(handle, &PSDL_UpdateWindowSurface, "SDL_UpdateWindowSurface"))
        return false;

    return true;
}

#define SDL_GetPerformanceCounter PSDL_GetPerformanceCounter
#define SDL_GetPerformanceFrequency PSDL_GetPerformanceFrequency
#define SDL_Init PSDL_Init
#define SDL_GetError PSDL_GetError
#define SDL_CreateWindow PSDL_CreateWindow
// #define SDL_CreateRGBSurface PSDL_CreateRGBSurface
#define SDL_PollEvent PSDL_PollEvent
#define SDL_GetWindowSurface PSDL_GetWindowSurface
#define SDL_UpdateWindowSurface PSDL_UpdateWindowSurface

#endif // _WIN32

////////////////////////////////////////////////////////////////////////////////
/// MISCELLANEOUS                                                            ///
////////////////////////////////////////////////////////////////////////////////

float dist(float x1, float y1, float x2, float y2) {
    return sqrtf(sq(x2 - x1) + sq(y2 - y1));
}

float dist(Vec2 l, Vec2 r) {
    return sqrtf(sq(r.x - l.x) + sq(r.y - l.y));
}

//[min, max) inclusive / exclusive
int random_int(int min, int max) {
    return rand() % (max - min) + min;
}

////////////////////////////////////////////////////////////////////////////////
/// SDL TIME                                                                 ///
////////////////////////////////////////////////////////////////////////////////

u64 applicationStartupTimeValue;

double get_time() {
    u64 currentTimeValue = SDL_GetPerformanceCounter();
    u64 diffTimeValue = currentTimeValue - applicationStartupTimeValue;
    double elapsedSeconds = (double)diffTimeValue / (double)SDL_GetPerformanceFrequency();
    return elapsedSeconds;
}

////////////////////////////////////////////////////////////////////////////////
/// AUDIO                                                                    ///
////////////////////////////////////////////////////////////////////////////////

SoLoud::Soloud loud;

SoLoud::Wav ball_hit_club;
SoLoud::Wav ball_hit_wall;
SoLoud::Wav ball_hole;
SoLoud::Wav ball_out;
SoLoud::Wav music_credits;
SoLoud::Wav music_ingame;
SoLoud::Wav music_menu;
SoLoud::Wav upgrade_get;

////////////////////////////////////////////////////////////////////////////////
/// DRAWING OPERATIONS                                                       ///
////////////////////////////////////////////////////////////////////////////////

const Color COLOR_GOLD = { 0, 159, 255, 255 };

void solid_rect_unsafe(Canvas * canvas, int rx, int ry, int rw, int rh, Color color) {
    for (int y = ry; y < ry + rh; ++y) {
        Pixel * row = canvas->pixels + y * canvas->pitch;
        for (int x = rx; x < rx + rw; ++x) {
            row[x] = color;
        }
    }
}

void solid_rect(Canvas * canvas, int rx, int ry, int rw, int rh, Color color) {
    int minx = imax(0, rx);
    int miny = imax(0, ry);
    int maxx = imin(canvas->width, rx + rw);
    int maxy = imin(canvas->height, ry + rh);
    solid_rect_unsafe(canvas, minx, miny, maxx - minx, maxy - miny, color);
}

//TODO: make and use unsafe versions of this and blend()
inline void set(Canvas * canvas, int x, int y, Color color) {
    if (x >= 0 && x < canvas->width && y >= 0 && y < canvas->height) {
        Pixel * row = canvas->pixels + y * canvas->pitch;
        row[x] = color;
    }
}

inline void composite(Canvas * canvas, int x, int y, Color color) {
    if (color.a != 0 && x >= 0 && x < canvas->width && y >= 0 && y < canvas->height) {
        Pixel * row = canvas->pixels + y * canvas->pitch;
        row[x] = color;
    }
}

//assumes destination alpha = 1
inline void blend(Canvas * canvas, int x, int y, Color color) {
    if (x >= 0 && x < canvas->width && y >= 0 && y < canvas->height) {
        Pixel * row = canvas->pixels + y * canvas->pitch;

        float alpha = sq(color.a / 255.0f);
        row[x].b = color.b * alpha + row[x].b * (1 - alpha);
        row[x].g = color.g * alpha + row[x].g * (1 - alpha);
        row[x].r = color.r * alpha + row[x].r * (1 - alpha);
    }
}

//NOTE: this is lifted from my 3D software renderer, which is why some things are done in
//      seeming unnecessary ways
void transparent_triangle(Canvas * canvas, Vec2 v1, Vec2 v2, Vec2 v3, Color color) {
    Vec2 triangle[3] = { v1, v2, v3 };

    struct Edge {
        Vec2 v1, v2; //sorted by y
        float yfactor; //factor for interpolating vertex attributes vertically
    };

    Edge edges[3];

    float miny = canvas->height;
    float maxy = 0;

    //convert triangles to edges
    for (int i = 0; i < 3; ++i) {
        Vec2 v1 = triangle[i];
        Vec2 v2 = triangle[(i + 1) % 3];

        //update the triangle's vertical extent
        miny = v1.y < miny? v1.y : miny;
        maxy = v1.y > maxy? v1.y : maxy;

        //sort vertices by y
        edges[i].v1 = v1.y < v2.y? v1 : v2;
        edges[i].v2 = v1.y < v2.y? v2 : v1;

        edges[i].yfactor = 1.0f/(edges[i].v2.y - edges[i].v1.y);
    }

    //convert the triangle's vertical extent to pixels
    int firstLine = miny + 1;
    int lastLine = maxy;
    //clamp vertical extent of triangle to within the screen for rasterization
    if (firstLine < 0) firstLine = 0;
    if (lastLine > canvas->height - 1) lastLine = canvas->height - 1;

    for (int y = firstLine; y <= lastLine; ++y) {
        // Pixel * row = canvas->pixels + y * canvas->pitch;

        //the current pixel row will be within the vertical extend of only two
        //of the three edges at any time, so find those two and discard the third
        Edge e1, e2;
        if (y < edges[0].v1.y || y > edges[0].v2.y) {
            e1 = edges[1];
            e2 = edges[2];
        } else if (y < edges[1].v1.y || y > edges[1].v2.y) {
            e1 = edges[0];
            e2 = edges[2];
        } else {
            e1 = edges[0];
            e2 = edges[1];
        }

        //calculate vertical blend amounts for this scanline
        float f1a = (e1.v2.y - y) * e1.yfactor;
        float f2a = (e2.v2.y - y) * e2.yfactor;
        float f1b = 1 - f1a;
        float f2b = 1 - f2a;

        //find intersection with each edge by interpolating x along the edge
        float x1 = f1a * e1.v1.x + f1b * e1.v2.x;
        float x2 = f2a * e2.v1.x + f2b * e2.v2.x;

        //sort edges based on intersections
        float minx = x1 < x2? x1 : x2;
        float maxx = x1 > x2? x1 : x2;
        if (x1 > x2) {
            swap(e1, e2);
            swap(f1a, f2a);
            swap(f1b, f2b);
        }

        //convert horizontal extent to pixels
        int first = minx + 1;
        int last = maxx;
        //clamp horizontal extent of scanline to within the screen
        if (first < 0) first = 0;
        if (last > canvas->height - 1) last = canvas->height - 1;

        for (int x = first; x <= last; ++x) {
            blend(canvas, x, y, color);
            // set(canvas, x, y, color);
        }
    }
}

void transparent_triangle(Canvas * canvas,
        float x1, float y1, float x2, float y2, float x3, float y3, Color color) {
    transparent_triangle(canvas, vec2(x1, y1), vec2(x2, y2), vec2(x3, y3), color);
}

//TODO: reduce code duplication?
void transparent_rect_unsafe(Canvas * canvas, int rx, int ry, int rw, int rh, Color color) {
    for (int y = ry; y < ry + rh; ++y) {
        for (int x = rx; x < rx + rw; ++x) {
            blend(canvas, x, y, color);
        }
    }
}

//TODO: reduce code duplication?
inline void transparent_rect(Canvas * canvas, int rx, int ry, int rw, int rh, Color color) {
    int minx = imax(0, rx);
    int miny = imax(0, ry);
    int maxx = imin(canvas->width, rx + rw);
    int maxy = imin(canvas->height, ry + rh);
    transparent_rect_unsafe(canvas, minx, miny, maxx - minx, maxy - miny, color);
}

void circle_point_impl(Canvas * canvas, int x, int y, int x0, int y0, Color color) {
    blend(canvas,  x + x0,  y + y0, color); blend(canvas,  y + x0,  x + y0, color);
    blend(canvas, -x + x0,  y + y0, color); blend(canvas, -y + x0,  x + y0, color);
    blend(canvas, -x + x0, -y + y0, color); blend(canvas, -y + x0, -x + y0, color);
    blend(canvas,  x + x0, -y + y0, color); blend(canvas,  y + x0, -x + y0, color);
}

//TODO: fix the fact that certain pixels at the edges of octants get draws multiple times,
//      causing visible artifacts when drawing partially transparent circles
void draw_circle(Canvas * canvas, int x0, int y0, int radius, bool thick, Color color) {
    int x = radius;
    int y = 0;
    int radiusError = 1 - x;

    while (x >= y) {
        circle_point_impl(canvas, x, y, x0, y0, color);
        ++y;
        if (radiusError < 0) {
            radiusError += 2 * y + 1;
        } else {
            if (thick) {
                circle_point_impl(canvas, x - 1, y - 1, x0, y0, color);
            }
            --x;
            radiusError += 2 * (y - x + 1);
        }
    }
}

bool draw_thick_line(Canvas * canvas, int x1, int y1, int x2, int y2, Color color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);

    int sx = x1 < x2? 1 : -1;
    int sy = y1 < y2? 1 : -1;

    int err = dx - dy;

    int x = x1;
    int y = y1;

    while (true) {
        { //op goes here
            if (x != x1 || y != y1) {
                blend(canvas, x, y, color);
            }
        } //op ends here

        if (x == x2 && y == y2) {
            return true;
        }

        int e2 = 2 * err;

        if (e2 > -dy) {
            err -= dy;
            x += sx;

            { //op goes here
                blend(canvas, x, y, color);
            } //op ends here
        }

        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

bool draw_thick_line(Canvas * canvas, Vec2 p1, Vec2 p2, Color color) {
    return draw_thick_line(canvas, p1.x, p1.y, p2.x, p2.y, color);
}

void draw_line(Canvas * canvas, int x1, int y1, int x2, int y2, Color color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);

    int sx = x1 < x2? 1 : -1;
    int sy = y1 < y2? 1 : -1;

    int err = dx - dy;

    int x = x1;
    int y = y1;

    while (true) {
        blend(canvas, x, y, color);

        if (x == x2 && y == y2) {
            return;
        }

        int e2 = 2 * err;

        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
    }
}

bool draw_thicc_line(Canvas * canvas, int x1, int y1, int x2, int y2, float r, Color color) {
    float dx = abs(x2 - x1);
    float dy = abs(y2 - y1);

    if (dx < dy) {
        int ir = r * dist(x1, y1, x2, y2) / dy;
        for (int i = -ir; i <= ir; ++i) {
            draw_line(canvas, x1 + i, y1, x2 + i, y2, color);
        }
    } else {
        int ir = r * dist(x1, y1, x2, y2) / dx;
        for (int i = -ir; i <= ir; ++i) {
            draw_line(canvas, x1, y1 + i, x2, y2 + i, color);
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// FONT OPERATIONS                                                          ///
////////////////////////////////////////////////////////////////////////////////

//TODO: make font rendering use tileset ops?

struct MonoFont {
    u8 * pixels;
    int textureWidth, textureHeight;
    int glyphWidth, glyphHeight;
    int rows, columns;
};

MonoFont load_mono_font(const char * filepath, int rows, int columns) {
    int stbx, stby, stbn;
    Pixel * stbdata = (Pixel *) stbi_load(filepath, &stbx, &stby, &stbn, 4);
    if (stbdata == nullptr) {
        printf("FAILED TO LOAD FONT TEXTURE: %s\n", filepath);
    }

    //extract only the alpha channel, becaues for fonts that's all we care about
    u8 * data = (u8 *) malloc(stbx * stby * sizeof(u8));
    for (int i = 0; i < stbx * stby; ++i) {
        data[i] = stbdata[i].a;
    }
    stbi_image_free(stbdata);

    MonoFont font = {};
    font.pixels = data;
    font.textureWidth = stbx;
    font.textureHeight = stby;
    font.glyphWidth = stbx / columns;
    font.glyphHeight = stby / rows;
    font.rows = rows;
    font.columns = columns;

    return font;
}

void draw_glyph(Canvas * canvas, MonoFont font, int cx, int cy, Color color, int glyph) {
    int row = glyph / font.columns;
    int col = glyph % font.columns;
    int srcx = col * font.glyphWidth;
    int srcy = row * font.glyphHeight;

    //"blit" glyph to the screen
    for (int y = 0; y < font.glyphHeight; ++y) {
        for (int x = 0; x < font.glyphWidth; ++x) {
            int idx = (srcy + y) * font.textureWidth + (srcx + x);
            if (font.pixels[idx] > 0) {
                blend(canvas, cx + x, cy + y, color);
            }
        }
    }
}

void draw_text(Canvas * canvas, MonoFont font, int cx, int cy, Color color, const char * text) {
    for (int i = 0; text[i] != '\0'; ++i) {
        draw_glyph(canvas, font, cx + i * font.glyphWidth, cy, color, text[i]);

        //draw extenders
        if (text[i] == 'g' || text[i] == 'y') {
            draw_glyph(canvas, font, cx + i * font.glyphWidth, cy + font.glyphHeight, color, 16);
        } else if (text[i] == 'j') {
            draw_glyph(canvas, font, cx + i * font.glyphWidth, cy + font.glyphHeight, color, 19);
        } else if (text[i] == 'p') {
            draw_glyph(canvas, font, cx + i * font.glyphWidth, cy + font.glyphHeight, color, 17);
        } else if (text[i] == 'q') {
            draw_glyph(canvas, font, cx + i * font.glyphWidth, cy + font.glyphHeight, color, 18);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/// SPRITE OPERATIONS                                                        ///
////////////////////////////////////////////////////////////////////////////////

struct Image {
    int width;
    int height;
    Pixel * pixels;
};

typedef Image Sprite;

struct Tileset {
    int tileWidth;
    int tileHeight;
    int width;
    int height;
    Image image;
};

Image load_image(const char * filepath) {
    int w, h, c;
    Pixel * pixels = (Pixel *) stbi_load(filepath, &w, &h, &c, 4);
    assert(pixels);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            swap(pixels[y * w + x].b, pixels[y * w + x].r);
        }
    }
    return { w, h, pixels };
}

Tileset load_tileset(const char * filepath, int tileWidth, int tileHeight) {
    Image image = load_image(filepath);
    return { tileWidth, tileHeight, image.width / tileWidth, image.height / tileHeight, image };
}

Pixel get(Image image, int x, int y) {
    return image.pixels[y * image.width + x];
}

void draw_sprite(Canvas * canvas, Tileset set, int tx, int ty, int cx, int cy) {
    //destination coords
    int minx = imax(0, cx);
    int miny = imax(0, cy);
    int maxx = imin(canvas->width, cx + set.tileWidth);
    int maxy = imin(canvas->height, cy + set.tileHeight);

    //source coords
    int srcx = tx * set.tileWidth + (minx - cx);
    int srcy = ty * set.tileHeight + (miny - cy);
    int srcw = maxx - minx;
    int srch = maxy - miny;

    for (int y = 0; y < srch; ++y) {
        // Pixel * row = canvas->pixels + (miny + y) * canvas->pitch;
        for (int x = 0; x < srcw; ++x) {
            ::composite(canvas, minx + x, miny + y, get(set.image, srcx + x, srcy + y));
            // Color color = get(set.image, srcx + x, srcy + y);
            // if (color.a != 0) {
            //     row[minx + x] = color;
            // }
        }
    }
}

void draw_sprite(Canvas * canvas, Image image, int cx, int cy) {
    //destination coords
    int minx = imax(0, cx);
    int miny = imax(0, cy);
    int maxx = imin(canvas->width, cx + image.width);
    int maxy = imin(canvas->height, cy + image.height);

    //source coords
    int srcx = minx - cx;
    int srcy = miny - cy;
    int srcw = maxx - minx;
    int srch = maxy - miny;

    for (int y = 0; y < srch; ++y) {
        for (int x = 0; x < srcw; ++x) {
            ::composite(canvas, minx + x, miny + y, get(image, srcx + x, srcy + y));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/// LEVEL OPERATIONS                                                         ///
////////////////////////////////////////////////////////////////////////////////

struct Tile {
    i8 x;
    i8 y;
};

//XXX: remember to reserve space after this struct when writing to disk
struct Layer {
    int width;
    int height;
    Tile * tiles;
};

char * load_entire_file(const char * filepath) {
    FILE * f = fopen(filepath, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  //same as rewind(f);

    char * string = (char *) malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);

    string[fsize] = 0;

    return string;
}

int count_ch(char * text, char ch) {
    int count = 0;
    while (*text != '\0') {
        if (*text == ch) {
            ++count;
        }
        ++text;
    }
    return count;
}

ArrayList<char *> split_lines(char * text) {
    ArrayList<char *> list = create_array_list<char *>(count_ch(text, '\n'));
    char * token = strtok(text, "\r\n");
    while (token) {
        list.add(token);
        token = strtok(nullptr, "\r\n");
    }
    return list;
}

Layer load_csv(const char * filepath, Tileset set) {
    char * text = load_entire_file(filepath);
    ArrayList<char *> lines = split_lines(text);
    fflush(stdout);
    Layer layer = {};
    // printf("w: %d\n", layer.width = count_ch(lines[0], ',') + 1);
    // printf("h: %d\n", layer.height = lines.len);
    layer.width = count_ch(lines[0], ',') + 1;
    layer.height = lines.len;
    layer.tiles = (Tile *) malloc(layer.width * layer.height * sizeof(Tile));
    for (int i = 0; i < lines.len; ++i) {
        char * token = strtok(lines[i], ",");
        for (int n = 0; token; ++n) {
            int t = atoi(token);
            // printf("%d,", t);
            layer.tiles[i * layer.width + n] =
                { (i8)(t % set.width), (i8)(t / set.width) };
            token = strtok(nullptr, ",");
        }
        // printf("\n");
    }

    free(text);
    lines.finalize();

    //debug
    // printf("w: %d\n", layer.width);
    // printf("h: %d\n", layer.height);
    // for (int y = 0; y < layer.height; ++y) {
    //     for (int x = 0; x < layer.width; ++x) {
    //         Tile t = layer.tiles[y * layer.width + x];
    //         printf("%d,", t.x + (int)t.y * set.tileWidth);
    //     }
    //     printf("\n");
    // }

    return layer;
}

struct Button {
    int x, y;
    int idx; //sprite index
    float time;
};

struct Upgrade {
    int x, y;
    int idx;
};

struct Level {
    ArrayList<Layer> layers;
    ArrayList<Button> buttons;
    ArrayList<Upgrade> upgrades;
    float buttonTimer;
    int layerCount;
};

#include <initializer_list>

Level load_level(std::initializer_list<const char *> paths, Tileset set) {
    Level level = {};
    level.layers = create_array_list<Layer>();

    for (auto it = paths.begin(); it != paths.end(); ++it) {
        level.layers.add(load_csv(*it, set));
    }

    level.layerCount = level.layers.len - 1;

    level.buttons = create_array_list<Button>();
    level.upgrades = create_array_list<Upgrade>();

    //TODO: actually store this somehow instead of hardcoding it!
    level.buttons.add({ 40 * 16 + 4, 66 * 16 + 4, 2, 1.0f });
    level.buttons.add({ 15 * 16 + 8, 69 * 16 + 8, 1, 4.0f });
    level.buttons.add({ 10 * 16 + 8, 51 * 16 + 8, 1, 2.5f });
    level.buttons.add({ 16 * 16 + 8, 27 * 16 + 8, 0, 2.5f });
    level.buttons.add({ 45 * 16 + 8, 12 * 16 + 8, 2, 1.0f });
    level.buttons.add({ 88 * 16 + 8, 27 * 16 + 8, 2, 1.0f });
    level.buttons.add({ 98 * 16 + 8, 81 * 16 + 8, 1, 2.0f });
    level.buttons.add({ 81 * 16 + 8, 47 * 16 + 8, 2, 1.0f });
    level.buttons.add({ 94 * 16 + 8, 72 * 16 + 8, 3, 0.0f });

    level.upgrades.add({  26 * 16 + 8,  9 * 16 + 8, 0 });
    level.upgrades.add({ 121 * 16 + 8, 14 * 16 + 8, 1 });
    level.upgrades.add({  76 * 16 + 8, 76 * 16 + 8, 2 });

    return level;
}

////////////////////////////////////////////////////////////////////////////////
/// TILE OPs                                                                 ///
////////////////////////////////////////////////////////////////////////////////

//TD: draw edges of map
//TD: allow navigating with arrow keys
//TO: allow navigating with mouse drag
//TD: allow expanding/contracting of map edges
//TD: allow cycling of tile type under cursor with mouse wheel
    // (draw tiles on map as different colors)
//TD: right click to copy tile
    // (show buffered tile somewhere)
//TD: left click and drag to place copied tile
//TD: number keys to set direction of first triangle (or entire tile?)
    // draw correct color for a given direction
//TD: toggle which triangle we're changing
//TD: draw triangle slopes correctly

//TD: allow swapping the slopes of the two triangles???
//TD: save to disk
//TD: load from disk
//TD: draw actual sprites!

enum TileType {
    TILE_NONE = 0,
    TILE_BEGIN_TERRAIN,
    TILE_STONE,
    TILE_LIGHT,
    TILE_GREEN,
    TILE_ROUGH,
    TILE_SAND,
    TILE_END_TERRAIN,
    TILE_BEGIN_HAZARD,
    TILE_BRUSH,
    TILE_WATER,
    TILE_PIT,
    TILE_END_HAZARD,
    TILE_BEGIN_WALL,
    TILE_WALL,
    TILE_TREE,
    TILE_BUMPER,
    TILE_BARRIER,
    TILE_END_WALL,
    TILE_COUNT,
};

const char * TileTypeString[TILE_COUNT + 1] = {
    "TILE_NONE",
    "TILE_BEGIN_TERRAIN",
    "TILE_STONE",
    "TILE_LIGHT",
    "TILE_GREEN",
    "TILE_ROUGH",
    "TILE_SAND",
    "TILE_END_TERRAIN",
    "TILE_BEGIN_HAZARD",
    "TILE_BRUSH",
    "TILE_WATER",
    "TILE_PIT",
    "TILE_END_HAZARD",
    "TILE_BEGIN_WALL",
    "TILE_WALL",
    "TILE_TREE",
    "TILE_BUMPER",
    "TILE_BARRIER",
    "TILE_END_WALL",
    "TILE_COUNT",
};

enum DiagType {
    DIAG_DN, //left to right
    DIAG_UP,
};

const char * DiagTypeString[] = {
    "DIAG_DN",
    "DIAG_UP",
};

enum TileDir {
    DIR_NONE = 0, //empty triangle
    DIR_SW = 1,
    DIR_S,
    DIR_SE,
    DIR_W,
    DIR_UP,
    DIR_E,
    DIR_NW,
    DIR_N,
    DIR_NE,
    DIR_WALL,
    DIR_HAZARD,
    DIR_COUNT,
};

const char * TileDirString[DIR_COUNT + 1] = {
    "DIR_NONE",
    "DIR_SW",
    "DIR_S",
    "DIR_SE",
    "DIR_W",
    "DIR_UP",
    "DIR_E",
    "DIR_NW",
    "DIR_N",
    "DIR_NE",
    "DIR_WALL",
    "DIR_HAZARD",
    "DIR_COUNT",
};

struct TileInfo {
    u8 type;
    u8 diag;
    u8 dir1;
    u8 dir2;
};

TileInfo get_tile_info(Tile tile) {
    if (tile.x < 0 || tile.y < 0) {
        return { TILE_NONE };
    }

    struct Region {
        int x, y, w, h;
        TileType type;
    };

    Region regions[] = {
        { 15, 11, 6, 5, TILE_STONE   },
        {  0,  0, 6, 5, TILE_GREEN   },
        {  6,  0, 6, 5, TILE_LIGHT   },
        {  0,  5, 6, 5, TILE_ROUGH   },
        { 13,  0, 6, 5, TILE_SAND    },

        {  7,  5, 5, 3, TILE_BRUSH   },
        { 18,  5, 5, 3, TILE_WATER   },
        { 18,  8, 5, 3, TILE_PIT     },

        { 13,  5, 5, 3, TILE_WALL    },
        {  7,  8, 5, 3, TILE_TREE    },
        { 13,  8, 5, 3, TILE_BUMPER  },
        { 19,  0, 1, 3, TILE_BARRIER },
    };

    TileInfo slopeInfo[5][6] = {
        {
            { TILE_NONE, DIAG_DN, DIR_NE, DIR_SW },
            { TILE_NONE, DIAG_UP, DIR_NW, DIR_SE },
            { TILE_NONE, DIAG_UP, DIR_UP, DIR_NW },
            { TILE_NONE, DIAG_UP, DIR_NW, DIR_UP },
            { TILE_NONE, DIAG_DN, DIR_UP, DIR_NE },
            { TILE_NONE, DIAG_DN, DIR_NE, DIR_UP },
        },
        {
            { TILE_NONE, DIAG_UP, DIR_NONE, DIR_UP },
            { TILE_NONE, DIAG_DN, DIR_UP, DIR_UP },
            { TILE_NONE, DIAG_DN, DIR_UP, DIR_NONE },
            { TILE_NONE, DIAG_DN, DIR_NW, DIR_NW },
            { TILE_NONE, DIAG_DN, DIR_N , DIR_N  },
            { TILE_NONE, DIAG_DN, DIR_NE, DIR_NE },
        },
        {
            { TILE_NONE, DIAG_DN, DIR_UP, DIR_UP },
            { TILE_NONE, DIAG_DN, DIR_UP, DIR_UP },
            { TILE_NONE, DIAG_DN, DIR_UP, DIR_UP },
            { TILE_NONE, DIAG_DN, DIR_W , DIR_W  },
            { TILE_NONE, DIAG_DN, DIR_UP, DIR_UP },
            { TILE_NONE, DIAG_DN, DIR_E , DIR_E  },
        },
        {
            { TILE_NONE, DIAG_DN, DIR_NONE, DIR_UP },
            { TILE_NONE, DIAG_DN, DIR_UP, DIR_UP },
            { TILE_NONE, DIAG_UP, DIR_UP, DIR_NONE },
            { TILE_NONE, DIAG_DN, DIR_SW, DIR_SW },
            { TILE_NONE, DIAG_DN, DIR_S , DIR_S  },
            { TILE_NONE, DIAG_DN, DIR_SE, DIR_SE },
        },
        {
            { TILE_NONE, DIAG_DN, DIR_SW, DIR_NE },
            { TILE_NONE, DIAG_UP, DIR_SE, DIR_NW },
            { TILE_NONE, DIAG_DN, DIR_UP, DIR_SW },
            { TILE_NONE, DIAG_DN, DIR_SW, DIR_UP },
            { TILE_NONE, DIAG_UP, DIR_UP, DIR_SE },
            { TILE_NONE, DIAG_UP, DIR_SE, DIR_UP },
        },
    };

    TileInfo info = {};
    for (int i = 0; i < ARR_SIZE(regions); ++i) {
        Region r = regions[i];
        if (tile.x >= r.x && tile.y >= r.y && tile.x < r.x + r.w && tile.y < r.y + r.h) {
            info.type = r.type;

            if (info.type > TILE_BEGIN_TERRAIN && info.type < TILE_END_TERRAIN) {
                TileInfo slope = slopeInfo[tile.y - r.y][tile.x - r.x];
                slope.type = info.type;
                info = slope;
            } else if (info.type > TILE_BEGIN_HAZARD && info.type < TILE_END_HAZARD) {
                int tx = tile.x - r.x;
                int ty = tile.y - r.y;

                info.diag = DIAG_DN;
                info.dir1 = DIR_HAZARD;
                info.dir2 = DIR_HAZARD;

                //TODO: maybe just replace this with a lookup table...
                if (tx == 0) {
                    if (ty != 1) {
                        info.dir1 = DIR_NONE;
                    }
                    if (ty == 0) {
                        info.diag = DIAG_UP;
                    }
                } else if (tx == 2) {
                    if (ty != 1) {
                        info.dir2 = DIR_NONE;
                    }
                    if (ty == 2) {
                        info.diag = DIAG_UP;
                    }
                }
            } else if (info.type > TILE_BEGIN_WALL && info.type < TILE_END_WALL) {
                //TODO: de-dup with above
                int tx = tile.x - r.x;
                int ty = tile.y - r.y;

                info.diag = DIAG_DN;
                info.dir1 = DIR_WALL;
                info.dir2 = DIR_WALL;

                if (info.type != TILE_BARRIER) {
                    if (tx == 0) {
                        if (ty != 1) {
                            info.dir1 = DIR_NONE;
                        }
                        if (ty == 0) {
                            info.diag = DIAG_UP;
                        }
                    } else if (tx == 2) {
                        if (ty != 1) {
                            info.dir2 = DIR_NONE;
                        }
                        if (ty == 2) {
                            info.diag = DIAG_UP;
                        }
                    }
                }
            }
        }
    }

    return info;
}

TileInfo get_info(Vec2 pos, Layer layer, Tileset tileset) {
    int bx = pos.x / tileset.tileWidth;
    int by = pos.y / tileset.tileHeight;

    TileInfo info = {};
    //TODO: invert this condition
    //TODO: factor this out with the other int rect code into something reusable
    if (!(bx < 0 || bx >= layer.width || by < 0 || by >= layer.height)) {
        info = get_tile_info(layer.tiles[by * layer.width + bx]);
    }

    return info;
}

TileDir get_slope(Vec2 pos, Layer layer, Tileset tileset) {
    //TODO: de-dupe this
    int bx = pos.x / tileset.tileWidth;
    int by = pos.y / tileset.tileHeight;

    float rx = pos.x - bx * tileset.tileWidth;
    float ry = pos.y - by * tileset.tileHeight;

    TileInfo info = get_info(pos, layer, tileset);
    if (info.diag == DIAG_UP) {
        //FIXME: assumes tiles are square
        return rx + ry < tileset.tileWidth? (TileDir) info.dir1 : (TileDir) info.dir2;
    } else {
        //FIXME: assumes tiles are square
        return rx < ry? (TileDir) info.dir1 : (TileDir) info.dir2;
    }
}

DiagType get_diag(Vec2 pos, Layer layer, Tileset tileset) {
    return (DiagType) get_info(pos, layer, tileset).diag;
}

TileType get_type(Vec2 pos, Layer layer, Tileset tileset) {
    return (TileType) get_info(pos, layer, tileset).type;
}

//TODO: further reduce the massive amount of code duplication happening here
TileDir get_slope(Vec2 pos, Level level, Tileset tileset) {
    for (int i = level.layerCount - 1; i >= 0; --i) {
        TileDir dir = get_slope(pos, level.layers[i], tileset);
        if (dir != DIR_NONE) {
            return dir;
        }
    }

    return DIR_NONE;
}

TileType get_type(Vec2 pos, Level level, Tileset tileset) {
    // return (TileType) get_info(pos, level, tileset).type;
    for (int i = level.layerCount - 1; i >= 0; --i) {
        TileDir dir = get_slope(pos, level.layers[i], tileset);
        if (dir != DIR_NONE) {
            return get_type(pos, level.layers[i], tileset);
        }
    }

    return TILE_NONE;
}

DiagType get_diag(Vec2 pos, Level level, Tileset tileset) {
    // return (DiagType) get_info(pos, level, tileset).diag;
    for (int i = level.layerCount - 1; i >= 0; --i) {
        TileDir dir = get_slope(pos, level.layers[i], tileset);
        if (dir != DIR_NONE) {
            return get_diag(pos, level.layers[i], tileset);
        }
    }

    return DIAG_DN;
}

////////////////////////////////////////////////////////////////////////////////
/// MAIN FUNCTION                                                            ///
////////////////////////////////////////////////////////////////////////////////

float minBallVel = 10.0f;
float respawnTime = 2.0f;
float flashSpeed = 0.2f;

struct Coord {
    int x, y;
};

//TODO: refactor other parts of the code to use this
Coord get_coord(Vec2 pos, Tileset set) {
    return { (int)(pos.x / set.tileWidth), (int)(pos.y / set.tileHeight) };
}

struct Ball {
    Vec2 pos;
    Vec2 vel;
    Image sprite;
    Vec2 oldPos;
    float respawnTimer;
    float turn;
    bool stopped;
    bool inverted;

    bool hasTurn;
    bool hasStop;
    bool hasInvert;
};

Ball update(Ball ball, float tick, Level level, Tileset tileset, bool sound) {
    if (ball.stopped) {
        ball.vel = {};
        return ball;
    }

    Vec2 slopeAcc[DIR_COUNT] = { //starts at 0 (DIR_NONE)
        {  0,  0 }, //NONE
        { -1,  1 },
        {  0,  1 },
        {  1,  1 },
        { -1,  0 },
        {  0,  0 },
        {  1,  0 },
        { -1, -1 },
        {  0, -1 },
        {  1, -1 },
        {  0,  0 }, //WALL
        {  0,  0 },
    };

    Vec2 acc = slopeAcc[get_slope(ball.pos, level, tileset)];
    if (ball.inverted) {
        acc *= -1;
    }

    // if (len(ball.vel) < minBallVel ) {
        // ball.vel = {};
    // } else {
        //x = constant slowdown factor, y = proportional slowdown factor
        Vec2 tileDec[TILE_COUNT] = { //starts at 0
            {},
            {},
            { 0.04f, 0.004f },
            { 0.06f, 0.005f },
            { 0.10f, 0.01f },
            { 0.20f, 0.02f },
            { 0.32f, 0.04f },
        };

        // Vec2 dec = vec2(0.1f, 0.01f);
        Vec2 dec = tileDec[get_type(ball.pos, level, tileset)];
        // printf("dec %f %f\n", dec.x, dec.y);

        ball.vel += acc * 200 * tick;
        //TODO: create nor() function
        ball.vel -= noz(ball.vel) * dec.x;
        ball.vel -= ball.vel * dec.y;
        // ball.vel *= 0.98f;
    // }

    Vec2 turn = vec2(ball.vel.y, -ball.vel.x);
    turn = turn * len(turn) * ball.turn * tick * 0.02f;
    // ball.vel = noz(ball.vel + turn) * len(ball.vel);
    ball.vel += turn;

    // ball.pos += ball.vel * tick;
    Vec2 pos = ball.pos + ball.vel * tick;

    //collision
    TileDir dir = get_slope(pos, level, tileset);
    if (dir == DIR_WALL) {
        if (sound) loud.play(ball_hit_wall);

        Coord a = get_coord(ball.pos, tileset);
        Coord b = get_coord(pos, tileset);

        if (a.x == b.x && a.y == b.y) {
            DiagType diag = get_diag(pos, level, tileset);
            //reflect across y = x
            swap(ball.vel.x, ball.vel.y);
            if (diag == DIAG_UP) {
                //invert
                ball.vel *= -1;
            }
        } else {
            if (a.x != b.x) {
                ball.vel.x *= -1;
            }
            if (a.y != b.y) {
                ball.vel.y *= -1;
            }
        }
    } else if (dir == DIR_HAZARD || dir == DIR_NONE) {
        if (sound) loud.play(ball_out);

        ball.vel = {};
        ball.pos = ball.oldPos;
        ball.respawnTimer = 0;
    }

    // ball.pos = pos;
    ball.pos += ball.vel * tick;

    return ball;
}

//SDL defines main to SDL_Main, which causes errors when linking manually on Windows
#ifdef _WIN32
# undef main
#endif

int main(int argc, char ** argv) {
#ifdef _WIN32
    bool success = load_sdl_functions("link/SDL2.dll");
    if (!success) {
        printf("exiting application because we couldn't load SDL dynamically\n");
        exit(1);
    }
#endif

    //initialize timer and startup SDL
    applicationStartupTimeValue = SDL_GetPerformanceCounter();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER)) {
        printf("SDL FAILED TO INIT: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL init: %f seconds\n", get_time());



    const int gameWidth = 192*3; //576
    const int gameHeight = 120*3; //360
    const int gameScale = 2;



    SDL_Window * window = SDL_CreateWindow("Test Window",
        SDL_WINDOWPOS_CENTERED_DISPLAY(1), SDL_WINDOWPOS_CENTERED_DISPLAY(1),
        gameWidth * gameScale, gameHeight * gameScale,
        SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        printf("SDL FAILED TO CREATE WINDOW: %s\n", SDL_GetError());
        return 1;
    }

    printf("SDL create window: %f seconds\n", get_time());

    Canvas canv = create_canvas(gameWidth, gameHeight, 16);
    Canvas * canvas = &canv;

    printf("SDL full init: %f seconds\n", get_time());

    //init audio things

    printf("init: %d\n", loud.init());
    ball_hit_club.load("res/sound/ball_hit_club.ogg");
    ball_hit_wall.load("res/sound/ball_hit_wall.ogg");
    ball_hole.load("res/sound/ball_hole.ogg");
    ball_out.load("res/sound/ball_out.ogg");
    upgrade_get.load("res/sound/upgrade_get.ogg");
    music_menu.load("res/sound/music_menu.ogg");
    // music_ingame.load("res/sound/music_ingame.ogg");
    music_ingame.load("res/sound/music_menu.ogg"); //DEBUG
    music_credits.load("res/sound/music_credits.ogg");
    music_menu.setLooping(true);
    music_ingame.setLooping(true);
    music_credits.setLooping(true);
    //TODO: figure out how to detect errors in soloud calls
    //TODO: figure out why wav files won't load

    int musicHandle = loud.play(music_menu);

    printf("soloud audio init: %f seconds\n", get_time());

    Tileset tileset = load_tileset("res/tileset.png", 16, 16);
    Tileset switches = load_tileset("res/switch.png", 16, 16);
    Tileset upTiles = load_tileset("res/upgrades.png", 16, 16);

    Level level = load_level({
        "res/mapgoeshere/layer_sand.csv",
        "res/mapgoeshere/layer_green.csv",
        "res/mapgoeshere/layer_paving.csv",
        "res/mapgoeshere/layer_fairway.csv",
        "res/mapgoeshere/layer_rough.csv",
        "res/mapgoeshere/layer_out of bounds.csv",
        "res/mapgoeshere/layer_barriers.csv",
        "res/mapgoeshere/layer_water.csv",
        "res/mapgoeshere/layer_pit.csv",
        "res/mapgoeshere/layer_bush.csv",
        "res/mapgoeshere/layer_barrier1.csv",
        "res/mapgoeshere/layer_barrier2.csv",
        "res/mapgoeshere/layer_trees.csv",
    }, tileset);

    Ball ball = {};
    // ball.pos = vec2(320, 160);
    ball.pos = vec2(37 * 16, 83 * 16);
    ball.sprite = load_image("res/ball.png");
    Sprite cursor = load_image("res/cursor.png");

    Vec2 puttStart = {};
    Vec2 puttEnd = {};
    float puttFactor = 2;
    float maxPutt = 400;
    bool putting = false;
    bool stopped = true;

    int puttCount = 0;

    //load font
    MonoFont font = load_mono_font("res/font-16-white.png", 8, 16);

    printf("done initializing: %f seconds\n", get_time());
    fflush(stdout);



    bool leftKey = false;
    bool rightKey = false;
    bool upKey = false;
    bool downKey = false;



    //TODO: lord oh lord, clean up this awful gamemode code later
    int gameMode = 0; //0 = menu, 1 = play, 2 = win

    // struct Gamepad {
    //     SDL_Joystick * joy;
    //     SDL_GameController * con;
    //     SDL_JoystickID id;
    //     //maps to enumeration SDL_CONTROLLER_AXIS_*
    //     float axis[SDL_CONTROLLER_AXIS_MAX]; //6
    //     //maps to enumeration SDL_CONTROLLER_BUTTON_*
    //     bool button[SDL_CONTROLLER_BUTTON_MAX]; //15
    // };

    // const int MAX_PADS = 4;
    // int padCount = 0;
    // Gamepad gamepads[MAX_PADS] = {};

    int offx = 0;
    int offy = 0;
    Vec2 cam = ball.pos;

    float frameTimes[100] = {};
    float time = get_time();
    float lastTime = 0;

    float accumulator = 0;
    float shakeTimer = 1;
    float modeTimer = 0;

    bool shouldExit = false;
    int frameCount = 0;
    while (!shouldExit) {
        bool breakKey = false;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                shouldExit = true;
            // } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
            //     printf("CONTROLLER ADDED: %d, %d\n",
            //         event.cdevice.timestamp, event.cdevice.which);

            //     if (padCount < MAX_PADS) {
            //         gamepads[padCount].joy = SDL_JoystickOpen(event.cdevice.which);
            //         gamepads[padCount].con = SDL_GameControllerOpen(event.cdevice.which);
            //         gamepads[padCount].id = SDL_JoystickInstanceID(gamepads[padCount].joy);
            //         padCount += 1;
            //     }
            // } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            //     printf("CONTROLLER REMOVED: %d, %d\n",
            //         event.cdevice.timestamp, event.cdevice.which);

            //     for (int i = 0; i < MAX_PADS; ++i) {
            //         if (gamepads[i].id == event.cdevice.which) {
            //             printf("Found controller to disconnect (idx %d)\n", i);
            //             SDL_GameControllerClose(gamepads[i].con);
            //             SDL_JoystickClose(gamepads[i].joy);

            //             //remove controller from list
            //             for (int j = i + 1; j < MAX_PADS; ++j) {
            //                 gamepads[j - 1] = gamepads[j];
            //             }
            //             padCount -= 1;

            //             break;
            //         }
            //     }
            // } else if (event.type == SDL_CONTROLLERAXISMOTION) {
            //     DEBUG_ASSERT(event.caxis.axis < SDL_CONTROLLER_AXIS_MAX);

            //     for (int i = 0; i < padCount; ++i) {
            //         if (gamepads[i].id == event.caxis.which) {
            //             gamepads[i].axis[event.caxis.axis] =
            //                 fmax(event.caxis.value / 32767.0f, -1);
            //             break;
            //         }
            //     }
            // } else if (event.type == SDL_CONTROLLERBUTTONDOWN ||
            //            event.type == SDL_CONTROLLERBUTTONUP) {
            //     DEBUG_ASSERT(event.cbutton.button < SDL_CONTROLLER_BUTTON_MAX);

            //     for (int i = 0; i < padCount; ++i) {
            //         if (gamepads[i].id == event.cbutton.which) {
            //             gamepads[i].button[event.cbutton.button] = event.cbutton.state;
            //             break;
            //         }
            //     }
            } else if (gameMode == 0) {
                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                            loud.stop(musicHandle);
                            musicHandle = loud.play(music_ingame);
                            gameMode = 1;
                            modeTimer = 0;
                        }
                    }
            } else if (gameMode == 1) {
                if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                    if (event.key.keysym.scancode == SDL_SCANCODE_LEFT ||
                        event.key.keysym.scancode == SDL_SCANCODE_A) {
                        leftKey = event.type == SDL_KEYDOWN;
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_RIGHT ||
                               event.key.keysym.scancode == SDL_SCANCODE_D) {
                        rightKey = event.type == SDL_KEYDOWN;
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_UP ||
                               event.key.keysym.scancode == SDL_SCANCODE_W) {
                        upKey = event.type == SDL_KEYDOWN;
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_DOWN ||
                               event.key.keysym.scancode == SDL_SCANCODE_S) {
                        downKey = event.type == SDL_KEYDOWN;
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_LSHIFT ||
                               event.key.keysym.scancode == SDL_SCANCODE_RSHIFT) {
                        ball.inverted = event.type == SDL_KEYDOWN && ball.hasInvert;
                    }

                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.scancode == SDL_SCANCODE_GRAVE) {
                            breakKey = true;
                        } else if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                            if (ball.hasStop) {
                                ball.stopped = !ball.stopped;
                                shakeTimer = 0;
                            }
                        // } else if (event.key.keysym.scancode == SDL_SCANCODE_1) {
                        //     ball.hasTurn = !ball.hasTurn;
                        // } else if (event.key.keysym.scancode == SDL_SCANCODE_2) {
                        //     ball.hasStop = !ball.hasStop;
                        // } else if (event.key.keysym.scancode == SDL_SCANCODE_3) {
                        //     ball.hasInvert = !ball.hasInvert;
                        }
                    }
                } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (stopped && !ball.stopped) {
                            putting = true;
                            puttEnd = puttStart = vec2(event.button.x, event.button.y);
                        }
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        //DEBUG place ball
                        // Vec2 m = vec2(event.button.x, event.button.y);
                        // ball.pos = m * (1.0f / gameScale) - vec2(offx, offy);
                    }
                } else if (event.type == SDL_MOUSEMOTION) {
                    if (event.motion.state & SDL_BUTTON_LMASK) {
                        if (putting) {
                            puttEnd = vec2(event.button.x, event.button.y);
                        }
                    }
                } else if (event.type == SDL_MOUSEBUTTONUP) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (putting) {
                            loud.play(ball_hit_club);
                            puttCount += 1;
                            putting = false;
                            Vec2 diff = puttStart - puttEnd;
                            ball.vel = noz(diff) * fmin(len(diff) * puttFactor, maxPutt);
                            ball.oldPos = ball.pos;
                            ball.turn = 0;
                        }
                    }
                }
            } else { //XXX: copied from alakajam project - clean this up!!
                //nop?
            }
        }

        //update timestep
        lastTime = time;
        time = get_time();
        float dt = time - lastTime;

        modeTimer += dt;
        shakeTimer += dt;
        accumulator += dt;
        ball.respawnTimer += dt;
        level.buttonTimer -= dt;



        //TODO: limit the maximum number of ticks per frame so it's impossible to spinlock
        float tickLength = 1.0f/240;
        float gameSpeed = 1;//0.75f;
        while (accumulator > tickLength) {
            float tick = tickLength * gameSpeed;

            // if (!ball.hasTurn) {
            //     if (leftKey)  { offx += 2; }
            //     if (rightKey) { offx -= 2; }
            //     if (upKey)    { offy += 2; }
            //     if (downKey)  { offy -= 2; }
            // }

            //FIXME: why does this not work?
            // if (gameMode == 1) {
                ball = update(ball, tick, level, tileset, true);
            // }

            stopped = len(ball.vel) < minBallVel && ball.respawnTimer > respawnTime;

            if (!stopped) {
                putting = false;
            }

            //follow cam
            if (!stopped) {
                Vec2 diff = ball.pos - cam;
                cam += diff * 0.02f;
            }

            offx = gameWidth / 2 - cam.x;
            offy = gameHeight / 2 - cam.y;

            //curve/steer
            if (ball.hasTurn) {
                float turnFactor = 1;
                if (rightKey) {
                    ball.turn = fmax(-1, ball.turn - turnFactor * tick);
                } if (leftKey) {
                    ball.turn = fmin( 1, ball.turn + turnFactor * tick);
                }
            }

            //handle buttons
            for (int i = 0; i < level.buttons.len; ++i) {
                Button b = level.buttons[i];
                if (dist(ball.pos, vec2(b.x, b.y)) < 12) {
                    level.buttonTimer = b.time;

                    //XXX: magic number! aaaahh!!
                    if (b.idx == 3) {
                        if (gameMode == 1) loud.play(ball_hole);

                        loud.stop(musicHandle);
                        musicHandle = loud.play(music_credits);
                        gameMode = 2;
                        modeTimer = 0;
                    }
                }
            }

            if (level.buttonTimer > 0) {
                level.layerCount = level.layers.len - 2;
            } else {
                level.layerCount = level.layers.len - 1;
            }

            //handle upgrades
            for (int i = 0; i < level.upgrades.len; ++i) {
                Upgrade u = level.upgrades[i];
                if (dist(ball.pos, vec2(u.x, u.y)) < 8) {
                    loud.play(upgrade_get);

                    if (u.idx == 0) {
                        ball.hasTurn = true;
                    } else if (u.idx == 1) {
                        ball.hasStop = true;
                    } else if (u.idx == 2) {
                        ball.hasInvert = true;
                    }

                    level.upgrades.remove(i);
                    break;
                }
            }

            //eat the time
            accumulator -= tickLength;
        }

        // printf("%f\n", len(ball.vel));



        //clear the screen
        for (int y = 0; y < canvas->height; ++y) {
            Pixel * row = canvas->pixels + y * canvas->pitch;
            for (int x = 0; x < canvas->width; ++x) {
                row[x] = { 191, 127, 0, 255 };
            }
        }

        //fill the buffer with a test pattern
        // for (int y = 0; y < canvas->height; ++y) {
        //     Pixel * row = canvas->pixels + y * canvas->pitch;
        //     for (int x = 0; x < canvas->width; ++x) {
        //         row[x] = { (u8)x, (u8)(x * y), (u8)y, 255 };
        //         // row[x] = { (u8)(x * y), (u8)(x * y), (u8)(x * y), 255 };
        //     }
        // }

        if (breakKey) {
            shakeTimer = 0;
        }

        //TODO: screen shake when the ball gets reset?

        //screenshake if the player has just been hurt or a shot has been fired
        if (shakeTimer < 0.2f) {
            float factor = 1 - shakeTimer / 0.2f;
            int offShake = imax(1, factor * 5);
            offx += random_int(-offShake, offShake);
            offy += random_int(-offShake, offShake);
        }



        //draw tile map
        //TODO: only draw the subrect of tiles that are on screen (optimization)
        for (int i = 0; i < level.layerCount; ++i) {
            Layer layer = level.layers[i];
            for (int y = 0; y < layer.height; ++y) {
                for (int x = 0; x < layer.width; ++x) {
                    Tile t = layer.tiles[y * layer.width + x];
                    if (t.x >= 0 && t.y >= 0) {
                        draw_sprite(canvas, tileset, t.x, t.y,
                            x * tileset.tileWidth + offx, y * tileset.tileHeight + offy);
                    }
                }
            }
        }

        Vec2 off = vec2(offx, offy);

        //draw buttons
        for (int i = 0; i < level.buttons.len; ++i) {
            Button b = level.buttons[i];
            int x = level.buttonTimer > 0? 1 : 0;
            draw_sprite(canvas, switches, x, b.idx, b.x - 8 + offx, b.y - 8 + offy);
        }

        //draw upgrades
        for (int i = 0; i < level.upgrades.len; ++i) {
            Upgrade u = level.upgrades[i];
            draw_sprite(canvas, upTiles, u.idx, 0, u.x - 8 + offx, u.y - 8 + offy);
        }

        //draw predictive trail
        if (putting) {
            Ball phantom = ball;
            Vec2 diff = puttStart - puttEnd;
            //TODO: de-dupe this
            phantom.vel += noz(diff) * fmin(len(diff) * puttFactor, maxPutt);
            phantom.stopped = false;
            for (int i = 0; i < 255; ++i) {
                // Color red = { 0, 0, (u8)(255 - i), (u8)(255 - i) };
                Color red = { 0, 63, 191, (u8)(255 - i) };
                Vec2 before = phantom.pos;
                phantom = update(phantom, tickLength * gameSpeed, level, tileset, false);
                if (phantom.respawnTimer < respawnTime) {
                    break;
                }
                Vec2 after = phantom.pos;
                draw_thick_line(canvas, before + off, after + off, red);
            }
        }

        //draw ball
        if (ball.respawnTimer > respawnTime || (int)(ball.respawnTimer / flashSpeed) % 2) {
            draw_sprite(canvas, ball.sprite,
                ball.pos.x - ball.sprite.width/2 + offx, ball.pos.y - ball.sprite.height/2 + offy);
        }

        //draw trees
        //TODO: factor most of this into a function with the above duplicated code
        {
            Layer layer = level.layers[level.layers.len - 1];
            for (int y = 0; y < layer.height; ++y) {
                for (int x = 0; x < layer.width; ++x) {
                    Tile t = layer.tiles[y * layer.width + x];
                    if (t.x >= 0 && t.y >= 0) {
                        draw_sprite(canvas, tileset, t.x, t.y,
                            x * tileset.tileWidth + offx, y * tileset.tileHeight + offy);
                    }
                }
            }
        }

        //draw cursor
        if (putting) {
            // int x = puttStart.x / gameScale;
            draw_sprite(canvas, cursor,
                // puttStart.x - cursor.width + offx, puttStart.y - cursor.height + offy);
                puttStart.x / gameScale - cursor.width/2,
                puttStart.y / gameScale - cursor.height/2);
        }



        //DEBUG: draw triangle
        // transparent_triangle(canvas,
        //     layer.entities[0].pos.x + offx, layer.entities[0].pos.y + offy,
        //     layer.entities[1].pos.x + offx, layer.entities[1].pos.y + offy,
        //     layer.entities[2].pos.x + offx, layer.entities[2].pos.y + offy,
        //     { 0, 255, 255, 255 });



        //draw HUD text
        //TODO: optionally toggle the overlay
        // Color white = { 0, 0, 0, 255 };
        Color white = { 255, 255, 255, 255 };
        draw_text(canvas, font, font.glyphWidth, font.glyphHeight, white,
            "Sink the ball in as few strokes as possible!");

        // const char * instructions = "left click and drag to putt | right mouse to place ball";
        const char * instructions = "Left click and drag to aim, release to hit the ball";
        if (ball.hasTurn) instructions = "AD or \x15\x14: curve shot";
        if (ball.hasStop) instructions = "AD or \x15\x14: curve shot | SPACE: stop ball";
        if (ball.hasInvert)
            instructions = "AD or \x15\x14: curve shot | SPACE: stop ball | SHIFT: invert gravity";
        draw_text(canvas, font, font.glyphWidth, canvas->height - font.glyphHeight * 2, white,
            instructions);
            // "AD or \x15\x14: curve shot | SPACE: stop ball | SHIFT: invert gravity");
            // "\x16\x17\x15\x14: look     ");

        //DEBUG print type of tile under ball
        char buffer[200];
        // TileType type = get_type(ball.pos, level, tileset);
        // TileDir dir = get_slope(ball.pos, level, tileset);
        // sprintf(buffer, "%s %s %d %d %d", TileTypeString[type], TileDirString[dir],
        //     ball.hasTurn, ball.hasStop, ball.hasInvert);
        // draw_text(canvas, font, font.glyphWidth, 3 * font.glyphHeight, white, buffer);

        //print putt count
        sprintf(buffer, "strokes: %5d", puttCount);
        draw_text(canvas, font,
            canvas->width - font.glyphWidth * (strlen(buffer) + 1), font.glyphHeight,
            white, buffer);



        //tint the whole screen slightly red if the player has just been hurt
        // if (player.player.hurtTimer < 0.5f) {
        //     int additive = 255 * (1 - player.player.hurtTimer);
        //     for (int y = 0; y < canvas->height; ++y) {
        //         Pixel * row = canvas->pixels + y * canvas->pitch;
        //         for (int x = 0; x < canvas->width; ++x) {
        //             row[x].r = (u8)(imin(255, row[x].r + additive));
        //         }
        //     }
        // }



        //update sliding window filter for framerate
        float timeSum = 0;
        for (int i = 1; i < ARR_SIZE(frameTimes); ++i) {
            frameTimes[i - 1] = frameTimes[i];
            timeSum += frameTimes[i - 1];
        }
        frameTimes[ARR_SIZE(frameTimes) - 1] = dt;
        timeSum += dt;

        //print framerate every so often
        float framerate = ARR_SIZE(frameTimes) / timeSum;
        if (frameCount % 100 == 99) {
            printf("frame %5d     fps %3d\n", frameCount, (int)(framerate + 0.5f));
        }



        sprintf(buffer, "fps: %3d", (int)framerate);
        draw_text(canvas, font,
            canvas->width - font.glyphWidth * (strlen(buffer) + 1),
            canvas->height - font.glyphHeight * 4,
            white, buffer);



        if (gameMode == 0) {
            const Color COLOR_GOLD = { 15, 240, 127, 255 };
            solid_rect(canvas, 0, 0, canvas->width, canvas->height, COLOR_GOLD);

            //draw stats
            float percent = fmax(0, fmin(1, modeTimer - 1));
            Color color = { 255, 255, 255, (u8)(255 * percent) };

            char buffer[100];
            snprintf(buffer, 100, "Welcome to Par 99.");
            draw_text(canvas, font, (canvas->width - strlen(buffer) * font.glyphWidth) / 2,
                (canvas->height - 4 * font.glyphHeight) / 2, color, buffer);

            const char * str = "Press SPACE to begin";
            draw_text(canvas, font, (canvas->width - strlen(str) * font.glyphWidth) / 2,
                (canvas->height + 2 * font.glyphHeight) / 2, color, str);
        }

        if (gameMode == 2) {
            const Color COLOR_GOLD = { 15, 127, 240, 255 };
            float easeLength = 0.9f;
            if (modeTimer < easeLength) {
                //XXX: ouch! hardcoded array position
                Button g = level.buttons[level.buttons.len - 1];
                Vec2 gpos = vec2(g.x, g.y);

                int r = sq(sq((modeTimer + 0.1f) / (easeLength - 0.1f))) * canvas->width / 2;
                solid_rect(canvas, gpos.x + offx - r, gpos.y + offy - r,
                    r * 2 + 1, r * 2 + 1, COLOR_GOLD);
            } else {
                solid_rect(canvas, 0, 0, canvas->width, canvas->height, COLOR_GOLD);

                //draw stats
                float percent = fmax(0, fmin(1, modeTimer - 1));
                Color color = { 255, 255, 255, (u8)(255 * percent) };

                char buffer[100];
                snprintf(buffer, 100, "It took you %d shots to reach the goal.", puttCount);
                draw_text(canvas, font, (canvas->width - strlen(buffer) * font.glyphWidth) / 2,
                    (canvas->height - 4 * font.glyphHeight) / 2, color, buffer);

                // if (modeTimer > 3.0f) {
                    // float osc = fabs(fmod(modeTimer - 3.0f, 2) - 1);
                    // Color color = { 255, 255, 255, (u8)(255 - 255 * osc) };
                    const char * str = "Thanks for playing!";
                    // float x = (canvas->width - strlen(str) * font.glyphWidth) / 2;
                    draw_text(canvas, font, (canvas->width - strlen(str) * font.glyphWidth) / 2,
                        (canvas->height + 2 * font.glyphHeight) / 2, color, str);
                // }
            }
        }



        //test chromatic aberration effect
        //TODO: alloc this once instead of re-allocating it every frame
        Canvas postProcess = {
            (Pixel *) malloc(canvas->width * canvas->height * sizeof(Pixel)),
            canvas->width, canvas->height, canvas->width
        };

        if (ball.inverted) {
            for (int y = 0; y < canvas->height; ++y) {
                Pixel * row = canvas->pixels + y * canvas->pitch;
                for (int x = 0; x < canvas->width; ++x) {
                    Color c = row[x];
                    row[x] = { (u8)(255 - c.b), (u8)(255 - c.g), (u8)(255 - c.r), c.a };
                    //TODO: better invert effect?
                    // u8 lum = (c.b + c.b + c.r) / 3;
                    // i8 adj = (127 - lum * 2);
                }
            }
        }

        if (ball.stopped) {
            for (int y = 0; y < canvas->height; ++y) {
                Pixel * row = canvas->pixels + y * canvas->pitch;
                for (int x = 0; x < canvas->width; ++x) {
                    Color c = row[x];
                    row[x] = { (u8)(c.b/2 + 63), (u8)(c.g/2 + 63), (u8)(c.r/2 + 63), c.a };
                }
            }
        }

        for (int y = 0; y < canvas->height; ++y) {
            Pixel * srcRow = canvas->pixels + y * canvas->pitch;
            Pixel * destRow = postProcess.pixels + y * postProcess.pitch;
            for (int x = 0; x < canvas->width; ++x) {
                float factor = 1 - shakeTimer / 0.2f;
                int r = imax(0, factor * 5);
                destRow[x].r = srcRow[x - r].r;
                destRow[x].g = srcRow[x    ].g;
                destRow[x].b = srcRow[x + r].b;
                destRow[x].a = 255;
            }
        }

        //upscale into the window's frame buffer
        fast_scaled_blit(SDL_GetWindowSurface(window), &postProcess, gameScale);

        free(postProcess.pixels);

        SDL_UpdateWindowSurface(window);

        fflush(stdout);
        fflush(stderr);

        frameCount += 1;

        // SDL_Delay(8);

        //uncomment this to make the game exit immediately (good for testing compile+load times)
        // shouldExit = true;
    }

    //TODO: shutdown stuff?

    return 0;
}
