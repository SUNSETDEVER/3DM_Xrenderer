/**
 * @file 3DM_Xrenderer.h
 * @brief Lightweight ray tracing renderer with external tile buffer.
 */

#ifndef _3DM_XRENDERER_H
#define _3DM_XRENDERER_H

#include <Arduino.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x, y, z;
} xr_vec3_t;

typedef struct {
    xr_vec3_t albedo;
    float roughness;
    float metallic;
} xr_material_t;

typedef struct {
    uint16_t *data;
    int width;
    int height;
} xr_texture_t;

/* Predefined materials */
extern const xr_material_t xr_mat_gold;
extern const xr_material_t xr_mat_wood;
extern const xr_material_t xr_mat_glass;
extern const xr_material_t xr_mat_wall;
extern const xr_material_t xr_mat_floor;
extern const xr_material_t xr_mat_red_plastic;
extern const xr_material_t xr_mat_blue_plastic;
extern const xr_material_t xr_mat_green_plastic;
extern const xr_material_t xr_mat_white_plastic;
extern const xr_material_t xr_mat_black_plastic;
extern const xr_material_t xr_mat_chrome;
extern const xr_material_t xr_mat_brass;

static inline xr_material_t xr_material_make(xr_vec3_t albedo, float roughness) {
    xr_material_t m = {albedo, roughness, 0.0f};
    return m;
}

/* Core renderer */
void xr_init(int width, int height, int max_bounces, int aa_samples);
void xr_set_camera(xr_vec3_t pos, xr_vec3_t target, xr_vec3_t up);
void xr_set_light(xr_vec3_t pos, xr_vec3_t color);
void xr_set_bias(float bias);
void xr_set_aa_samples(int samples);
void xr_set_max_bounces(int bounces);
void xr_enable_shadow(bool enable);
void xr_enable_microsurface(bool enable);
void xr_enable_bvh(bool enable);
void xr_set_tile_buffer(uint16_t* buffer, int size);   // NEW: external buffer for tiling
xr_vec3_t xr_conv_angle_to_normal(float ax, float ay, float az);

/* Scene editing */
void xr_set_boundary(float xmin, float xmax, float ymin, float ymax, float zmin, float zmax);
void xr_set_wall_material(int face, xr_material_t mat);
void xr_enable_wall(int face, bool enable);
void xr_set_floor_material(xr_material_t mat);
void xr_set_floor_checker(bool enable);
void xr_set_light_region(xr_vec3_t center, float half_size);

/* Object adders (return index) */
int xr_add_cube(xr_vec3_t center, float size, float angle_rad, xr_material_t mat);
int xr_add_pyramid(xr_vec3_t center, float size, float angle_rad, xr_material_t mat);
int xr_add_sphere(xr_vec3_t center, float radius, xr_material_t mat);
int xr_add_table(xr_vec3_t center, float width, float depth, float height, float leg_size, xr_material_t mat);
int xr_add_glass_panel(xr_vec3_t center, xr_vec3_t normal, float half_width, float half_height, float thickness, float refractive_index);
int xr_add_textured_plane(xr_vec3_t center, xr_vec3_t normal, float width, float height,
                           xr_texture_t tex, xr_material_t mat);
int xr_add_mesh(const xr_vec3_t* vertices, const uint16_t* indices,
                 int num_vertices, int num_triangles,
                 xr_vec3_t position, xr_vec3_t rotation_euler, xr_vec3_t scale,
                 xr_material_t mat);

void xr_rotate_element(int idx, float rx, float ry, float rz);
void xr_set_texture_flip(int idx, bool flip_h, bool flip_v);
void xr_render(uint16_t *framebuffer);
void xr_render_region(int start_x, int start_y, int w, int h, uint16_t *framebuffer);

#ifdef __cplusplus
}
#endif

#endif