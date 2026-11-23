#include "g3d.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
extern HWND g_hwnd;
static HDC g_hdc = NULL;
static HGLRC g_hglrc = NULL;
#else
#include <vitasdk.h>
#include <vitaGL.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

typedef struct { float x, y, z, u, v; uint32_t c; } Vtx;
#define MAX_QUADS 4096
static Vtx g_v[MAX_QUADS * 6];
static int g_nv = 0;
static GLuint g_cur_tex = 0xFFFFFFFF;
static int g_layer = -1;
static g3d_tex *g_white = NULL;
static g3d_tex *g_oval_tex = NULL;


#define G3D_CACHE_MAX_VTX  196608
#define G3D_CACHE_MAX_RUNS 64
typedef struct { int layer; GLuint tex; int first; int count; } G3dCacheRun;
static Vtx *g_cache_vtx = NULL;
static G3dCacheRun g_cache_runs[G3D_CACHE_MAX_RUNS];
static int g_cache_nv = 0, g_cache_nruns = 0;
static int g_cache_capturing = 0, g_cache_ok = 0;

static float g_vp[16], g_ortho[16];
static float g_eye[3] = {0, 500, 500};
static float g_right[3] = {1, 0, 0}, g_north[3] = {0, 0, -1};
static float g_tan_half = 0.36f;
static float g_f[3], g_r[3], g_u[3];
static float g_fog_on = 0, g_fog_near = 0, g_fog_far = 1, g_fog_min = 1, g_fog_near_sq = 0, g_fog_inv_span = 1;

#ifndef _WIN32

static int    g_use_shaders = 0;
static GLuint g_prog_world_opaque = 0, g_prog_world_alpha = 0, g_prog_ui = 0;
static GLuint g_cur_prog = 0;
static GLint  g_loc_wo_mvp = -1, g_loc_wo_fog = -1, g_loc_wo_grade = -1;
static GLint  g_loc_wa_mvp = -1, g_loc_wa_fog = -1, g_loc_wa_grade = -1, g_loc_wa_acut = -1;
static GLint  g_loc_ui_mvp = -1;
static GLuint g_cache_vbo = 0;
static int    g_cache_vbo_ready = 0;

static GLuint g3d_compile_shader(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    if (!sh) return 0;
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(sh); return 0; }
    return sh;
}

static GLuint g3d_link_program(const char *vs_src, const char *fs_src) {
    GLuint vs = g3d_compile_shader(GL_VERTEX_SHADER, vs_src);
    if (!vs) return 0;
    GLuint fs = g3d_compile_shader(GL_FRAGMENT_SHADER, fs_src);
    if (!fs) { glDeleteShader(vs); return 0; }
    GLuint prog = glCreateProgram();
    if (!prog) { glDeleteShader(vs); glDeleteShader(fs); return 0; }
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "a_pos");
    glBindAttribLocation(prog, 1, "a_uv");
    glBindAttribLocation(prog, 2, "a_col");
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) { glDeleteProgram(prog); return 0; }
    return prog;
}

static void g3d_init_vita_shaders(void) {
    static const char *VS_WORLD =
        "attribute vec3 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "attribute vec4 a_col;\n"
        "uniform mat4 u_mvp;\n"
        "uniform vec4 u_fog;\n"
        "uniform vec4 u_grade;\n"
        "varying vec2 v_uv;\n"
        "varying vec4 v_col;\n"
        "void main() {\n"
        "    gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
        "    v_uv = a_uv;\n"
        "    vec3 rgb = a_col.rgb;\n"
        "    if (u_grade.x > 0.5) {\n"
        "        vec2 d = a_pos.xz - u_fog.xy;\n"
        "        float dist = length(d);\n"
        "        float t = clamp((dist - u_fog.z) * u_fog.w, 0.0, 1.0);\n"
        "        float m = 1.0 - t * (1.0 - u_grade.y);\n"
        "        rgb *= vec3(m + 0.06 * t, m - 0.01 * t, m - 0.12 * t);\n"
        "    }\n"
        "    if (u_grade.z > 0.5) {\n"
        "        rgb = min(rgb * vec3(1.0390625, 0.9609375, 0.828125), vec3(1.0));\n"
        "    }\n"
        "    v_col = vec4(rgb, a_col.a);\n"
        "}\n";

    static const char *FS_WORLD_OPAQUE =
        "precision lowp float;\n"
        "uniform sampler2D u_tex;\n"
        "varying vec2 v_uv;\n"
        "varying vec4 v_col;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(u_tex, v_uv) * v_col;\n"
        "}\n";

    static const char *FS_WORLD_ALPHA =
        "precision lowp float;\n"
        "uniform sampler2D u_tex;\n"
        "uniform float u_alpha_cut;\n"
        "varying vec2 v_uv;\n"
        "varying vec4 v_col;\n"
        "void main() {\n"
        "    vec4 c = texture2D(u_tex, v_uv) * v_col;\n"
        "    if (c.a <= u_alpha_cut) discard;\n"
        "    gl_FragColor = c;\n"
        "}\n";

    static const char *VS_UI =
        "attribute vec3 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "attribute vec4 a_col;\n"
        "uniform mat4 u_mvp;\n"
        "varying vec2 v_uv;\n"
        "varying vec4 v_col;\n"
        "void main() {\n"
        "    gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
        "    v_uv = a_uv;\n"
        "    v_col = a_col;\n"
        "}\n";

    g_prog_world_opaque = g3d_link_program(VS_WORLD, FS_WORLD_OPAQUE);
    g_prog_world_alpha  = g3d_link_program(VS_WORLD, FS_WORLD_ALPHA);
    g_prog_ui           = g3d_link_program(VS_UI,    FS_WORLD_OPAQUE);

    if (g_prog_world_opaque && g_prog_world_alpha && g_prog_ui) {
        g_loc_wo_mvp   = glGetUniformLocation(g_prog_world_opaque, "u_mvp");
        g_loc_wo_fog   = glGetUniformLocation(g_prog_world_opaque, "u_fog");
        g_loc_wo_grade = glGetUniformLocation(g_prog_world_opaque, "u_grade");
        glUseProgram(g_prog_world_opaque);
        GLint t0 = glGetUniformLocation(g_prog_world_opaque, "u_tex");
        if (t0 >= 0) glUniform1i(t0, 0);

        g_loc_wa_mvp   = glGetUniformLocation(g_prog_world_alpha, "u_mvp");
        g_loc_wa_fog   = glGetUniformLocation(g_prog_world_alpha, "u_fog");
        g_loc_wa_grade = glGetUniformLocation(g_prog_world_alpha, "u_grade");
        g_loc_wa_acut  = glGetUniformLocation(g_prog_world_alpha, "u_alpha_cut");
        glUseProgram(g_prog_world_alpha);
        GLint t1 = glGetUniformLocation(g_prog_world_alpha, "u_tex");
        if (t1 >= 0) glUniform1i(t1, 0);

        g_loc_ui_mvp = glGetUniformLocation(g_prog_ui, "u_mvp");
        glUseProgram(g_prog_ui);
        GLint t2 = glGetUniformLocation(g_prog_ui, "u_tex");
        if (t2 >= 0) glUniform1i(t2, 0);

        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glUseProgram(0);
        g_cur_prog = 0;
        g_use_shaders = 1;
    } else {
        if (g_prog_world_opaque) { glDeleteProgram(g_prog_world_opaque); g_prog_world_opaque = 0; }
        if (g_prog_world_alpha)  { glDeleteProgram(g_prog_world_alpha);  g_prog_world_alpha = 0; }
        if (g_prog_ui)           { glDeleteProgram(g_prog_ui);           g_prog_ui = 0; }
        g_use_shaders = 0;
    }
    glGenBuffers(1, &g_cache_vbo);
}

static void g3d_upload_world_uniforms(GLuint prog, int is_shadow) {
    if (prog == g_prog_world_opaque) {
        if (g_loc_wo_mvp >= 0)   glUniformMatrix4fv(g_loc_wo_mvp, 1, GL_FALSE, g_vp);
        if (g_loc_wo_fog >= 0)   glUniform4f(g_loc_wo_fog, g_eye[0], g_eye[2], g_fog_near, g_fog_inv_span);
        if (g_loc_wo_grade >= 0) glUniform4f(g_loc_wo_grade, g_fog_on, g_fog_min, is_shadow ? 0.0f : 1.0f, 0.0f);
    } else if (prog == g_prog_world_alpha) {
        if (g_loc_wa_mvp >= 0)   glUniformMatrix4fv(g_loc_wa_mvp, 1, GL_FALSE, g_vp);
        if (g_loc_wa_fog >= 0)   glUniform4f(g_loc_wa_fog, g_eye[0], g_eye[2], g_fog_near, g_fog_inv_span);
        if (g_loc_wa_grade >= 0) glUniform4f(g_loc_wa_grade, g_fog_on, g_fog_min, is_shadow ? 0.0f : 1.0f, 0.0f);
    }
}
#endif

static void flush(void) {
    if (!g_nv) return;
    if (g_cache_capturing && g_cache_ok && g_cache_vtx) {
        if (g_cache_nv + g_nv <= G3D_CACHE_MAX_VTX) {
            int merged = 0;
            if (g_cache_nruns > 0) {
                G3dCacheRun *prev = &g_cache_runs[g_cache_nruns - 1];
                if (prev->layer == g_layer && prev->tex == g_cur_tex && prev->first + prev->count == g_cache_nv) {
                    memcpy(g_cache_vtx + g_cache_nv, g_v, (size_t)g_nv * sizeof(Vtx));
                    prev->count += g_nv;
                    g_cache_nv += g_nv;
                    merged = 1;
                }
            }
            if (!merged) {
                if (g_cache_nruns < G3D_CACHE_MAX_RUNS) {
                    G3dCacheRun *r = &g_cache_runs[g_cache_nruns++];
                    r->layer = g_layer; r->tex = g_cur_tex; r->first = g_cache_nv; r->count = g_nv;
                    memcpy(g_cache_vtx + g_cache_nv, g_v, (size_t)g_nv * sizeof(Vtx));
                    g_cache_nv += g_nv;
                } else {
                    g_cache_ok = 0;
                }
            }
        } else {
            g_cache_ok = 0;
        }
    }
    glBindTexture(GL_TEXTURE_2D, g_cur_tex);
#ifndef _WIN32
    if (g_use_shaders) {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), &g_v[0].x);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), &g_v[0].u);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vtx), &g_v[0].c);
    } else
