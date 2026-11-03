// eyes_display.h — Ojos animados (estilo "robot eyes") para la OLED 128x64 de XiaoZhi
// Copiar en: main/boards/bread-compact-wifi/eyes_display.h
//
// - Reemplaza la carita/emoji por dos ojos grandes con pupilas, párpados y parpadeo.
// - La barra superior (hora, WiFi, "Escuchando...", "Avanzando") sigue visible.
// - Las emociones que manda la IA (happy, sad, angry...) se convierten en expresiones de ojos.
// - El carro usa nombres "car_*" (car_forward, car_left, ... car_idle) para fijar la mirada
//   mientras se mueve, sin que la IA la cambie.
#ifndef __EYES_DISPLAY_H__
#define __EYES_DISPLAY_H__

#include "display/oled_display.h"

#include <lvgl.h>
#include <string.h>
#include <string>

namespace eyes {

// Forma de UN ojo.
// lid_out / lid_in: altura (px desde arriba) del borde del párpado superior en el lado
//                   exterior / interior del ojo (0 = sin párpado). Distintos = párpado inclinado.
// bottom: cuánto sube el párpado inferior (0 = nada).
struct Eye {
    int8_t w, h, radius;       // radius 0 = ovalado
    int8_t pdx, pdy;           // desplazamiento de la pupila
    int8_t lid_out, lid_in, bottom;
    bool pupil;
};
struct Expr { Eye l, r; };

constexpr Eye E(int w, int h, int pdx = 0, int pdy = 0, int lo = 0, int li = 0,
                       int bot = 0, bool pupil = true, int radius = 0) {
    return Eye{(int8_t)w, (int8_t)h, (int8_t)radius, (int8_t)pdx, (int8_t)pdy,
               (int8_t)lo, (int8_t)li, (int8_t)bot, pupil};
}
constexpr Expr Same(Eye e) { return Expr{e, e}; }

// ---------- catálogo de expresiones (como la tabla de EYES_*) ----------
constexpr Expr kFront      = Same(E(34, 40));
constexpr Expr kWide       = Same(E(38, 46));
constexpr Expr kNarrow     = Same(E(34, 26));
constexpr Expr kUp         = Same(E(34, 40, 0, -11));
constexpr Expr kDown       = Same(E(34, 40, 0, 11));
constexpr Expr kLeft       = Same(E(34, 40, -9, 0));
constexpr Expr kRight      = Same(E(34, 40, 9, 0));
constexpr Expr kLeftUp     = Same(E(34, 40, -9, -10));
constexpr Expr kRightUp    = Same(E(34, 40, 9, -10));
constexpr Expr kCrossed    = Expr{E(34, 40, 8, 0), E(34, 40, -8, 0)};
constexpr Expr kConfused   = Expr{E(34, 28, 0, 2), E(36, 44, 0, -4)};
constexpr Expr kMad        = Same(E(36, 40, 0, 3, 4, 20));        // cejas hacia el centro
constexpr Expr kGlare      = Same(E(36, 40, 0, 2, 4, 16, 10));    // mirada entrecerrada
constexpr Expr kCry        = Same(E(34, 40, 0, 6, 20, 4));        // párpados tristes
constexpr Expr kDistressed = Same(E(36, 42, 0, 4, 14, 2, 6));
constexpr Expr kTired      = Same(E(34, 40, 0, 6, 18, 18));
constexpr Expr kSleep      = Same(E(34, 40, 0, 0, 33, 33, 0, false));
constexpr Expr kHappy      = Same(E(36, 40, 0, 0, 0, 0, 20, false)); // ojos "^ ^"
constexpr Expr kGlasses    = Same(E(40, 22, 0, 0, 0, 0, 0, true, 4));
constexpr Expr kWink       = Expr{E(34, 40), E(34, 40, 0, 0, 33, 33, 0, false)};

}  // namespace eyes

class EyesOledDisplay : public OledDisplay {
public:
    // Si en tu pantalla los ojos salen "en negativo" (negros sobre fondo blanco),
    // pon esta constante en true y vuelve a compilar.
    static constexpr bool kInvertColors = false;

private:
    using Eye = eyes::Eye;
    using Expr = eyes::Expr;

    // ---------- geometría ----------
    static constexpr int kAreaY   = 16;   // debajo de la barra de estado
    static constexpr int kAreaH   = 48;
    static constexpr int kCxLeft  = 38;   // centro de cada ojo dentro del área
    static constexpr int kCxRight = 90;
    static constexpr int kCy      = 24;
    static constexpr int kLidT    = 60;   // grosor de la "línea" que hace de párpado

