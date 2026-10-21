/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Host harness for avatar.react: when a reaction Muse asked for shows, and
// when it's dropped. The runner extracts muse_state.c's reactions into
// reactions.inc.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "muse_state.h"

#include "reactions.inc"

#define S(x) ((int64_t)((x) * 1000000.0))

static muse_reaction_t step(reaction_t *r, double secs, muse_mode_t mode, bool *sleep) {
    return reaction_step(r, S(secs), mode, sleep);
}

int main(void) {
    bool sleep;

    assert(muse_reaction_from_name("happy") == MUSE_REACTION_HAPPY);
    assert(muse_reaction_from_name("thinking") == MUSE_REACTION_THINKING);
    assert(muse_reaction_from_name("shrug") == MUSE_REACTION_SHRUG);
    assert(muse_reaction_from_name("goodnight") == MUSE_REACTION_GOODNIGHT);
    assert(muse_reaction_from_name("dance") == MUSE_REACTION_NONE);
    assert(muse_reaction_from_name("") == MUSE_REACTION_NONE);
    assert(muse_reaction_from_name(NULL) == MUSE_REACTION_NONE);

    // Asked while the reply is spoken: it waits, then shows for a moment once idle.
    reaction_t r = { .asked = MUSE_REACTION_HAPPY, .asked_us = S(10) };
    assert(step(&r, 11, MUSE_MODE_SPEAKING, &sleep) == MUSE_REACTION_NONE);
    assert(step(&r, 40, MUSE_MODE_SPEAKING, &sleep) == MUSE_REACTION_NONE);
    assert(step(&r, 41, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_HAPPY && !sleep);
    assert(step(&r, 42.5, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_HAPPY);
    assert(step(&r, 42.7, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_NONE && !sleep);
    assert(step(&r, 50, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_NONE);   // once only

    // A new turn drops a reaction that hasn't shown yet.
    r = (reaction_t){ .asked = MUSE_REACTION_SHRUG, .asked_us = S(100) };
    assert(step(&r, 101, MUSE_MODE_LISTENING, &sleep) == MUSE_REACTION_NONE);
    assert(step(&r, 110, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_NONE);

    // Never idle for two minutes: stale, dropped.
    r = (reaction_t){ .asked = MUSE_REACTION_THINKING, .asked_us = S(200) };
    assert(step(&r, 319, MUSE_MODE_THINKING, &sleep) == MUSE_REACTION_NONE);
    assert(step(&r, 321, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_NONE);

    // Goodnight shows for the goodbye, then sleeps the screen, once.
    r = (reaction_t){ .asked = MUSE_REACTION_GOODNIGHT, .asked_us = S(400) };
    assert(step(&r, 401, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_GOODNIGHT && !sleep);
    assert(step(&r, 406.4, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_GOODNIGHT && !sleep);
    assert(step(&r, 406.5, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_GOODNIGHT && sleep);   // asleep to the end
    assert(step(&r, 407, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_NONE && !sleep);

    // Talking over it cancels it, and the screen stays on.
    r = (reaction_t){ .asked = MUSE_REACTION_GOODNIGHT, .asked_us = S(500) };
    assert(step(&r, 501, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_GOODNIGHT);
    assert(step(&r, 503, MUSE_MODE_LISTENING, &sleep) == MUSE_REACTION_NONE && !sleep);
    assert(step(&r, 510, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_NONE && !sleep);

    // A second reaction while one shows takes over and starts afresh.
    r = (reaction_t){ .asked = MUSE_REACTION_HAPPY, .asked_us = S(600) };
    assert(step(&r, 600, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_HAPPY);
    r.asked = MUSE_REACTION_SHRUG;
    r.asked_us = S(601);
    assert(step(&r, 601, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_SHRUG);
    assert(step(&r, 602.5, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_SHRUG);
    assert(step(&r, 602.7, MUSE_MODE_IDLE, &sleep) == MUSE_REACTION_NONE);
    return 0;
}
