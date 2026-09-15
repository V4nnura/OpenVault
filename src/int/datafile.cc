#include "int/datafile.h"

#include <string.h>

#include "int/memdbg.h"
#include "int/pcx.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/db/db.h"

namespace fallout {

constexpr size_t INDEXED_PALETTE_MAX = 256;
constexpr size_t DATA_FILE_PALETTE_MAX = INDEXED_PALETTE_MAX * 3;

// 0x504EAC
static DatafileLoader* loadFunc = NULL;

// 0x504EB0
static DatafileNameMangler* mangleName = defaultMangleName;

// 0x56BF70
uint8_t pal[DATA_FILE_PALETTE_MAX];

// 0x429450
char* defaultMangleName(char* path)
{
    return path;
}

// 0x429454
void datafileSetFilenameFunc(DatafileNameMangler* mangler)
{
    mangleName = mangler;
}

// 0x42945C
void setBitmapLoadFunc(DatafileLoader* loader)
{
    loadFunc = loader;
}

// 0x429464
void datafileConvertData(uint8_t* data, uint8_t* palette, int width, int height)
{
    uint8_t indexedPalette[INDEXED_PALETTE_MAX];

    indexedPalette[0] = 0;
    for (int index = 1; index < INDEXED_PALETTE_MAX; index++) {
        int r = palette[index * 3] >> 3;
        int g = palette[index * 3 + 1] >> 3;
        int b = palette[index * 3 + 2] >> 3;
        int colorTableIndex = (r << 10) | (g << 5) | b;
        indexedPalette[index] = colorTable[colorTableIndex];
    }

    int size = width * height;
    for (int index = 0; index < size; index++) {
        data[index] = indexedPalette[data[index]];
    }
}

// 0x4294D8
void datafileConvertDataVGA(uint8_t* data, uint8_t* palette, int width, int height)
{
    uint8_t indexedPalette[INDEXED_PALETTE_MAX];

    indexedPalette[0] = 0;
    for (int index = 1; index < INDEXED_PALETTE_MAX; index++) {
        int r = palette[index * 3] >> 1;
        int g = palette[index * 3 + 1] >> 1;
        int b = palette[index * 3 + 2] >> 1;
        int colorTableIndex = (r << 10) | (g << 5) | b;
        indexedPalette[index] = colorTable[colorTableIndex];
    }

    int size = width * height;
    for (int index = 0; index < size; index++) {
        data[index] = indexedPalette[data[index]];
    }
}

// 0x429540
uint8_t* loadRawDataFile(char* path, int* widthPtr, int* heightPtr)
{
    char* mangledPath = mangleName(path);
    char* dot = strrchr(mangledPath, '.');
    if (dot != NULL) {
        if (compat_stricmp(dot + 1, "pcx") == 0) {
            return loadPCX(mangledPath, widthPtr, heightPtr, pal);
        }
    }

    if (loadFunc != NULL) {
        return loadFunc(mangledPath, pal, widthPtr, heightPtr);
    }

    return NULL;
}

// 0x4295AC
uint8_t* loadDataFile(char* path, int* widthPtr, int* heightPtr)
{
    uint8_t* imageData = loadRawDataFile(path, widthPtr, heightPtr);
    if (imageData != NULL) {
        datafileConvertData(imageData, pal, *widthPtr, *heightPtr);
    }
    return imageData;
}

// 0x4295D4
uint8_t* load256Palette(char* path)
{
    int width;
    int height;
    uint8_t* imageData = loadRawDataFile(path, &width, &height);
    if (imageData != NULL) {
        myfree(imageData, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 148
        return pal;
    }

    return NULL;
}

// 0x429604
void trimBuffer(uint8_t* data, int* widthPtr, int* heightPtr)
{
    int width = *widthPtr;
    int height = *heightPtr;
    uint8_t* compactDataWritePtr = (uint8_t*)mymalloc(width * height, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 157

    // NOTE: Original code does not initialize `x`.
    int y = 0;
    int x = 0;
    uint8_t* rowStart = data;

    for (y = 0; y < height; y++) {
        if (*rowStart == 0) {
            break;
        }

        uint8_t* currentPixel = rowStart;
        for (x = 0; x < width; x++) {
            if (*currentPixel == 0) {
                break;
            }

            *compactDataWritePtr++ = *currentPixel++;
        }

        rowStart += width;
    }

    memcpy(data, compactDataWritePtr, x * y);
    myfree(compactDataWritePtr, __FILE__, __LINE__); // // "..\\int\\DATAFILE.C", 171
}

// 0x4296C4
uint8_t* datafileGetPalette()
{
    return pal;
}

// 0x4296CC
uint8_t* datafileLoadBlock(char* path, int* sizePtr)
{
    const char* mangledPath = mangleName(path);
    DB_FILE* stream = db_fopen(mangledPath, "rb");
    if (stream == NULL) {
        return NULL;
    }

    int size = db_filelength(stream);
    uint8_t* data = (uint8_t*)mymalloc(size, __FILE__, __LINE__); // "..\\int\\DATAFILE.C", 185
    if (data == NULL) {
        // NOTE: This code is unreachable, mymalloc never fails.
        // Otherwise it leaks stream.
        *sizePtr = 0;
        return NULL;
    }

    db_fread(data, 1, size, stream);
    db_fclose(stream);
    *sizePtr = size;
    return data;
}

} // namespace fallout
