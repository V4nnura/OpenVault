#ifndef FALLOUT_INT_AUDIOF_H_
#define FALLOUT_INT_AUDIOF_H_

namespace fallout {

typedef bool(AudioFileQueryCompressedFunc)(char* filePath);

int audiofOpen(const char* fname, int* sampleRate);
int audiofCloseFile(int fileHandle);
int audiofRead(int fileHandle, void* buf, unsigned int size);
long audiofSeek(int fileHandle, long offset, int origin);
long audiofFileSize(int fileHandle);
long audiofTell(int fileHandle);
int audiofWrite(int fileHandle, const void* buf, unsigned int size);
int initAudiof(AudioFileQueryCompressedFunc* isCompressedProc);
void audiofClose();

} // namespace fallout

#endif /* FALLOUT_INT_AUDIOF_H_ */
