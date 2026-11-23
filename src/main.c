#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "game.h"
#include "renderer.h"
#include "audio.h"

#ifdef __vita__
#include <vitasdk.h>
#include <psp2/power.h>
#include <psp2/kernel/threadmgr.h>


unsigned int _newlib_heap_size_user = 128 * 1024 * 1024;
unsigned int _newlib_time_slice = 1000;

static volatile bool g_vita_auto_pause_req = false;

static int vita_power_cb(int notifyId, int notifyCount, int powerInfo, void *common) {
    (void)notifyId; (void)notifyCount; (void)common;
    if (powerInfo & (SCE_POWER_CB_APP_SUSPEND | SCE_POWER_CB_APP_RESUMING)) {
        g_vita_auto_pause_req = true;
    }
    return 0;
}

static int vita_power_cb_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("PokePowerCB", 0, vita_power_cb, NULL);
    if (cbid >= 0) {
        scePowerRegisterCallback(cbid);
        while (1) {
            sceKernelDelayThreadCB(100000);
        }
    }
    return 0;
}

static void vita_boot_log(const char *msg) {
    FILE *f = fopen("ux0:data/poke-survivors vita/boot_log.txt", "a");
    if (!f) f = fopen("ux0:data/boot_log.txt", "a");
    if (f) {
        fputs(msg, f);
        fputs("\n", f);
        fclose(f);
    }
}

