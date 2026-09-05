#include "int/audio.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <adecode/adecode.h>

#include "int/memdbg.h"
#include "int/sound.h"
#include "plib/db/db.h"
#include "plib/gnw/debug.h"

namespace fallout {

typedef enum AudioFlags {
    AUDIO_FILE_IN_USE = 0x01,
    AUDIO_FILE_COMPRESSED = 0x02,
} AudioFlags;

typedef struct Audio {
    int flags;
    DB_FILE* stream;
    AudioDecoder* audioDecoder;
    int fileSize;
    int sampleRate;
    int channels;
    int position;
} Audio;

static bool defaultCompressionFunc(char* filePath);
static unsigned int decodeRead(void* stream, void* buf, unsigned int size);

// 0x4FEC00
static AudioQueryCompressedFunc* queryCompressedFunc = defaultCompressionFunc;

// 0x56B860
static int numAudio;

// 0x56B864
static Audio* audio;

// 0x4198F0
static bool defaultCompressionFunc(char* filePath)
{
    char* pch = strrchr(filePath, '.');
    if (pch != NULL) {
        strcpy(pch + 1, "raw");
    }

    return false;
}

// 0x419910
static unsigned int decodeRead(void* stream, void* buffer, unsigned int size)
{
    return static_cast<unsigned int>(db_fread(buffer, 1, size, (DB_FILE*)stream));
}

// 0x41992C
int audioOpen(const char* fname, int* sampleRate)
{
    char path[80];
    snprintf(path, sizeof(path), "%s", fname);

    int compression;
    if (queryCompressedFunc(path)) {
        compression = 2;
    } else {
        compression = 0;
    }

    DB_FILE* stream = db_fopen(path, "rb");
    if (stream == NULL) {
        debug_printf("AudioOpen: Couldn't open %s for read\n", path);
        return -1;
    }

    int index;
    for (index = 0; index < numAudio; index++) {
        if ((audio[index].flags & AUDIO_FILE_IN_USE) == 0) {
            break;
        }
    }

    if (index == numAudio) {
        if (audio != NULL) {
            audio = (Audio*)myrealloc(audio, sizeof(*audio) * (numAudio + 1), __FILE__, __LINE__); // "..\int\audio.c", 216
        } else {
            audio = (Audio*)mymalloc(sizeof(*audio), __FILE__, __LINE__); // "..\int\audio.c", 218
        }
        numAudio++;
    }

    Audio* audioFile = &(audio[index]);
    audioFile->flags = AUDIO_FILE_IN_USE;
    audioFile->stream = stream;

    if (compression == 2) {
        audioFile->flags |= AUDIO_FILE_COMPRESSED;
        audioFile->audioDecoder = Create_AudioDecoder(decodeRead, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));
        if (audioFile->audioDecoder == NULL) {
            db_fclose(stream);
            return -1;
        }
        audioFile->fileSize *= 2;

        *sampleRate = audioFile->sampleRate;
    } else {
        audioFile->fileSize = static_cast<int>(db_filelength(stream));
    }

    audioFile->position = 0;

    return index + 1;
}

// 0x419B4C
int audioCloseFile(int fileHandle)
{
    Audio* audioFile = &(audio[fileHandle - 1]);
    db_fclose(audioFile->stream);

    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        AudioDecoder_Close(audioFile->audioDecoder);
    }

    memset(audioFile, 0, sizeof(Audio));

    return 0;
}

// 0x419BB4
int audioRead(int fileHandle, void* buffer, unsigned int size)
{
    Audio* audioFile = &(audio[fileHandle - 1]);

    int bytesRead;
    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        bytesRead = AudioDecoder_Read(audioFile->audioDecoder, buffer, size);
    } else {
        bytesRead = static_cast<int>(db_fread(buffer, 1, size, audioFile->stream));
    }

    audioFile->position += bytesRead;

    return bytesRead;
}

