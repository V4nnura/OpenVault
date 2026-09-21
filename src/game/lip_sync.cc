#include "game/lip_sync.h"

#include <stdio.h>
#include <string.h>

#include "game/gsound.h"
#include "int/audio.h"
#include "int/sound.h"
#include "platform_compat.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"

namespace fallout {

static char* lips_fix_string(const char* fileName, size_t length);
static int lips_stop_speech();
static int lips_read_phoneme_type(unsigned char* phoneme_type, DB_FILE* stream);
static int lips_read_marker_type(SpeechMarker* marker_type, DB_FILE* stream);
static int lips_read_lipsynch_info(LipsData* lipsData, DB_FILE* stream);
static int lips_make_speech();
static unsigned char lips_get_phoneme(int index);

// 0x5057E4
unsigned char head_phoneme_current = 0;

// 0x5057E5
unsigned char head_phoneme_drawn = 0;

// 0x5057E8
int head_marker_current = 0;

// 0x5057EC
bool lips_draw_head = true;

// 0x5057F0
LipsData lip_info = {
    2,
    22528,
    0,
    NULL,
    -1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    50,
    100,
    0,
    0,
    0,
    "TEST",
    "VOC",
    "TXT",
    "LIP",
};

// 0x505958
int speechStartTime = 0;

// 0x612220
static char lips_subdir_name[14];

// 0x46CC30
static char* lips_fix_string(const char* fileName, size_t length)
{
    // 0x61222E
    static char tmp_str[50];

    strncpy(tmp_str, fileName, length);
    return tmp_str;
}

static unsigned char lips_get_phoneme(int index)
{
    // The terminal marker has no corresponding phoneme.
    if (lip_info.phonemes == NULL || index < 0 || index >= lip_info.phoneme_count) {
        return 0;
    }

    unsigned char phoneme = lip_info.phonemes[index];
    return phoneme < PHONEME_COUNT ? phoneme : 0;
}

// 0x46CC48
void lips_bkg_proc()
{
    int markerIndex;
    SpeechMarker* speech_marker;
    int wrapCheckMarkerIndex;

    markerIndex = head_marker_current;

    if (lip_info.sound == NULL || lip_info.markers == NULL
        || markerIndex < 0 || markerIndex >= lip_info.marker_count) {
        lip_info.flags &= ~(LIPS_FLAG_LOOPING | LIPS_FLAG_PLAYING);
        head_phoneme_current = 0;
        markerIndex = 0;
    }

    if ((lip_info.flags & LIPS_FLAG_PLAYING) != 0) {
        int audioPosition = soundGetPosition(lip_info.sound);

        speech_marker = &(lip_info.markers[markerIndex]);
        while (audioPosition > speech_marker->position) {
            head_phoneme_current = lips_get_phoneme(markerIndex);
            markerIndex++;

            if (markerIndex >= lip_info.marker_count) {
                markerIndex = 0;
                head_phoneme_current = lips_get_phoneme(0);

                if ((lip_info.flags & LIPS_FLAG_LOOPING) == 0) {
                    // NOTE: Uninline.
                    lips_stop_speech();
                    markerIndex = head_marker_current;
                }

                break;
            }

            speech_marker = &(lip_info.markers[markerIndex]);
        }

        if (markerIndex >= lip_info.marker_count - 1) {
            head_marker_current = markerIndex;

            wrapCheckMarkerIndex = 0;
            if (lip_info.marker_count <= 5) {
                debug_printf("Error: Too few markers to stop speech!");
            } else {
                wrapCheckMarkerIndex = 3;
            }

            speech_marker = &(lip_info.markers[wrapCheckMarkerIndex]);
            if (audioPosition < speech_marker->position) {
                markerIndex = 0;
                head_phoneme_current = lips_get_phoneme(0);

                if ((lip_info.flags & LIPS_FLAG_LOOPING) == 0) {
                    // NOTE: Uninline.
                    lips_stop_speech();
                    markerIndex = head_marker_current;
                }
            }
        }
    }

    if (head_phoneme_drawn != head_phoneme_current) {
        head_phoneme_drawn = head_phoneme_current;
        lips_draw_head = true;
    }

    head_marker_current = markerIndex;

    soundUpdate();
}

// 0x46CD9C
int lips_play_speech()
{
    lip_info.flags &= ~LIPS_FLAG_PLAYING;
    head_marker_current = 0;

    if (lip_info.sound == NULL || lip_info.markers == NULL || lip_info.marker_count <= 0) {
        return -1;
    }

    if (soundSetPosition(lip_info.sound, lip_info.start_offset) != 0) {
        debug_printf("Failed set of start_offset!\n");
    }

    int markerIndex = head_marker_current;
    while (markerIndex < lip_info.marker_count) {
        head_marker_current = markerIndex;

        SpeechMarker* speechEntry = &(lip_info.markers[markerIndex]);
        if (lip_info.start_offset <= speechEntry->position) {
            break;
        }

        head_phoneme_current = lips_get_phoneme(markerIndex);
        markerIndex++;
    }

    if (markerIndex == lip_info.markerCount) {
        head_marker_current = 0;
        soundStop(lip_info.sound);
        return -1;
    }

    lip_info.flags |= LIPS_FLAG_PLAYING;

    int speechVolume = gsound_speech_volume_get();
    soundVolume(lip_info.sound, (int)(speechVolume * 0.69));

    speechStartTime = get_time();

    if (soundPlay(lip_info.sound) != 0) {
        debug_printf("Failed play!\n");

        // NOTE: Uninline.
        lips_stop_speech();
    }

    return 0;
}

// 0x46CE9C
static int lips_stop_speech()
{
    head_marker_current = 0;
    soundStop(lip_info.sound);
    lip_info.flags &= ~(LIPS_FLAG_LOOPING | LIPS_FLAG_PLAYING);
    return 0;
}

// 0x46CEBC
static int lips_read_phoneme_type(unsigned char* phoneme_type, DB_FILE* stream)
{
    return db_freadByte(stream, phoneme_type);
}

// 0x46CECC
static int lips_read_marker_type(SpeechMarker* marker_type, DB_FILE* stream)
{
    int marker;

    // Marker is read into temporary variable.
    if (db_freadInt32(stream, &marker) == -1) return -1;

    // Position is read directly into struct.
    if (db_freadInt32(stream, &(marker_type->position)) == -1) return -1;

    marker_type->marker = marker;

    return 0;
}

// 0x46CF08
static int lips_read_lipsynch_info(LipsData* lipsData, DB_FILE* stream)
{
    int sound;
    int field_14;
    int phonemes;
    int markers;

    if (db_freadInt32(stream, &(lipsData->version)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->field_4)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->flags)) == -1) return -1;
    if (db_freadInt32(stream, &(sound)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->field_10)) == -1) return -1;
    if (db_freadInt32(stream, &(field_14)) == -1) return -1;
    if (db_freadInt32(stream, &(phonemes)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->field_1C)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->start_offset)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->phoneme_count)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->field_28)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->marker_count)) == -1) return -1;
    if (db_freadInt32(stream, &(markers)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->field_34)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->field_38)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->field_3C)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->field_40)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->field_44)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->field_48)) == -1) return -1;
    if (db_freadInt32(stream, &(lipsData->field_4C)) == -1) return -1;
    if (db_freadInt8List(stream, lipsData->file_name, 8) == -1) return -1;
    if (db_freadInt8List(stream, lipsData->audio_ext, 4) == -1) return -1;
    if (db_freadInt8List(stream, lipsData->text_ext, 4) == -1) return -1;
    if (db_freadInt8List(stream, lipsData->lip_ext, 4) == -1) return -1;
    if (db_freadInt8List(stream, lipsData->field_64, 260) == -1) return -1;

    // NOTE: Original code is different. For unknown reason it assigns values
    // from file (integers) and treat them as pointers, which is obviously wrong
    // is in this case.
    lipsData->sound = nullptr;
    lipsData->field_14 = nullptr;
    lipsData->phonemes = nullptr;
    lipsData->markers = nullptr;

    return 0;
}

