// remote_receiver.h — Recibe los botones del control remoto (ESP8266 D1 mini) por ESP-NOW
// Copiar en: main/boards/bread-compact-wifi/remote_receiver.h
//
// Mensaje del control: "XZRC" + comando (1 byte) + número de secuencia (1 byte)
//   1 = HABLAR presionado   -> empieza a escuchar
//   2 = HABLAR soltado      -> deja de escuchar
//   3 = VOLUMEN +           -> +10
//   4 = VOLUMEN -           -> -10
//   9 = PING                -> solo para que el control encuentre el canal WiFi
#ifndef __REMOTE_RECEIVER_H__
#define __REMOTE_RECEIVER_H__

#include "application.h"
#include "board.h"
#include "display/display.h"

#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <string>

class RemoteReceiver {
private:
    static constexpr const char* TAG = "RemoteRx";

    enum : uint8_t { kTalkDown = 1, kTalkUp = 2, kVolUp = 3, kVolDown = 4, kPing = 9 };

    struct __attribute__((packed)) Msg {
        char magic[4];   // "XZRC"
        uint8_t cmd;
        uint8_t seq;
    };

    static inline int last_seq_ = -1;

    static void ChangeVolume(int delta) {
        auto codec = Board::GetInstance().GetAudioCodec();
        int v = codec->output_volume() + delta;
        if (v > 100) v = 100;
        if (v < 0) v = 0;
        codec->SetOutputVolume(v);
        auto d = Board::GetInstance().GetDisplay();
        if (d) d->ShowNotification("Volumen " + std::to_string(v), 1500);
    }

    // Se ejecuta en la tarea de WiFi: solo copiamos el comando y lo pasamos a la aplicación
    static void OnRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
        if (len < (int)sizeof(Msg)) return;
        Msg m;
        memcpy(&m, data, sizeof(Msg));
        if (memcmp(m.magic, "XZRC", 4) != 0) return;
        if (m.cmd == kPing) return;
        if (m.seq == last_seq_) return;          // repetido (reintento del control)
        last_seq_ = m.seq;

        uint8_t cmd = m.cmd;
        Application::GetInstance().Schedule([cmd]() {
            auto& app = Application::GetInstance();
            switch (cmd) {
                case kTalkDown: app.StartListening(); break;
                case kTalkUp:   app.StopListening();  break;
                case kVolUp:    ChangeVolume(+10);    break;
                case kVolDown:  ChangeVolume(-10);    break;
                default: break;
            }
        });
    }

    // Espera a que el WiFi esté conectado al router y recién entonces inicia ESP-NOW
    static void InitTask(void*) {
        while (true) {
            wifi_ap_record_t ap;
            if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
                if (esp_now_init() == ESP_OK) {
                    esp_now_register_recv_cb(&RemoteReceiver::OnRecv);
                    uint8_t mac[6];
                    esp_wifi_get_mac(WIFI_IF_STA, mac);
                    ESP_LOGI(TAG, "ESP-NOW listo. Canal %d, MAC %02X:%02X:%02X:%02X:%02X:%02X",
                             ap.primary, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                    vTaskDelete(nullptr);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

public:
    static void Start() {
        xTaskCreate(&RemoteReceiver::InitTask, "remote_rx", 3072, nullptr, 3, nullptr);
    }
};

#endif  // __REMOTE_RECEIVER_H__
