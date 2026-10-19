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

/* The ElevenLabs request: URL and body, with no ESP-IDF in them (tests/test_muse_elevenlabs.py). */
#include "muse_elevenlabs.h"

#include <ctype.h>
#include <stdio.h>

#include "cJSON.h"

bool muse_el_url(char *out, size_t cap, const char *voice_id)
{
    if (!voice_id || !voice_id[0]) {
        return false;
    }
    /* The id goes in the path: allow only what ElevenLabs ids are made of. */
    for (const char *p = voice_id; *p; p++) {
        if (!isalnum((unsigned char)*p)) {
            return false;
        }
    }
    int n = snprintf(out, cap, MUSE_EL_HOST "/v1/text-to-speech/%s/stream?output_format=" MUSE_EL_FORMAT, voice_id);
    return n > 0 && (size_t)n < cap;
}

char *muse_el_body(const char *text, const char *model)
{
    if (!text || !text[0]) {
        return NULL;
    }
    cJSON *root = cJSON_CreateObject();
    if (!root || !cJSON_AddStringToObject(root, "text", text) ||
        (model && model[0] && !cJSON_AddStringToObject(root, "model_id", model))) {
        cJSON_Delete(root);
        return NULL;
    }
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return body;
}
