

#ifndef G3D_H
#define G3D_H
#include <stdint.h>

#define SCREEN_W_G 960
#define SCREEN_H_G 544
#define RGBA8(r,g,b,a) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(g)<<8)|((uint32_t)(r)))

typedef struct g3d_tex { unsigned id; int w, h; } g3d_tex;

enum { G3D_UI = 0, G3D_GROUND, G3D_GROUND_3D, G3D_DECAL, G3D_SPRITE, G3D_FX, G3D_SHADOW };

int  g3d_init(void);
void g3d_fini(void);
void g3d_frame_begin(uint32_t clear_rgba);
void g3d_frame_end(void);

g3d_tex *g3d_load_png(const char *path);
g3d_tex *g3d_tex_create_rgba(int w, int h);
void g3d_tex_update(g3d_tex *t, int x, int y, int w, int h, const unsigned char *rgba);
void g3d_set_nearest(const g3d_tex *t);
void g3d_set_lift_fn(float (*fn)(float x, float z));
void g3d_set_lift_z_shift(int on);
int  g3d_get_lift_z_shift(void);
void g3d_set_cam_lift(float lift);
g3d_tex *g3d_load_padded_atlas(const char *path, int tile_px, int pad_px);
void     g3d_free_tex(g3d_tex *t);


void g3d_layer(int layer);


void g3d_camera(float tx, float tz, float pitch_deg, float yaw_deg, float zoom, float fov_deg);
void g3d_project(float x, float y, float z, float *sx, float *sy, float *scale);
void g3d_axes(float right[3], float up[3], float north[3]);
void g3d_eye(float e[3]);
void g3d_view_dir(float f[3]);
void g3d_ground_hit(float sx, float sy, float out[2]);
g3d_tex *g3d_white(void);
void g3d_fog(int on, float near_d, float far_d, float min_mul);


void g3d_plane(const g3d_tex *t, const float c[3], const float A[3], const float B[3],
               float lx0, float ly0, float lx1, float ly1, float rot,
               float u0, float v0, float u1, float v1, uint32_t col);

void g3d_plane_abs(const g3d_tex *t, const float c[3], const float A[3], const float B[3],
                   float lx0, float ly0, float lx1, float ly1, float rot,
                   float u0, float v0, float u1, float v1, uint32_t col);

void g3d_quad_elev(const g3d_tex *t, float x0, float z0, float x1, float z1,
                   float y00, float y10, float y11, float y01,
                   float u0, float v0, float u1, float v1, uint32_t col);

void g3d_quad_elev_flow(const g3d_tex *t, float x0, float z0, float x1, float z1,
                        float y00, float y10, float y11, float y01,
                        float u0, float v0, float u1, float v1,
                        float flow_u, float flow_v, uint32_t col);

void g3d_shadow_plane(const g3d_tex *t, float gx, float gz, float lift_h,
                      float lx0, float ly0, float lx1, float ly1, float rot,
                      float u0, float v0, float u1, float v1, uint32_t col);

void g3d_shadow_disc(float gx, float gz, float rx, float rz, float height, uint32_t col);

void g3d_shadow_oval(float gx, float gz, float rx, float rz, uint32_t col);

void g3d_shadow_oval_sun(float gx, float gz, float rx, float rz, float height, uint32_t col);

void g3d_set_border_bounds(float w, float h, float margin);

void g3d_rock_3d(const g3d_tex *t, float cx, float cz, float rx, float rz, float height,
                 float tu0, float tv0, float tu1, float tv1,
                 float su0, float sv0, float su1, float sv1,
                 uint32_t tint, uint32_t seed, int crystal);

void g3d_tree_base_3d(const g3d_tex *t, float cx, float by, float bz, float radius, float height,
                      float tu0, float tv0, float tu1, float tv1,
                      uint32_t bark_tint, uint32_t seed);

void g3d_bridge_3d(const g3d_tex *t, float x0, float z0, float x1, float z1, float deck_y, int dir,
                   float pu0, float pv0, float pu1, float pv1);

void g3d_tris(const g3d_tex *t, const void *verts, int n, float s);

void g3d_tris_split(const g3d_tex *t, const void *verts, int n, float s, int mode);
void g3d_tris_bb(const g3d_tex *t, const void *verts, int n, float s, const float R[3], const float D[3], int pass);
void g3d_tris_shadow(const g3d_tex *t, const void *verts, int n, float s, float sx_dir, float sz_dir, uint32_t col);
void g3d_disc(const float c[3], const float A[3], const float B[3], float radius, uint32_t col);
void g3d_ribbon(const float p1[3], const float p2[3], const float perp[3], float width, uint32_t col);


void g3d_ui_rect(float x, float y, float w, float h, uint32_t col);
void g3d_ui_circle(float x, float y, float r, uint32_t col);
void g3d_ui_line(float x1, float y1, float x2, float y2, float width, uint32_t col);
void g3d_ui_tex(const g3d_tex *t, float x, float y, float sx0, float sy0, float sw, float sh,
                float scx, float scy, uint32_t tint);
void g3d_ui_tex_rot(const g3d_tex *t, float x, float y, float scx, float scy, float rad,
                    float hx, float hy, uint32_t tint);


void g3d_cache_begin(void);
void g3d_cache_end(void);
int  g3d_cache_valid(void);
void g3d_cache_replay(void);
void g3d_cache_invalidate(void);
#endif
