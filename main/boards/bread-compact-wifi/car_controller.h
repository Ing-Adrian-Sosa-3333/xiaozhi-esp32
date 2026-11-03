// car_controller.h — Control de 2 motores DC con L298N como herramientas MCP de XiaoZhi
// v3: ojos animados en la OLED (usa eyes_display.h) para cada movimiento
// Copiar en: main/boards/bread-compact-wifi/car_controller.h
#ifndef __CAR_CONTROLLER_H__
#define __CAR_CONTROLLER_H__

#include "mcp_server.h"
#include "board.h"
#include "display/display.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <string>

class CarController {
private:
    // Motor A = izquierdo (IN1, IN2, ENA) | Motor B = derecho (IN3, IN4, ENB)
    gpio_num_t in1_, in2_, in3_, in4_, ena_, enb_;
    esp_timer_handle_t stop_timer_ = nullptr;   // detiene el carro al terminar el tiempo
    esp_timer_handle_t face_timer_ = nullptr;   // suelta la mirada después de "pare"
    std::string state_ = "stopped";
    int speed_ = 0;

    static constexpr ledc_timer_t   kTimer = LEDC_TIMER_1;
    static constexpr ledc_channel_t kChA   = LEDC_CHANNEL_2;
    static constexpr ledc_channel_t kChB   = LEDC_CHANNEL_3;
    static constexpr int kMaxDuty = 1023;               // 10 bits

    // Mirada de los ojos para cada movimiento: ver la tabla "movimientos del carro"
    // en eyes_display.h (forward, backward, left, right, stop).
    struct Gesture { const char* eyes; const char* text; };
    static constexpr Gesture kForward  = {"car_forward",  "Avanzando"};
    static constexpr Gesture kBackward = {"car_backward", "Retrocediendo"};
    static constexpr Gesture kLeft     = {"car_left",     "Girando izq."};
    static constexpr Gesture kRight    = {"car_right",    "Girando der."};

    void SetupPwm() {
        ledc_timer_config_t t = {};
        t.speed_mode = LEDC_LOW_SPEED_MODE;
        t.duty_resolution = LEDC_TIMER_10_BIT;
        t.timer_num = kTimer;
        t.freq_hz = 1000;
        t.clk_cfg = LEDC_AUTO_CLK;
        ESP_ERROR_CHECK(ledc_timer_config(&t));

        ledc_channel_config_t c = {};
        c.speed_mode = LEDC_LOW_SPEED_MODE;
        c.timer_sel = kTimer;
        c.duty = 0;
        c.hpoint = 0;
        c.intr_type = LEDC_INTR_DISABLE;

        c.gpio_num = ena_; c.channel = kChA;
        ESP_ERROR_CHECK(ledc_channel_config(&c));
        c.gpio_num = enb_; c.channel = kChB;
        ESP_ERROR_CHECK(ledc_channel_config(&c));
    }

    void SetDuty(ledc_channel_t ch, int speed_pct) {
        int duty = speed_pct * kMaxDuty / 100;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
    }

    // dir: 1 adelante, -1 atrás, 0 parado
    void MotorA(int dir) { gpio_set_level(in1_, dir > 0); gpio_set_level(in2_, dir < 0); }
    void MotorB(int dir) { gpio_set_level(in3_, dir > 0); gpio_set_level(in4_, dir < 0); }

    Display* GetDisplay() { return Board::GetInstance().GetDisplay(); }

    void ShowEmotion(const char* name) {
        auto d = GetDisplay();
        if (d) d->SetEmotion(name);
    }

    void Notify(const char* msg) {
        auto d = GetDisplay();
        if (d) d->ShowNotification(msg, 1500);
    }

    // --- callbacks de los timers ---
    static void StopTimerCb(void* arg) {
        auto self = static_cast<CarController*>(arg);
        self->StopMotors();
        self->EndGesture(false);
    }

    static void FaceTimerCb(void* arg) {
        static_cast<CarController*>(arg)->ShowEmotion("car_idle");
    }

    esp_timer_handle_t MakeTimer(esp_timer_cb_t cb, const char* name) {
        esp_timer_handle_t h = nullptr;
        esp_timer_create_args_t a = {};
        a.callback = cb;
        a.arg = this;
        a.dispatch_method = ESP_TIMER_TASK;
        a.name = name;
        ESP_ERROR_CHECK(esp_timer_create(&a, &h));
        return h;
    }

