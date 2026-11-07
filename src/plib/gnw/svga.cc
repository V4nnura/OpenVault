#include "plib/gnw/svga.h"

#include "int/movie.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/mouse.h"
#include "plib/gnw/winmain.h"

namespace fallout {

static bool createRenderer(int width, int height);
static void destroyRenderer();

// screen rect
Rect scr_size;

// 0x6ACA18
ScreenBlitFunc* scr_blit = GNW95_ShowRect;

SDL_Window* gSdlWindow = NULL;
SDL_Surface* gSdlSurface = NULL;
SDL_Renderer* gSdlRenderer = NULL;
SDL_Texture* gSdlTexture = NULL;
SDL_Surface* gSdlTextureSurface = NULL;

// TODO: Remove once migration to update-render cycle is completed.
FpsLimiter sharedFpsLimiter;

// 0x4CB310
void GNW95_SetPaletteEntries(unsigned char* palette, int start, int count)
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
        SDL_Color colors[256];

        if (count != 0) {
            for (int index = 0; index < count; index++) {
                colors[index].r = palette[index * 3] << 2;
                colors[index].g = palette[index * 3 + 1] << 2;
                colors[index].b = palette[index * 3 + 2] << 2;
                colors[index].a = 255;
            }
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, start, count);
        SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
    }
}

// 0x4CB568
void GNW95_SetPalette(unsigned char* palette)
{
    if (gSdlSurface != NULL && gSdlSurface->format->palette != NULL) {
        SDL_Color colors[256];

        for (int index = 0; index < 256; index++) {
            colors[index].r = palette[index * 3] << 2;
            colors[index].g = palette[index * 3 + 1] << 2;
            colors[index].b = palette[index * 3 + 2] << 2;
            colors[index].a = 255;
        }

        SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);
        SDL_BlitSurface(gSdlSurface, NULL, gSdlTextureSurface, NULL);
    }
}

// 0x4CB850
void GNW95_ShowRect(unsigned char* src, unsigned int srcPitch, unsigned int a3, unsigned int srcX, unsigned int srcY, unsigned int srcWidth, unsigned int srcHeight, unsigned int destX, unsigned int destY)
{
    // Clip copy to the bounds of gSdlSurface to avoid out-of-bounds when logical
    // resolution is smaller than interface assets (e.g. 512-wide surface vs 640-wide bar).
    int copyX = static_cast<int>(destX);
    int copyY = static_cast<int>(destY);
    int copyW = static_cast<int>(srcWidth);
    int copyH = static_cast<int>(srcHeight);
    int srcOffsetX = static_cast<int>(srcX);
    int srcOffsetY = static_cast<int>(srcY);

    if (copyX < 0) {
        srcOffsetX -= copyX;
        copyW += copyX;
        copyX = 0;
    }
    if (copyY < 0) {
        srcOffsetY -= copyY;
        copyH += copyY;
        copyY = 0;
    }
    if (copyX + copyW > gSdlSurface->w) {
        copyW = gSdlSurface->w - copyX;
    }
    if (copyY + copyH > gSdlSurface->h) {
        copyH = gSdlSurface->h - copyY;
    }

    if (copyW <= 0 || copyH <= 0) {
        return;
    }

    buf_to_buf(src + srcPitch * srcOffsetY + srcOffsetX,
        copyW,
        copyH,
        srcPitch,
        (unsigned char*)gSdlSurface->pixels + gSdlSurface->pitch * copyY + copyX,
        gSdlSurface->pitch);

    SDL_Rect srcRect;
    srcRect.x = copyX;
    srcRect.y = copyY;
    srcRect.w = copyW;
    srcRect.h = copyH;

    SDL_Rect destRect;
    destRect.x = copyX;
    destRect.y = copyY;
    destRect.w = copyW;
    destRect.h = copyH;

    // Also clip destination to texture surface bounds
    if (destRect.x + destRect.w > gSdlTextureSurface->w) {
        destRect.w = gSdlTextureSurface->w - destRect.x;
        srcRect.w = destRect.w;
    }
    if (destRect.y + destRect.h > gSdlTextureSurface->h) {
        destRect.h = gSdlTextureSurface->h - destRect.y;
        srcRect.h = destRect.h;
    }

    if (destRect.w <= 0 || destRect.h <= 0) {
        return;
    }

    SDL_BlitSurface(gSdlSurface, &srcRect, gSdlTextureSurface, &destRect);
}

bool svga_init(VideoOptions* video_options)
{
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        return false;
    }

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

