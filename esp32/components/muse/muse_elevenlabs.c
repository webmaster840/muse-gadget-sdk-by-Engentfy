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
 * ElevenLabs text to speech. Each request runs on a task of its own, so the
 * chat session's task never blocks on a second TLS connection: the MP3 goes
 * into a stream buffer that the session drains into its reply speech.
 */
#include "muse_elevenlabs.h"

#include <string.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

static const char *TAG = "muse_elevenlabs";

#define MP3_STREAM (32 * 1024)   /* ~8 s at 32 kbps; the session drains it every few ms */
#define URL_MAX 160
#define READ_CHUNK 2048

/* api.elevenlabs.io's chain ends at GTS Root R1 cross-signed by GlobalSign
 * Root CA, which IDF's bundle no longer carries, so the bundle rejects it.
 * ponytail: pinned to Google's roots (valid to 2036); if ElevenLabs changes
 * CA, replies fall back to text until elevenlabs_roots.pem is updated. */
extern const char roots_pem_start[] asm("_binary_elevenlabs_roots_pem_start");

static StreamBufferHandle_t s_mp3;
static volatile muse_el_state_t s_state = MUSE_EL_IDLE;
static volatile bool s_cancel;
/* ponytail: one request at a time; a reply arriving while the last one winds
 * down is shown as text. Queue requests if that turns out to matter. */
static volatile bool s_busy;

bool muse_el_configured(void)
{
    return CONFIG_MUSE_ELEVENLABS_API_KEY[0] && CONFIG_MUSE_ELEVENLABS_VOICE_ID[0];
}

/* Sends to the stream buffer, waiting while the session catches up. False if cancelled. */
static bool push(const uint8_t *data, size_t len)
{
    while (len) {
        if (s_cancel) {
            return false;
        }
        size_t n = xStreamBufferSend(s_mp3, data, len, pdMS_TO_TICKS(50));
        data += n;
        len -= n;
    }
    return true;
}

static bool fetch(const char *body)
{
    char url[URL_MAX];
    if (!muse_el_url(url, sizeof(url), CONFIG_MUSE_ELEVENLABS_VOICE_ID)) {
        ESP_LOGE(TAG, "bad voice ID");
        return false;
    }
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .buffer_size = 4096,
        .buffer_size_tx = 2048,
        .cert_pem = roots_pem_start,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        return false;
    }
    esp_http_client_set_header(c, "xi-api-key", CONFIG_MUSE_ELEVENLABS_API_KEY);
    esp_http_client_set_header(c, "Content-Type", "application/json");
    esp_http_client_set_header(c, "Accept", "audio/mpeg");
    bool ok = false;
    int len = strlen(body);
    if (esp_http_client_open(c, len) != ESP_OK || esp_http_client_write(c, body, len) != len) {
        ESP_LOGW(TAG, "request failed");
        goto out;
    }
    esp_http_client_fetch_headers(c);
    int status = esp_http_client_get_status_code(c);
    if (status != 200) {
        char err[160] = "";
        int n = esp_http_client_read(c, err, sizeof(err) - 1);
        err[n > 0 ? n : 0] = '\0';
        ESP_LOGW(TAG, "HTTP %d: %s", status, err);   /* 401: check the key; 404: the voice ID */
        goto out;
    }
    static uint8_t buf[READ_CHUNK];
    for (;;) {
        int n = esp_http_client_read(c, (char *)buf, sizeof(buf));
        if (n < 0) {
            ESP_LOGW(TAG, "read failed");
            goto out;
        }
        if (n == 0) {
            ok = esp_http_client_is_complete_data_received(c);
            break;
        }
        if (!push(buf, n)) {
            goto out;
        }
    }
out:
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return ok;
}

static void tts_task(void *arg)
{
    char *body = arg;
    bool ok = fetch(body);
    cJSON_free(body);
    if (s_cancel) {
        xStreamBufferReset(s_mp3);
        s_state = MUSE_EL_IDLE;
    } else {
        s_state = ok ? MUSE_EL_DONE : MUSE_EL_FAILED;
    }
    s_busy = false;
    vTaskDeleteWithCaps(NULL);
}

bool muse_el_start(const char *text)
{
    if (!muse_el_configured() || s_busy) {
        return false;
    }
    if (!s_mp3) {
        s_mp3 = xStreamBufferCreateWithCaps(MP3_STREAM, 1, MALLOC_CAP_SPIRAM);
        if (!s_mp3) {
            return false;
        }
    }
    char *body = muse_el_body(text, CONFIG_MUSE_ELEVENLABS_MODEL);
    if (!body) {
        return false;
    }
    xStreamBufferReset(s_mp3);
    s_cancel = false;
    s_busy = true;
    s_state = MUSE_EL_RUNNING;
    /* Stack in PSRAM like the chat session's: TLS runs here too. */
    if (xTaskCreatePinnedToCoreWithCaps(tts_task, "muse_tts", 16 * 1024, body, 4, NULL, 0,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        cJSON_free(body);
        s_busy = false;
        s_state = MUSE_EL_FAILED;
        return false;
    }
    ESP_LOGI(TAG, "speaking %u chars", (unsigned)strlen(text));
    return true;
}

size_t muse_el_read(uint8_t *buf, size_t cap)
{
    return s_mp3 && cap ? xStreamBufferReceive(s_mp3, buf, cap, 0) : 0;
}

size_t muse_el_pending(void)
{
    return s_mp3 ? xStreamBufferBytesAvailable(s_mp3) : 0;
}

muse_el_state_t muse_el_state(void)
{
    return s_state;
}

void muse_el_cancel(void)
{
    if (s_busy) {
        s_cancel = true;   /* the task resets the buffer on its way out */
    } else {
        if (s_mp3) {
            xStreamBufferReset(s_mp3);
        }
        s_state = MUSE_EL_IDLE;
    }
}