int main(int argc, char *argv[]) {

    if (scePowerSetArmClockFrequency(500) < 0) scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    vita_boot_log("=== poke-survivors vita Boot ===");


    render_init();
    vita_boot_log("render_init OK");

    audio_init();
    vita_boot_log("audio_init OK");


    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    SceCtrlData pad, old_pad;
    SceTouchData touch;
    bool old_touch_down = false;
    memset(&touch, 0, sizeof(touch));
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    memset(&pad, 0, sizeof(pad));
    memset(&old_pad, 0, sizeof(old_pad));


    if (!renderer_has_data_assets()) {
        while (!renderer_has_data_assets()) {
            memcpy(&old_pad, &pad, sizeof(pad));
            sceCtrlPeekBufferPositive(0, &pad, 1);


            if (pad.buttons & SCE_CTRL_CIRCLE) {
                audio_fini();
                render_fini();
                sceKernelExitProcess(0);
                return 0;
            }


            if ((pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS)) {
                render_fini();
                render_init();
                if (renderer_has_data_assets()) break;
            }

            render_start_frame();
            render_draw_data_missing_screen();
            render_end_frame();
        }
    }


    game_init();
    vita_boot_log("game_init OK, entering main loop");


    SceUID cb_th = sceKernelCreateThread("PokePowerCBThread", vita_power_cb_thread, 0x10000100, 0x4000, 0, 0, NULL);
    if (cb_th >= 0) sceKernelStartThread(cb_th, 0, NULL);

    SceUInt64 prev_us = sceKernelGetProcessTimeWide();

    GameStateEnum prev_state = g_game.state;
    while (1) {
        SceUInt64 now_us = sceKernelGetProcessTimeWide();
        SceUInt64 diff_us = (now_us > prev_us) ? (now_us - prev_us) : 1000ULL;
        float dt = (float)diff_us / 1000000.0f;
        prev_us = now_us;

        if (diff_us > 1500000ULL && prev_state == STATE_PLAYING && g_game.state == STATE_PLAYING && g_game.run_timer > 0.5f) {
            g_vita_auto_pause_req = true;
        }
        prev_state = g_game.state;
        if (dt < 0.001f) dt = 0.001f;
        if (dt > 0.05f) dt = 0.05f;


        memcpy(&old_pad, &pad, sizeof(pad));
        sceCtrlPeekBufferPositive(0, &pad, 1);

        InputState input = {0};
        if (g_vita_auto_pause_req) {
            g_vita_auto_pause_req = false;
            if (g_game.state == STATE_PLAYING) {
                g_game.state = STATE_PAUSED;
                g_game.pause_selected_idx = 0;
                g_game.pause_sub_menu = 0;
                g_game.grace_timer = 0.35f;
            }
        }
        memset(&touch, 0, sizeof(touch));
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
        bool touch_down = touch.reportNum > 0;
        input.touch_pressed = touch_down && !old_touch_down;
        old_touch_down = touch_down;
        if (touch_down) {
            input.touch_x = (float)touch.report[0].x * 0.5f;
            input.touch_y = (float)touch.report[0].y * 0.5f;
        }


        float ax = ((float)pad.lx - 128.0f) / 128.0f;
        float ay = ((float)pad.ly - 128.0f) / 128.0f;
        float old_ax = ((float)old_pad.lx - 128.0f) / 128.0f;
        float old_ay = ((float)old_pad.ly - 128.0f) / 128.0f;

        if (fabsf(ax) > 0.2f) input.move_x = ax;
        if (fabsf(ay) > 0.2f) input.move_y = ay;


        if (pad.buttons & SCE_CTRL_LEFT)  input.move_x = -1.0f;
        if (pad.buttons & SCE_CTRL_RIGHT) input.move_x = 1.0f;
        if (pad.buttons & SCE_CTRL_UP)    input.move_y = -1.0f;
        if (pad.buttons & SCE_CTRL_DOWN)  input.move_y = 1.0f;


        {
            float rx = ((float)pad.rx - 128.0f) / 128.0f;
            float ry = ((float)pad.ry - 128.0f) / 128.0f;
            input.cam_yaw  = (fabsf(rx) > 0.25f) ? rx : 0.0f;
            input.cam_tilt = (fabsf(ry) > 0.25f) ? ry : 0.0f;
            bool lb = (pad.buttons & SCE_CTRL_LTRIGGER) != 0, rb = (pad.buttons & SCE_CTRL_RTRIGGER) != 0;
            bool olb = (old_pad.buttons & SCE_CTRL_LTRIGGER) != 0, orb = (old_pad.buttons & SCE_CTRL_RTRIGGER) != 0;
            input.btn_l_pressed = lb && !olb;
            input.btn_r_pressed = rb && !orb;
            input.cam_preset_pressed = lb && rb && !(olb && orb);
            if (rb && !lb) input.cam_zoom = 1.0f;
            else if (lb && !rb) input.cam_zoom = -1.0f;
        }


        bool stick_left  = (ax < -0.5f) && (old_ax >= -0.5f);
        bool stick_right = (ax > 0.5f)  && (old_ax <= 0.5f);
        bool stick_up    = (ay < -0.5f) && (old_ay >= -0.5f);
        bool stick_down  = (ay > 0.5f)  && (old_ay <= 0.5f);


        input.btn_confirm = (pad.buttons & SCE_CTRL_CROSS) != 0;
        input.btn_confirm_pressed = (pad.buttons & SCE_CTRL_CROSS) && !(old_pad.buttons & SCE_CTRL_CROSS);


        input.btn_cancel = (pad.buttons & SCE_CTRL_CIRCLE) != 0;
        input.btn_cancel_pressed = (pad.buttons & SCE_CTRL_CIRCLE) && !(old_pad.buttons & SCE_CTRL_CIRCLE);


        input.btn_square = (pad.buttons & SCE_CTRL_SQUARE) != 0;
        input.btn_square_pressed = (pad.buttons & SCE_CTRL_SQUARE) && !(old_pad.buttons & SCE_CTRL_SQUARE);


        input.btn_skill_pressed[0] = (pad.buttons & SCE_CTRL_TRIANGLE) && !(old_pad.buttons & SCE_CTRL_TRIANGLE);
        input.btn_skill_pressed[1] = (pad.buttons & SCE_CTRL_SQUARE)   && !(old_pad.buttons & SCE_CTRL_SQUARE);
        input.btn_skill_pressed[2] = (pad.buttons & SCE_CTRL_CIRCLE)   && !(old_pad.buttons & SCE_CTRL_CIRCLE);
        input.btn_skill_pressed[3] = (pad.buttons & SCE_CTRL_CROSS)    && !(old_pad.buttons & SCE_CTRL_CROSS);
        input.btn_triangle = (pad.buttons & SCE_CTRL_TRIANGLE) != 0;
        input.btn_triangle_pressed = input.btn_skill_pressed[0];


        input.btn_pause = (pad.buttons & (SCE_CTRL_START | SCE_CTRL_SELECT)) != 0;
        input.btn_pause_pressed = (pad.buttons & (SCE_CTRL_START | SCE_CTRL_SELECT)) && !(old_pad.buttons & (SCE_CTRL_START | SCE_CTRL_SELECT));

        input.btn_left_pressed  = ((pad.buttons & SCE_CTRL_LEFT)  && !(old_pad.buttons & SCE_CTRL_LEFT))  || stick_left;
        input.btn_right_pressed = ((pad.buttons & SCE_CTRL_RIGHT) && !(old_pad.buttons & SCE_CTRL_RIGHT)) || stick_right;
        input.btn_up_pressed    = ((pad.buttons & SCE_CTRL_UP)    && !(old_pad.buttons & SCE_CTRL_UP))    || stick_up;
        input.btn_down_pressed  = ((pad.buttons & SCE_CTRL_DOWN)  && !(old_pad.buttons & SCE_CTRL_DOWN))  || stick_down;


        game_update(dt, &input);
        game_render();
    }

    audio_fini();
    render_fini();
    sceKernelExitProcess(0);
    return 0;
}