#endif
    {
        glVertexPointer(3, GL_FLOAT, sizeof(Vtx), &g_v[0].x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vtx), &g_v[0].u);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vtx), &g_v[0].c);
    }
    glDrawArrays(GL_TRIANGLES, 0, g_nv);
    g_nv = 0;
}

void g3d_cache_invalidate(void) {
    g_cache_ok = 0;
    g_cache_nv = 0;
    g_cache_nruns = 0;
#ifndef _WIN32
    g_cache_vbo_ready = 0;
#endif
}

void g3d_cache_begin(void) {
    flush();
    if (!g_cache_vtx) {
        g_cache_vtx = (Vtx *)malloc((size_t)G3D_CACHE_MAX_VTX * sizeof(Vtx));
    }
    g_cache_nv = 0;
    g_cache_nruns = 0;
    g_cache_ok = (g_cache_vtx != NULL);
    g_cache_capturing = g_cache_ok;
#ifndef _WIN32
    g_cache_vbo_ready = 0;
#endif
}

void g3d_cache_end(void) {
    flush();
    g_cache_capturing = 0;
#ifndef _WIN32
    if (g_cache_ok && g_cache_vtx && g_cache_nv > 0 && g_cache_vbo) {
        glBindBuffer(GL_ARRAY_BUFFER, g_cache_vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)g_cache_nv * sizeof(Vtx)), g_cache_vtx, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        g_cache_vbo_ready = 1;
    }
#endif
}

int g3d_cache_valid(void) {
    return g_cache_ok && g_cache_vtx != NULL && g_cache_nruns > 0;
}

void g3d_cache_replay(void) {
    if (!g3d_cache_valid()) return;
    flush();
    int was_cap = g_cache_capturing;
    g_cache_capturing = 0;
#ifndef _WIN32
    if (g_use_shaders && g_cache_vbo_ready && g_cache_vbo) {
        glBindBuffer(GL_ARRAY_BUFFER, g_cache_vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void *)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void *)12);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vtx), (const void *)20);
        for (int i = 0; i < g_cache_nruns; i++) {
            const G3dCacheRun *r = &g_cache_runs[i];
            g3d_layer(r->layer);
            if (r->tex != g_cur_tex) {
                glBindTexture(GL_TEXTURE_2D, r->tex);
                g_cur_tex = r->tex;
            }
            glDrawArrays(GL_TRIANGLES, r->first, r->count);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        g_cache_capturing = was_cap;
        return;
    } else if (g_use_shaders) {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), &g_cache_vtx[0].x);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), &g_cache_vtx[0].u);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vtx), &g_cache_vtx[0].c);
    } else
#endif
    {
        glVertexPointer(3, GL_FLOAT, sizeof(Vtx), &g_cache_vtx[0].x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vtx), &g_cache_vtx[0].u);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vtx), &g_cache_vtx[0].c);
    }
    for (int i = 0; i < g_cache_nruns; i++) {
        const G3dCacheRun *r = &g_cache_runs[i];
        g3d_layer(r->layer);
        if (r->tex != g_cur_tex) {
            glBindTexture(GL_TEXTURE_2D, r->tex);
            g_cur_tex = r->tex;
        }
        glDrawArrays(GL_TRIANGLES, r->first, r->count);
    }
    g_cache_capturing = was_cap;
}

static void use_tex(const g3d_tex *t) {
    if (!t) t = g_white;
    if (t && t->id != g_cur_tex) { flush(); g_cur_tex = t->id; }
    if (g_nv + 6 > MAX_QUADS * 6) flush();
}

static uint32_t fogc(float x, float z, uint32_t c) {
    if (g_layer == G3D_UI) return c;
    uint32_t r = c & 255u, g = (c >> 8) & 255u, b = (c >> 16) & 255u;
    {
        if (g_fog_on) {
            float dx = x - g_eye[0], dz = z - g_eye[2];
            float d2 = dx * dx + dz * dz;
            if (d2 > g_fog_near_sq) {
                float t = (sqrtf(d2) - g_fog_near) * g_fog_inv_span;
                if (t > 1.0f) t = 1.0f;

                float m = 1.0f - t * (1.0f - g_fog_min);
                r = (uint32_t)(r * (m + 0.06f * t));
                g = (uint32_t)(g * (m - 0.01f * t));
                b = (uint32_t)(b * (m - 0.12f * t));
            }
        }

        if (g_layer != G3D_SHADOW) {
            r = (r * 266u) >> 8;
            g = (g * 246u) >> 8;
            b = (b * 212u) >> 8;
            if (r > 255u) r = 255u;
            if (g > 255u) g = 255u;
            if (b > 255u) b = 255u;
        }
    }
    return (c & 0xFF000000u) | (b << 16) | (g << 8) | r;
}

static float g_lift = 0.0f;
static float (*g_lift_fn)(float x, float z) = 0;
static int   g_lift_z_shift = 1;
static float g_cam_lift = 0.0f;
static inline void put(float x, float y, float z, float u, float v, uint32_t c) {
    if (g_nv >= MAX_QUADS * 6) flush();
    Vtx *p = &g_v[g_nv++];
    if (g_layer != G3D_UI) { y += g_lift; if (g_lift_z_shift) z += g_lift; }
    p->x = x; p->y = y; p->z = z; p->u = u; p->v = v;
#ifndef _WIN32
    p->c = g_use_shaders ? c : fogc(x, z, c);
#else
    p->c = fogc(x, z, c);
#endif
}

static void mat_mul(float *o, const float *a, const float *b) {
    float r[16];
    for (int c = 0; c < 4; c++)
        for (int rr = 0; rr < 4; rr++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a[k * 4 + rr] * b[c * 4 + k];
            r[c * 4 + rr] = s;
        }
    memcpy(o, r, sizeof(r));
}

int g3d_init(void) {
#ifdef _WIN32
    if (!g_hwnd) return -1;
    g_hdc = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0, 0, 0, 0
    };
    int pf = ChoosePixelFormat(g_hdc, &pfd);
    SetPixelFormat(g_hdc, pf, &pfd);
    g_hglrc = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, g_hglrc);
#else
    vglSetupRuntimeShaderCompiler(SHARK_OPT_UNSAFE, 1, 1, 1);
    vglUseExtraMem(GL_TRUE);
    vglUseCachedMem(GL_TRUE);
    vglInitExtended(0, SCREEN_W_G, SCREEN_H_G, 0x1000000, SCE_GXM_MULTISAMPLE_NONE);
    vglWaitVblankStart(GL_TRUE);
#endif
    glClearColor(0.12f, 0.16f, 0.11f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glViewport(0, 0, SCREEN_W_G, SCREEN_H_G);
    memset(g_ortho, 0, sizeof(g_ortho));
    g_ortho[0] = 2.0f / SCREEN_W_G; g_ortho[5] = -2.0f / SCREEN_H_G; g_ortho[10] = -1.0f;
    g_ortho[12] = -1.0f; g_ortho[13] = 1.0f; g_ortho[15] = 1.0f;
    uint32_t w = 0xFFFFFFFFu;
    g_white = (g3d_tex *)malloc(sizeof(g3d_tex));
    g_white->w = g_white->h = 1;
    glGenTextures(1, &g_white->id);
    glBindTexture(GL_TEXTURE_2D, g_white->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &w);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    {
        unsigned char ov[32 * 32 * 4];
        for (int yy = 0; yy < 32; yy++) {
            float dy = ((float)yy + 0.5f - 16.0f) / 15.0f;
            for (int xx = 0; xx < 32; xx++) {
                float dx = ((float)xx + 0.5f - 16.0f) / 15.0f;
                float d = sqrtf(dx * dx + dy * dy);
                float a = 0.0f;
                if (d <= 0.68f) a = 1.0f;
                else if (d < 0.98f) {
                    float t = (0.98f - d) / 0.30f;
                    a = t * t * (3.0f - 2.0f * t);
                }
                unsigned char *p = &ov[(yy * 32 + xx) * 4];
                p[0] = 255; p[1] = 255; p[2] = 255; p[3] = (unsigned char)(a * 255.0f + 0.5f);
            }
        }
        g_oval_tex = (g3d_tex *)malloc(sizeof(g3d_tex));
        g_oval_tex->w = g_oval_tex->h = 32;
        glGenTextures(1, &g_oval_tex->id);
        glBindTexture(GL_TEXTURE_2D, g_oval_tex->id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, ov);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#ifndef _WIN32
    g3d_init_vita_shaders();
#endif
    g3d_camera(0, 0, 90.0f, 0, 1.0f, 40.0f);
    return 0;
}

void g3d_fini(void) {
    flush();
    if (g_white) {
        g3d_free_tex(g_white);
        g_white = NULL;
    }
    if (g_oval_tex) {
        g3d_free_tex(g_oval_tex);
        g_oval_tex = NULL;
    }
#ifdef _WIN32
    if (g_hglrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g_hglrc);
        g_hglrc = NULL;
    }
    if (g_hdc && g_hwnd) {
        ReleaseDC(g_hwnd, g_hdc);
        g_hdc = NULL;
    }
#else
    if (g_cache_vbo) { glDeleteBuffers(1, &g_cache_vbo); g_cache_vbo = 0; }
    if (g_prog_world_opaque) { glDeleteProgram(g_prog_world_opaque); g_prog_world_opaque = 0; }
    if (g_prog_world_alpha)  { glDeleteProgram(g_prog_world_alpha);  g_prog_world_alpha = 0; }
    if (g_prog_ui)           { glDeleteProgram(g_prog_ui);           g_prog_ui = 0; }
    g_use_shaders = 0;
#endif
}