// 0x46D11C
int lips_load_file(const char* audioFileName, const char* headFileName)
{
    char* sep;
    int i;
    char audioBaseName[16];

    SpeechMarker* speech_marker;
    SpeechMarker* prev_speech_marker;

    char path[260];
    strcpy(path, "SOUND\\SPEECH\\");

    strcpy(lips_subdir_name, headFileName);

    strcat(path, lips_subdir_name);

    strcat(path, "\\");

    sep = strchr(path, '.');
    if (sep != NULL) {
        *sep = '\0';
    }

    strcpy(audioBaseName, audioFileName);

    sep = strchr(audioBaseName, '.');
    if (sep != NULL) {
        *sep = '\0';
    }

    strncpy(lip_info.file_name, audioBaseName, sizeof(lip_info.file_name));

    strcat(path, lips_fix_string(lip_info.file_name, sizeof(lip_info.file_name)));
    strcat(path, ".");
    strcat(path, lip_info.lip_ext);

    lips_free_speech();

    // FIXME: stream is not closed if any error is encountered during reading.
    DB_FILE* stream = db_fopen(path, "rb");
    if (stream == NULL) {
        return -1;
    }
    if (stream != NULL) {
        if (db_freadInt32(stream, &(lip_info.version)) == -1) {
            return -1;
        }

        if (lip_info.version == 1) {
            debug_printf("\nLoading old save-file version (1)");

            if (db_fseek(stream, 0, SEEK_SET) != 0) {
                return -1;
            }

            if (lips_read_lipsynch_info(&lip_info, stream) != 0) {
                return -1;
            }
        } else if (lip_info.version == 2) {
            debug_printf("\nLoading current save-file version (2)");

            if (db_freadInt32(stream, &(lip_info.field_4)) == -1) return -1;
            if (db_freadInt32(stream, &(lip_info.flags)) == -1) return -1;
            if (db_freadInt32(stream, &(lip_info.field_10)) == -1) return -1;
            if (db_freadInt32(stream, &(lip_info.field_1C)) == -1) return -1;
            if (db_freadInt32(stream, &(lip_info.phoneme_count)) == -1) return -1;
            if (db_freadInt32(stream, &(lip_info.field_28)) == -1) return -1;
            if (db_freadInt32(stream, &(lip_info.marker_count)) == -1) return -1;
            if (db_freadInt8List(stream, lip_info.file_name, 8) == -1) return -1;
            if (db_freadInt8List(stream, lip_info.audio_ext, 4) == -1) return -1;
        } else {
            debug_printf("\nError: Lips file WRONG version!");
            db_fclose(stream);
            return -1;
        }
    }

    // Check the serialized arrays before allocating or indexing them.
    if (lip_info.phoneme_count <= 0 || lip_info.marker_count <= 0) {
        debugPrint("lips_load_file: Invalid phoneme or marker count.\n");
        db_fclose(stream);
        return -1;
    }

    long remaining = db_filelength(stream) - db_ftell(stream);
    if (remaining < lip_info.phoneme_count
        || lip_info.marker_count > (remaining - lip_info.phoneme_count) / 8) {
        debugPrint("lips_load_file: Invalid phoneme or marker count.\n");
        db_fclose(stream);
        return -1;
    }

    lip_info.phonemes = (unsigned char*)mem_malloc(lip_info.phoneme_count);
    if (lip_info.phonemes == NULL) {
        debug_printf("Out of memory in lips_load_file.'\n");
        return -1;
    }

    if (stream != NULL) {
        for (i = 0; i < lip_info.phoneme_count; i++) {
            if (lips_read_phoneme_type(&(lip_info.phonemes[i]), stream) != 0) {
                debug_printf("lips_load_file: Error reading phoneme type.\n");
                return -1;
            }
        }

        for (i = 0; i < lip_info.phoneme_count; i++) {
            unsigned char phoneme = lip_info.phonemes[i];
            if (phoneme >= PHONEME_COUNT) {
                debug_printf("\nLoad error: Speech phoneme %d is invalid (%d)!", i, phoneme);
            }
        }
    }

    lip_info.markers = (SpeechMarker*)mem_malloc(sizeof(*speech_marker) * lip_info.marker_count);
    if (lip_info.markers == NULL) {
        debug_printf("Out of memory in lips_load_file.'\n");
        return -1;
    }

    if (stream != NULL) {
        for (i = 0; i < lip_info.marker_count; i++) {
            // NOTE: Uninline.
            if (lips_read_marker_type(&(lip_info.markers[i]), stream) != 0) {
                debug_printf("lips_load_file: Error reading marker type.");
                return -1;
            }
        }

        speech_marker = &(lip_info.markers[0]);

        if (speech_marker->marker != 1 && speech_marker->marker != 0) {
            debug_printf("\nLoad error: Speech marker 0 is invalid (%d)!", speech_marker->marker);
        }

        if (speech_marker->position != 0) {
            debug_printf("Load error: Speech marker 0 has invalid position(%d)!", speech_marker->position);
        }

        for (i = 1; i < lip_info.marker_count; i++) {
            speech_marker = &(lip_info.markers[i]);
            prev_speech_marker = &(lip_info.markers[i - 1]);

            if (speech_marker->marker != 1 && speech_marker->marker != 0) {
                debug_printf("\nLoad error: Speech marker %d is invalid (%d)!", i, speech_marker->marker);
            }

            if (speech_marker->position < prev_speech_marker->position) {
                debug_printf("Load error: Speech marker %d has invalid position(%d)!", i, speech_marker->position);
            }
        }
    }

    if (stream != NULL) {
        db_fclose(stream);
    }

    lip_info.field_38 = 0;
    lip_info.field_34 = 0;
    lip_info.field_48 = 0;
    lip_info.start_offset = 0;
    lip_info.field_3C = 50;
    lip_info.field_40 = 100;

    if (lip_info.version == 1) {
        lip_info.field_4 = 22528;
    }

    strcpy(lip_info.audio_ext, "VOC");
    strcpy(lip_info.audio_ext, "ACM");
    strcpy(lip_info.text_ext, "TXT");
    strcpy(lip_info.lip_ext, "LIP");

    if (lips_make_speech() == -1) return -1;

    head_marker_current = 0;
    head_phoneme_current = lips_get_phoneme(0);

    return 0;
}

