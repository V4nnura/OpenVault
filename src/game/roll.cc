#include "game/roll.h"

#include <limits.h>
#include <stdlib.h>

#include <random>

#include "game/scripts.h"
#include "platform_compat.h"
#include "plib/gnw/debug.h"

namespace fallout {

static int ran1(int max);
static void init_random();
static int random_seed();
static void seed_generator(int seed);
static unsigned int timer_read();
static void check_chi_squared();

// 0x507834
static int iy = 0;

// 0x662F50
static int iv[32];

// 0x662FD0
static int idum;

// 0x4913F0
void roll_init()
{
    // NOTE: Uninline.
    init_random();

    check_chi_squared();
}

// 0x49140C
int roll_reset()
{
    return 0;
}

// 0x49140C
int roll_exit()
{
    return 0;
}

// 0x49140C
int roll_save(DB_FILE* stream)
{
    return 0;
}

// 0x49140C
int roll_load(DB_FILE* stream)
{
    return 0;
}

// Rolls d% against [difficulty].
//
// 0x491410
int roll_check(int difficulty, int criticalSuccessModifier, int* howMuchPtr)
{
    int delta = difficulty - roll_random(1, 100);
    int result = roll_check_critical(delta, criticalSuccessModifier);

    if (howMuchPtr != NULL) {
        *howMuchPtr = delta;
    }

    return result;
}

// Translates raw d% result into [Roll] constants, possibly upgrading to
// criticals (starting from day 2).
//
// 0x491440
int roll_check_critical(int delta, int criticalSuccessModifier)
{
    unsigned int gameTime = game_time();

    int roll;
    if (delta < 0) {
        roll = ROLL_FAILURE;

        if ((gameTime / GAME_TIME_TICKS_PER_DAY) >= 1) {
            // 10% to become critical failure.
            if (roll_random(1, 100) <= -delta / 10) {
                roll = ROLL_CRITICAL_FAILURE;
            }
        }
    } else {
        roll = ROLL_SUCCESS;

        if ((gameTime / GAME_TIME_TICKS_PER_DAY) >= 1) {
            // 10% + modifier to become critical success.
            if (roll_random(1, 100) <= delta / 10 + criticalSuccessModifier) {
                roll = ROLL_CRITICAL_SUCCESS;
            }
        }
    }

    return roll;
}

// 0x4914D0
int roll_random(int min, int max)
{
    int result;

    if (min <= max) {
        result = min + ran1(max - min + 1);
    } else {
        result = max + ran1(min - max + 1);
    }

    if (result < min || result > max) {
        debug_printf("Random number %d is not in range %d to %d", result, min, max);
        result = min;
    }

    return result;
}

// 0x49150C
static int ran1(int max)
{
    int seed = 16807 * (idum % 127773) - 2836 * (idum / 127773);

    if (seed < 0) {
        seed += 0x7FFFFFFF;
    }

    if (seed < 0) {
        seed += 0x7FFFFFFF;
    }

    int idx = iy & 0x1F;
    int value = iv[idx];
    iv[idx] = seed;
    iy = value;
    idum = seed;

    return value % max;
}

// 0x491584
static void init_random()
{
    std::srand(timer_read());
    seed_generator(random_seed());
}

// 0x4915B0
void roll_set_seed(int seed)
{
    if (seed == -1) {
        // NOTE: Uninline.
        seed = random_seed();
    }

    seed_generator(seed);
}

// 0x4915D4
static int random_seed()
{
    return std::rand();
}

// 0x4915F0
static void seed_generator(int seed)
{
    int num = seed;
    if (num < 1) {
        num = 1;
    }

    for (int index = 39; index >= 0; index--) {
        num = 16807 * (num % 127773) - 2836 * (num / 127773);

        if (num < 0) {
            num += INT_MAX;
        }

        if (index < 32) {
            iv[index] = num;
        }
    }

    iy = iv[0];
    idum = num;
}

// Provides seed for random number generator.
//
// 0x491668
static unsigned int timer_read()
{
    return compat_timeGetTime();
}

// 0x491674
static void check_chi_squared()
{
    int results[25];

    for (int index = 0; index < 25; index++) {
        results[index] = 0;
    }

    for (int attempt = 0; attempt < 100000; attempt++) {
        int value = roll_random(1, 25);
        if (value - 1 < 0) {
            debug_printf("I made a negative number %d\n", value - 1);
        }

        results[value - 1]++;
    }

    double chiSquared = 0.0;

    for (int index = 0; index < 25; index++) {
        double contrib = ((double)results[index] - 4000.0) * ((double)results[index] - 4000.0) / 4000.0;
        chiSquared += contrib;
    }

    debug_printf("Chi squared is %f, P = %f at 0.05\n", chiSquared, 4000.0);

    if (chiSquared < 36.42) {
        debug_printf("Sequence is random, 95%% confidence.\n");
    } else {
        debug_printf("Warning! Sequence is not random, 95%% confidence.\n");
    }
}

} // namespace fallout