void g3d_frame_begin(uint32_t c) {
    g_nv = 0; g_layer = -1; g_cur_tex = 0xFFFFFFFF;
#ifndef _WIN32
    g_cur_prog = 0;
#endif
#ifdef _WIN32
    if (g_hwnd) {
        RECT rc;
        GetClientRect(g_hwnd, &rc);
        int cw = rc.right - rc.left;
        int ch = rc.bottom - rc.top;
        if (cw > 0 && ch > 0) {
            glViewport(0, 0, cw, ch);
        }
    }
#endif
    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
    glClearColor((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    g3d_layer(G3D_UI);
}

void g3d_frame_end(void) {
    flush();
#ifdef _WIN32
    if (g_hdc) SwapBuffers(g_hdc);
#else
    vglSwapBuffers(GL_FALSE);
#endif
}

void g3d_set_cam_lift(float l) { g_cam_lift = l; }
void g3d_set_lift_fn(float (*fn)(float x, float z)) { g_lift_fn = fn; }
void g3d_set_lift_z_shift(int on) { g_lift_z_shift = on ? 1 : 0; }
int  g3d_get_lift_z_shift(void) { return g_lift_z_shift; }
static void lift_at(float x, float z) { g_lift = (g_lift_fn && g_layer != G3D_UI && g_layer != -99) ? g_lift_fn(x, z) : 0.0f; }

void g3d_set_nearest(const g3d_tex *t) {
    if (!t) return;
    flush();
    glBindTexture(GL_TEXTURE_2D, t->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    g_cur_tex = 0xFFFFFFFF;
}

g3d_tex *g3d_load_png(const char *path) {
    int w, h, n;
    unsigned char *px = stbi_load(path, &w, &h, &n, 4);
    if (!px) return NULL;
    g3d_tex *t = (g3d_tex *)malloc(sizeof(g3d_tex));
    if (!t) { stbi_image_free(px); return NULL; }
    t->w = w; t->h = h;
    flush();
    glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    GLint f = (w * h > 200000) ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_cur_tex = 0xFFFFFFFF;
    stbi_image_free(px);
    return t;
}

g3d_tex *g3d_tex_create_rgba(int w, int h) {
    g3d_tex *t = (g3d_tex *)malloc(sizeof(g3d_tex));
    if (!t) return NULL;
    unsigned char *z = (unsigned char *)calloc((size_t)w * (size_t)h, 4);
    if (!z) { free(t); return NULL; }
    t->w = w; t->h = h;
    flush();
    glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, z);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_cur_tex = 0xFFFFFFFF;
    free(z);
    return t;
}

void g3d_tex_update(g3d_tex *t, int x, int y, int w, int h, const unsigned char *rgba) {
    flush();
    glBindTexture(GL_TEXTURE_2D, t->id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    g_cur_tex = 0xFFFFFFFF;
}

g3d_tex *g3d_load_padded_atlas(const char *path, int tile_px, int pad_px) {
    int w, h, n;
    unsigned char *px = stbi_load(path, &w, &h, &n, 4);
    if (!px) return NULL;
    if (tile_px <= 0 || pad_px <= 0 || (w % tile_px) != 0 || (h % tile_px) != 0) {
        stbi_image_free(px);
        return g3d_load_png(path);
    }
    int cols = w / tile_px, rows = h / tile_px;
    int cell = tile_px + 2 * pad_px;
    int dw = cols * cell, dh = rows * cell;
    unsigned char *dst = (unsigned char *)calloc((size_t)dw * (size_t)dh * 4, 1);
    if (!dst) {
        stbi_image_free(px);
        return g3d_load_png(path);
    }
    for (int cy = 0; cy < rows; cy++) {
        for (int cx = 0; cx < cols; cx++) {
            for (int dy = 0; dy < cell; dy++) {
                int sy = dy - pad_px;
                if (sy < 0) sy = 0;
                if (sy >= tile_px) sy = tile_px - 1;
                int src_y = cy * tile_px + sy;
                int dst_y = cy * cell + dy;
                for (int dx = 0; dx < cell; dx++) {
                    int sx = dx - pad_px;
                    if (sx < 0) sx = 0;
                    if (sx >= tile_px) sx = tile_px - 1;
                    int src_x = cx * tile_px + sx;
                    int dst_x = cx * cell + dx;
                    const unsigned char *sp = px + ((size_t)src_y * (size_t)w + (size_t)src_x) * 4;
                    unsigned char *dp = dst + ((size_t)dst_y * (size_t)dw + (size_t)dst_x) * 4;
                    dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = sp[3];
                }
            }
        }
    }
    stbi_image_free(px);

    g3d_tex *t = (g3d_tex *)malloc(sizeof(g3d_tex));
    if (!t) { free(dst); return NULL; }
    t->w = dw; t->h = dh;
    flush();
    glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dw, dh, 0, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_cur_tex = 0xFFFFFFFF;
    free(dst);
    return t;
}

void g3d_free_tex(g3d_tex *t) {
    if (!t) return;
    flush();
    glDeleteTextures(1, &t->id);
    g_cur_tex = 0xFFFFFFFF;
    free(t);
}

void g3d_layer(int l) {
    if (l == g_layer) return;
    flush();
    int was_ui = (g_layer == G3D_UI || g_layer == -1);
    int is_ui = (l == G3D_UI);
    if (g_layer == G3D_SHADOW && l != G3D_SHADOW) {
        glDisable(GL_STENCIL_TEST);
    }
    g_layer = l;
#ifndef _WIN32
    if (g_use_shaders) {
        GLuint prog = (l == G3D_UI) ? g_prog_ui :
                      ((l == G3D_GROUND || l == G3D_GROUND_3D) ? g_prog_world_opaque : g_prog_world_alpha);
        if (prog != g_cur_prog) {
            glUseProgram(prog);
            g_cur_prog = prog;
        }
        if (l == G3D_UI) {
            if (g_loc_ui_mvp >= 0) glUniformMatrix4fv(g_loc_ui_mvp, 1, GL_FALSE, g_ortho);
        } else {
            g3d_upload_world_uniforms(prog, l == G3D_SHADOW);
            if (prog == g_prog_world_alpha && g_loc_wa_acut >= 0) {
                float acut = (l == G3D_SPRITE) ? 0.35f : (l == G3D_FX) ? 0.25f : (l == G3D_SHADOW) ? 0.15f : 0.12f;
                glUniform1f(g_loc_wa_acut, acut);
            }
        }
        switch (l) {
        case G3D_UI:
            glDisable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE); break;
        case G3D_GROUND:
            glDisable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE); break;
        case G3D_GROUND_3D:
            glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE); break;
        case G3D_DECAL:
            glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE); break;
        case G3D_FX:
            glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE); break;
        case G3D_SPRITE:
            glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE); break;
        case G3D_SHADOW:
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glEnable(GL_STENCIL_TEST);
            glClearStencil(0);
            glClear(GL_STENCIL_BUFFER_BIT);
            glStencilFunc(GL_EQUAL, 0, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
            break;
        }
        return;
    }
#endif
    if (was_ui != is_ui) {
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(is_ui ? g_ortho : g_vp);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }
    switch (l) {
    case G3D_UI:
        glDisable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE); glDisable(GL_ALPHA_TEST); break;
    case G3D_GROUND:
        glDisable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE); glDisable(GL_ALPHA_TEST); break;
    case G3D_GROUND_3D:
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE); glDisable(GL_ALPHA_TEST); break;
    case G3D_DECAL:
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
        glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.12f); break;
    case G3D_FX:
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
        glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.25f); break;
    case G3D_SPRITE:
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
        glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.35f); break;
    case G3D_SHADOW:
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_STENCIL_TEST);
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_EQUAL, 0, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
        glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.15f); break;
    }
}