#if __APPLE__ && TARGET_OS_IOS && __ANDROID__
    // This hides the status bar on iPadOS, which otherwise interferes
    // with the cursor in the top margin of the screen. (Also on Android Tablets too.)
    windowFlags |= SDL_WINDOW_BORDERLESS;
#endif

    if (video_options->fullscreen) {
        if (video_options->exclusive)
            windowFlags |= SDL_WINDOW_FULLSCREEN;
        else
            windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    gSdlWindow = SDL_CreateWindow(GNW95_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        video_options->width * video_options->scale,
        video_options->height * video_options->scale,
        windowFlags);
    if (gSdlWindow == NULL) {
        return false;
    }

    if (!createRenderer(video_options->width, video_options->height)) {
        destroyRenderer();

        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = NULL;

        return false;
    }

    gSdlSurface = SDL_CreateRGBSurface(0,
        video_options->width,
        video_options->height,
        8,
        0,
        0,
        0,
        0);
    if (gSdlSurface == NULL) {
        destroyRenderer();

        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = NULL;
    }

    SDL_Color colors[256];
    for (int index = 0; index < 256; index++) {
        colors[index].r = index;
        colors[index].g = index;
        colors[index].b = index;
        colors[index].a = 255;
    }

    SDL_SetPaletteColors(gSdlSurface->format->palette, colors, 0, 256);

    scr_size.ulx = 0;
    scr_size.uly = 0;
    scr_size.lrx = video_options->width - 1;
    scr_size.lry = video_options->height - 1;

    mouse_blit_trans = NULL;
    scr_blit = GNW95_ShowRect;
    mouse_blit = GNW95_ShowRect;

    return true;
}

void svga_exit()
{
    destroyRenderer();

    if (gSdlWindow != NULL) {
        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

int screenGetWidth()
{
    // TODO: Make it on par with _xres;
    return rectGetWidth(&scr_size);
}

int screenGetHeight()
{
    // TODO: Make it on par with _yres.
    return rectGetHeight(&scr_size);
}

static bool createRenderer(int width, int height)
{
    gSdlRenderer = SDL_CreateRenderer(gSdlWindow, -1, SDL_RENDERER_PRESENTVSYNC);
    if (gSdlRenderer == NULL) {
        return false;
    }

    // Log display refresh rate for diagnostic purposes
    int displayIndex = SDL_GetWindowDisplayIndex(gSdlWindow);
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(displayIndex, &mode) == 0) {
        debug_printf("Display refresh rate: %d Hz\n", mode.refresh_rate);
        debug_printf("Display resolution: %dx%d\n", mode.w, mode.h);
    } else {
        debug_printf("Could not query display mode: %s\n", SDL_GetError());
    }

    if (SDL_RenderSetLogicalSize(gSdlRenderer, width, height) != 0) {
        return false;
    }

    // Set render scale quality to nearest (fastest for pixel art)
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    gSdlTexture = SDL_CreateTexture(gSdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (gSdlTexture == NULL) {
        return false;
    }

    Uint32 format;
    if (SDL_QueryTexture(gSdlTexture, &format, NULL, NULL, NULL) != 0) {
        return false;
    }

    gSdlTextureSurface = SDL_CreateRGBSurfaceWithFormat(0, width, height, SDL_BITSPERPIXEL(format), format);
    if (gSdlTextureSurface == NULL) {
        return false;
    }

    return true;
}

static void destroyRenderer()
{
    if (gSdlTextureSurface != NULL) {
        SDL_FreeSurface(gSdlTextureSurface);
        gSdlTextureSurface = NULL;
    }

    if (gSdlTexture != NULL) {
        SDL_DestroyTexture(gSdlTexture);
        gSdlTexture = NULL;
    }

    if (gSdlRenderer != NULL) {
        SDL_DestroyRenderer(gSdlRenderer);
        gSdlRenderer = NULL;
    }
}

void handleWindowSizeChanged()
{
    movieHandleRendererReset();
    destroyRenderer();
    createRenderer(screenGetWidth(), screenGetHeight());
}

void renderPresent()
{
    // Standard SDL rendering
    SDL_UpdateTexture(gSdlTexture, NULL, gSdlTextureSurface->pixels, gSdlTextureSurface->pitch);
    SDL_RenderClear(gSdlRenderer);
    SDL_RenderCopy(gSdlRenderer, gSdlTexture, NULL, NULL);
    // render movie SDL texture if present
    movieRenderDirectOverlay();
    SDL_RenderPresent(gSdlRenderer);
}

} // namespace fallout
