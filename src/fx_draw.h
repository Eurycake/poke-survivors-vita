
#ifndef FX_DRAW_H
#define FX_DRAW_H
#include <math.h>
#include <stdint.h>
#define FXC(r,g,b,a) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(g)<<8)|((uint32_t)(r)))
static inline float fx_clamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }


static inline void fx_draw_whip_ex(float px, float py, float angle, float u, float radius, float arc, float seed, int whip_mode) {
    float half = fminf(arc, 6.2831853f) * 0.5f;
    float reach = fminf(1.0f, u * 6.0f);
    if (u > 0.78f) reach = 1.0f - (u - 0.78f) / 0.22f * 0.5f;
    float fade = u < 0.82f ? 1.0f : fmaxf(0.0f, (1.0f - u) / 0.18f);
    if (whip_mode == 2) radius *= 0.65f;
    float len = radius * reach;
    enum { N = 44 };
    float xs[N + 1], ys[N + 1];
    for (int i = 0; i <= N; i++) {
        float f = (float)i / N;
        float s = fx_clamp((u - 0.34f * f) / 0.62f, 0.0f, 1.0f);
        s = 1.0f - (1.0f - s) * (1.0f - s);
        float a = angle - half + s * half * 2.0f;
        float ripple_amp = (whip_mode == 2) ? 4.5f : 9.0f;
        float ripple = sinf(f * 11.0f - u * 26.0f + seed) * ripple_amp * sinf(3.14159265f * f) * sinf(3.14159265f * fx_clamp(u * 1.15f, 0.0f, 1.0f));
        float d = f * len;
        xs[i] = px + cosf(a) * d - sinf(a) * ripple;
        ys[i] = py + sinf(a) * d + cosf(a) * ripple;
    }
    for (int i = 0; i < N; i++) {
        float f = (float)i / N;
        if (whip_mode != 2 && f > 0.82f && (i & 1)) continue;
        float tf = fx_clamp(1.0f - f * 0.9f, 0.0f, 1.0f);
        uint8_t al = (uint8_t)(255.0f * fade * (0.35f + 0.65f * tf));
        float th = (whip_mode == 2) ? (f < 0.50f ? 4.5f : 3.0f) : (f < 0.45f ? 2.0f : 1.0f);
        render_draw_line(xs[i] + 10.0f, ys[i] + 18.5f, xs[i + 1] + 10.0f, ys[i + 1] + 18.5f, FXC(8, 12, 22, (uint8_t)(al * 0.38f)), th + 2.0f);
        if (whip_mode == 1) {
            render_draw_line(xs[i], ys[i], xs[i + 1], ys[i + 1], FXC(0, 0, 0, (uint8_t)(al * 0.95f)), th + 2.5f);
            render_draw_line(xs[i], ys[i], xs[i + 1], ys[i + 1], FXC(210, 45, 10, al), th + 1.0f);
            render_draw_line(xs[i], ys[i], xs[i + 1], ys[i + 1], f < 0.45f ? FXC(255, 130, 20, al) : FXC(255, 220, 50, al), th);
        } else if (whip_mode == 2) {
            render_draw_line(xs[i], ys[i], xs[i + 1], ys[i + 1], FXC(60, 20, 40, (uint8_t)(al * 0.95f)), th + 3.0f);
            render_draw_line(xs[i], ys[i], xs[i + 1], ys[i + 1], FXC(230, 75, 130, al), th + 1.2f);
            render_draw_line(xs[i], ys[i], xs[i + 1], ys[i + 1], f < 0.50f ? FXC(255, 140, 185, al) : FXC(255, 190, 220, al), th);
        } else {

            render_draw_line(xs[i], ys[i], xs[i + 1], ys[i + 1], FXC(0, 0, 0, (uint8_t)(al * 0.95f)), th + 2.5f);
            render_draw_line(xs[i], ys[i], xs[i + 1], ys[i + 1], FXC(24, 84, 28, al), th + 1.0f);
            render_draw_line(xs[i], ys[i], xs[i + 1], ys[i + 1], f < 0.45f ? FXC(92, 205, 82, al) : FXC(150, 232, 120, al), th);
        }
    }
    if (reach > 0.9f) {
        uint32_t tip_col = (whip_mode == 1) ? FXC(255, 255, 200, (uint8_t)(255.0f * fade)) :
                           (whip_mode == 2) ? FXC(255, 240, 250, (uint8_t)(255.0f * fade)) :
                                              FXC(235, 255, 190, (uint8_t)(255.0f * fade));
        render_draw_circle(xs[N], ys[N], whip_mode == 2 ? 3.2f : 1.8f, tip_col);
    }
}

