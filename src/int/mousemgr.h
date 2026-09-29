#ifndef FALLOUT_INT_MOUSEMGR_H_
#define FALLOUT_INT_MOUSEMGR_H_

namespace fallout {

#define MOUSE_MGR_CACHE_CAPACITY 32

typedef char*(MouseManagerNameMangler)(char* fileName);
typedef int(MouseManagerRateProvider)();
typedef int(MouseManagerTimeProvider)();

typedef enum MouseManagerMouseType {
    MOUSE_MANAGER_MOUSE_TYPE_NONE,
    MOUSE_MANAGER_MOUSE_TYPE_STATIC,
    MOUSE_MANAGER_MOUSE_TYPE_ANIMATED,
} MouseManagerMouseType;

typedef struct MouseManagerStaticData {
    unsigned char* data;
    int field_4;
    int field_8;
    int width;
    int height;
} MouseManagerStaticData;

typedef struct MouseManagerAnimatedData {
    unsigned char** field_0;
    unsigned char** field_4;
    int* field_8;
    int* field_C;
    int width;
    int height;
    float field_18;
    int field_1C;
    int field_20;
    signed char field_24;
    signed char frameCount;
    signed char field_26;
} MouseManagerAnimatedData;

typedef struct MouseManagerCacheEntry {
    union {
        void* data;
        MouseManagerStaticData* staticData;
        MouseManagerAnimatedData* animatedData;
    };
    int type;
    unsigned char palette[256 * 3];
    int ref;
    char fileName[32];
    char field_32C[32];
} MouseManagerCacheEntry;

void mousemgrSetNameMangler(MouseManagerNameMangler* func);
void mousemgrSetTimeCallback(MouseManagerRateProvider* rateFunc, MouseManagerTimeProvider* currentTimeFunc);
void initMousemgr();
void mousemgrClose();
void mousemgrUpdate();
int mouseSetFrame(char* fileName, int a2);
bool mouseSetMouseShape(char* fileName, int a2, int a3);
bool mouseSetMousePointer(char* fileName);
void mousemgrResetMouse();
void mouseHide();
void mouseShow();

} // namespace fallout

#endif /* FALLOUT_INT_MOUSEMGR_H_ */