    static const Expr& Lookup(const char* name) {
        using namespace eyes;
        struct Item { const char* name; const Expr* expr; };
        static const Item table[] = {
            // nombres directos (como en la tabla)
            {"eyes_front", &kFront}, {"eyes_wide", &kWide}, {"eyes_narrow", &kNarrow},
            {"eyes_up", &kUp}, {"eyes_down", &kDown}, {"eyes_left", &kLeft},
            {"eyes_right", &kRight}, {"eyes_left_up", &kLeftUp}, {"eyes_right_up", &kRightUp},
            {"eyes_crossed", &kCrossed}, {"eyes_confused", &kConfused}, {"eyes_mad", &kMad},
            {"eyes_glare", &kGlare}, {"eyes_cry", &kCry}, {"eyes_distressed", &kDistressed},
            {"eyes_tired", &kTired}, {"eyes_sleep", &kSleep}, {"eyes_happy", &kHappy},
            {"eyes_glasses", &kGlasses}, {"eyes_wink", &kWink},
            // ===== movimientos del carro (cámbialos a tu gusto) =====
            {"forward",  &kWide},        // "Avanza"
            {"backward", &kDistressed},  // "Retrocede"
            {"left",     &kLeft},        // "Izquierda"
            {"right",    &kRight},       // "Derecha"
            {"stop",     &kMad},         // "Pare"
            // ===== emociones que manda la IA de XiaoZhi =====
            {"neutral", &kFront}, {"happy", &kHappy}, {"laughing", &kHappy},
            {"funny", &kHappy}, {"delicious", &kHappy}, {"sad", &kCry}, {"crying", &kCry},
            {"angry", &kMad}, {"loving", &kWide}, {"embarrassed", &kDown},
            {"surprised", &kWide}, {"shocked", &kWide}, {"thinking", &kLeftUp},
            {"winking", &kWink}, {"kissy", &kWink}, {"cool", &kGlasses},
            {"relaxed", &kTired}, {"confident", &kGlare}, {"sleepy", &kSleep},
            {"silly", &kCrossed}, {"confused", &kConfused},
        };
        for (const auto& it : table) {
            if (strcmp(it.name, name) == 0) return *it.expr;
        }
        return eyes::kFront;
    }

    // ---------- objetos LVGL ----------
    struct EyeObjs {
        lv_obj_t* eye = nullptr;
        lv_obj_t* pupil = nullptr;
        lv_obj_t* top = nullptr;
        lv_obj_t* bot = nullptr;
        lv_point_precise_t top_pts[2];
        lv_point_precise_t bot_pts[2];
    };
    lv_obj_t* area_ = nullptr;
    EyeObjs left_, right_;
    lv_timer_t* blink_timer_ = nullptr;
    bool blinking_ = false;

    Expr current_ = eyes::kFront;
    std::string ai_emotion_ = "neutral";
    bool car_hold_ = false;

    static lv_color_t On()  { return kInvertColors ? lv_color_white() : lv_color_black(); }
    static lv_color_t Off() { return kInvertColors ? lv_color_black() : lv_color_white(); }

    lv_obj_t* MakeLid(lv_obj_t* parent) {
        lv_obj_t* line = lv_line_create(parent);
        lv_obj_set_style_line_width(line, kLidT, 0);
        lv_obj_set_style_line_color(line, Off(), 0);
        lv_obj_set_style_line_rounded(line, false, 0);
        lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
        return line;
    }