// 0x419C20
long audioSeek(int fileHandle, long offset, int origin)
{
    int pos;
    unsigned char* buf;
    int remainingBytesToSkip;

    Audio* audioFile = &(audio[fileHandle - 1]);

    switch (origin) {
    case SEEK_SET:
        pos = static_cast<int>(offset);
        break;
    case SEEK_CUR:
        pos = static_cast<int>(offset) + audioFile->position;
        break;
    case SEEK_END:
        pos = static_cast<int>(offset) + audioFile->fileSize;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    if (pos < 0) {
        pos = 0;
    }
    if (pos > audioFile->fileSize) {
        pos = audioFile->fileSize;
    }

    if ((audioFile->flags & AUDIO_FILE_COMPRESSED) != 0) {
        if (pos < audioFile->position) {
            AudioDecoder_Close(audioFile->audioDecoder);
            db_fseek(audioFile->stream, 0, SEEK_SET);
            audioFile->audioDecoder = Create_AudioDecoder(decodeRead, audioFile->stream, &(audioFile->channels), &(audioFile->sampleRate), &(audioFile->fileSize));

            // If decoder creation failed, disable compression and fall back to uncompressed seeking
            if (audioFile->audioDecoder == NULL) {
                audioFile->flags &= ~AUDIO_FILE_COMPRESSED;
                audioFile->fileSize = db_filelength(audioFile->stream);
                audioFile->position = 0;
                return db_fseek(audioFile->stream, pos, SEEK_SET);
            }

            audioFile->position = 0;
            audioFile->fileSize *= 2;

            // After reinitializing decoder, just return position 0 if seeking to start
            // Don't try to skip ahead - let normal reads handle it
            if (pos == 0) {
                return audioFile->position;
            }

            if (pos != 0) {
                buf = (unsigned char*)mymalloc(4096, __FILE__, __LINE__); // "..\int\audio.c", 361
                while (pos > 4096) {
                    pos -= 4096;
                    audioRead(fileHandle, buf, 4096);
                }

                if (pos != 0) {
                    audioRead(fileHandle, buf, pos);
                }

                myfree(buf, __FILE__, __LINE__); // // "..\int\audio.c", 367
            }
        } else {
            buf = (unsigned char*)mymalloc(1024, __FILE__, __LINE__); // "..\int\audio.c", 321
            remainingBytesToSkip = pos - audioFile->position;
            while (remainingBytesToSkip > 1024) {
                remainingBytesToSkip -= 1024;
                audioRead(fileHandle, buf, 1024);
            }

            if (remainingBytesToSkip != 0) {
                audioRead(fileHandle, buf, remainingBytesToSkip);
            }

            myfree(buf, __FILE__, __LINE__);
        }

        return audioFile->position;
    } else {
        return db_fseek(audioFile->stream, offset, origin);
    }
}

// 0x419DCC
long audioFileSize(int fileHandle)
{
    Audio* audioFile = &(audio[fileHandle - 1]);
    return audioFile->fileSize;
}

// 0x419DE8
long audioTell(int fileHandle)
{
    Audio* audioFile = &(audio[fileHandle - 1]);
    return audioFile->position;
}

// 0x419E04
int audioWrite(int handle, const void* buf, unsigned int size)
{
    debug_printf("AudioWrite shouldn't be ever called\n");
    return 0;
}

// 0x419E14
int initAudio(AudioQueryCompressedFunc* isCompressedProc)
{
    queryCompressedFunc = isCompressedProc;
    audio = NULL;
    numAudio = 0;

    return soundSetDefaultFileIO(audioOpen, audioCloseFile, audioRead, audioWrite, audioSeek, audioTell, audioFileSize);
}

// 0x419E58
void audioClose()
{
    if (audio != NULL) {
        myfree(audio, __FILE__, __LINE__); // "..\int\audio.c", 406
    }

    numAudio = 0;
    audio = NULL;
}

} // namespace fallout