#else

#include <windows.h>

HWND g_hwnd = NULL;
static bool g_keys[256] = {0};
static bool g_prev_keys[256] = {0};

static int g_mouse_wheel_delta = 0;
static bool g_rmb_down = false;
static int g_last_mouse_x = 0, g_last_mouse_y = 0;
static float g_mouse_yaw_delta = 0.0f;
static float g_mouse_pitch_delta = 0.0f;

static bool g_lmb_down = false;
static bool g_lmb_clicked = false;
static float g_lmb_x = 0.0f, g_lmb_y = 0.0f;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
            if (wParam < 256) g_keys[wParam] = true;
            break;
        case WM_KEYUP:
            if (wParam < 256) g_keys[wParam] = false;
            break;
        case WM_MOUSEWHEEL:
            g_mouse_wheel_delta += GET_WHEEL_DELTA_WPARAM(wParam);
            break;
        case WM_RBUTTONDOWN:
            g_rmb_down = true;
            g_last_mouse_x = (short)LOWORD(lParam);
            g_last_mouse_y = (short)HIWORD(lParam);
            SetCapture(hwnd);
            break;
        case WM_RBUTTONUP:
            g_rmb_down = false;
            ReleaseCapture();
            break;
        case WM_MOUSEMOVE:
            if (g_rmb_down) {
                int mx = (short)LOWORD(lParam);
                int my = (short)HIWORD(lParam);
                int dx = mx - g_last_mouse_x;
                int dy = my - g_last_mouse_y;
                g_mouse_yaw_delta += (float)dx * 0.35f;
                g_mouse_pitch_delta += (float)dy * 0.35f;
                g_last_mouse_x = mx;
                g_last_mouse_y = my;
            }
            break;
        case WM_LBUTTONDOWN: {
            g_lmb_down = true;
            g_lmb_clicked = true;
            RECT rc;
            GetClientRect(hwnd, &rc);
            int cw = rc.right - rc.left;
            int ch = rc.bottom - rc.top;
            if (cw > 0 && ch > 0) {
                g_lmb_x = (float)((short)LOWORD(lParam)) * (float)SCREEN_W / (float)cw;
                g_lmb_y = (float)((short)HIWORD(lParam)) * (float)SCREEN_H / (float)ch;
            }
            break;
        }
        case WM_LBUTTONUP:
            g_lmb_down = false;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    char exe_dir[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir))) {
        char *s1 = strrchr(exe_dir, '\\');
        char *s2 = strrchr(exe_dir, '/');
        char *slash = s1 > s2 ? s1 : s2;
        if (slash) {
            *slash = '\0';
            SetCurrentDirectoryA(exe_dir);
        }
    }


    if (GetFileAttributesA("assets/ui/font_atlas.png") != INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesA("assets/world/emerald_tiles.png") != INVALID_FILE_ATTRIBUTES) {
        /* assets already in the current directory */
    } else if (GetFileAttributesA("assets/assets/ui/font_atlas.png") != INVALID_FILE_ATTRIBUTES ||
               GetFileAttributesA("assets/assets/world/emerald_tiles.png") != INVALID_FILE_ATTRIBUTES) {
        SetCurrentDirectoryA("assets");
    } else if (GetFileAttributesA("../assets/ui/font_atlas.png") != INVALID_FILE_ATTRIBUTES ||
               GetFileAttributesA("../assets/world/emerald_tiles.png") != INVALID_FILE_ATTRIBUTES) {
        SetCurrentDirectoryA("..");
    } else if (GetFileAttributesA("../assets/assets/ui/font_atlas.png") != INVALID_FILE_ATTRIBUTES ||
               GetFileAttributesA("../assets/assets/world/emerald_tiles.png") != INVALID_FILE_ATTRIBUTES) {
        SetCurrentDirectoryA("../assets");
    } else if (GetFileAttributesA("../assets/assets/ui/font_atlas.png") != INVALID_FILE_ATTRIBUTES ||
               GetFileAttributesA("../assets/assets/world/emerald_tiles.png") != INVALID_FILE_ATTRIBUTES) {
        SetCurrentDirectoryA("../assets");
    }

    WNDCLASS wc = {0};
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "PokeSurvivorVitaPC";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClass(&wc);

    RECT wr = { 0, 0, SCREEN_W, SCREEN_H };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    g_hwnd = CreateWindow("PokeSurvivorVitaPC", "poke-survivors vita",
                          WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          wr.right - wr.left, wr.bottom - wr.top,
                          NULL, NULL, hInstance, NULL);

    render_init();
    audio_init();
    game_init();

    LARGE_INTEGER freq, t1, t2;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t1);

    MSG msg;
    bool running = true;
    const float target_dt = 1.0f / 60.0f;

    while (running) {
        LARGE_INTEGER t_now;
        QueryPerformanceCounter(&t_now);
        float dt = (float)(t_now.QuadPart - t1.QuadPart) / (float)freq.QuadPart;
        t1 = t_now;
        if (dt < 0.001f) dt = 0.001f;
        if (dt > 0.05f) dt = 0.05f;

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        InputState input = {0};


        if (g_keys[VK_LEFT] || g_keys['A']) input.move_x -= 1.0f;
        if (g_keys[VK_RIGHT] || g_keys['D']) input.move_x += 1.0f;
        if (g_keys[VK_UP] || g_keys['W']) input.move_y -= 1.0f;
        if (g_keys[VK_DOWN] || g_keys['S']) input.move_y += 1.0f;


        input.cam_preset_pressed = (g_keys['C'] && !g_prev_keys['C']) || (g_keys['V'] && !g_prev_keys['V']);


        float cyaw = 0.0f;
        if (g_keys['Q'] || g_keys[VK_OEM_4] || g_keys[VK_NUMPAD4]) cyaw -= 1.0f;
        if (g_keys['E'] || g_keys[VK_OEM_6] || g_keys[VK_NUMPAD6]) cyaw += 1.0f;


        float ctilt = 0.0f;
        if (g_keys[VK_PRIOR] || g_keys['I'] || g_keys[VK_NUMPAD8]) ctilt += 1.0f;
        if (g_keys[VK_NEXT]  || g_keys['K'] || g_keys[VK_NUMPAD2]) ctilt -= 1.0f;


        float czoom = 0.0f;
        if (g_keys[VK_OEM_PLUS] || g_keys[VK_ADD] || g_keys['U']) czoom += 1.0f;
        if (g_keys[VK_OEM_MINUS] || g_keys[VK_SUBTRACT] || g_keys['O']) czoom -= 1.0f;


        if (g_mouse_wheel_delta != 0) {
            czoom += (g_mouse_wheel_delta > 0) ? 2.5f : -2.5f;
            g_mouse_wheel_delta = 0;
        }
        if (fabsf(g_mouse_yaw_delta) > 0.001f) {
            cyaw += g_mouse_yaw_delta;
            g_mouse_yaw_delta = 0.0f;
        }
        if (fabsf(g_mouse_pitch_delta) > 0.001f) {
            ctilt += g_mouse_pitch_delta;
            g_mouse_pitch_delta = 0.0f;
        }

        input.cam_yaw = cyaw;
        input.cam_tilt = ctilt;
        input.cam_zoom = czoom;


        input.touch_pressed = g_lmb_clicked;
        g_lmb_clicked = false;
        input.touch_x = g_lmb_x;
        input.touch_y = g_lmb_y;


        bool cur_conf = g_keys[VK_RETURN] || g_keys[VK_SPACE] || g_keys['Z'] || g_keys['X'] || g_keys['J'];
        bool prev_conf = g_prev_keys[VK_RETURN] || g_prev_keys[VK_SPACE] || g_prev_keys['Z'] || g_prev_keys['X'] || g_prev_keys['J'];
        input.btn_confirm = cur_conf;
        input.btn_confirm_pressed = cur_conf && !prev_conf;


        bool cur_canc = g_keys[VK_ESCAPE] || g_keys[VK_BACK];
        bool prev_canc = g_prev_keys[VK_ESCAPE] || g_prev_keys[VK_BACK];
        input.btn_cancel = cur_canc;
        input.btn_cancel_pressed = cur_canc && !prev_canc;


        bool cur_sq = g_keys['R'] || g_keys['T'];
        bool prev_sq = g_prev_keys['R'] || g_prev_keys['T'];
        input.btn_square = cur_sq;
        input.btn_square_pressed = cur_sq && !prev_sq;


        input.btn_skill_pressed[0] = (g_keys[VK_SHIFT] || g_keys['F'] || g_keys['1']) && !(g_prev_keys[VK_SHIFT] || g_prev_keys['F'] || g_prev_keys['1']);
        input.btn_skill_pressed[1] = g_keys['2'] && !g_prev_keys['2'];
        input.btn_skill_pressed[2] = g_keys['3'] && !g_prev_keys['3'];
        input.btn_skill_pressed[3] = g_keys['4'] && !g_prev_keys['4'];
        bool cur_tr = g_keys[VK_TAB] || g_keys['Y'] || g_keys[VK_SHIFT] || g_keys['F'] || g_keys['1'];
        bool prev_tr = g_prev_keys[VK_TAB] || g_prev_keys['Y'] || g_prev_keys[VK_SHIFT] || g_prev_keys['F'] || g_prev_keys['1'];
        input.btn_triangle = cur_tr;
        input.btn_triangle_pressed = cur_tr && !prev_tr;


        input.btn_pause = g_keys['P'] || g_keys[VK_PAUSE];
        input.btn_pause_pressed = input.btn_pause && !(g_prev_keys['P'] || g_prev_keys[VK_PAUSE]);


        input.btn_left_pressed  = (g_keys[VK_LEFT] || g_keys['A']) && !(g_prev_keys[VK_LEFT] || g_prev_keys['A']);
        input.btn_right_pressed = (g_keys[VK_RIGHT] || g_keys['D']) && !(g_prev_keys[VK_RIGHT] || g_prev_keys['D']);
        input.btn_up_pressed    = (g_keys[VK_UP] || g_keys['W']) && !(g_prev_keys[VK_UP] || g_prev_keys['W']);
        input.btn_down_pressed  = (g_keys[VK_DOWN] || g_keys['S']) && !(g_prev_keys[VK_DOWN] || g_prev_keys['S']);
        input.btn_l_pressed     = g_keys['Q'] && !g_prev_keys['Q'];
        input.btn_r_pressed     = g_keys['E'] && !g_prev_keys['E'];

        memcpy(g_prev_keys, g_keys, sizeof(g_keys));

        game_update(dt, &input);
        game_render();


        QueryPerformanceCounter(&t2);
        float elapsed = (float)(t2.QuadPart - t1.QuadPart) / (float)freq.QuadPart;
        while (elapsed < target_dt) {
            Sleep(1);
            QueryPerformanceCounter(&t2);
            elapsed = (float)(t2.QuadPart - t1.QuadPart) / (float)freq.QuadPart;
        }
    }

    audio_fini();
    render_fini();
    return 0;
}

#endif