void g3d_camera(float tx, float tz, float pitch_deg, float yaw_deg, float zoom, float fov_deg) {
    float ty = g_lift_fn ? g_lift_fn(tx, tz) : 0.0f; g_cam_lift = ty; if (g_lift_z_shift) tz += ty;
    float p = pitch_deg * 0.01745329252f, y = yaw_deg * 0.01745329252f, fv = fov_deg * 0.01745329252f;
    g_tan_half = tanf(fv * 0.5f);
    float dist = ((float)SCREEN_H_G * 0.5f) / g_tan_half / (zoom > 0.1f ? zoom : 0.1f);
    float bx = sinf(y), bz = cosf(y);
    g_eye[0] = tx + dist * bx * cosf(p);
    g_eye[1] = ty + dist * sinf(p);
    g_eye[2] = tz + dist * bz * cosf(p);
    g_right[0] = cosf(y); g_right[1] = 0; g_right[2] = -sinf(y);
    g_north[0] = -bx; g_north[1] = 0; g_north[2] = -bz;

    float f[3] = { tx - g_eye[0], ty - g_eye[1], tz - g_eye[2] };
    float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); f[0] /= fl; f[1] /= fl; f[2] /= fl;
    float r[3] = { -f[2], 0, f[0] };
    float rl = sqrtf(r[0] * r[0] + r[2] * r[2]); r[0] /= rl; r[2] /= rl;
    float u[3] = { r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0] };
    for (int i = 0; i < 3; i++) { g_f[i] = f[i]; g_r[i] = r[i]; g_u[i] = u[i]; }
    float V[16] = {0};
    V[0] = r[0]; V[4] = r[1]; V[8]  = r[2]; V[12] = -(r[0] * g_eye[0] + r[1] * g_eye[1] + r[2] * g_eye[2]);
    V[1] = u[0]; V[5] = u[1]; V[9]  = u[2]; V[13] = -(u[0] * g_eye[0] + u[1] * g_eye[1] + u[2] * g_eye[2]);
    V[2] = -f[0]; V[6] = -f[1]; V[10] = -f[2]; V[14] = (f[0] * g_eye[0] + f[1] * g_eye[1] + f[2] * g_eye[2]);
    V[15] = 1;
    float n = dist * 0.04f; if (n < 4) n = 4;
    float fa = dist + 9000.0f;
    float fc = 1.0f / g_tan_half;
    float P[16] = {0};
    P[0] = fc / ((float)SCREEN_W_G / SCREEN_H_G); P[5] = fc;
    P[10] = (fa + n) / (n - fa); P[11] = -1.0f; P[14] = 2.0f * fa * n / (n - fa);
    mat_mul(g_vp, P, V);
    if (g_layer != -1 && g_layer != G3D_UI) {
        flush();
#ifndef _WIN32
        if (g_use_shaders) {
            g3d_upload_world_uniforms(g_cur_prog, g_layer == G3D_SHADOW);
        } else
#endif
        {
            glMatrixMode(GL_PROJECTION); glLoadMatrixf(g_vp); glMatrixMode(GL_MODELVIEW);
        }
    }
}

void g3d_project(float x, float y, float z, float *sx, float *sy, float *scale) {
    const float *m = g_vp;
    float cx = m[0] * x + m[4] * y + m[8] * z + m[12];
    float cy = m[1] * x + m[5] * y + m[9] * z + m[13];
    float cw = m[3] * x + m[7] * y + m[11] * z + m[15];
    if (cw < 0.01f) { *sx = -1000; *sy = -1000; *scale = 0; return; }
    *sx = (cx / cw * 0.5f + 0.5f) * SCREEN_W_G;
    *sy = (0.5f - cy / cw * 0.5f) * SCREEN_H_G;
    *scale = ((float)SCREEN_H_G * 0.5f / g_tan_half) / cw;
}

void g3d_axes(float right[3], float up[3], float north[3]) {
    for (int i = 0; i < 3; i++) { right[i] = g_right[i]; north[i] = g_north[i]; }
    up[0] = 0; up[1] = 1; up[2] = 0;
}
void g3d_eye(float e[3]) { e[0] = g_eye[0]; e[1] = g_eye[1]; e[2] = g_eye[2]; }
void g3d_view_dir(float f[3]) { f[0] = g_f[0]; f[1] = g_f[1]; f[2] = g_f[2]; }
void g3d_fog(int on, float nd, float fd, float mn) {
    g_fog_on = (float)on; g_fog_near = nd; g_fog_far = fd > nd + 1 ? fd : nd + 1; g_fog_min = mn;
    g_fog_near_sq = nd > 0.0f ? nd * nd : 0.0f;
    g_fog_inv_span = 1.0f / (g_fog_far - g_fog_near);
#ifndef _WIN32
    if (g_use_shaders && g_layer != -1 && g_layer != G3D_UI) {
        flush();
        g3d_upload_world_uniforms(g_cur_prog, g_layer == G3D_SHADOW);
    }
#endif
}

void g3d_plane(const g3d_tex *t, const float c[3], const float A[3], const float B[3],
               float lx0, float ly0, float lx1, float ly1, float rot,
               float u0, float v0, float u1, float v1, uint32_t col) {
    use_tex(t);
    lift_at(c[0], c[2]);
    float cs = 1, sn = 0;
    if (rot != 0.0f) { cs = cosf(rot); sn = sinf(rot); }
    float lx[4] = { lx0, lx1, lx1, lx0 }, ly[4] = { ly0, ly0, ly1, ly1 };
    float uu[4] = { u0, u1, u1, u0 }, vv[4] = { v0, v0, v1, v1 };
    float px[4], py[4], pz[4];
    for (int i = 0; i < 4; i++) {
        float rx = lx[i] * cs - ly[i] * sn, ry = lx[i] * sn + ly[i] * cs;
        px[i] = c[0] + A[0] * rx + B[0] * ry;
        py[i] = c[1] + A[1] * rx + B[1] * ry;
        pz[i] = c[2] + A[2] * rx + B[2] * ry;
    }
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; k++) { int i = idx[k]; put(px[i], py[i], pz[i], uu[i], vv[i], col); }
}

void g3d_plane_abs(const g3d_tex *t, const float c[3], const float A[3], const float B[3],
                   float lx0, float ly0, float lx1, float ly1, float rot,
                   float u0, float v0, float u1, float v1, uint32_t col) {
    use_tex(t);
    g_lift = 0.0f;
    float cs = 1.0f, sn = 0.0f;
    if (rot != 0.0f) { cs = cosf(rot); sn = sinf(rot); }
    float lx[4] = { lx0, lx1, lx1, lx0 }, ly[4] = { ly0, ly0, ly1, ly1 };
    float uu[4] = { u0, u1, u1, u0 }, vv[4] = { v0, v0, v1, v1 };
    float px[4], py[4], pz[4];
    for (int i = 0; i < 4; i++) {
        float rx = lx[i] * cs - ly[i] * sn, ry = lx[i] * sn + ly[i] * cs;
        px[i] = c[0] + A[0] * rx + B[0] * ry;
        py[i] = c[1] + A[1] * rx + B[1] * ry;
        pz[i] = c[2] + A[2] * rx + B[2] * ry;
    }
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; k++) { int i = idx[k]; put(px[i], py[i], pz[i], uu[i], vv[i], col); }
}


#define G3D_SH_DX  0.36f
#define G3D_SH_DZ  0.72f
#define G3D_SH_LEN 0.80498447f
#define G3D_SH_SX  0.44721360f
#define G3D_SH_SZ  0.89442719f
#define G3D_SH_WX  0.89442719f
#define G3D_SH_WZ -0.44721360f
#define G3D_SUN_LX -0.32117f
#define G3D_SUN_LY  0.69587f
#define G3D_SUN_LZ -0.64235f

static inline uint32_t g3d_shade_col(uint32_t col, float mul) {
    uint32_t r = (uint32_t)((float)(col & 255u) * mul);
    uint32_t g = (uint32_t)((float)((col >> 8) & 255u) * mul);
    uint32_t b = (uint32_t)((float)((col >> 16) & 255u) * mul);
    if (r > 255u) r = 255u;
    if (g > 255u) g = 255u;
    if (b > 255u) b = 255u;
    return (col & 0xFF000000u) | (b << 16) | (g << 8) | r;
}

static void g3d_emit_lit_tri(const float p0[3], const float p1[3], const float p2[3],
                             float u0, float v0, float u1, float v1, float u2, float v2,
                             uint32_t col, int terrain_mode, const float ref_in[3]) {
    float e1x = p1[0] - p0[0], e1y = p1[1] - p0[1], e1z = p1[2] - p0[2];
    float e2x = p2[0] - p0[0], e2y = p2[1] - p0[1], e2z = p2[2] - p0[2];
    float nx = e1y * e2z - e1z * e2y;
    float ny = e1z * e2x - e1x * e2z;
    float nz = e1x * e2y - e1y * e2x;
    float nl = sqrtf(nx * nx + ny * ny + nz * nz);
    if (nl > 1e-6f) { nx /= nl; ny /= nl; nz /= nl; }
    else { nx = 0.0f; ny = 1.0f; nz = 0.0f; }
    if (terrain_mode) {
        if (ny < 0.0f) { nx = -nx; ny = -ny; nz = -nz; }
    } else if (ref_in) {
        float mx = (p0[0] + p1[0] + p2[0]) * 0.3333333f - ref_in[0];
        float my = (p0[1] + p1[1] + p2[1]) * 0.3333333f - ref_in[1];
        float mz = (p0[2] + p1[2] + p2[2]) * 0.3333333f - ref_in[2];
        if (nx * mx + ny * my + nz * mz < 0.0f) { nx = -nx; ny = -ny; nz = -nz; }
    }
    float ndotl = nx * G3D_SUN_LX + ny * G3D_SUN_LY + nz * G3D_SUN_LZ;
    float shade;
    if (terrain_mode) {

        shade = 0.56f + 0.44f * (ndotl / G3D_SUN_LY);
        if (shade < 0.56f) shade = 0.56f;
        if (shade > 1.10f) shade = 1.10f;
    } else {

        float dpos = ndotl > 0.0f ? ndotl : 0.0f;
        float upb  = ny > 0.0f ? ny : 0.0f;
        shade = 0.48f + 0.56f * dpos + 0.12f * upb;
        if (shade < 0.46f) shade = 0.46f;
        if (shade > 1.14f) shade = 1.14f;
    }
    uint32_t c = g3d_shade_col(col, shade);
    put(p0[0], p0[1], p0[2], u0, v0, c);
    put(p1[0], p1[1], p1[2], u1, v1, c);
    put(p2[0], p2[1], p2[2], u2, v2, c);
}

