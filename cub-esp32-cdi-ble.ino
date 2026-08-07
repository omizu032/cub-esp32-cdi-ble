// ==========================================
// CDI Tuning Dashboard - ESP32 Firmware (OTA & Safety)
// ==========================================

// 【新規追加】バージョン情報とビルド日時
#define FIRMWARE_VERSION "1.0.0"
#define BUILD_TIMESTAMP  "20260731021451" // フォーマット: yyyyMMddhhmmss

#define BASE_PULSE_DEG_X10 270  
#define MAP_SIZE 9              
#define IGNITION_HOLD_US 16     
#define MIN_DEAD_TIME_US 3500   

// 【安全装置】ハードウェア進角リミット (最大45.0度)
#define MAX_ADVANCE_DEG_X10 450 

#define AB_FILTER_ALPHA_SHIFT 1  
#define AB_FILTER_BETA_SHIFT  3  

// --- BLE UUIDs ---
#define SERVICE_UUID           "12345678-1234-5678-1234-56789abcdef0"
#define CHAR_TELEMETRY_UUID    "12345678-1234-5678-1234-56789abcdef1"
#define CHAR_MAP_UUID          "12345678-1234-5678-1234-56789abcdef2"
#define CHAR_CONFIG_UUID       "12345678-1234-5678-1234-56789abcdef3" // レブリミット等
#define CHAR_OTA_CTRL_UUID     "12345678-1234-5678-1234-56789abcdef4" // OTA制御
#define CHAR_OTA_DATA_UUID     "12345678-1234-5678-1234-56789abcdef5" // OTAデータ
#define CHAR_INFO_UUID         "12345678-1234-5678-1234-56789abcdef6" // 【新規追加】デバイス情報

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h> 
#include <WiFi.h> 
#include <Preferences.h> 
#include <Update.h> 

#include "driver/gptimer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

const int PIN_PULSE_IN  = 4;  
const int PIN_TRIG_OUT  = 5;
const int PIN_KILL_SW   = 6;  

// ==========================================
// グローバル変数 & 安全装置用設定
// ==========================================
volatile uint32_t last_pulse_time = 0; 
volatile uint32_t current_rpm = 0;     
volatile bool is_new_pulse = false;
volatile bool spark_fired = true;

volatile int32_t ab_interval = 15000;
volatile int32_t ab_velocity = 0;
volatile int16_t current_advance_deg_x10 = 0;
volatile bool rev_limit_active = false; 

gptimer_handle_t delay_timer = NULL;

const uint32_t rpm_axis[MAP_SIZE] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000};

volatile int16_t target_map_deg_x10[MAP_SIZE] = {
    170, 220, 270, 290, 300, 310, 320, 330, 340
};

struct __attribute__((packed)) ConfigData {
    uint16_t rev_limit_rpm;
};
ConfigData current_config = {10000}; 

struct __attribute__((packed)) TelemetryData {
    uint16_t rpm;
    int16_t advance_deg_x10;
    uint8_t status_flags; 
};

BLECharacteristic* pCharTelemetry = NULL;
volatile bool deviceConnected = false;
uint32_t connected_time = 0; 
Preferences preferences; 

bool ota_updating = false;
size_t ota_written = 0;

// ==========================================
// BLE コールバック処理
// ==========================================
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true; connected_time = millis(); 
    };
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false; BLEDevice::startAdvertising(); 
    }
};

class MapCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        uint8_t* rxData = pChar->getData();
        if (pChar->getLength() == sizeof(target_map_deg_x10)) {
            memcpy((void*)target_map_deg_x10, rxData, sizeof(target_map_deg_x10));
            preferences.begin("cdi-app", false);
            preferences.putBytes("ignitionMap", (void*)target_map_deg_x10, sizeof(target_map_deg_x10));
            preferences.end();
        } 
    }
};

class ConfigCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        uint8_t* rxData = pChar->getData();
        if (pChar->getLength() == sizeof(ConfigData)) {
            memcpy((void*)&current_config, rxData, sizeof(ConfigData));
            preferences.begin("cdi-app", false);
            preferences.putBytes("configData", (void*)&current_config, sizeof(ConfigData));
            preferences.end();
        } 
    }
};

class OtaCtrlCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        uint8_t* rxData = pChar->getData();
        size_t len = pChar->getLength();
        if(len >= 1) {
            uint8_t cmd = rxData[0];
            if(cmd == 0x01 && len == 5) {
                uint32_t firmware_size = (rxData[1] << 24) | (rxData[2] << 16) | (rxData[3] << 8) | rxData[4];
                if (Serial) Serial.printf("[OTA] Start. Size: %d\n", firmware_size);
                if (Update.begin(firmware_size, U_FLASH)) {
                    ota_updating = true;
                    ota_written = 0;
                }
            } 
            else if (cmd == 0x02) { 
                if (Serial) Serial.println("[OTA] End command received.");
                if (Update.end(true)) {
                    if (Serial) Serial.println("[OTA] Success! Restarting...");
                    delay(500);
                    ESP.restart();
                } else {
                    if (Serial) Serial.printf("[OTA] Failed: %d\n", Update.getError());
                }
            }
        }
    }
};

class OtaDataCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        if(!ota_updating) return;
        uint8_t* rxData = pChar->getData();
        size_t len = pChar->getLength();
        if (Update.write(rxData, len) != len) {
            if (Serial) Serial.printf("[OTA] Write failed: %d\n", Update.getError());
            ota_updating = false;
        } else {
            ota_written += len;
        }
    }
};

// ==========================================
// 【Core 1】 ハードウェア割り込み領域 (点火制御)
// ==========================================
static bool IRAM_ATTR on_ignition_timer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    if (digitalRead(PIN_KILL_SW) == HIGH && !rev_limit_active) {
        gpio_set_level((gpio_num_t)PIN_TRIG_OUT, 0); 
        esp_rom_delay_us(IGNITION_HOLD_US);
        gpio_set_level((gpio_num_t)PIN_TRIG_OUT, 1); 
    }
    spark_fired = true; 
    return true; 
}

void IRAM_ATTR pulsar_isr() {
    uint32_t current_micros = esp_timer_get_time();
    uint32_t intervaltime = current_micros - last_pulse_time;

    if (intervaltime < MIN_DEAD_TIME_US) return; 

    if (digitalRead(PIN_KILL_SW) == LOW) { 
        last_pulse_time = current_micros; return; 
    }

    if (!spark_fired && !rev_limit_active) {
        gpio_set_level((gpio_num_t)PIN_TRIG_OUT, 0); 
        esp_rom_delay_us(IGNITION_HOLD_US);
        gpio_set_level((gpio_num_t)PIN_TRIG_OUT, 1);
    }
    
    last_pulse_time = current_micros;
    spark_fired = false; 

    int32_t measurement = (int32_t)intervaltime;
    int32_t error = measurement - ab_interval;
    if (error < 0) error = -error; 

    if (error > (ab_interval >> 1)) {
        ab_interval = measurement; ab_velocity = 0;
    } else {
        int32_t predicted_measurement = ab_interval + ab_velocity;
        int32_t residual = measurement - predicted_measurement;
        ab_interval = predicted_measurement + (residual >> AB_FILTER_ALPHA_SHIFT);
        ab_velocity = ab_velocity + (residual >> AB_FILTER_BETA_SHIFT);
    }

    int32_t predicted_interval = ab_interval + ab_velocity;
    if (predicted_interval < 4000) predicted_interval = 4000;
    if (predicted_interval > 120000) predicted_interval = 120000;

    current_rpm = 60000000 / ab_interval;
    is_new_pulse = true;

    if (current_rpm >= current_config.rev_limit_rpm) {
        rev_limit_active = true; return; 
    } else {
        rev_limit_active = false;
    }

    int32_t fdeg_x10 = target_map_deg_x10[0];
    if (current_rpm <= rpm_axis[0]) {
        fdeg_x10 = target_map_deg_x10[0];
    } else if (current_rpm >= rpm_axis[MAP_SIZE - 1]) {
        fdeg_x10 = target_map_deg_x10[MAP_SIZE - 1];
    } else {
        for (int i = 0; i < MAP_SIZE - 1; i++) {
            if (current_rpm >= rpm_axis[i] && current_rpm < rpm_axis[i+1]) {
                int32_t span = (int32_t)(rpm_axis[i+1] - rpm_axis[i]); 
                int32_t rpm_diff = (int32_t)(current_rpm - rpm_axis[i]);
                int32_t factor_x16 = (rpm_diff << 16) / span;
                int32_t delta_map = target_map_deg_x10[i+1] - target_map_deg_x10[i];
                fdeg_x10 = target_map_deg_x10[i] + ((delta_map * factor_x16) >> 16);
                break;
            }
        }
    }

    if (fdeg_x10 > MAX_ADVANCE_DEG_X10) {
        fdeg_x10 = MAX_ADVANCE_DEG_X10;
    }

    current_advance_deg_x10 = (int16_t)fdeg_x10;

    int32_t final_delay_us = 0;
    if (fdeg_x10 <= BASE_PULSE_DEG_X10) {
        int32_t delay_deg_x10 = BASE_PULSE_DEG_X10 - fdeg_x10;
        final_delay_us = (int32_t)(((int64_t)delay_deg_x10 * (int64_t)ab_interval) / 3600LL);
    } else {
        int32_t advance_deg_x10 = fdeg_x10 - BASE_PULSE_DEG_X10;
        int32_t advance_us = (int32_t)(((int64_t)advance_deg_x10 * (int64_t)predicted_interval) / 3600LL);
        final_delay_us = predicted_interval - advance_us;
        if (final_delay_us < 10) final_delay_us = 10; 
    }

    gptimer_set_raw_count(delay_timer, 0); 
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = (final_delay_us > 0) ? final_delay_us : 1,
        .reload_count = 0,
        .flags = { .auto_reload_on_alarm = false }
    };
    gptimer_set_alarm_action(delay_timer, &alarm_config); 
}

