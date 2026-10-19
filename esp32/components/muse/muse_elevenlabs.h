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

/*
 * ElevenLabs text to speech (CONFIG_MUSE_ELEVENLABS_TTS): one request at a
 * time, fetched on its own task. The chat session reads the MP3 as it arrives
 * and plays it like any reply speech.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { MUSE_EL_IDLE, MUSE_EL_RUNNING, MUSE_EL_DONE, MUSE_EL_FAILED } muse_el_state_t;

/* True when a key and a voice are configured. */
bool muse_el_configured(void);

/* Starts speaking `text` (copied). False if unconfigured, busy or out of memory. */
bool muse_el_start(const char *text);

/* Copies up to `cap` bytes of MP3 that have arrived; never blocks. */
size_t muse_el_read(uint8_t *buf, size_t cap);

/* Bytes arrived but not read yet. */
size_t muse_el_pending(void);

muse_el_state_t muse_el_state(void);

/* Stops the request in progress; its leftover audio is dropped. */
void muse_el_cancel(void);

/* Request pieces, kept free of ESP-IDF so the host tests can build them. */
#define MUSE_EL_HOST "https://api.elevenlabs.io"
#define MUSE_EL_FORMAT "mp3_22050_32"

/* The stream URL for `voice_id`. False if it doesn't fit or the id isn't [A-Za-z0-9]. */
bool muse_el_url(char *out, size_t cap, const char *voice_id);

/* The JSON body (free it with cJSON_free), or NULL. */
char *muse_el_body(const char *text, const char *model);

#ifdef __cplusplus
}
#endif
