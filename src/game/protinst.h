#ifndef FALLOUT_GAME_PROTINST_H_
#define FALLOUT_GAME_PROTINST_H_

#include "game/object_types.h"

namespace fallout {

enum UseItemResultCode {
    USE_ITEM_RESULT_ERROR = -1,
    USE_ITEM_RESULT_OK = 0,
    USE_ITEM_RESULT_REMOVE = 1,
};

int obj_sid(Object* object, int* sidPtr);
int obj_new_sid(Object* object, int* sidPtr);
int obj_new_sid_inst(Object* obj, int scriptType, int scriptIndex);
int obj_look_at(Object* critter, Object* target);
int obj_look_at_func(Object* critter, Object* target, void (*fn)(const char* string));
int obj_examine(Object* critter, Object* target);
int obj_examine_func(Object* critter, Object* target, void (*fn)(const char* string));
int obj_pickup(Object* critter, Object* item);
int obj_remove_from_inven(Object* critter, Object* item);
int obj_drop(Object* inven, Object* item);
int obj_destroy(Object* obj);
UseItemResultCode obj_use_radio(Object* item);
UseItemResultCode protinst_use_item(Object* critter, Object* item);
UseItemResultCode obj_use_item(Object* user, Object* item);
UseItemResultCode protinst_use_item_on(Object* critter, Object* target, Object* item);
UseItemResultCode obj_use_item_on(Object* user, Object* target, Object* item);
int check_scenery_ap_cost(Object* obj, Object* a2);
int obj_use(Object* user, Object* target);
int obj_use_door(Object* user, Object* door, bool animateOnly = false);
int obj_use_container(Object* critter, Object* item);
int obj_use_skill_on(Object* source, Object* target, int skill);
bool obj_is_a_portal(Object* obj);
bool obj_is_lockable(Object* obj);
bool obj_is_locked(Object* obj);
int obj_lock(Object* obj);
int obj_unlock(Object* obj);
bool obj_is_openable(Object* obj);
int obj_is_open(Object* obj);
int obj_toggle_open(Object* obj);
int obj_open(Object* obj);
int obj_close(Object* obj);
bool obj_lock_is_jammed(Object* obj);
int obj_jam_lock(Object* obj);
int obj_unjam_lock(Object* obj);
int obj_unjam_all_locks();
int obj_attempt_placement(Object* obj, int tile, int elevation, int radius);

} // namespace fallout

#endif /* FALLOUT_GAME_PROTINST_H_ */
