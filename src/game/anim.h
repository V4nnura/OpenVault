#ifndef FALLOUT_GAME_ANIMATION_H_
#define FALLOUT_GAME_ANIMATION_H_

#include "game/object_types.h"

namespace fallout {

// Signature of animation callback accepting 2 parameters.
typedef int AnimationCallback(void*, void*);

// Signature of animation callback accepting 3 parameters.
typedef int AnimationCallback3(void*, void*, void*);

typedef Object* PathBuilderCallback(Object* object, int tile, int elevation);

typedef struct StraightPathNode {
    int tile;
    int elevation;
    int x;
    int y;
} StraightPathNode;

void anim_init();
void anim_reset();
void anim_exit();
int register_begin(int a1);
int register_priority(int a1);
int register_clear(Object* a1);
int register_end();
int check_registry(Object* obj);
int anim_busy(Object* a1);
int register_object_move_to_object(Object* owner, Object* destination, int actionPoints, int delay);
int register_object_run_to_object(Object* owner, Object* destination, int actionPoints, int delay);
int register_object_move_to_tile(Object* owner, int tile, int elevation, int actionPoints, int delay);
int register_object_run_to_tile(Object* owner, int tile, int elevation, int actionPoints, int delay);
int register_object_move_straight_to_tile(Object* object, int tile, int elevation, int anim, int delay);
int register_object_animate_and_move_straight(Object* owner, int tile, int elev, int anim, int delay);
int register_object_move_on_stairs(Object* owner, Object* stairs, int delay);
int register_object_check_falling(Object* owner, int delay);
int register_object_animate(Object* owner, int anim, int delay);
int register_object_animate_reverse(Object* owner, int anim, int delay);
int register_object_animate_and_hide(Object* owner, int anim, int delay);
int register_object_turn_towards(Object* owner, int tile);
int register_object_inc_rotation(Object* owner);
int register_object_dec_rotation(Object* owner);
int register_object_erase(Object* object);
int register_object_must_erase(Object* object);
int register_object_call(void* a1, void* a2, AnimationCallback* proc, int delay);
int register_object_call3(void* a1, void* a2, void* a3, AnimationCallback3* proc, int delay);
int register_object_must_call(void* a1, void* a2, AnimationCallback* proc, int delay);
int register_object_fset(Object* object, int flag, int delay);
int register_object_funset(Object* object, int flag, int delay);
int register_object_flatten(Object* object, int delay);
int register_object_change_fid(Object* owner, int fid, int delay);
int register_object_take_out(Object* owner, int weaponAnimationCode, int delay);
int register_object_light(Object* owner, int lightDistance, int delay);
int register_object_outline(Object* object, bool outline, int delay);
int register_object_play_sfx(Object* owner, const char* soundEffectName, int delay);
int register_object_animate_forever(Object* owner, int anim, int delay);
int register_ping(int flags, int delay);
int make_path(Object* object, int from, int to, unsigned char* a4, int a5);
int make_path_func(Object* object, int from, int to, unsigned char* rotations, int a5, PathBuilderCallback* callback);
int idist(int a1, int a2, int a3, int a4);
int EST(int tile1, int tile2);
int make_straight_path(Object* a1, int from, int to, StraightPathNode* pathNodes, Object** a5, int a6);
int make_straight_path_func(Object* a1, int from, int to, StraightPathNode* pathNodes, Object** a5, int a6, PathBuilderCallback* callback);
int anim_move_on_stairs(Object* obj, int tile, int elevation, int anim, int animationSequenceIndex);
int check_for_falling(Object* obj, int anim, int a3);
void object_animate();
int check_move(int* a1);
int dude_move(int a1);
int dude_run(int a1);
void dude_fidget();
void dude_stand(Object* obj, int rotation, int fid);
void dude_standup(Object* a1);
int anim_hide(Object* object, int animationSequenceIndex);
int anim_change_fid(Object* obj, int animationSequenceIndex, int fid);
void anim_stop();
unsigned int compute_tpf(Object* object, int fid);

} // namespace fallout

#endif /* FALLOUT_GAME_ANIMATION_H_ */