void g3d_quad_elev(const g3d_tex *t, float x0, float z0, float x1, float z1,
                   float y00, float y10, float y11, float y01,
                   float u0, float v0, float u1, float v1, uint32_t col) {
    use_tex(t);
    g_lift = 0.0f;
    float zs = g_lift_z_shift ? 1.0f : 0.0f;
    float p0[3] = { x0, y00, z0 + y00 * zs };
    float p1[3] = { x1, y10, z0 + y10 * zs };
    float p2[3] = { x1, y11, z1 + y11 * zs };
    float p3[3] = { x0, y01, z1 + y01 * zs };

    if (fabsf(y00 - y11) > fabsf(y10 - y01) + 0.1f) {
        g3d_emit_lit_tri(p0, p1, p3, u0, v0, u1, v0, u0, v1, col, 1, NULL);
        g3d_emit_lit_tri(p1, p2, p3, u1, v0, u1, v1, u0, v1, col, 1, NULL);
    } else {
        g3d_emit_lit_tri(p0, p1, p2, u0, v0, u1, v0, u1, v1, col, 1, NULL);
        g3d_emit_lit_tri(p0, p2, p3, u0, v0, u1, v1, u0, v1, col, 1, NULL);
    }
}

static void g3d_quad_elev_flow_x(const g3d_tex *t, float x0, float z0, float x1, float z1,
                                 float y00, float y10, float y11, float y01,
                                 float u0, float v0, float u1, float v1,
                                 float fu, uint32_t col) {
    if (fu <= 0.002f || fu >= 0.998f) {
        g3d_quad_elev(t, x0, z0, x1, z1, y00, y10, y11, y01, u0, v0, u1, v1, col);
        return;
    }
    float xm = x0 + (x1 - x0) * fu;
    float yt = y00 + (y10 - y00) * fu;
    float yb = y01 + (y11 - y01) * fu;
    float um = u1 - (u1 - u0) * fu;
    g3d_quad_elev(t, x0, z0, xm, z1, y00, yt, yb, y01, um, v0, u1, v1, col);
    g3d_quad_elev(t, xm, z0, x1, z1, yt, y10, y11, yb, u0, v0, um, v1, col);
}

void g3d_quad_elev_flow(const g3d_tex *t, float x0, float z0, float x1, float z1,
                        float y00, float y10, float y11, float y01,
                        float u0, float v0, float u1, float v1,
                        float flow_u, float flow_v, uint32_t col) {
    float fu = flow_u - floorf(flow_u);
    float fv = flow_v - floorf(flow_v);
    if (fv <= 0.002f || fv >= 0.998f) {
        g3d_quad_elev_flow_x(t, x0, z0, x1, z1, y00, y10, y11, y01, u0, v0, u1, v1, fu, col);
        return;
    }
    float zm = z0 + (z1 - z0) * fv;
    float yl = y00 + (y01 - y00) * fv;
    float yr = y10 + (y11 - y10) * fv;
    float vm = v1 - (v1 - v0) * fv;
    g3d_quad_elev_flow_x(t, x0, z0, x1, zm, y00, y10, yr, yl, u0, vm, u1, v1, fu, col);
    g3d_quad_elev_flow_x(t, x0, zm, x1, z1, yl, yr, y11, y01, u0, v0, u1, vm, fu, col);
}

void g3d_shadow_plane(const g3d_tex *t, float gx, float gz, float lift_h,
                      float lx0, float ly0, float lx1, float ly1, float rot,
                      float u0, float v0, float u1, float v1, uint32_t col) {
    use_tex(t);
    g_lift = 0.0f;
    float ox = gx + lift_h * G3D_SH_DX;
    float oz = gz + lift_h * G3D_SH_DZ;
    float cs = 1.0f, sn = 0.0f;
    if (rot != 0.0f) { cs = cosf(rot); sn = sinf(rot); }
    float lx[4] = { lx0, lx1, lx1, lx0 }, ly[4] = { ly0, ly0, ly1, ly1 };
    float uu[4] = { u0, u1, u1, u0 }, vv[4] = { v0, v0, v1, v1 };
    float px[4], py[4], pz[4];
    float zs = g_lift_z_shift ? 1.0f : 0.0f;

    float r_x = G3D_SH_WX;
    float r_z = G3D_SH_WZ;
    for (int i = 0; i < 4; i++) {
        float rx = lx[i] * cs - ly[i] * sn, ry = lx[i] * sn + ly[i] * cs;
        float wx = ox + r_x * rx - G3D_SH_DX * ry;
        float wz = oz + r_z * rx - G3D_SH_DZ * ry;
        float l = g_lift_fn ? g_lift_fn(wx, wz) : 0.0f;
        px[i] = wx;
        py[i] = l + 0.65f;
        pz[i] = wz + l * zs;
    }
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; k++) { int i = idx[k]; put(px[i], py[i], pz[i], uu[i], vv[i], col); }
}

void g3d_shadow_disc(float gx, float gz, float rx, float rz, float height, uint32_t col) {
    use_tex(g_white);
    g_lift = 0.0f;
    const int n = 12;
    if (g_nv + n * 3 > MAX_QUADS * 6) flush();
    float zs = g_lift_z_shift ? 1.0f : 0.0f;
    float cx = gx + height * 0.28f * G3D_SH_DX;
    float cz = gz + height * 0.28f * G3D_SH_DZ;
    float cl = g_lift_fn ? g_lift_fn(cx, cz) : 0.0f;
    float r_along = ((rx + rz) * 0.5f) + height * 0.34f;
    float r_perp  = ((rx + rz) * 0.5f) * 1.02f;
    float vx[12], vy[12], vz[12];
    for (int i = 0; i < n; i++) {
        float a = (float)i / (float)n * 6.2831853f;
        float ca = cosf(a) * r_along, sa = sinf(a) * r_perp;
        float wx = cx + G3D_SH_SX * ca + G3D_SH_WX * sa;
        float wz = cz + G3D_SH_SZ * ca + G3D_SH_WZ * sa;
        float l = g_lift_fn ? g_lift_fn(wx, wz) : 0.0f;
        vx[i] = wx; vy[i] = l + 0.65f; vz[i] = wz + l * zs;
    }
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        put(cx, cl + 0.65f, cz + cl * zs, 0.5f, 0.5f, col);
        put(vx[i], vy[i], vz[i], 0.5f, 0.5f, col);
        put(vx[j], vy[j], vz[j], 0.5f, 0.5f, col);
    }
}

static float g_border_w = 0.0f, g_border_h = 0.0f, g_border_m = 0.0f;
void g3d_set_border_bounds(float w, float h, float margin) {
    g_border_w = w; g_border_h = h; g_border_m = margin;
}

void g3d_shadow_oval(float gx, float gz, float rx, float rz, uint32_t col) {
    use_tex(g_oval_tex ? g_oval_tex : g_white);
    g_lift = 0.0f;
    float zs = g_lift_z_shift ? 1.0f : 0.0f;
    float cx = gx + rx * 0.25f * G3D_SH_DX;
    float cz_raw = gz + rx * 0.25f * G3D_SH_DZ;
    float cl = g_lift_fn ? g_lift_fn(cx, cz_raw) : 0.0f;
    float cy = cl + 0.65f;
    float cz = cz_raw + cl * zs;

    float r_along = (rx > rz ? rx : rz) * 1.08f;
    float r_perp  = (rx < rz ? rx : rz) * 1.00f;
    float sx = G3D_SH_SX * r_along, sz = G3D_SH_SZ * r_along;
    float wx = G3D_SH_WX * r_perp,  wz = G3D_SH_WZ * r_perp;
    float p0x = cx - sx - wx, p0z = cz - sz - wz;
    float p1x = cx + sx - wx, p1z = cz + sz - wz;
    float p2x = cx + sx + wx, p2z = cz + sz + wz;
    float p3x = cx - sx + wx, p3z = cz - sz + wz;
    put(p0x, cy, p0z, 0.0f, 0.0f, col);
    put(p1x, cy, p1z, 1.0f, 0.0f, col);
    put(p2x, cy, p2z, 1.0f, 1.0f, col);
    put(p0x, cy, p0z, 0.0f, 0.0f, col);
    put(p2x, cy, p2z, 1.0f, 1.0f, col);
    put(p3x, cy, p3z, 0.0f, 1.0f, col);
}

void g3d_shadow_oval_sun(float gx, float gz, float rx, float rz, float height, uint32_t col) {
    use_tex(g_oval_tex ? g_oval_tex : g_white);
    g_lift = 0.0f;
    float zs = g_lift_z_shift ? 1.0f : 0.0f;
    float cx = gx + height * 0.34f * G3D_SH_DX;
    float cz_raw = gz + height * 0.34f * G3D_SH_DZ;
    float cl = g_lift_fn ? g_lift_fn(cx, cz_raw) : 0.0f;
    float cy = cl + 0.65f;
    float cz = cz_raw + cl * zs;
    float r_along = rx + height * 0.24f;
    float r_perp  = rz;
    float sx = G3D_SH_SX * r_along, sz = G3D_SH_SZ * r_along;
    float wx = G3D_SH_WX * r_perp,  wz = G3D_SH_WZ * r_perp;
    float p0x = cx - sx - wx, p0z = cz - sz - wz;
    float p1x = cx + sx - wx, p1z = cz + sz - wz;
    float p2x = cx + sx + wx, p2z = cz + sz + wz;
    float p3x = cx - sx + wx, p3z = cz - sz + wz;
    put(p0x, cy, p0z, 0.0f, 0.0f, col);
    put(p1x, cy, p1z, 1.0f, 0.0f, col);
    put(p2x, cy, p2z, 1.0f, 1.0f, col);
    put(p0x, cy, p0z, 0.0f, 0.0f, col);
    put(p2x, cy, p2z, 1.0f, 1.0f, col);
    put(p3x, cy, p3z, 0.0f, 1.0f, col);
}

