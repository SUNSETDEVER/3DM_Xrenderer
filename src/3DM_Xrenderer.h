/**
 * @file 3DM_Xrenderer.h
 * @brief 轻量级光线追踪渲染器，支持外部瓦片缓冲区、三角形、平面、锁定组、背面剔除、纹理旋转以及网格纹理。
 * @brief Lightweight ray tracing renderer with external tile buffer, triangles, planes, lock groups, backface culling, texture rotation, and mesh textures.
 * @details 提供场景构建、材质定义、光照和渲染控制，面向嵌入式或桌面环境。
 * @details Provides scene construction, material definition, lighting and rendering control, targeting embedded or desktop environments.
 * @version 1.3
 * @date 2026-08-17
 */

#ifndef _3DM_XRENDERER_H
#define _3DM_XRENDERER_H

#include <Arduino.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 三维向量类型，用于表示点、方向、颜色等。
 * @brief 3D vector type, used for points, directions, colors, etc.
 */
typedef struct {
    float x, y, z;
} xr_vec3_t;

/**
 * @brief 材质结构，定义物体表面的光学属性。
 * @brief Material structure defining the optical properties of a surface.
 */
typedef struct {
    xr_vec3_t albedo;   ///< 漫反射颜色（RGB） / Diffuse color (RGB)
    float roughness;    ///< 表面粗糙度（0.0 镜面，1.0 完全漫反射） / Surface roughness (0.0 mirror, 1.0 fully diffuse)
    float metallic;     ///< 金属度（当前未使用，保留） / Metallic factor (currently unused, reserved)
} xr_material_t;

/**
 * @brief 纹理结构，用于贴图，数据为 RGB565 格式。
 * @brief Texture structure for mapping, data stored in RGB565 format.
 */
typedef struct {
    uint16_t *data;     ///< 纹理数据指针 / Pointer to texture data
    int width;          ///< 纹理宽度（像素） / Texture width in pixels
    int height;         ///< 纹理高度（像素） / Texture height in pixels
} xr_texture_t;

/* ======================= 预定义材质 / Predefined materials ======================= */

extern const xr_material_t xr_mat_gold;          ///< 金色 / Gold
extern const xr_material_t xr_mat_wood;          ///< 木质 / Wood
extern const xr_material_t xr_mat_glass;         ///< 玻璃 / Glass
extern const xr_material_t xr_mat_wall;          ///< 墙壁默认 / Default wall
extern const xr_material_t xr_mat_floor;         ///< 地板默认 / Default floor
extern const xr_material_t xr_mat_red_plastic;   ///< 红色塑料 / Red plastic
extern const xr_material_t xr_mat_blue_plastic;  ///< 蓝色塑料 / Blue plastic
extern const xr_material_t xr_mat_green_plastic; ///< 绿色塑料 / Green plastic
extern const xr_material_t xr_mat_white_plastic; ///< 白色塑料 / White plastic
extern const xr_material_t xr_mat_black_plastic; ///< 黑色塑料 / Black plastic
extern const xr_material_t xr_mat_chrome;        ///< 铬金属 / Chrome
extern const xr_material_t xr_mat_brass;         ///< 黄铜 / Brass

/**
 * @brief 快速创建材质的内联辅助函数。
 * @brief Inline helper to quickly create a material.
 * @param albedo   漫反射颜色 / Diffuse color
 * @param roughness 粗糙度（0~1） / Roughness (0~1)
 * @return 填充好的 xr_material_t 结构 / Filled xr_material_t structure
 */
static inline xr_material_t xr_material_make(xr_vec3_t albedo, float roughness) {
    xr_material_t m = {albedo, roughness, 0.0f};
    return m;
}

/* ======================= 核心渲染器控制 / Core renderer control ======================= */

