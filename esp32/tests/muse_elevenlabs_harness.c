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

/* Checks the ElevenLabs request pieces (components/muse/muse_elevenlabs_req.c). */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "muse_elevenlabs.h"

int main(void)
{
    char url[160];
    assert(muse_el_url(url, sizeof(url), "21m00Tcm4TlvDq8ikWAM"));
    assert(strcmp(url, "https://api.elevenlabs.io/v1/text-to-speech/21m00Tcm4TlvDq8ikWAM/stream"
                       "?output_format=mp3_22050_32") == 0);
    /* Nothing that could change the path or query. */
    assert(!muse_el_url(url, sizeof(url), ""));
    assert(!muse_el_url(url, sizeof(url), NULL));
    assert(!muse_el_url(url, sizeof(url), "abc/../x"));
    assert(!muse_el_url(url, sizeof(url), "abc?x=1"));
    assert(!muse_el_url(url, 20, "21m00Tcm4TlvDq8ikWAM"));

    /* Quotes, newlines and accents survive as JSON. */
    char *body = muse_el_body("¡Hola! Dijo \"miau\"\nadiós", "eleven_flash_v2_5");
    assert(body);
    cJSON *root = cJSON_Parse(body);
    assert(root);
    assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(root, "text")), "¡Hola! Dijo \"miau\"\nadiós") == 0);
    assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(root, "model_id")), "eleven_flash_v2_5") == 0);
    cJSON_Delete(root);
    cJSON_free(body);

    body = muse_el_body("hola", "");
    root = cJSON_Parse(body);
    assert(root && !cJSON_GetObjectItem(root, "model_id"));
    cJSON_Delete(root);
    cJSON_free(body);

    assert(!muse_el_body("", "m"));
    assert(!muse_el_body(NULL, "m"));
    puts("ok");
    return 0;
}