void g3d_rock_3d(const g3d_tex *t, float cx, float cz, float rx, float rz, float height,
                 float tu0, float tv0, float tu1, float tv1,
                 float su0, float sv0, float su1, float sv1,
                 uint32_t tint, uint32_t seed, int crystal) {
    use_tex(t);
    g_lift = 0.0f;
    const int N = 8;
    if (g_nv + (N * 5) * 3 > MAX_QUADS * 6) flush();
    float base_y = g_lift_fn ? g_lift_fn(cx, cz) : 0.0f;
    float zs = g_lift_z_shift ? 1.0f : 0.0f;
    float cz_w = cz + base_y * zs;
    float ref_in[3] = { cx, base_y - 4.0f, cz_w };

    float ring[3][8][3];
    float phase = (float)(seed & 255u) * (6.2831853f / 256.0f);
    float r_s1 = crystal ? 0.86f : 0.96f;
    float r_s2 = crystal ? 0.44f : 0.58f;
    float h_s1 = crystal ? 0.48f : 0.44f;
    float h_s2 = crystal ? 0.84f : 0.80f;

    for (int i = 0; i < N; i++) {
        uint32_t h1 = (seed ^ (uint32_t)(i * 2654435761u)) * 1664525u + 1013904223u;
        uint32_t h2 = h1 * 1664525u + 1013904223u;
        float rj = 0.86f + 0.26f * ((float)((h1 >> 16) & 255u) / 255.0f);
        float hj = 0.88f + 0.22f * ((float)((h2 >> 16) & 255u) / 255.0f);
        float a = phase + (float)i * (6.2831853f / (float)N);
        float ca = cosf(a), sa = sinf(a);

        ring[0][i][0] = cx + ca * rx * rj;
        ring[0][i][1] = base_y - 3.5f;
        ring[0][i][2] = cz_w + sa * rz * rj;

        ring[1][i][0] = cx + ca * rx * r_s1 * rj;
        ring[1][i][1] = base_y + height * h_s1 * hj;
        ring[1][i][2] = cz_w + sa * rz * r_s1 * rj;

        ring[2][i][0] = cx + ca * rx * r_s2 * rj;
        ring[2][i][1] = base_y + height * h_s2 * hj;
        ring[2][i][2] = cz_w + sa * rz * r_s2 * rj;
    }
    float apex[3] = {
        cx - rx * (crystal ? 0.04f : 0.10f),
        base_y + height,
        cz_w - rz * (crystal ? 0.04f : 0.08f)
    };
    float tum = 0.5f * (tu0 + tu1), tvm = 0.5f * (tv0 + tv1);
    for (int i = 0; i < N; i++) {
        int j = (i + 1) % N;

        g3d_emit_lit_tri(ring[0][i], ring[1][i], ring[1][j], su0, sv1, su0, sv0, su1, sv0, tint, 0, ref_in);
        g3d_emit_lit_tri(ring[0][i], ring[1][j], ring[0][j], su0, sv1, su1, sv0, su1, sv1, tint, 0, ref_in);

        g3d_emit_lit_tri(ring[1][i], ring[2][i], ring[2][j], tu0, tv1, tu0, tv0, tu1, tv0, tint, 0, ref_in);
        g3d_emit_lit_tri(ring[1][i], ring[2][j], ring[1][j], tu0, tv1, tu1, tv0, tu1, tv1, tint, 0, ref_in);

        g3d_emit_lit_tri(ring[2][i], apex, ring[2][j], tu0, tv1, tum, tvm, tu1, tv1, tint, 0, ref_in);
    }
}

void g3d_tree_base_3d(const g3d_tex *t, float cx, float by, float bz, float radius, float height,
                      float tu0, float tv0, float tu1, float tv1,
                      uint32_t bark_tint, uint32_t seed) {
    if (!t || radius < 1.0f || height < 2.0f) return;
    use_tex(t);
    g_lift = 0.0f;
    const int N = 4;
    if (g_nv + 24 > MAX_QUADS * 6) flush();

    float phase = (float)(seed & 255u) * (6.2831853f / 256.0f);
    int slender = (height > radius * 2.85f) ? 1 : 0;


    float bot[4][3], top[4][3];
    float trk_ref[3] = { cx, by + height * 0.16f, bz };
    float r_top = radius * (slender ? 0.30f : 0.28f);
    float r_bot = radius * (slender ? 0.48f : 0.50f);
    float y_bot = by - 0.8f;
    float y_top = by + height * 0.18f;

    for (int i = 0; i < N; i++) {
        float a = phase + (float)i * (6.2831853f / (float)N);
        float ca = cosf(a), sa = sinf(a);
        bot[i][0] = cx + ca * r_bot; bot[i][1] = y_bot; bot[i][2] = bz + sa * r_bot;
        top[i][0] = cx + ca * r_top; top[i][1] = y_top; top[i][2] = bz + sa * r_top;
    }
    for (int i = 0; i < N; i++) {
        int j = (i + 1) % N;
        g3d_emit_lit_tri(bot[i], top[i], top[j], tu0, tv1, tu0, tv0, tu1, tv0, bark_tint, 0, trk_ref);
        g3d_emit_lit_tri(bot[i], top[j], bot[j], tu0, tv1, tu1, tv0, tu1, tv1, bark_tint, 0, trk_ref);
    }
}

static void g3d_box_3d(const g3d_tex *t, float x0, float y0, float z0, float x1, float y1, float z1,
                       float u0, float v0, float u1, float v1, uint32_t col) {
    use_tex(t);
    g_lift = 0.0f;
    if (g_nv + 30 > MAX_QUADS * 6) flush();
    float ref[3] = { 0.5f * (x0 + x1), 0.5f * (y0 + y1), 0.5f * (z0 + z1) };
    float a[3] = { x0, y1, z0 }, b[3] = { x1, y1, z0 }, c[3] = { x1, y1, z1 }, d[3] = { x0, y1, z1 };
    float e[3] = { x0, y0, z0 }, f[3] = { x1, y0, z0 }, g[3] = { x1, y0, z1 }, h[3] = { x0, y0, z1 };

    g3d_emit_lit_tri(a, b, c, u0, v0, u1, v0, u1, v1, col, 0, ref);
    g3d_emit_lit_tri(a, c, d, u0, v0, u1, v1, u0, v1, col, 0, ref);

    g3d_emit_lit_tri(d, c, g, u0, v0, u1, v0, u1, v1, col, 0, ref);
    g3d_emit_lit_tri(d, g, h, u0, v0, u1, v1, u0, v1, col, 0, ref);

    g3d_emit_lit_tri(b, a, e, u0, v0, u1, v0, u1, v1, col, 0, ref);
    g3d_emit_lit_tri(b, e, f, u0, v0, u1, v1, u0, v1, col, 0, ref);

    g3d_emit_lit_tri(a, d, h, u0, v0, u1, v0, u1, v1, col, 0, ref);
    g3d_emit_lit_tri(a, h, e, u0, v0, u1, v1, u0, v1, col, 0, ref);

    g3d_emit_lit_tri(c, b, f, u0, v0, u1, v0, u1, v1, col, 0, ref);
    g3d_emit_lit_tri(c, f, g, u0, v0, u1, v1, u0, v1, col, 0, ref);
}

