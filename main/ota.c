#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "string.h"
#include <sys/socket.h>

#include "ota.h"

#define HASH_LEN 32

static const char *TAG = "[OTA]";

#define OTA_URL_SIZE 256

SemaphoreHandle_t xOTAUpdateTaskFinished;

static void print_sha256(const uint8_t *image_hash, const char *label);

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
        print_sha256(running_app_info.app_elf_sha256, "Running firmware hash");
    }

    print_sha256(new_app_info->app_elf_sha256, "OTA firmware hash");

#ifdef CONFIG_OTA_UPDATE_IF_HASH_CHANGED
    bool hash_changed = memcmp(new_app_info->app_elf_sha256, running_app_info.app_elf_sha256, sizeof(new_app_info->app_elf_sha256)) != 0;
#else 
    bool hash_changed = false;
#endif

#ifndef CONFIG_OTA_SKIP_VERSION_CHECK
    ESP_LOGI(TAG, "OTA firmware version: %s", new_app_info->version);

    bool version_changed = memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) != 0;
    if (!version_changed && !hash_changed) {
        ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
        return ESP_ERR_INVALID_VERSION;
    }
#else
    if (!hash_changed) {
        ESP_LOGW(TAG, "Current running firmware hash is the same as the new. We will not continue the update.");
        return ESP_ERR_INVALID_VERSION;
    }
#endif

#ifdef CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
    /**
     * Secure version check from firmware image header prevents subsequent download and flash write of
     * entire firmware image. However this is optional because it is also taken care in API
     * esp_https_ota_finish at the end of OTA update procedure.
     */
    const uint32_t hw_sec_version = esp_efuse_read_secure_version();
    if (new_app_info->secure_version < hw_sec_version) {
        ESP_LOGW(TAG, "New firmware security version is less than eFuse programmed, %"PRIu32" < %"PRIu32, new_app_info->secure_version, hw_sec_version);
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

static esp_err_t _http_client_init_cb(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_OK;
    /* Uncomment to add custom headers to HTTP request */
    // err = esp_http_client_set_header(http_client, "Custom-Header", "Value");
    return err;
}

void ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA task");

    esp_err_t ota_finish_err = ESP_OK;

    esp_http_client_config_t config = {
        .url = CONFIG_OTA_FIRMWARE_UPGRADE_URL,
        .timeout_ms = CONFIG_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .http_client_init_cb = _http_client_init_cb, // Register a callback to be invoked after esp_http_client is initialized
#ifdef CONFIG_OTA_ENABLE_PARTIAL_HTTP_DOWNLOAD
        .partial_http_download = true,
        .max_http_request_size = CONFIG_OTA_HTTP_REQUEST_SIZE,
#endif
    };

    ESP_LOGI(TAG, "Attempting to download update from %s", config.url);

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        goto ota_end_task;
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
        goto ota_end;
    }

    err = validate_image_header(&app_desc);
    if (err == ESP_ERR_INVALID_VERSION) {
        goto ota_end_task;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "image header verification failed");
        goto ota_end;
    }
    ESP_LOGI(TAG, "OTA update image header passed verification, the update will be installed");

    ESP_LOGI(TAG, "Waiting for OTA update download to finish...");
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        // ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
    }

    if (err != ESP_OK) {
        esp_https_ota_abort(https_ota_handle);
        goto ota_end;
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
    } else {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            goto ota_end_task;
        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");
ota_end_task:
    xSemaphoreGive(xOTAUpdateTaskFinished);
    vTaskDelete(NULL);
}

static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s %s", label, hash_print);
}

void ota_check_for_update(void)
{
#ifdef CONFIG_OTA_ALLOW_HTTP
    ESP_LOGW(TAG, "WARNING: CONFIG_OTA_ALLOW_HTTP is enabled, this is NOT SECURE, please use this carefully.\n");
#endif

    xOTAUpdateTaskFinished = xSemaphoreCreateBinary();
    if (xOTAUpdateTaskFinished == NULL) {
        ESP_LOGE(TAG, "Failed to create ota task semaphone.");
        return;
    }

    // Start update task
    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
    
    // Wait up to 60 seconds for ota update to finish
    ESP_LOGI(TAG, "Waiting for OTA Update task to finish...");
    if( xSemaphoreTake( xOTAUpdateTaskFinished, 60000 / portTICK_PERIOD_MS ) != pdTRUE ) {
        ESP_LOGE(TAG, "Timed out waiting for OTA Update task.");
    }
}