    void BuildEye(EyeObjs& o) {
        o.eye = lv_obj_create(area_);
        lv_obj_remove_flag(o.eye, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(o.eye, On(), 0);
        lv_obj_set_style_bg_opa(o.eye, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(o.eye, 0, 0);
        lv_obj_set_style_pad_all(o.eye, 0, 0);
        lv_obj_set_style_clip_corner(o.eye, true, 0);

        o.pupil = lv_obj_create(o.eye);
        lv_obj_remove_flag(o.pupil, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(o.pupil, 12, 14);
        lv_obj_set_style_radius(o.pupil, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(o.pupil, Off(), 0);
        lv_obj_set_style_bg_opa(o.pupil, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(o.pupil, 0, 0);

        o.top = MakeLid(o.eye);   // los párpados se crean después: tapan la pupila
        o.bot = MakeLid(o.eye);
    }

    // Dibuja un ojo. is_left decide qué lado es el "interior" (hacia la nariz).
    void ApplyEye(EyeObjs& o, const Eye& e, int cx, bool is_left, bool closed) {
        int w = e.w, h = e.h;
        lv_obj_set_size(o.eye, w, h);
        lv_obj_set_pos(o.eye, cx - w / 2, kCy - h / 2);
        lv_obj_set_style_radius(o.eye, e.radius ? e.radius : LV_RADIUS_CIRCLE, 0);

        if (e.pupil) {
            lv_obj_remove_flag(o.pupil, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(o.pupil, w / 2 - 6 + e.pdx, h / 2 - 7 + e.pdy);
        } else {
            lv_obj_add_flag(o.pupil, LV_OBJ_FLAG_HIDDEN);
        }

        // Párpado superior
        int y_out = closed ? h + 2 : e.lid_out;
        int y_in  = closed ? h + 2 : e.lid_in;
        if (y_out <= 0 && y_in <= 0) {
            lv_obj_add_flag(o.top, LV_OBJ_FLAG_HIDDEN);
        } else {
            int y_l = is_left ? y_out : y_in;   // borde en el lado izquierdo del ojo
            int y_r = is_left ? y_in : y_out;   // borde en el lado derecho del ojo
            int c_l = y_l - kLidT / 2, c_r = y_r - kLidT / 2;
            int base = c_l < c_r ? c_l : c_r;
            o.top_pts[0] = {0, (lv_value_precise_t)(c_l - base)};
            o.top_pts[1] = {(lv_value_precise_t)(w + 16), (lv_value_precise_t)(c_r - base)};
            lv_line_set_points(o.top, o.top_pts, 2);
            lv_obj_set_pos(o.top, -8, base);
            lv_obj_remove_flag(o.top, LV_OBJ_FLAG_HIDDEN);
        }

        // Párpado inferior (plano)
        if (e.bottom <= 0 || closed) {
            lv_obj_add_flag(o.bot, LV_OBJ_FLAG_HIDDEN);
        } else {
            int c = (h - e.bottom) + kLidT / 2;
            o.bot_pts[0] = {0, 0};
            o.bot_pts[1] = {(lv_value_precise_t)(w + 16), 0};
            lv_line_set_points(o.bot, o.bot_pts, 2);
            lv_obj_set_pos(o.bot, -8, c);
            lv_obj_remove_flag(o.bot, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Debe llamarse con la pantalla bloqueada (o desde un lv_timer)
    void Render(bool closed = false) {
        if (area_ == nullptr) return;
        ApplyEye(left_, current_.l, kCxLeft, true, closed);
        ApplyEye(right_, current_.r, kCxRight, false, closed);
    }

    void Show(const Expr& e) {
        current_ = e;
        DisplayLockGuard lock(this);
        Render(blinking_);
    }

    // Parpadeo automático: cierra ~120 ms cada 2,5–5,5 s
    static void BlinkCb(lv_timer_t* t) {
        auto self = static_cast<EyesOledDisplay*>(lv_timer_get_user_data(t));
        if (!self->blinking_) {
            // no parpadear si los ojos ya están casi cerrados
            if (self->current_.l.lid_out > 25 && self->current_.r.lid_out > 25) {
                lv_timer_set_period(t, 3000);
                return;
            }
            self->blinking_ = true;
            self->Render(true);
            lv_timer_set_period(t, 120);
        } else {
            self->blinking_ = false;
            self->Render(false);
            lv_timer_set_period(t, 2500 + lv_rand(0, 3000));
        }
    }

public:
    using OledDisplay::OledDisplay;

    void SetupUI() override {
        OledDisplay::SetupUI();
        if (height_ != 64) return;   // solo para la OLED 128x64

        DisplayLockGuard lock(this);
        area_ = lv_obj_create(lv_screen_active());
        lv_obj_remove_flag(area_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(area_, 0, kAreaY);
        lv_obj_set_size(area_, width_, kAreaH);
        lv_obj_set_style_radius(area_, 0, 0);
        lv_obj_set_style_border_width(area_, 0, 0);
        lv_obj_set_style_pad_all(area_, 0, 0);
        lv_obj_set_style_bg_color(area_, Off(), 0);
        lv_obj_set_style_bg_opa(area_, LV_OPA_COVER, 0);

        BuildEye(left_);
        BuildEye(right_);
        Render(false);
        blink_timer_ = lv_timer_create(&EyesOledDisplay::BlinkCb, 3000, this);
    }

    bool SetEmotion(const char* emotion) override {
        if (emotion == nullptr) return false;
        if (area_ == nullptr) return OledDisplay::SetEmotion(emotion);

        if (strncmp(emotion, "car_", 4) == 0) {
            if (strcmp(emotion + 4, "idle") == 0) {
                car_hold_ = false;
                Show(Lookup(ai_emotion_.c_str()));
            } else {
                car_hold_ = true;
                Show(Lookup(emotion + 4));
            }
            return true;
        }

        ai_emotion_ = emotion;
        if (!car_hold_) Show(Lookup(emotion));
        return true;
    }
};

#endif  // __EYES_DISPLAY_H__