// 0x46D740
static int lips_make_speech()
{
    if (lip_info.field_14 != NULL) {
        mem_free(lip_info.field_14);
        lip_info.field_14 = NULL;
    }

    char path[COMPAT_MAX_PATH];
    char* audioBaseName = lips_fix_string(lip_info.file_name, sizeof(lip_info.file_name));
    snprintf(path, sizeof(path), "%s%s\\%s.%s", "SOUND\\SPEECH\\", lips_subdir_name, audioBaseName, "ACM");

    if (lip_info.sound != NULL) {
        soundDelete(lip_info.sound);
        lip_info.sound = NULL;
    }

    lip_info.sound = soundAllocate(SOUND_TYPE_MEMORY, SOUND_16BIT);
    if (lip_info.sound == NULL) {
        debug_printf("\nsoundAllocate falied in lips_make_speech!");
        return -1;
    }

    if (soundSetFileIO(lip_info.sound, audioOpen, audioCloseFile, audioRead, NULL, audioSeek, NULL, audioFileSize)) {
        debug_printf("Ack!");
        debug_printf("Error!");
    }

    if (soundLoad(lip_info.sound, path)) {
        soundDelete(lip_info.sound);
        lip_info.sound = NULL;

        debug_printf("lips_make_speech: soundLoad failed with path ");
        debug_printf("%s -- file probably doesn't exist.\n", path);
        return -1;
    }

    return 0;
}

// 0x46D8A0
int lips_free_speech()
{
    head_marker_current = 0;
    lip_info.flags &= ~(LIPS_FLAG_LOOPING | LIPS_FLAG_PLAYING);
    head_phoneme_current = 0;
    head_phoneme_drawn = 0;
    lips_draw_head = true;

    if (lip_info.field_14 != NULL) {
        mem_free(lip_info.field_14);
        lip_info.field_14 = NULL;
    }

    if (lip_info.sound != NULL) {
        // NOTE: Uninline.
        lips_stop_speech();

        soundDelete(lip_info.sound);

        lip_info.sound = NULL;
    }

    if (lip_info.phonemes != NULL) {
        mem_free(lip_info.phonemes);
        lip_info.phonemes = NULL;
    }

    if (lip_info.markers != NULL) {
        mem_free(lip_info.markers);
        lip_info.markers = NULL;
    }

    return 0;
}

} // namespace fallout
