#ifndef FALLOUT_INT_MOVIE_H_
#define FALLOUT_INT_MOVIE_H_

#include "plib/gnw/rect.h"

namespace fallout {

typedef enum MovieFlags {
    MOVIE_FLAG_SCALE = 0x01,
    MOVIE_FLAG_DIRECT = 0x02,
    MOVIE_FLAG_DIRECT_CENTERED = 0x04,
    MOVIE_FLAG_SUBTITLES = 0x08,
} MovieFlags;

typedef enum MovieExtendedFlags {
    MOVIE_EXTENDED_FLAG_ERROR = 0x01,
    MOVIE_EXTENDED_FLAG_STOP_REQUESTED = 0x02,
    MOVIE_EXTENDED_FLAG_DIRECT = 0x04,
    MOVIE_EXTENDED_FLAG_CENTERED = 0x08,
    MOVIE_EXTENDED_FLAG_SUBTITLES = 0x10,
} MovieExtendedFlags;

typedef char*(MovieSubtitleFunc)(char* movieFilePath);
typedef void(MoviePaletteFunc)(unsigned char* palette, int start, int end);
typedef void(MovieUpdateCallbackProc)(int frame);
typedef int(MovieBlitFunc)(int win, unsigned char* data, int width, int height, int pitch);

void initMovie();
void movieClose();
void movieStop();
int movieSetFlags(int a1);
void movieSetSubtitleFont(int font);
void movieSetSubtitleColor(float r, float g, float b);
void movieSetPaletteFunc(MoviePaletteFunc* func);
void movieSetCallback(MovieUpdateCallbackProc* func);
int movieRun(int win, char* filePath);
int movieRunRect(int win, char* filePath, int a3, int a4, int a5, int a6);
void movieSetSubtitleFunc(MovieSubtitleFunc* proc);
void movieSetVolume(int volume);
void movieUpdate();
int moviePlaying();
void movieHandleRendererReset();
void movieRenderDirectOverlay();

} // namespace fallout

#endif /* FALLOUT_INT_MOVIE_H_ */