// ==========================================
// 【Core 0】 バックグラウンドタスク (BLE通信)
// ==========================================
void background_system_task(void *pvParameters) {
    BLEDevice::init("CDI-ESP32-S3");
    BLEDevice::setMTU(512);

    BLEServer* pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // テレメトリ
    pCharTelemetry = pService->createCharacteristic(CHAR_TELEMETRY_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pCharTelemetry->addDescriptor(new BLE2902());
    
    // マップ設定
    BLECharacteristic* pCharMap = pService->createCharacteristic(CHAR_MAP_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCharMap->setCallbacks(new MapCallbacks());
    pCharMap->setValue((uint8_t*)target_map_deg_x10, sizeof(target_map_deg_x10));

    // コンフィグ
    BLECharacteristic* pCharConfig = pService->createCharacteristic(CHAR_CONFIG_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCharConfig->setCallbacks(new ConfigCallbacks());
    pCharConfig->setValue((uint8_t*)&current_config, sizeof(ConfigData));

    // OTA制御用
    BLECharacteristic* pCharOtaCtrl = pService->createCharacteristic(CHAR_OTA_CTRL_UUID, BLECharacteristic::PROPERTY_WRITE);
    pCharOtaCtrl->setCallbacks(new OtaCtrlCallbacks());

    // OTAデータ用
    BLECharacteristic* pCharOtaData = pService->createCharacteristic(CHAR_OTA_DATA_UUID, BLECharacteristic::PROPERTY_WRITE_NR);
    pCharOtaData->setCallbacks(new OtaDataCallbacks());

    // 【新規追加】デバイス情報 (バージョン、ビルド日時)
    BLECharacteristic* pCharInfo = pService->createCharacteristic(CHAR_INFO_UUID, BLECharacteristic::PROPERTY_READ);
    char infoStr[32];
    snprintf(infoStr, sizeof(infoStr), "%s,%s", FIRMWARE_VERSION, BUILD_TIMESTAMP);
    pCharInfo->setValue((uint8_t*)infoStr, strlen(infoStr));

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    uint32_t filtered_rpm = 0;
    TelemetryData tData = {0, 0, 0}; 

    for (;;) {
        if (ota_updating) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (is_new_pulse) {
            is_new_pulse = false;
            filtered_rpm = (filtered_rpm * 3 + current_rpm) >> 2; 
        } else if (esp_timer_get_time() - last_pulse_time > 500000) {
            filtered_rpm = 0;
            current_advance_deg_x10 = 0;
            rev_limit_active = false;
        }

        bool is_run = (digitalRead(PIN_KILL_SW) == HIGH);

        if (deviceConnected) {
            tData.rpm = (uint16_t)filtered_rpm;
            tData.advance_deg_x10 = current_advance_deg_x10; 
            
            tData.status_flags = 0;
            if (is_run) tData.status_flags |= 0x01;
            if (rev_limit_active) tData.status_flags |= 0x02;

            pCharTelemetry->setValue((uint8_t*)&tData, sizeof(TelemetryData));
            if (millis() - connected_time > 2000) pCharTelemetry->notify(); 
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// ==========================================
// セットアップ & メインループ
// ==========================================
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_OFF); 
    
    preferences.begin("cdi-app", true); 
    preferences.getBytes("ignitionMap", (void*)target_map_deg_x10, sizeof(target_map_deg_x10));
    preferences.getBytes("configData", (void*)&current_config, sizeof(ConfigData));
    preferences.end();
    
    pinMode(PIN_PULSE_IN, INPUT_PULLUP);
    pinMode(PIN_KILL_SW, INPUT_PULLUP);
    gpio_reset_pin((gpio_num_t)PIN_TRIG_OUT);
    pinMode(PIN_TRIG_OUT, OUTPUT);
    digitalWrite(PIN_TRIG_OUT, HIGH); 

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,        
        .resolution_hz = 1000000, 
    };
    gptimer_new_timer(&timer_config, &delay_timer);
    gptimer_event_callbacks_t cbs = {.on_alarm = on_ignition_timer_cb};
    gptimer_register_event_callbacks(delay_timer, &cbs, NULL);
    gptimer_enable(delay_timer);
    gptimer_start(delay_timer); 

    attachInterrupt(digitalPinToInterrupt(PIN_PULSE_IN), pulsar_isr, FALLING);

    xTaskCreatePinnedToCore(background_system_task, "SystemTask_Core0", 8192, NULL, 1, NULL, 0);
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}