    void StartGesture(const Gesture& g) {
        esp_timer_stop(face_timer_);
        ShowEmotion(g.eyes);
        Notify(g.text);
    }

    // manual = true cuando el usuario dijo "pare"
    void EndGesture(bool manual) {
        if (manual) {
            ShowEmotion("car_stop");
            Notify("Detenido");
            esp_timer_stop(face_timer_);
            esp_timer_start_once(face_timer_, 1500 * 1000);   // luego vuelve a la mirada normal
        } else {
            ShowEmotion("car_idle");
        }
    }

    void StopMotors() {
        MotorA(0); MotorB(0);
        SetDuty(kChA, 0); SetDuty(kChB, 0);
        state_ = "stopped"; speed_ = 0;
    }

public:
    CarController(gpio_num_t in1, gpio_num_t in2, gpio_num_t in3, gpio_num_t in4,
                  gpio_num_t ena, gpio_num_t enb)
        : in1_(in1), in2_(in2), in3_(in3), in4_(in4), ena_(ena), enb_(enb) {

        gpio_config_t io = {};
        io.pin_bit_mask = (1ULL << in1_) | (1ULL << in2_) | (1ULL << in3_) | (1ULL << in4_);
        io.mode = GPIO_MODE_OUTPUT;
        io.pull_up_en = GPIO_PULLUP_DISABLE;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&io));

        SetupPwm();

        stop_timer_ = MakeTimer(&CarController::StopTimerCb, "car_stop");
        face_timer_ = MakeTimer(&CarController::FaceTimerCb, "car_face");

        StopMotors();          // aquí NO se toca la pantalla: la placa aún se está creando
        RegisterTools();
    }

    // Movimiento no bloqueante: arranca y un timer lo detiene solo (seguridad)
    bool Move(const std::string& direction, int speed, int duration_ms) {
        int a, b;
        const Gesture* g;
        if      (direction == "forward")  { a =  1; b =  1; g = &kForward;  }
        else if (direction == "backward") { a = -1; b = -1; g = &kBackward; }
        else if (direction == "left")     { a = -1; b =  1; g = &kLeft;     }   // giro en el lugar
        else if (direction == "right")    { a =  1; b = -1; g = &kRight;    }
        else return false;

        esp_timer_stop(stop_timer_);            // si ya había un movimiento, se reemplaza
        MotorA(a); MotorB(b);
        SetDuty(kChA, speed); SetDuty(kChB, speed);
        state_ = direction; speed_ = speed;
        StartGesture(*g);
        esp_timer_start_once(stop_timer_, (uint64_t)duration_ms * 1000);
        return true;
    }

    void Stop() {
        esp_timer_stop(stop_timer_);
        StopMotors();
        EndGesture(true);
    }

    void RegisterTools() {
        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("self.car.move",
            "Move the robot car (two DC wheels). Use when the user asks the robot to move, e.g. "
            "'avanza' (forward), 'retrocede' (backward), 'izquierda' (left), 'derecha' (right). "
            "Do NOT use this tool for general questions; only for movement commands.\n"
            "direction: forward | backward | left | right (left/right spin in place).\n"
            "speed: percent 30-100 (default 70). duration_ms: how long to move, 200-10000 (default 1000); "
            "the car stops automatically afterwards. For 'a little' use ~500 ms, for 'a lot' ~3000 ms. "
            "A 90-degree turn is roughly 400-600 ms at speed 70 (calibrate). "
            "The display already shows a face for the movement, so do not change the emotion yourself.",
            PropertyList({
                Property("direction", kPropertyTypeString),
                Property("speed", kPropertyTypeInteger, 70, 30, 100),
                Property("duration_ms", kPropertyTypeInteger, 1000, 200, 10000)
            }),
            [this](const PropertyList& p) -> ReturnValue {
                auto dir = p["direction"].value<std::string>();
                bool ok = Move(dir, p["speed"].value<int>(), p["duration_ms"].value<int>());
                if (!ok) return "{\"success\": false, \"error\": \"invalid direction\"}";
                return true;
            });

        mcp.AddTool("self.car.stop",
            "Stop the robot car immediately ('pare', 'para', 'detente', 'alto', 'stop').",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                Stop();
                return true;
            });

        mcp.AddTool("self.car.get_state",
            "Get the current movement state of the robot car.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                return "{\"state\": \"" + state_ + "\", \"speed\": " + std::to_string(speed_) + "}";
            });
    }
};

#endif // __CAR_CONTROLLER_H__
