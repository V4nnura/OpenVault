#include "int/movie.h"

#include <string.h>

#include <SDL.h>

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

static void* movieMalloc(size_t size);
static void movieFree(void* ptr);
static bool movieRead(void* handle, void* buf, int count);
static void movieDirect(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y);
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

// 0x505B34
static int subtitleFont = -1;

// 0x505B38
static MovieBlitFunc* showFrameFuncs[2][2][2] = {
    {
        {
            blitNormal,
            blitNormal,
        },
        {
            movieScaleWindow,
            movieScaleSubRect,
        },
    },
    {
        {
            blitAlpha,
            blitAlpha,
        },
        {
            movieScaleSubRectAlpha,
            movieScaleWindowAlpha,
        },
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

// 0x637380
static Rect movieRect;

// 0x637390
static void (*movieCallback)();

// 0x637394
static MovieUpdateCallbackProc* updateCallbackFunc;

// 0x6373D4
static MovieSubtitleFunc* subtitleFilenameFunc;

// 0x6373E4
static int subtitleW;

// 0x637398
static int lastMovieBH;

// 0x63739C
static int lastMovieBW;

// 0x6373A4
static int lastMovieSX;

// 0x6373A8
static int lastMovieSY;

// 0x6373AC
static int movieScaleFlag;

// 0x6373B4
static int lastMovieH;

// 0x6373B8
static int lastMovieW;

// 0x6373F0
static int lastMovieX;

// 0x6373BC
static int lastMovieY;

// 0x6373F4
static MovieSubtitleListNode* subtitleList;

// 0x6373C4
static unsigned int movieFlags;

// 0x6373C8
static int movieAlphaFlag;

// 0x6373CC
static bool movieSubRectFlag;

// 0x6373F8
static int movieH;

// 0x6373FC
static int movieOffset;

// 0x637410
static unsigned char* lastMovieBuffer;

// 0x637414
static int movieW;

// 0x637420
static int subtitleH;

// 0x6373E0
static int running;

// 0x6373E8
static DB_FILE* handle;

// 0x6373EC
static unsigned char* alphaWindowBuf;

// 0x637400
static int movieX;

// 0x637404
static int movieY;

// 0x63740C
static DB_FILE* alphaHandle;

// 0x637418
static unsigned char* alphaBuf;

static unsigned char* MVE_lastBuffer = NULL;

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
static void movieDirect(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y)
{
    int movWinWidth;
    int movWinSpan;

    SDL_Rect srcRect;
    srcRect.x = src_x;
    srcRect.y = src_y;
    srcRect.w = src_width;
    srcRect.h = src_height;

    movWinWidth = winRect.lrx - winRect.ulx;
    movWinSpan = movWinWidth + 1;

    SDL_Rect destRect;

    if (movieScaleFlag) {
        if ((movieFlags & MOVIE_EXTENDED_FLAG_0x08) != 0) {
            destRect.y = (winRect.lry - winRect.uly + 1 - dst_height) / 2;
            destRect.x = (movWinSpan - 4 * src_width / 3) / 2;
        } else {
            destRect.y = movieY + winRect.uly;
            destRect.x = winRect.ulx + movieX;
        }

        destRect.w = 4 * src_width / 3 + destRect.x;
        destRect.h = dst_height + destRect.y;
    } else {
        if ((movieFlags & MOVIE_EXTENDED_FLAG_0x08) != 0) {
            destRect.y = (winRect.lry - winRect.uly + 1 - dst_height) / 2;
            destRect.x = (movWinSpan - dst_width) / 2;
        } else {
            destRect.y = movieY + winRect.uly;
            destRect.x = winRect.ulx + movieX;
        }
        destRect.w = dst_width;
        destRect.h = dst_height;
    }

    lastMovieSX = src_x;
    lastMovieSY = src_y;
    lastMovieX = destRect.x;
    lastMovieY = destRect.y;
    lastMovieBH = src_height;
    lastMovieW = destRect.w;
    MVE_lastBuffer = pixels;
    lastMovieBW = src_width;
    lastMovieH = destRect.h;

    destRect.x += winRect.ulx;
    destRect.y += winRect.uly;

    scr_blit(pixels, src_width, src_height, src_x, src_y, dst_width, dst_height, dst_x, dst_y);
    renderPresent();
}

// 0x478710
static void movieBuffered(unsigned char* pixels, int src_width, int src_height, int src_x, int src_y, int dst_width, int dst_height, int dst_x, int dst_y)
{
    if (GNWWin == -1) {
        return;
    }

    lastMovieBW = src_width;
    MVE_lastBuffer = pixels;
    lastMovieBH = src_width;
    lastMovieW = dst_width;
    lastMovieH = dst_height;
    lastMovieX = src_x;
    lastMovieY = src_y;
    lastMovieSX = src_x;
    lastMovieSY = src_y;

    MovieBlitFunc* func = showFrameFuncs[movieAlphaFlag][movieScaleFlag][movieSubRectFlag];
    if (func(GNWWin, pixels, src_width, src_height, src_width) != 0) {
        win_draw_rect(GNWWin, &movieRect);
    }
}

// 0x478978
static int movieScaleSubRect(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = win_width(win);
    unsigned char* windowBuffer = win_get_buf(win) + windowWidth * movieY + movieX;
    if (width * 4 / 3 > movieW) {
        movieFlags |= 0x01;
        return 0;
    }

    int v1 = width / 3;
    for (int y = 0; y < height; y++) {
        int x;
        for (x = 0; x < v1; x++) {
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

// 0x478A84
static int movieScaleWindowAlpha(int win, unsigned char* data, int width, int height, int pitch)
{
    movieFlags |= 1;
    return 0;
}

// 0x478A84
static int movieScaleSubRectAlpha(int win, unsigned char* data, int width, int height, int pitch)
{
    movieFlags |= 1;
    return 0;
}

// 0x478A90
static int blitAlpha(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = win_width(win);
    unsigned char* windowBuffer = win_get_buf(win);
    alphaBltBuf(data, width, height, pitch, alphaWindowBuf, alphaBuf, windowBuffer + windowWidth * movieY + movieX, windowWidth);
    return 1;
}

// 0x478AE4
static int movieScaleWindow(int win, unsigned char* data, int width, int height, int pitch)
{
    int windowWidth = win_width(win);
    if (width != 3 * windowWidth / 4) {
        movieFlags |= 1;
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

    int frame;
    int dropped;
    MVE_rmFrameCounts(&frame, &dropped);
    debug_printf("Frames %d, dropped %d\n", frame, dropped);

    if (lastMovieBuffer != NULL) {
        myfree(lastMovieBuffer, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 787
        lastMovieBuffer = NULL;
    }

    if (MVE_lastBuffer != NULL) {
        lastMovieBuffer = (unsigned char*)mymalloc(lastMovieBH * lastMovieBW, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 802
        memcpy(lastMovieBuffer, MVE_lastBuffer, lastMovieBW * lastMovieBH);
        MVE_lastBuffer = NULL;
    }

    if (shouldEndMovie) {
        MVE_rmEndMovie();
    }

    MVE_ReleaseMem();

    db_fclose(handle);

    if (alphaWindowBuf != NULL) {
        buf_to_buf(alphaWindowBuf, movieW, movieH, movieW, win_get_buf(GNWWin) + movieY * win_width(GNWWin) + movieX, win_width(GNWWin));
        win_draw_rect(GNWWin, &movieRect);
    }

    if (alphaHandle != NULL) {
        db_fclose(alphaHandle);
        alphaHandle = NULL;
    }

    if (alphaBuf != NULL) {
        myfree(alphaBuf, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 840
        alphaBuf = NULL;
    }

    if (alphaWindowBuf != NULL) {
        myfree(alphaWindowBuf, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 845
        alphaWindowBuf = NULL;
    }

    while (subtitleList != NULL) {
        MovieSubtitleListNode* next = subtitleList->next;
        myfree(subtitleList->text, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 851
        myfree(subtitleList, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 852
        subtitleList = next;
    }

    running = 0;
    movieSubRectFlag = 0;
    movieScaleFlag = 0;
    movieAlphaFlag = 0;
    movieFlags = 0;
    GNWWin = -1;
}

// 0x478F2C
void movieClose()
{
    cleanupMovie(1);

    if (lastMovieBuffer) {
        myfree(lastMovieBuffer, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 869
        lastMovieBuffer = NULL;
    }
}

// 0x478F60
void movieStop()
{
    if (running) {
        movieFlags |= MOVIE_EXTENDED_FLAG_0x02;
    }
}

// 0x478F74
int movieSetFlags(int flags)
{
    if ((flags & MOVIE_FLAG_0x04) != 0) {
        movieFlags |= MOVIE_EXTENDED_FLAG_0x04 | MOVIE_EXTENDED_FLAG_0x08;
    } else {
        movieFlags &= ~MOVIE_EXTENDED_FLAG_0x08;
        if ((flags & MOVIE_FLAG_0x02) != 0) {
            movieFlags |= MOVIE_EXTENDED_FLAG_0x04;
        } else {
            movieFlags &= ~MOVIE_EXTENDED_FLAG_0x04;
        }
    }

    if ((flags & MOVIE_FLAG_0x01) != 0) {
        movieScaleFlag = 1;
    } else {
        movieScaleFlag = 0;

        if ((movieFlags & MOVIE_EXTENDED_FLAG_0x04) == 0) {
            movieFlags &= ~MOVIE_EXTENDED_FLAG_0x08;
        }
    }

    if ((flags & MOVIE_FLAG_0x08) != 0) {
        movieFlags |= MOVIE_EXTENDED_FLAG_0x10;
    } else {
        movieFlags &= ~MOVIE_EXTENDED_FLAG_0x10;
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

// 0x4790EC
static void cleanupLast()
{
    if (lastMovieBuffer != NULL) {
        myfree(lastMovieBuffer, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 981
        lastMovieBuffer = NULL;
    }

    MVE_lastBuffer = NULL;
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
    subtitleW = win_width(GNWWin);
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
        movieFlags &= ~MOVIE_EXTENDED_FLAG_0x10;
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

    if ((movieFlags & MOVIE_EXTENDED_FLAG_0x10) == 0) {
        return;
    }

    int v1 = text_height();
    int v2 = (480 - lastMovieH - lastMovieY - v1) / 2 + lastMovieH + lastMovieY;

    if (subtitleH + v2 > windowGetYres()) {
        subtitleH = windowGetYres() - v2;
    }

    int frame;
    int dropped;
    MVE_rmFrameCounts(&frame, &dropped);

    while (subtitleList != NULL) {
        if (frame < subtitleList->num) {
            break;
        }

        MovieSubtitleListNode* next = subtitleList->next;

        win_fill(GNWWin, 0, v2, subtitleW, subtitleH, 0);

        int oldFont;
        if (subtitleFont != -1) {
            oldFont = text_curr();
            text_font(subtitleFont);
        }

        int colorIndex = (subtitleR << 10) | (subtitleG << 5) | subtitleB;
        windowWrapLine(GNWWin, subtitleList->text, subtitleW, subtitleH, 0, v2, colorTable[colorIndex] | 0x2000000, TEXT_ALIGNMENT_CENTER);

        Rect rect;
        rect.lrx = subtitleW;
        rect.uly = v2;
        rect.lry = v2 + subtitleH;
        rect.ulx = 0;
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

    cleanupLast();

    handle = openFile(filePath);
    if (handle == NULL) {
        return 1;
    }

    GNWWin = win;
    running = 1;
    movieFlags &= ~MOVIE_EXTENDED_FLAG_0x01;

    if ((movieFlags & MOVIE_EXTENDED_FLAG_0x10) != 0) {
        openSubtitle(filePath);
    }

    if ((movieFlags & MOVIE_EXTENDED_FLAG_0x04) != 0) {
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

    if (alphaHandle != NULL) {
        unsigned long size;
        db_freadLong(alphaHandle, &size);

        short tmp;
        db_freadInt16(alphaHandle, &tmp);
        db_freadInt16(alphaHandle, &tmp);

        alphaBuf = (unsigned char*)mymalloc(size, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1178
        alphaWindowBuf = (unsigned char*)mymalloc(movieH * movieW, __FILE__, __LINE__); // "..\\int\\MOVIE.C", 1179

        unsigned char* windowBuffer = win_get_buf(GNWWin);
        buf_to_buf(windowBuffer + win_width(GNWWin) * movieY + movieX,
            movieW,
            movieH,
            win_width(GNWWin),
            alphaWindowBuf,
            movieW);
    }

    movieRect.ulx = movieX;
    movieRect.uly = movieY;
    movieRect.lrx = movieW + movieX;
    movieRect.lry = movieH + movieY;

    return 0;
}

// 0x479768
static bool localMovieCallback()
{
    doSubtitle();

    if (movieCallback != NULL) {
        movieCallback();
    }

    return get_input() != -1;
}

// 0x4798CC
int movieRun(int win, char* filePath)
{
    if (running) {
        return 1;
    }

    movieX = 0;
    movieY = 0;
    movieOffset = 0;
    movieW = win_width(win);
    movieH = win_height(win);
    movieSubRectFlag = 0;
    return movieStart(win, filePath);
}

// 0x479920
int movieRunRect(int win, char* filePath, int a3, int a4, int a5, int a6)
{
    if (running) {
        return 1;
    }

    movieX = a3;
    movieY = a4;
    movieOffset = a3 + a4 * win_width(win);
    movieW = a5;
    movieH = a6;
    movieSubRectFlag = 1;

    return movieStart(win, filePath);
}

// 0x479980
static int stepMovie()
{
    if (alphaHandle != NULL) {
        unsigned long size;
        db_freadLong(alphaHandle, &size);
        db_fread(alphaBuf, 1, size, alphaHandle);
    }

    int rc = _MVE_rmStepMovie();
    if (rc != -1) {
        doSubtitle();
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

    if ((movieFlags & MOVIE_EXTENDED_FLAG_0x02) != 0) {
        debug_printf("Movie aborted\n");
        cleanupMovie(1);
        return;
    }

    if ((movieFlags & MOVIE_EXTENDED_FLAG_0x01) != 0) {
        debug_printf("Movie error\n");
        cleanupMovie(1);
        return;
    }

    if (stepMovie() == -1) {
        cleanupMovie(1);
        return;
    }

    if (updateCallbackFunc != NULL) {
        int frame;
        int dropped;
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
