#ifndef FALLOUT_GAME_ANIMATION_DEFS_H_
#define FALLOUT_GAME_ANIMATION_DEFS_H_

namespace fallout {

#define FID_ANIM_TYPE(value) ((value) & 0xFF0000) >> 16

typedef enum AnimationRequestOptions {
    ANIMATION_REQUEST_UNRESERVED = 0x01,
    ANIMATION_REQUEST_RESERVED = 0x02,
    ANIMATION_REQUEST_NO_STAND = 0x04,
    ANIMATION_REQUEST_PING = 0x100,
    ANIMATION_REQUEST_INSIGNIFICANT = 0x200,
} AnimationRequestOptions;

// Basic animations: 0-19
// Knockdown and death: 20-35
// Change positions: 36-37
// Weapon: 38-47
// Single-frame death animations (the last frame of knockdown and death animations): 48-63
typedef enum AnimationType {
    ANIM_STAND = 0,
    ANIM_WALK = 1,
    ANIM_JUMP_BEGIN = 2,
    ANIM_JUMP_END = 3,
    ANIM_CLIMB_LADDER = 4,
    ANIM_FALLING = 5,
    ANIM_UP_STAIRS_RIGHT = 6,
    ANIM_UP_STAIRS_LEFT = 7,
    ANIM_DOWN_STAIRS_RIGHT = 8,
    ANIM_DOWN_STAIRS_LEFT = 9,
    ANIM_MAGIC_HANDS_GROUND = 10,
    ANIM_MAGIC_HANDS_MIDDLE = 11,
    ANIM_MAGIC_HANDS_UP = 12,
    ANIM_DODGE_ANIM = 13,
    ANIM_HIT_FROM_FRONT = 14,
    ANIM_HIT_FROM_BACK = 15,
    ANIM_THROW_PUNCH = 16,
    ANIM_KICK_LEG = 17,
    ANIM_THROW_ANIM = 18,
    ANIM_RUNNING = 19,
    ANIM_FALL_BACK = 20,
    ANIM_FALL_FRONT = 21,
    ANIM_BAD_LANDING = 22,
    ANIM_BIG_HOLE = 23,
    ANIM_CHARRED_BODY = 24,
    ANIM_CHUNKS_OF_FLESH = 25,
    ANIM_DANCING_AUTOFIRE = 26,
    ANIM_ELECTRIFY = 27,
    ANIM_SLICED_IN_HALF = 28,
    ANIM_BURNED_TO_NOTHING = 29,
    ANIM_ELECTRIFIED_TO_NOTHING = 30,
    ANIM_EXPLODED_TO_NOTHING = 31,
    ANIM_MELTED_TO_NOTHING = 32,
    ANIM_FIRE_DANCE = 33,
    ANIM_FALL_BACK_BLOOD = 34,
    ANIM_FALL_FRONT_BLOOD = 35,
    ANIM_PRONE_TO_STANDING = 36,
    ANIM_BACK_TO_STANDING = 37,
    ANIM_TAKE_OUT = 38,
    ANIM_PUT_AWAY = 39,
    ANIM_PARRY_ANIM = 40,
    ANIM_THRUST_ANIM = 41,
    ANIM_SWING_ANIM = 42,
    ANIM_POINT = 43,
    ANIM_UNPOINT = 44,
    ANIM_FIRE_SINGLE = 45,
    ANIM_FIRE_BURST = 46,
    ANIM_FIRE_CONTINUOUS = 47,
    ANIM_FALL_BACK_SF = 48,
    ANIM_FALL_FRONT_SF = 49,
    ANIM_BAD_LANDING_SF = 50,
    ANIM_BIG_HOLE_SF = 51,
    ANIM_CHARRED_BODY_SF = 52,
    ANIM_CHUNKS_OF_FLESH_SF = 53,
    ANIM_DANCING_AUTOFIRE_SF = 54,
    ANIM_ELECTRIFY_SF = 55,
    ANIM_SLICED_IN_HALF_SF = 56,
    ANIM_BURNED_TO_NOTHING_SF = 57,
    ANIM_ELECTRIFIED_TO_NOTHING_SF = 58,
    ANIM_EXPLODED_TO_NOTHING_SF = 59,
    ANIM_MELTED_TO_NOTHING_SF = 60,
    ANIM_FIRE_DANCE_SF = 61,
    ANIM_FALL_BACK_BLOOD_SF = 62,
    ANIM_FALL_FRONT_BLOOD_SF = 63,
    ANIM_CALLED_SHOT_PIC = 64,
    ANIM_COUNT = 65,
    FIRST_KNOCKDOWN_AND_DEATH_ANIM = ANIM_FALL_BACK,
    LAST_KNOCKDOWN_AND_DEATH_ANIM = ANIM_FALL_FRONT_BLOOD,
    FIRST_SF_DEATH_ANIM = ANIM_FALL_BACK_SF,
    LAST_SF_DEATH_ANIM = ANIM_FALL_FRONT_BLOOD_SF,
} AnimationType;

typedef enum AnimationKind {
    ANIM_KIND_MOVE_TO_OBJECT = 0,
    ANIM_KIND_MOVE_TO_TILE = 1,
    ANIM_KIND_MOVE_TO_TILE_STRAIGHT = 2,
    ANIM_KIND_MOVE_TO_TILE_STRAIGHT_AND_WAIT_FOR_COMPLETE = 3,
    ANIM_KIND_ANIMATE = 4,
    ANIM_KIND_ANIMATE_REVERSED = 5,
    ANIM_KIND_ANIMATE_AND_HIDE = 6,
    ANIM_KIND_ROTATE_TO_TILE = 7,
    ANIM_KIND_ROTATE_CLOCKWISE = 8,
    ANIM_KIND_ROTATE_COUNTER_CLOCKWISE = 9,
    ANIM_KIND_HIDE = 10,
    ANIM_KIND_CALLBACK = 11,
    ANIM_KIND_CALLBACK3 = 12,
    ANIM_KIND_SET_FLAG = 14,
    ANIM_KIND_UNSET_FLAG = 15,
    ANIM_KIND_TOGGLE_FLAT = 16,
    ANIM_KIND_SET_FID = 17,
    ANIM_KIND_TAKE_OUT_WEAPON = 18,
    ANIM_KIND_SET_LIGHT_DISTANCE = 19,
    ANIM_KIND_MOVE_ON_STAIRS = 20,
    ANIM_KIND_CHECK_FALLING = 23,
    ANIM_KIND_TOGGLE_OUTLINE = 24,
    ANIM_KIND_ANIMATE_FOREVER = 25,
    ANIM_KIND_PING = 26,
    ANIM_KIND_CONTINUE = 28,
} AnimationKind;

typedef enum AnimationSequenceFlags {
    // Specifies that the animation sequence has
    // high priority, it cannot be cleared.
    ANIM_SEQ_PRIORITIZED = 0x01,

    // Specifies that the animation sequence started combat animation mode
    // and therefore should balance it with appropriate finish call.
    ANIM_SEQ_COMBAT_ANIM_STARTED = 0x02,

    // TODO: Explain what it actually means.
    // Specifies that the animation sequence is reserved.
    ANIM_SEQ_RESERVED = 0x04,

    // Specifies that the animation sequence is in the process of adding
    // actions to it (that is in the middle of begin/end calls).
    ANIM_SEQ_ACCUMULATING = 0x08,

    // TODO: Add description.
    ANIM_SEQ_0x10 = 0x10,

    // TODO: Add description.
    ANIM_SEQ_0x20 = 0x20,

    // Specifies that the animation sequence is negligible
    // and will be end immediately when a new animation
    // sequence is requested for the same object.
    ANIM_SEQ_INSIGNIFICANT = 0x40,

    // Specifies that the animation sequence should
    // not return to ANIM_STAND when it's completed.
    ANIM_SEQ_NO_STAND = 0x80,
} AnimationSequenceFlags;

typedef enum AnimationSadFlags {
    // Specifies that the animation should play from end to start.
    ANIM_SAD_REVERSE = 0x01,

    // Specifies that the animation should use straight
    // movement mode (as opposed to normal movement mode).
    ANIM_SAD_STRAIGHT = 0x02,

    // Specifies that no frame change should occur during animation.
    ANIM_SAD_NO_ANIM = 0x04,

    // Specifies that the animation should be played fully from start to finish.
    //
    // NOTE: This flag is only used together with straight movement mode to
    // implement knockdown. Without this flag when the knockdown distance is
    // short, say 1 or 2 tiles, knockdown animation might not be completed by
    // the time critter reached it's destination. With this flag set animation
    // will be played to it's final frame.
    ANIM_SAD_WAIT_FOR_COMPLETION = 0x10,

    // Unknown, set once, never read.
    ANIM_SAD_0x20 = 0x20,

    // Specifies that the owner of the animation should
    // be hidden when animation is completed.
    ANIM_SAD_HIDE_ON_END = 0x40,

    // Specifies that the animation should never end.
    ANIM_SAD_FOREVER = 0x80,
} AnimationSadFlags;

} // namespace fallout

#endif /* FALLOUT_GAME_ANIMATION_DEFS_H_ */