void g3d_bridge_3d(const g3d_tex *t, float x0, float z0, float x1, float z1, float deck_y, int dir,
                   float pu0, float pv0, float pu1, float pv1) {

    g3d_layer(G3D_DECAL);
    use_tex(g_white);
    g_lift = 0.0f;
    float sdx = deck_y * G3D_SH_DX, sdz = deck_y * G3D_SH_DZ;
    uint32_t shc = RGBA8(10, 22, 46, 84);
    put(x0 + sdx, 0.65f, z0 + sdz, 0.5f, 0.5f, shc);
    put(x1 + sdx, 0.65f, z0 + sdz, 0.5f, 0.5f, shc);
    put(x1 + sdx, 0.65f, z1 + sdz, 0.5f, 0.5f, shc);
    put(x0 + sdx, 0.65f, z0 + sdz, 0.5f, 0.5f, shc);
    put(x1 + sdx, 0.65f, z1 + sdz, 0.5f, 0.5f, shc);
    put(x0 + sdx, 0.65f, z1 + sdz, 0.5f, 0.5f, shc);


    g3d_layer(G3D_SPRITE);
    uint32_t col_pile = RGBA8(122, 80, 48, 255);
    uint32_t col_beam = RGBA8(146, 98, 58, 255);
    uint32_t col_post = RGBA8(162, 112, 66, 255);
    uint32_t col_rail = RGBA8(192, 142, 88, 255);
    float ox0 = (dir == 0) ? (x0 - 2.0f) : x0;
    float ox1 = (dir == 0) ? (x1 + 2.0f) : x1;
    float oz0 = (dir == 1) ? (z0 - 2.0f) : z0;
    float oz1 = (dir == 1) ? (z1 + 2.0f) : z1;


    g3d_box_3d(g_white, ox0, deck_y - 6.0f, oz0, ox1, deck_y - 0.35f, oz1, 0, 0, 1, 1, col_beam);


    use_tex(t);
    g_lift = 0.0f;
    int tx0 = (int)floorf(x0 / 32.0f + 0.01f), tx1 = (int)floorf(x1 / 32.0f + 0.01f);
    int tz0 = (int)floorf(z0 / 32.0f + 0.01f), tz1 = (int)floorf(z1 / 32.0f + 0.01f);
    for (int tz = tz0; tz < tz1; tz++) {
        for (int tx = tx0; tx < tx1; tx++) {
            float ax0 = (float)(tx * 32) - ((dir == 0 && tx == tx0) ? 2.0f : 0.0f);
            float ax1 = (float)((tx + 1) * 32) + ((dir == 0 && tx == tx1 - 1) ? 2.0f : 0.0f);
            float az0 = (float)(tz * 32) - ((dir == 1 && tz == tz0) ? 2.0f : 0.0f);
            float az1 = (float)((tz + 1) * 32) + ((dir == 1 && tz == tz1 - 1) ? 2.0f : 0.0f);
            if (g_nv + 6 > MAX_QUADS * 6) flush();
            put(ax0, deck_y, az0, pu0, pv0, 0xFFFFFFFFu);
            put(ax1, deck_y, az0, pu1, pv0, 0xFFFFFFFFu);
            put(ax1, deck_y, az1, pu1, pv1, 0xFFFFFFFFu);
            put(ax0, deck_y, az0, pu0, pv0, 0xFFFFFFFFu);
            put(ax1, deck_y, az1, pu1, pv1, 0xFFFFFFFFu);
            put(ax0, deck_y, az1, pu0, pv1, 0xFFFFFFFFu);
        }
    }

    if (dir == 0) {

        float span = x1 - x0;
        int n_posts = (int)floorf(span / 32.0f + 0.5f);
        if (n_posts < 2) n_posts = 2;
        for (int s = 0; s < 2; s++) {
            float rz = (s == 0) ? (z0 + 3.0f) : (z1 - 3.0f);

            g3d_box_3d(g_white, x0, deck_y + 9.0f, rz - 2.2f, x1, deck_y + 12.2f, rz + 2.2f, 0, 0, 1, 1, col_rail);
            g3d_box_3d(g_white, x0, deck_y + 4.2f, rz - 1.6f, x1, deck_y + 6.4f,  rz + 1.6f, 0, 0, 1, 1, col_beam);

            for (int k = 0; k <= n_posts; k++) {
                float px = x0 + (float)k * (span / (float)n_posts);
                if (k == 0) px += 3.0f; else if (k == n_posts) px -= 3.0f;
                g3d_box_3d(g_white, px - 3.0f, deck_y, rz - 3.0f, px + 3.0f, deck_y + 13.5f, rz + 3.0f, 0, 0, 1, 1, col_post);
                if (px >= x0 + 36.0f && px <= x1 - 36.0f && (k == 2 || k == n_posts - 2 || (k % 2 == 0))) {
                    g3d_box_3d(g_white, px - 5.0f, -4.0f, rz - 5.0f, px + 5.0f, deck_y - 5.5f, rz + 5.0f, 0, 0, 1, 1, col_pile);
                }
            }
        }
    } else {

        float span = z1 - z0;
        int n_posts = (int)floorf(span / 32.0f + 0.5f);
        if (n_posts < 2) n_posts = 2;
        for (int s = 0; s < 2; s++) {
            float rx = (s == 0) ? (x0 + 3.0f) : (x1 - 3.0f);
            g3d_box_3d(g_white, rx - 2.2f, deck_y + 9.0f, z0, rx + 2.2f, deck_y + 12.2f, z1, 0, 0, 1, 1, col_rail);
            g3d_box_3d(g_white, rx - 1.6f, deck_y + 4.2f, z0, rx + 1.6f, deck_y + 6.4f,  z1, 0, 0, 1, 1, col_beam);
            for (int k = 0; k <= n_posts; k++) {
                float pz = z0 + (float)k * (span / (float)n_posts);
                if (k == 0) pz += 3.0f; else if (k == n_posts) pz -= 3.0f;
                g3d_box_3d(g_white, rx - 3.0f, deck_y, pz - 3.0f, rx + 3.0f, deck_y + 13.5f, pz + 3.0f, 0, 0, 1, 1, col_post);
                if (pz >= z0 + 36.0f && pz <= z1 - 36.0f && (k == 2 || k == n_posts - 2 || (k % 2 == 0))) {
                    g3d_box_3d(g_white, rx - 5.0f, -4.0f, pz - 5.0f, rx + 5.0f, deck_y - 5.5f, pz + 5.0f, 0, 0, 1, 1, col_pile);
                }
            }
        }
    }
}

void g3d_tris(const g3d_tex *t, const void *verts, int n, float s) {
    const unsigned char *p = (const unsigned char *)verts;
    use_tex(t);
    g_lift = 0.0f;
    for (int i = 0; i < n; i++, p += 24) {
        float f[5]; memcpy(f, p, 20);
        uint32_t sh = p[20];
        uint32_t sr = sh, sg = (sh * 244u + 11u * 200u) >> 8, sb = (sh * 224u + 31u * 170u) >> 8;
        put(f[0] * s, f[1] * s, f[2] * s, f[3], f[4], RGBA8(sr, sg, sb, 255));
    }
}

void g3d_tris_split(const g3d_tex *t, const void *verts, int n, float s, int mode) {
    if (n < 3) return;
    const unsigned char *p = (const unsigned char *)verts;
    use_tex(t);
    g_lift = 0.0f;
    float base_y = 1e9f, min_z = 1e9f;
    for (int i = 0; i < n; i++) {
        float yv, zv;
        memcpy(&yv, p + (size_t)i * 24 + 4, 4);
        memcpy(&zv, p + (size_t)i * 24 + 8, 4);
        yv *= s; zv *= s;
        if (yv < base_y) base_y = yv;
        if (zv < min_z) min_z = zv;
    }
    if (base_y > 1e8f) base_y = 0.0f;
    if (min_z > 1e8f) min_z = 0.0f;
    for (int i = 0; i + 3 <= n; i += 3, p += 72) {
        float f0[5], f1[5], f2[5];
        memcpy(f0, p, 20); memcpy(f1, p + 24, 20); memcpy(f2, p + 48, 20);
        float h0 = f0[1] * s - base_y, h1 = f1[1] * s - base_y, h2 = f2[1] * s - base_y;
        float hmax = h0 > h1 ? (h0 > h2 ? h0 : h2) : (h1 > h2 ? h1 : h2);
        float hmin = h0 < h1 ? (h0 < h2 ? h0 : h2) : (h1 < h2 ? h1 : h2);
        float zmax = f0[2] > f1[2] ? (f0[2] > f2[2] ? f0[2] : f2[2]) : (f1[2] > f2[2] ? f1[2] : f2[2]);
        zmax *= s;

        if (hmax >= 0.22f && hmax <= 0.42f && hmin >= 0.22f) continue;
        if (hmax < 0.95f && zmax <= min_z + 38.0f) continue;
        int is_flat = (hmax < 0.8f);
        if ((mode == 0 && !is_flat) || (mode != 0 && is_flat)) continue;
        const float *fv[3] = { f0, f1, f2 };
        for (int k = 0; k < 3; k++) {
            uint32_t sh = p[k * 24 + 20];
            uint32_t sr = sh, sg = (sh * 244u + 11u * 200u) >> 8, sb = (sh * 224u + 31u * 170u) >> 8;
            put(fv[k][0] * s, fv[k][1] * s, fv[k][2] * s, fv[k][3], fv[k][4], RGBA8(sr, sg, sb, 255));
        }
    }
}

void g3d_tris_bb(const g3d_tex *t, const void *verts, int n, float s, const float R[3], const float D[3], int pass) {
    const unsigned char *p = (const unsigned char *)verts;
    use_tex(t);
    g_lift = 0.0f;
    int i = 0;
    while (i < n) {
        uint8_t flag = p[21];
        if (flag >= 1 && i + 6 <= n) {
            float q[6][5];
            uint8_t sh[6];
            float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f, minu = 1e9f, maxv = -1e9f, minv = 1e9f;
            for (int k = 0; k < 6; k++) {
                memcpy(q[k], p + k * 24, 20);
                sh[k] = p[k * 24 + 20];
                if (q[k][0] < minx) minx = q[k][0];
                if (q[k][0] > maxx) maxx = q[k][0];
                if (q[k][1] < miny) miny = q[k][1];
                if (q[k][1] > maxy) maxy = q[k][1];
                if (q[k][3] < minu) minu = q[k][3];
                if (q[k][4] < minv) minv = q[k][4];
                if (q[k][4] > maxv) maxv = q[k][4];
            }
            float cx = 0.5f * (minx + maxx) * s;
            float by = miny * s;
            float bz = q[2][2] * s;
            float tw = (maxx - minx) * s;
            float th = (maxy - miny) * s;
            if (flag == 1) {
                int is_border = (g_border_m > 0.0f &&
                                 (cx < g_border_m || cx > g_border_w - g_border_m ||
                                  bz < g_border_m || bz > g_border_h - g_border_m));
                float rad = tw * 0.44f;
                float h3d = th * 0.96f;
                if (pass == 3) {
                    if (!is_border) {


                        float tu0 = 10.0f / 128.0f, tv0 = 29.0f / 128.0f;
                        float tu1 = 22.0f / 128.0f, tv1 = 34.0f / 128.0f;
                        uint32_t brk_tint = RGBA8(255, 242, 224, 255);
                        uint32_t seed = (uint32_t)((int)(cx * 0.25f) * 73856093u) ^ (uint32_t)((int)(bz * 0.25f) * 19349663u);
                        g3d_tree_base_3d(t, cx, by, bz, rad, h3d, tu0, tv0, tu1, tv1, brk_tint, seed);
                    }
                } else if (pass == 2) {
                    if (is_border) {

                        use_tex(t);
                        g_lift = 0.0f;
                        for (int k = 0; k < 6; k++) {
                            float lx = q[k][0] * s - cx;
                            float ly = q[k][1] * s - by;
                            float wx = cx + lx * R[0] - ly * D[0];
                            float wy = by + lx * R[1] - ly * D[1];
                            float wz = bz + lx * R[2] - ly * D[2];
                            uint32_t c = sh[k];
                            uint32_t cr = c, cg = (c * 248u) >> 8, cb = (c * 226u) >> 8;
                            put(wx, wy, wz, q[k][3], q[k][4], RGBA8(cr, cg, cb, 255));
                        }
                    } else {


                        use_tex(t);
                        g_lift = 0.0f;
                        float v_crop = (minu < 0.125f) ? (29.0f / 128.0f) : (56.0f / 128.0f);
                        float v_span = (maxv - minv) > 1e-5f ? (maxv - minv) : 1.0f;
                        float bot_frac = (maxv - v_crop) / v_span;
                        if (bot_frac < 0.0f) bot_frac = 0.0f;
                        if (bot_frac > 0.45f) bot_frac = 0.45f;
                        float ly_floor = th * bot_frac;
                        float can_by = by + ly_floor * 0.54f;
                        float bz_can = bz;
                        for (int k = 0; k < 6; k++) {
                            float lx = q[k][0] * s - cx;
                            float ly = q[k][1] * s - by;
                            float vv = q[k][4];
                            if (ly < ly_floor) { ly = ly_floor; vv = v_crop; }
                            float dy_c = ly - ly_floor;
                            float wx = cx + lx * R[0] - dy_c * D[0];
                            float wy = can_by + lx * R[1] - dy_c * D[1];
                            float wz = bz_can + lx * R[2] - dy_c * D[2];
                            uint32_t c = sh[k];
                            uint32_t cr = c, cg = (c * 248u) >> 8, cb = (c * 226u) >> 8;
                            put(wx, wy, wz, q[k][3], vv, RGBA8(cr, cg, cb, 255));
                        }
                    }
                } else if (pass == 1) {

                    g3d_shadow_oval_sun(cx, bz, tw * 0.36f, tw * 0.30f, th * 0.72f, RGBA8(16, 26, 40, 92));
                }
            } else if (pass == 2) {

                use_tex(t);
                float sink = 1.0f;
                for (int k = 0; k < 6; k++) {
                    float lx = q[k][0] * s - cx;
                    float ly = q[k][1] * s - by;
                    float wx = cx + lx * R[0] - ly * D[0];
                    float wy = (by - sink) + lx * R[1] - ly * D[1];
                    float wz = bz + lx * R[2] - ly * D[2];
                    uint32_t c = sh[k];
                    uint32_t cr = c, cg = (c * 248u) >> 8, cb = (c * 226u) >> 8;
                    put(wx, wy, wz, q[k][3], q[k][4], RGBA8(cr, cg, cb, 255));
                }
            }
            i += 6;
            p += 6 * 24;
        } else {
            if (pass == 0) {
                use_tex(t);
                float f[5]; memcpy(f, p, 20);
                uint32_t sh = p[20];
                put(f[0] * s, f[1] * s, f[2] * s, f[3], f[4], RGBA8(sh, sh, sh, 255));
            }
            i += 1;
            p += 24;
        }
    }
}