static inline void fx_draw_whip(float px, float py, float angle, float u, float radius, float arc, float seed) {
    fx_draw_whip_ex(px, py, angle, u, radius, arc, seed, 0);
}


static inline void fx_draw_slash_arc(float px, float py, float angle, float u, float radius, uint32_t col) {
    float sw = fx_clamp(u / 0.55f, 0.0f, 1.0f); sw = 1.0f - (1.0f - sw) * (1.0f - sw);
    float fade = u < 0.6f ? 1.0f : fmaxf(0.0f, (1.0f - u) / 0.4f);
    float span = 1.9f, head = angle - span * 0.5f + sw * span, tail = head - span * 0.85f * fminf(1.0f, 0.25f + sw);
    float R = radius * (0.80f + 0.2f * sw);
    uint8_t r = col & 255, g = (col >> 8) & 255, b = (col >> 16) & 255;
    enum { N = 64 };
    for (int i = 0; i <= N; i++) {
        float f = (float)i / N, a = tail + (head - tail) * f;
        float w = 1.0f + 8.0f * f * f;
        float cx = px + cosf(a) * R, cy = py + sinf(a) * R;
        uint8_t al = (uint8_t)(255.0f * fade * (0.30f + 0.70f * f));
        render_draw_circle_ground(cx + 10.0f, cy + 4.5f, w * 0.5f + 0.6f, FXC(8, 12, 22, (uint8_t)(al * 0.36f)));
        render_draw_circle(cx, cy, w * 0.5f + 0.8f, FXC(r / 3, g / 3, b / 3, al / 2));
        render_draw_circle(cx, cy, w * 0.5f, FXC(r, g, b, al));
        if (f > 0.55f) {
            float hx = px + cosf(a) * (R + w * 0.28f), hy = py + sinf(a) * (R + w * 0.28f);
            render_draw_circle(hx, hy, w * 0.2f + 0.3f, FXC(255, 255, 255, al));
        }
    }
}


static inline void fx_draw_egg(float x, float y, float u) {
    float ph = fx_clamp(u, 0.0f, 0.999f) * 3.0f; int bn = (int)ph; float t = ph - bn;
    float hop = sinf(3.14159265f * t) * (26.0f + 6.0f * bn);
    float sc = 0.8f + 0.35f * (bn + t);
    render_draw_circle(x, y + 4.0f, 6.0f * sc, FXC(0, 0, 0, 70));
    float ey = y - hop - 5.0f * sc;


    float pulse = 0.7f + 0.3f * sinf(u * 24.0f);
    uint8_t aura_al = (uint8_t)(140.0f * pulse);
    render_draw_circle(x - 2.0f * sc, ey, 7.5f * sc, FXC(255, 95, 20, aura_al));
    render_draw_circle(x + 2.0f * sc, ey, 7.5f * sc, FXC(255, 230, 40, aura_al));


    render_draw_circle(x, ey + 1.0f * sc, 6.0f * sc + 1.0f, FXC(96, 78, 52, 255));
    render_draw_circle(x, ey - 4.0f * sc, 4.6f * sc + 1.0f, FXC(96, 78, 52, 255));
    render_draw_circle(x, ey + 1.0f * sc, 6.0f * sc, FXC(255, 240, 206, 255));
    render_draw_circle(x, ey - 4.0f * sc, 4.6f * sc, FXC(255, 240, 206, 255));


    render_draw_circle(x - 2.5f * sc, ey - 1.0f * sc, 1.4f * sc, FXC(240, 60, 30, 255));
    render_draw_circle(x + 2.5f * sc, ey + 2.0f * sc, 1.4f * sc, FXC(255, 215, 30, 255));
    render_draw_circle(x - 1.5f * sc, ey - 5.0f * sc, 1.1f * sc, FXC(255, 255, 255, 230));
}
#endif
