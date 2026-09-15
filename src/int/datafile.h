#ifndef FALLOUT_INT_DATAFILE_H_
#define FALLOUT_INT_DATAFILE_H_

#include <cstdint>

namespace fallout {

typedef uint8_t*(DatafileLoader)(char* path, uint8_t* palette, int* widthPtr, int* heightPtr);
typedef char*(DatafileNameMangler)(char* path);

char* defaultMangleName(char* path);
void datafileConvertData(uint8_t* data, uint8_t* palette, int width, int height);
uint8_t* loadRawDataFile(char* path, int* widthPtr, int* heightPtr);
uint8_t* loadDataFile(char* path, int* widthPtr, int* heightPtr);
uint8_t* datafileGetPalette();
uint8_t* datafileLoadBlock(char* path, int* sizePtr);

} // namespace fallout

#endif /* FALLOUT_INT_DATAFILE_H_ */