void g3d_tris_shadow(const g3d_tex *t, const void *verts, int n, float s, float sx_dir, float sz_dir, uint32_t col) {
    if (n < 3) return;
    const unsigned char *p = (const unsigned char *)verts;
    use_tex(t);
    g_lift = 0.0f;

    float base_y = 1e9f;
    for (int i = 0; i < n; i++) {
        float yv; memcpy(&yv, p + (size_t)i * 24 + 4, 4);
        yv *= s;
        if (yv < base_y) base_y = yv;
    }
    if (base_y > 1e8f) base_y = 0.0f;
    for (int i = 0; i + 3 <= n; i += 3, p += 72) {
        float f0[5], f1[5], f2[5];
        memcpy(f0, p, 20); memcpy(f1, p + 24, 20); memcpy(f2, p + 48, 20);
        float h0 = f0[1] * s - base_y, h1 = f1[1] * s - base_y, h2 = f2[1] * s - base_y;
        if (h0 < 0.5f && h1 < 0.5f && h2 < 0.5f) continue;
        if (h0 < 0.0f) h0 = 0.0f;
        if (h1 < 0.0f) h1 = 0.0f;
        if (h2 < 0.0f) h2 = 0.0f;
        float wy = base_y + 0.14f;
        put(f0[0] * s + h0 * sx_dir, wy, f0[2] * s + h0 * sz_dir, f0[3], f0[4], col);
        put(f1[0] * s + h1 * sx_dir, wy, f1[2] * s + h1 * sz_dir, f1[3], f1[4], col);
        put(f2[0] * s + h2 * sx_dir, wy, f2[2] * s + h2 * sz_dir, f2[3], f2[4], col);
    }
}

void g3d_disc(const float c[3], const float A[3], const float B[3], float radius, uint32_t col) {
    use_tex(g_white);
    lift_at(c[0], c[2]);
    int n = radius < 4 ? 8 : (radius < 14 ? 12 : 20);
    if (g_nv + n * 3 > MAX_QUADS * 6) flush();
    float pa = 0;
    for (int i = 0; i < n; i++) {
        float a0 = pa, a1 = (float)(i + 1) / n * 6.2831853f;
        float x0 = cosf(a0) * radius, y0 = sinf(a0) * radius, x1 = cosf(a1) * radius, y1 = sinf(a1) * radius;
        put(c[0], c[1], c[2], 0.5f, 0.5f, col);
        put(c[0] + A[0] * x0 + B[0] * y0, c[1] + A[1] * x0 + B[1] * y0, c[2] + A[2] * x0 + B[2] * y0, 0.5f, 0.5f, col);
        put(c[0] + A[0] * x1 + B[0] * y1, c[1] + A[1] * x1 + B[1] * y1, c[2] + A[2] * x1 + B[2] * y1, 0.5f, 0.5f, col);
        pa = a1;
    }
}

void g3d_ribbon(const float p1[3], const float p2[3], const float perp[3], float width, uint32_t col) {
    use_tex(g_white);
    lift_at(p1[0], p1[2]);
    float h = width * 0.5f;
    float a[3] = { p1[0] + perp[0] * h, p1[1] + perp[1] * h, p1[2] + perp[2] * h };
    float b[3] = { p1[0] - perp[0] * h, p1[1] - perp[1] * h, p1[2] - perp[2] * h };
    float c[3] = { p2[0] - perp[0] * h, p2[1] - perp[1] * h, p2[2] - perp[2] * h };
    float d[3] = { p2[0] + perp[0] * h, p2[1] + perp[1] * h, p2[2] + perp[2] * h };
    put(a[0], a[1], a[2], 0.5f, 0.5f, col); put(b[0], b[1], b[2], 0.5f, 0.5f, col); put(c[0], c[1], c[2], 0.5f, 0.5f, col);
    put(a[0], a[1], a[2], 0.5f, 0.5f, col); put(c[0], c[1], c[2], 0.5f, 0.5f, col); put(d[0], d[1], d[2], 0.5f, 0.5f, col);
}

static const float UX[3] = {1, 0, 0}, UY[3] = {0, 1, 0};

void g3d_ui_rect(float x, float y, float w, float h, uint32_t col) {
    g3d_layer(G3D_UI);
    float c[3] = {x, y, 0};
    g3d_plane(g_white, c, UX, UY, 0, 0, w, h, 0, 0, 0, 1, 1, col);
}
void g3d_ui_circle(float x, float y, float r, uint32_t col) {
    g3d_layer(G3D_UI);
    float c[3] = {x, y, 0};
    g3d_disc(c, UX, UY, r, col);
}
void g3d_ui_line(float x1, float y1, float x2, float y2, float width, uint32_t col) {
    g3d_layer(G3D_UI);
    float dx = x2 - x1, dy = y2 - y1, l = sqrtf(dx * dx + dy * dy);
    if (l < 0.001f) return;
    float p1[3] = {x1, y1, 0}, p2[3] = {x2, y2, 0}, pp[3] = {-dy / l, dx / l, 0};
    g3d_ribbon(p1, p2, pp, width < 1.0f ? 1.0f : width, col);
}
void g3d_ui_tex(const g3d_tex *t, float x, float y, float sx0, float sy0, float sw, float sh,
                float scx, float scy, uint32_t tint) {
    if (!t || t->w <= 0 || t->h <= 0) return;
    g3d_layer(G3D_UI);
    float c[3] = {x, y, 0};
    g3d_plane(t, c, UX, UY, 0, 0, sw * scx, sh * scy, 0,
              sx0 / t->w, sy0 / t->h, (sx0 + sw) / t->w, (sy0 + sh) / t->h, tint);
}
void g3d_ui_tex_rot(const g3d_tex *t, float x, float y, float scx, float scy, float rad,
                    float hx, float hy, uint32_t tint) {
    if (!t || t->w <= 0 || t->h <= 0) return;
    g3d_layer(G3D_UI);
    float c[3] = {x, y, 0};
    g3d_plane(t, c, UX, UY, -hx * scx, -hy * scy, ((float)t->w - hx) * scx, ((float)t->h - hy) * scy, rad,
              0, 0, 1, 1, tint);
}

void g3d_ground_hit(float sx, float sy, float out[2]) {
    float nx = (sx / SCREEN_W_G) * 2.0f - 1.0f, ny = 1.0f - (sy / SCREEN_H_G) * 2.0f;
    float asp = (float)SCREEN_W_G / SCREEN_H_G, d[3];
    for (int i = 0; i < 3; i++) d[i] = g_f[i] + g_r[i] * nx * g_tan_half * asp + g_u[i] * ny * g_tan_half;
    float hl = sqrtf(d[0] * d[0] + d[2] * d[2]);
    if (d[1] < -1e-4f) {
        float t = (g_cam_lift - g_eye[1]) / d[1];
        out[0] = g_eye[0] + d[0] * t; out[1] = g_eye[2] + d[2] * t - (g_lift_z_shift ? g_cam_lift : 0.0f);
    } else {
        float k = hl > 1e-6f ? 6000.0f / hl : 0.0f;
        out[0] = g_eye[0] + d[0] * k; out[1] = g_eye[2] + d[2] * k;
    }
}
g3d_tex *g3d_white(void) { return g_white; }
