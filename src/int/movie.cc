#include "int/movie.h"

#include <SDL.h>
#include <string.h>
#include <vector>

#include "game/gconfig.h"
#include "game/moviefx.h"
#include "int/memdbg.h"
#include "int/sound.h"
#include "int/window.h"
#include "movie_lib.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/db/db.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"
#include "plib/gnw/winmain.h"

namespace fallout {

typedef struct MovieSubtitleListNode {
    int num;
    char* text;
    struct MovieSubtitleListNode* next;
} MovieSubtitleListNode;

static SDL_Rect movieComputeDirectRect(int srcWidth, int srcHeight);
static void movieDirectOverlayDestroy();
static bool movieDirectOverlayEnsureTexture(int width, int height);
static void movieDirectOverlayUpload(unsigned char* pixels, int srcWidth, int srcHeight);
static void* movieMalloc(size_t size);
static void movieFree(void* ptr);
static bool movieRead(void* handle, void* buf, int count);
static void movieDirect(unsigned char* pixels, int src_width, int src_height, int, int, int, int, int, int);
static void movieBuffered(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y);
static int movieScaleSubRect(int win, unsigned char* data, int width, int height, int pitch);
static int movieScaleWindowAlpha(int win, unsigned char* data, int width, int height, int pitch);
static int movieScaleSubRectAlpha(int win, unsigned char* data, int width, int height, int pitch);
static int blitAlpha(int win, unsigned char* data, int width, int height, int pitch);
static int movieScaleWindow(int win, unsigned char* data, int width, int height, int pitch);
static int blitNormal(int win, unsigned char* data, int width, int height, int pitch);
static void movieSetPalette(unsigned char* palette, int start, int end);
static void cleanupMovie(bool shouldEndMovie);
static void cleanupLast();
static DB_FILE* openFile(char* filePath);
static void openSubtitle(char* filePath);
static void doSubtitle();
static int movieStart(int win, char* filePath);
static bool localMovieCallback();
static int stepMovie();

// 0x505B30
static int GNWWin = -1;

struct MovieDirectOverlay {
    SDL_Texture* texture = NULL;
    int srcWidth = 0;
    int srcHeight = 0;
    SDL_Rect dstRect = { 0, 0, 0, 0 };
    bool active = false;
    std::vector<uint32_t> uploadBuffer;
};

static MovieDirectOverlay movieDirectOverlay;

static void movieDirectOverlayDestroy()
{
    if (movieDirectOverlay.texture != NULL) {
        SDL_DestroyTexture(movieDirectOverlay.texture);
        movieDirectOverlay.texture = NULL;
    }

    movieDirectOverlay.srcWidth = 0;
    movieDirectOverlay.srcHeight = 0;
    movieDirectOverlay.dstRect = { 0, 0, 0, 0 };
    movieDirectOverlay.active = false;
    movieDirectOverlay.uploadBuffer.clear();
}

static bool movieDirectOverlayEnsureTexture(int width, int height)
{
    if (movieDirectOverlay.texture != NULL
        && movieDirectOverlay.srcWidth == width
        && movieDirectOverlay.srcHeight == height) {
        return true;
    }

    movieDirectOverlayDestroy();

    movieDirectOverlay.texture = SDL_CreateTexture(gSdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (movieDirectOverlay.texture == NULL) {
        return false;
    }

    SDL_SetTextureBlendMode(movieDirectOverlay.texture, SDL_BLENDMODE_NONE);
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(movieDirectOverlay.texture, SDL_ScaleModeNearest);
#endif

    movieDirectOverlay.srcWidth = width;
    movieDirectOverlay.srcHeight = height;
    return true;
}

static void movieDirectOverlayUpload(unsigned char* pixels, int srcWidth, int srcHeight)
{
    if (movieDirectOverlay.texture == NULL || gSdlSurface == NULL || gSdlSurface->format == NULL || gSdlSurface->format->palette == NULL) {
        return;
    }

    int width = srcWidth;
    int height = srcHeight;
    if (width <= 0 || height <= 0) {
        return;
    }

    int pixelCount = width * height;
    if (static_cast<int>(movieDirectOverlay.uploadBuffer.size()) < pixelCount) {
        movieDirectOverlay.uploadBuffer.resize(pixelCount);
    }

    SDL_Color* colors = gSdlSurface->format->palette->colors;
    for (int y = 0; y < height; y++) {
        const unsigned char* srcRow = pixels + y * srcWidth;
        uint32_t* dstRow = movieDirectOverlay.uploadBuffer.data() + y * width;
        for (int x = 0; x < width; x++) {
            SDL_Color color = colors[srcRow[x]];
            dstRow[x] = (static_cast<uint32_t>(color.a) << 24)
                | (static_cast<uint32_t>(color.r) << 16)
                | (static_cast<uint32_t>(color.g) << 8)
                | static_cast<uint32_t>(color.b);
        }
    }

    SDL_Rect rect = { 0, 0, width, height };
    SDL_UpdateTexture(movieDirectOverlay.texture, &rect, movieDirectOverlay.uploadBuffer.data(), width * static_cast<int>(sizeof(uint32_t)));
}

// 0x505B34
static int subtitleFont = -1;

// 0x505B38
static MovieBlitFunc* showFrameFuncs[2][2] = {
    {
        blitNormal,
        blitNormal,
    },
    {
        movieScaleSubRect,
        movieScaleWindow,
    },
};

// 0x505B58
static MoviePaletteFunc* paletteFunc = setSystemPaletteEntries;

// 0x505B5C
static int subtitleR = 31;

// 0x505B60
static int subtitleG = 31;

// 0x505B64
static int subtitleB = 31;

// 0x637370
static Rect winRect;

// 0x637394
static MovieUpdateCallbackProc* updateCallbackFunc;

// 0x6373D4
static MovieSubtitleFunc* subtitleFilenameFunc;

// 0x6373E4
static int subtitleW;

// 0x6373AC
static int movieScaleFlag;

// Similar to 0x6373B4
static SDL_Rect lastMovieRect;

// 0x6373F4
static MovieSubtitleListNode* subtitleList;

// 0x6373C4
static unsigned int movieFlags;

// 0x6373CC
static bool movieSubRectFlag;

// 0x6373F8
static int movieH;

// 0x637414
static int movieW;

// 0x637420
static int subtitleH;

// 0x6373E0
static int running;

// 0x6373E8
static DB_FILE* handle;

// 0x637400
static int movieX;

// 0x637404
static int movieY;

bool gMovieScaleResolution = true;

static SDL_Rect movieComputeDirectRect(int srcWidth, int srcHeight)
{
    int availableX = movieX;
    int availableY = movieY;
    int availableWidth = movieW;
    int availableHeight = movieH;
    bool centered = (movieFlags & MOVIE_EXTENDED_FLAG_CENTERED) != 0;

    if (!gMovieScaleResolution) {
        if (!centered) {
            return {
                availableX,
                availableY,
                srcWidth,
                srcHeight,
            };
        }

        return {
            availableX + (availableWidth - srcWidth) / 2,
            availableY + (availableHeight - srcHeight) / 2,
            srcWidth,
            srcHeight,
        };
    }

    int subtitleReserve = 0;
    if ((movieFlags & MOVIE_EXTENDED_FLAG_SUBTITLES) != 0) {
        subtitleReserve = subtitleH + 4;
    }

    auto fitInto = [&](int fitHeight) {
        int movieWidth = availableWidth;
        int movieHeight = movieWidth * srcHeight / srcWidth;
        if (movieHeight > fitHeight) {
            movieHeight = fitHeight;
            movieWidth = fitHeight * srcWidth / srcHeight;
        }

        return SDL_Rect {
            centered ? availableX + (availableWidth - movieWidth) / 2 : availableX,
            centered ? availableY + (fitHeight - movieHeight) / 2 : availableY,
            movieWidth,
            movieHeight,
        };
    };

    SDL_Rect movieRect = fitInto(availableHeight);

    int marginHeight = (availableHeight - movieRect.h) / 2;
    if (subtitleReserve > 0 && marginHeight < subtitleReserve && availableHeight > subtitleReserve) {
        movieRect = fitInto(availableHeight - subtitleReserve);
    }

    return movieRect;
}

// 0x47840C
static void* movieMalloc(size_t size)
{
    return mymalloc(size, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 209
}

// 0x478424
static void movieFree(void* ptr)
{
    myfree(ptr, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 213
}

// 0x47843C
static bool movieRead(void* handle, void* buf, int count)
{
    return db_fread(buf, 1, count, reinterpret_cast<DB_FILE*>(handle)) == count;
}

// 0x478464
static void movieDirect(unsigned char* pixels, int src_width, int src_height, int, int, int, int, int, int)
{
    if (GNWWin == -1) {
        return;
    }

    if (lastMovieRect.w == 0) {
        // We don't know the movie dimensions until we have the first frame, at which point we can compute the output rect
        lastMovieRect = movieComputeDirectRect(src_width, src_height);
        debug_printf("Movie output: x=%d, y=%d, w=%d, h=%d", lastMovieRect.x, lastMovieRect.y, lastMovieRect.w, lastMovieRect.h);
    }

    if (lastMovieRect.w == src_width && lastMovieRect.h == src_height) {
        movieDirectOverlay.active = false;
        scr_blit(pixels, src_width, src_height, 0, 0, src_width, src_height, lastMovieRect.x, lastMovieRect.y);
    } else {
        // we're scaling, so use an SDL overlay to render
        if (!movieDirectOverlayEnsureTexture(src_width, src_height)) {
            movieFlags |= MOVIE_EXTENDED_FLAG_ERROR;
            return;
        }

        movieDirectOverlayUpload(pixels, src_width, src_height);
        movieDirectOverlay.dstRect = lastMovieRect;
        movieDirectOverlay.active = true;
    }
}

void movieRenderDirectOverlay()
{
    if (!movieDirectOverlay.active || movieDirectOverlay.texture == NULL) {
        return;
    }

    SDL_RenderCopy(gSdlRenderer, movieDirectOverlay.texture, NULL, &movieDirectOverlay.dstRect);
}

void movieHandleRendererReset()
{
    movieDirectOverlayDestroy();
}

// 0x478710
static void movieBuffered(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y)
{
    if (GNWWin == -1) {
        return;
    }

    lastMovieRect = { src_x, src_y, dst_width, dst_height };

    MovieBlitFunc* func = showFrameFuncs[movieScaleFlag][movieSubRectFlag];
    if (func(GNWWin, pixels, src_width, src_height, src_width) != 0) {
        Rect movieRect = { movieX, movieY, movieX + movieW, movieY + movieH };
        win_draw_rect(GNWWin, &movieRect);
    }
}

// 0x478978
static int movieScaleSubRect(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = win_width(win);
    unsigned char* windowBuffer = win_get_buf(win) + windowWidth * movieY + movieX;
    if (width * 4 / 3 > movieW) {
        movieFlags |= MOVIE_EXTENDED_FLAG_ERROR;
        return 0;
    }

    int tripletCount = width / 3;
    for (int y = 0; y < height; y++) {
        int x;
        for (x = 0; x < tripletCount; x++) {
            unsigned int value = data[0];
            value |= data[1] << 8;
            value |= data[2] << 16;
            value |= data[2] << 24;

            *(unsigned int*)windowBuffer = value;

            windowBuffer += 4;
            data += 3;
        }

        for (x = x * 3; x < width; x++) {
            *windowBuffer++ = *data++;
        }

        data += pitch - width;
        windowBuffer += windowWidth - movieW;
    }

    return 1;
}

// 0x478AE4
static int movieScaleWindow(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = win_width(win);
    if (width != 3 * windowWidth / 4) {
        movieFlags |= MOVIE_EXTENDED_FLAG_ERROR;
        return 0;
    }

    unsigned char* windowBuffer = win_get_buf(win);
    for (int y = 0; y < height; y++) {
        int scaledWidth = width / 3;
        for (int x = 0; x < scaledWidth; x++) {
            unsigned int value = data[0];
            value |= data[1] << 8;
            value |= data[2] << 16;
            value |= data[3] << 24;

            *(unsigned int*)windowBuffer = value;

            windowBuffer += 4;
            data += 3;
        }
        data += pitch - width;
    }

    return 1;
}

// 0x478B94
static int blitNormal(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = win_width(win);
    unsigned char* windowBuffer = win_get_buf(win);
    drawScaled(windowBuffer + windowWidth * movieY + movieX, movieW, movieH, windowWidth, data, width, height, pitch);
    return 1;
}

// 0x478BEC
static void movieSetPalette(unsigned char* palette, int start, int end)
{
    if (end != 0) {
        paletteFunc(palette + start * 3, start, end + start - 1);
    }
}

// 0x478C1C
void initMovie()
{
    movieDirectOverlayDestroy();
    MveSetMemory(movieMalloc, movieFree);
    MveSetPalette(movieSetPalette);
    MveSetScreenSize(screenGetWidth(), screenGetHeight());
    MveSetIO(movieRead);
}

// 0x478CA8
static void cleanupMovie(bool shouldEndMovie)
{
    if (!running) {
        return;
    }

    int frame, dropped;
    MVE_rmFrameCounts(&frame, &dropped);
    debug_printf("Frames %d, dropped %d\n", frame, dropped);

    if (shouldEndMovie) {
        MVE_rmEndMovie();
    }

    MVE_ReleaseMem();

    db_fclose(handle);
    handle = NULL;
    movieDirectOverlayDestroy();

    while (subtitleList != NULL) {
        MovieSubtitleListNode* next = subtitleList->next;
        myfree(subtitleList->text, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 851
        myfree(subtitleList, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 852
        subtitleList = next;
    }

    running = 0;
    movieSubRectFlag = 0;
    movieScaleFlag = 0;
    movieFlags = 0;
    GNWWin = -1;
}

// 0x478F2C
void movieClose()
{
    cleanupMovie(true);
}

// 0x478F60
void movieStop()
{
    if (running) {
        movieFlags |= MOVIE_EXTENDED_FLAG_STOP_REQUESTED;
    }
}

// 0x478F74
int movieSetFlags(int flags)
{
    if ((flags & MOVIE_FLAG_DIRECT_CENTERED) != 0) {
        movieFlags |= MOVIE_EXTENDED_FLAG_DIRECT | MOVIE_EXTENDED_FLAG_CENTERED;
    } else {
        movieFlags &= ~MOVIE_EXTENDED_FLAG_CENTERED;
        if ((flags & MOVIE_FLAG_DIRECT) != 0) {
            movieFlags |= MOVIE_EXTENDED_FLAG_DIRECT;
        } else {
            movieFlags &= ~MOVIE_EXTENDED_FLAG_DIRECT;
        }
    }

    if ((flags & MOVIE_FLAG_SCALE) != 0) {
        movieScaleFlag = 1;
    } else {
        movieScaleFlag = 0;

        if ((movieFlags & MOVIE_EXTENDED_FLAG_DIRECT) == 0) {
            movieFlags &= ~MOVIE_EXTENDED_FLAG_CENTERED;
        }
    }

    if ((flags & MOVIE_FLAG_SUBTITLES) != 0) {
        movieFlags |= MOVIE_EXTENDED_FLAG_SUBTITLES;
    } else {
        movieFlags &= ~MOVIE_EXTENDED_FLAG_SUBTITLES;
    }

    return 0;
}

// 0x47901C
void movieSetSubtitleFont(int font)
{
    subtitleFont = font;
}

// 0x479024
void movieSetSubtitleColor(float r, float g, float b)
{
    subtitleR = (int)(r * 31.0f);
    subtitleG = (int)(g * 31.0f);
    subtitleB = (int)(b * 31.0f);
}

// 0x479060
void movieSetPaletteFunc(MoviePaletteFunc* func)
{
    paletteFunc = func != NULL ? func : setSystemPaletteEntries;
}

// 0x479078
void movieSetCallback(MovieUpdateCallbackProc* func)
{
    updateCallbackFunc = func;
}

// 0x479120
static DB_FILE* openFile(char* filePath)
{
    handle = db_fopen(filePath, "rb");
    if (handle == NULL) {
        debug_printf("Couldn't find movie file %s\n", filePath);
        return 0;
    }
    return handle;
}

// 0x479184
static void openSubtitle(char* filePath)
{
    subtitleW = movieW;
    subtitleH = text_height() + 4;

    if (subtitleFilenameFunc != NULL) {
        filePath = subtitleFilenameFunc(filePath);
    }

    char path[COMPAT_MAX_PATH];
    strcpy(path, filePath);

    debug_printf("Opening subtitle file %s\n", path);
    DB_FILE* stream = db_fopen(path, "r");
    if (stream == NULL) {
        debug_printf("Couldn't open subtitle file %s\n", path);
        movieFlags &= ~MOVIE_EXTENDED_FLAG_SUBTITLES;
        return;
    }

    MovieSubtitleListNode* prev = NULL;
    int subtitleCount = 0;
    while (!db_feof(stream)) {
        char string[260];
        string[0] = '\0';
        db_fgets(string, 259, stream);
        if (*string == '\0') {
            break;
        }

        MovieSubtitleListNode* subtitle = (MovieSubtitleListNode*)mymalloc(sizeof(*subtitle), __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1050
        subtitle->next = NULL;

        subtitleCount++;

        char* pch;

        pch = strchr(string, '\n');
        if (pch != NULL) {
            *pch = '\0';
        }

        pch = strchr(string, '\r');
        if (pch != NULL) {
            *pch = '\0';
        }

        pch = strchr(string, ':');
        if (pch != NULL) {
            *pch = '\0';
            subtitle->num = atoi(string);
            subtitle->text = mystrdup(pch + 1, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1058

            if (prev != NULL) {
                prev->next = subtitle;
            } else {
                subtitleList = subtitle;
            }

            prev = subtitle;
        } else {
            debug_printf("subtitle: couldn't parse %s\n", string);
        }
    }

    db_fclose(stream);

    debug_printf("Read %d subtitles\n", subtitleCount);
}

// 0x479360
static void doSubtitle()
{
    if (subtitleList == NULL) {
        return;
    }

    if ((movieFlags & MOVIE_EXTENDED_FLAG_SUBTITLES) == 0) {
        return;
    }

    int subtitleX = movieX;
    int subtitleW = subtitleW;
    int movieY = movieY;
    int movieHeight = movieH;
    if ((movieFlags & MOVIE_EXTENDED_FLAG_DIRECT) != 0 && lastMovieRect.w > 0 && lastMovieRect.h > 0) {
        subtitleX = lastMovieRect.x;
        subtitleW = lastMovieRect.w;
        movieY = lastMovieRect.y;
        movieHeight = lastMovieRect.h;
    }

    int lineHeight = text_height();
    int subtitleY = (win_height(GNWWin) - movieHeight - movieY - lineHeight) / 2 + movieHeight + movieY;

    int subtitleHeight = subtitleH;
    int movieWindowHeight = win_height(GNWWin);
    if (subtitleHeight + subtitleY > movieWindowHeight) {
        subtitleHeight = movieWindowHeight - subtitleY;
    }
    if (subtitleHeight <= 0) {
        return;
    }

    int frame, dropped;
    MVE_rmFrameCounts(&frame, &dropped);

    while (subtitleList != NULL) {
        if (frame < subtitleList->num) {
            break;
        }

        MovieSubtitleListNode* next = subtitleList->next;

        win_fill(GNWWin, 0, subtitleX, subtitleY, subtitleW, subtitleHeight, 0);

        int oldFont;
        if (subtitleFont != -1) {
            oldFont = text_curr();
            text_font(subtitleFont);
        }

        int colorIndex = (subtitleR << 10) | (subtitleG << 5) | subtitleB;
        windowWrapLine(GNWWin, subtitleList->text, subtitleW, subtitleHeight, subtitleX, subtitleY, colorTable[colorIndex] | 0x2000000, TEXT_ALIGNMENT_CENTER);

        Rect rect;
        rect.lrx = subtitleX + subtitleW - 1;
        rect.uly = subtitleY;
        rect.lry = subtitleY + subtitleHeight - 1;
        rect.ulx = subtitleX;
        win_draw_rect(GNWWin, &rect);

        myfree(subtitleList->text, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1108
        myfree(subtitleList, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1109

        subtitleList = next;

        if (subtitleFont != -1) {
            text_font(oldFont);
        }
    }
}

// 0x479514
static int movieStart(int win, char* filePath)
{
    if (running) {
        return 1;
    }

    handle = openFile(filePath);
    if (handle == NULL) {
        return 1;
    }

    GNWWin = win;
    running = 1;
    movieFlags &= ~MOVIE_EXTENDED_FLAG_ERROR;
    lastMovieRect = { 0, 0, 0, 0 };

    if ((movieFlags & MOVIE_EXTENDED_FLAG_SUBTITLES) != 0) {
        openSubtitle(filePath);
    }

    if ((movieFlags & MOVIE_EXTENDED_FLAG_DIRECT) != 0) {
        debug_printf("Direct ");
        win_get_rect(GNWWin, &winRect);
        debug_printf("Playing at (%d, %d)  ", movieX + winRect.ulx, movieY + winRect.uly);
        MveSetShowFrame(movieDirect);

        MVE_rmPrepMovie(handle, movieX + winRect.ulx, movieY + winRect.uly, 0);
    } else {
        debug_printf("Buffered ");
        MveSetShowFrame(movieBuffered);
        MVE_rmPrepMovie(handle, 0, 0, 0);
    }

    if (movieScaleFlag) {
        debug_printf("scaled\n");
    } else {
        debug_printf("not scaled\n");
    }

    return 0;
}

// 0x4798CC
int movieRun(int win, char* filePath)
{
    if (running) {
        return 1;
    }

    movieX = 0;
    movieY = 0;
    movieW = win_width(win);
    movieH = win_height(win);
    movieSubRectFlag = 0;
    return movieStart(win, filePath);
}

// 0x479920
int movieRunRect(int win, char* filePath, int x, int y, int w, int h)
{
    if (running) {
        return 1;
    }

    movieX = x;
    movieY = y;
    movieW = w;
    movieH = h;
    movieSubRectFlag = 1;

    return movieStart(win, filePath);
}

// 0x479980
static int stepMovie()
{
    int rc = _MVE_rmStepMovie();
    if (rc != -1) {
        doSubtitle();
        renderPresent();
    }

    return rc;
}

// 0x4799CC
void movieSetSubtitleFunc(MovieSubtitleFunc* func)
{
    subtitleFilenameFunc = func;
}

// 0x4799D4
void movieSetVolume(int volume)
{
    int normalized_volume = soundVolumeHMItoDirectSound(volume);
    MveSetVolume(normalized_volume);
}

// 0x4799F0
void movieUpdate()
{
    if (!running) {
        return;
    }

    if ((movieFlags & MOVIE_EXTENDED_FLAG_STOP_REQUESTED) != 0) {
        debug_printf("Movie aborted\n");
        cleanupMovie(true);
        return;
    }

    if ((movieFlags & MOVIE_EXTENDED_FLAG_ERROR) != 0) {
        debug_printf("Movie error\n");
        cleanupMovie(true);
        return;
    }

    if (stepMovie() == -1) {
        cleanupMovie(true);
        return;
    }

    if (updateCallbackFunc != NULL) {
        int frame, dropped;
        MVE_rmFrameCounts(&frame, &dropped);
        updateCallbackFunc(frame);
    }
}

// 0x479A8C
int moviePlaying()
{
    return running;
}

} // namespace fallout