void xr_init(int width, int height, int max_bounces, int aa_samples);
void xr_set_camera(xr_vec3_t pos, xr_vec3_t target, xr_vec3_t up);
void xr_set_light(xr_vec3_t pos, xr_vec3_t color);
void xr_set_bias(float bias);
void xr_set_aa_samples(int samples);
void xr_set_max_bounces(int bounces);
void xr_enable_shadow(bool enable);
void xr_enable_microsurface(bool enable);
void xr_enable_bvh(bool enable);
void xr_enable_backface_culling(bool enable);
void xr_set_tile_buffer(uint16_t* buffer, int size);
xr_vec3_t xr_conv_angle_to_normal(float ax, float ay, float az);

/* ======================= 场景边界与光照区域 / Scene bounds and light area ======================= */
void xr_set_boundary(float xmin, float xmax, float ymin, float ymax, float zmin, float zmax);
void xr_set_wall_material(int face, xr_material_t mat);
void xr_enable_wall(int face, bool enable);
void xr_set_floor_material(xr_material_t mat);
void xr_set_floor_checker(bool enable);
void xr_set_light_region(xr_vec3_t center, float half_size);

/* ======================= 场景图元添加 / Scene primitive adders ======================= */
int xr_add_cube(xr_vec3_t center, float size, float angle_rad, xr_material_t mat);
int xr_add_pyramid(xr_vec3_t center, float size, float angle_rad, xr_material_t mat);
int xr_add_sphere(xr_vec3_t center, float radius, xr_material_t mat);
int xr_add_table(xr_vec3_t center, float width, float depth, float height, float leg_size, xr_material_t mat);
int xr_add_glass_panel(xr_vec3_t center, xr_vec3_t normal, float half_width, float half_height, float thickness, float refractive_index);
int xr_add_textured_plane(xr_vec3_t center, xr_vec3_t normal, float width, float height,
                           xr_texture_t tex, xr_material_t mat);

/**
 * @brief 添加一个自定义三角网格（支持纹理）。
 * @brief Adds a custom triangle mesh with texture support.
 * @param vertices         顶点位置数组 / Vertex position array
 * @param uvs              顶点 UV 坐标数组（每顶点 (u,v,0)），可为 NULL（表示无纹理） / Vertex UV array (u,v,0 per vertex), may be NULL (no texture)
 * @param indices          索引数组（每 3 个组成一个三角形） / Index array (3 per triangle)
 * @param num_vertices     顶点数 / Number of vertices
 * @param num_triangles    三角形数 / Number of triangles
 * @param position         网格位置（平移） / Mesh position (translation)
 * @param rotation_euler   欧拉旋转（弧度，顺序 ZYX） / Euler rotation (radians, order ZYX)
 * @param scale            缩放向量 / Scale vector
 * @param mat              材质 / Material
 * @param tex              纹理结构（若 data 为 NULL 则忽略） / Texture structure (ignored if data is NULL)
 * @return 固定返回 -1（内部管理） / Always returns -1 (internal)
 */
int xr_add_mesh(const xr_vec3_t* vertices, const xr_vec3_t* uvs, const uint16_t* indices,
                 int num_vertices, int num_triangles,
                 xr_vec3_t position, xr_vec3_t rotation_euler, xr_vec3_t scale,
                 xr_material_t mat, xr_texture_t tex);

int xr_add_triangle(xr_vec3_t v0, xr_vec3_t v1, xr_vec3_t v2, xr_material_t mat);
int xr_add_plane(xr_vec3_t center, xr_vec3_t normal, float width, float height, xr_material_t mat);

/* ======================= 锁定组与变换 / Lock groups and transformations ======================= */
int xr_lock_by_index(int first, ...);
void xr_rotate_element(int idx, float rx, float ry, float rz);
void xr_set_texture_flip(int idx, bool flip_h, bool flip_v);
void xr_set_texture_rotation(int idx, float angle);
void xr_rotate_axis(int idx, xr_vec3_t p1, xr_vec3_t p2, float arc);

/* ======================= 渲染函数 / Rendering functions ======================= */
void xr_render(uint16_t *framebuffer);
void xr_render_region(int start_x, int start_y, int w, int h, uint16_t *framebuffer);

#ifdef __cplusplus
}
#endif

#endif /* _3DM_XRENDERER_H */