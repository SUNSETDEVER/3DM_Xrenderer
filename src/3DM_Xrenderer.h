/**
 * @file 3DM_Xrenderer.h
 * @brief 轻量级光线追踪渲染器，支持外部瓦片缓冲区、三角形、平面、锁定组和背面剔除。
 * @brief Lightweight ray tracing renderer with external tile buffer, triangles, planes, lock groups, and backface culling.
 * @details 提供场景构建、材质定义、光照和渲染控制，面向嵌入式或桌面环境。
 * @details Provides scene construction, material definition, lighting and rendering control, targeting embedded or desktop environments.
 * @version 1.0
 * @date 2026-08-16
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

/**
 * @brief 初始化渲染器，设置画布尺寸、光线反弹次数和抗锯齿采样数。
 * @brief Initializes the renderer with canvas size, max bounces, and AA samples.
 * @param width       帧缓冲宽度（像素） / Framebuffer width (pixels)
 * @param height      帧缓冲高度（像素） / Framebuffer height (pixels)
 * @param max_bounces 最大光线反弹次数（>=0） / Maximum ray bounces (>=0)
 * @param aa_samples  抗锯齿采样数（>=1，1 表示关闭抗锯齿） / AA samples (>=1, 1 means off)
 * @note 调用此函数会重置所有场景对象、网格和锁定组。
 * @note This call resets all scene objects, meshes, and lock groups.
 */
void xr_init(int width, int height, int max_bounces, int aa_samples);

/**
 * @brief 设置相机位置、目标点和上方向。
 * @brief Sets camera position, target, and up vector.
 * @param pos    相机位置 / Camera position
 * @param target 视线目标点 / Look-at target
 * @param up     世界向上方向（通常为 {0,1,0}） / World up direction (usually {0,1,0})
 */
void xr_set_camera(xr_vec3_t pos, xr_vec3_t target, xr_vec3_t up);

/**
 * @brief 设置点光源位置和颜色。
 * @brief Sets point light position and color.
 * @param pos   光源位置 / Light position
 * @param color 光源颜色（RGB，各分量 0~1） / Light color (RGB, each component 0~1)
 */
void xr_set_light(xr_vec3_t pos, xr_vec3_t color);

/**
 * @brief 设置光线与表面相交时的偏置量，用于防止自阴影。
 * @brief Sets the bias value to avoid self-shadowing at intersection points.
 * @param bias 偏移值（默认 1e-3） / Bias value (default 1e-3)
 */
void xr_set_bias(float bias);

/**
 * @brief 设置抗锯齿采样数（会覆盖初始化时的值）。
 * @brief Sets anti-aliasing samples (overrides the value set in init).
 * @param samples 采样数（>=1） / Number of samples (>=1)
 */
void xr_set_aa_samples(int samples);

/**
 * @brief 设置最大光线反弹次数（会覆盖初始化时的值）。
 * @brief Sets maximum ray bounces (overrides the value set in init).
 * @param bounces 反弹次数（>=0） / Number of bounces (>=0)
 */
void xr_set_max_bounces(int bounces);

/**
 * @brief 启用/禁用阴影计算（软阴影暂不支持）。
 * @brief Enables/disables shadow computation (soft shadows not supported yet).
 * @param enable true 启用，false 禁用 / true to enable, false to disable
 */
void xr_enable_shadow(bool enable);

/**
 * @brief 启用/禁用微表面粗糙度散射（基于法线分布近似）。
 * @brief Enables/disables microfacet roughness scattering (approximated normal distribution).
 * @param enable true 启用，false 禁用 / true to enable, false to disable
 */
void xr_enable_microsurface(bool enable);

/**
 * @brief 启用/禁用 BVH（包围体层次结构）加速结构。
 * @brief Enables/disables BVH (Bounding Volume Hierarchy) acceleration.
 * @param enable true 启用（对网格有效），false 禁用（使用线性遍历）
 *                true to enable (effective for meshes), false to disable (use linear traversal)
 * @note 启用后会自动为所有网格构建 BVH；禁用时会释放 BVH 内存。
 * @note Enabling builds BVH for all meshes; disabling frees BVH memory.
 */
void xr_enable_bvh(bool enable);

/**
 * @brief 启用/禁用背面剔除（默认启用）。
 * @brief Enables/disables backface culling (enabled by default).
 * @param enable true 剔除朝向光线的三角形背面，false 则双面渲染
 *               true to cull triangles facing the ray, false for double-sided rendering
 */
void xr_enable_backface_culling(bool enable);

/**
 * @brief 设置用于瓦片渲染的外部临时缓冲区。
 * @brief Sets an external temporary buffer for tile‑based rendering.
 * @param buffer 缓冲区指针（至少 size*size 个 uint16_t） / Buffer pointer (at least size*size uint16_t)
 * @param size   瓦片大小（宽度/高度像素） / Tile size (width/height in pixels)
 * @details 若未设置或 size<=0，则整个帧缓冲直接渲染。
 * @details If not set or size<=0, the whole framebuffer is rendered directly.
 */
void xr_set_tile_buffer(uint16_t* buffer, int size);

/**
 * @brief 将角度（欧拉角）转换为单位法向量。
 * @brief Converts Euler angles to a unit normal vector.
 * @param ax 绕 X 轴旋转（弧度） / Rotation around X axis (radians)
 * @param ay 绕 Y 轴旋转（弧度） / Rotation around Y axis (radians)
 * @param az 绕 Z 轴旋转（弧度） / Rotation around Z axis (radians)
 * @return 归一化后的法向量 / Normalized normal vector
 */
xr_vec3_t xr_conv_angle_to_normal(float ax, float ay, float az);

/* ======================= 场景边界与光照区域 / Scene bounds and light area ======================= */

/**
 * @brief 设置世界边界（包围盒），用于墙壁和地板生成。
 * @brief Sets the world bounding box used for walls and floor.
 * @param xmin, xmax  X 轴最小/最大值 / X min/max
 * @param ymin, ymax  Y 轴最小/最大值 / Y min/max
 * @param zmin, zmax  Z 轴最小/最大值 / Z min/max
 */
void xr_set_boundary(float xmin, float xmax, float ymin, float ymax, float zmin, float zmax);

/**
 * @brief 设置指定墙壁面的材质。
 * @brief Sets the material for a specific wall face.
 * @param face 墙面索引：0(+X),1(-X),2(+Y),3(-Y),4(+Z),5(-Z)
 *             Face index: 0(+X),1(-X),2(+Y),3(-Y),4(+Z),5(-Z)
 * @param mat  材质 / Material
 */
void xr_set_wall_material(int face, xr_material_t mat);

/**
 * @brief 启用/禁用指定的墙壁面。
 * @brief Enables/disables a specific wall face.
 * @param face   墙面索引（同上） / Face index (same as above)
 * @param enable true 显示，false 隐藏 / true to show, false to hide
 */
void xr_enable_wall(int face, bool enable);

/**
 * @brief 设置地板材质（地板为 Y 轴负方向墙面）。
 * @brief Sets the floor material (floor is the -Y wall).
 * @param mat 材质 / Material
 */
void xr_set_floor_material(xr_material_t mat);

/**
 * @brief 启用/禁用地板棋盘格纹理（覆盖材质颜色）。
 * @brief Enables/disables checkerboard pattern on the floor (overrides material color).
 * @param enable true 显示棋盘格，false 使用材质固有色
 *               true for checkerboard, false for solid material color
 */
void xr_set_floor_checker(bool enable);

/**
 * @brief 设置顶部发光区域（模拟面光源）。
 * @brief Sets the top emissive area (simulates an area light).
 * @param center   发光区域中心 / Center of the emissive area
 * @param half_size 区域半宽度（方形区域） / Half‑size of the square area
 */
void xr_set_light_region(xr_vec3_t center, float half_size);

/* ======================= 场景图元添加 / Scene primitive adders ======================= */

/**
 * @brief 添加一个立方体。
 * @brief Adds a cube.
 * @param center    中心位置 / Center position
 * @param size      边长（半边长 = size/2） / Side length (half = size/2)
 * @param angle_rad 绕 Y 轴旋转角度（弧度） / Rotation around Y axis (radians)
 * @param mat       材质 / Material
 * @return 对象索引，可用于后续操作（如旋转） / Object index for later operations (e.g. rotation)
 */
int xr_add_cube(xr_vec3_t center, float size, float angle_rad, xr_material_t mat);

/**
 * @brief 添加一个四棱锥（底面为正方形）。
 * @brief Adds a square pyramid.
 * @param center    中心位置（几何中心） / Center position (geometric center)
 * @param size      底面边长 / Base side length
 * @param angle_rad 绕 Y 轴旋转角度（弧度） / Rotation around Y axis (radians)
 * @param mat       材质 / Material
 * @return 对象索引 / Object index
 */
int xr_add_pyramid(xr_vec3_t center, float size, float angle_rad, xr_material_t mat);

/**
 * @brief 添加一个球体。
 * @brief Adds a sphere.
 * @param center 球心位置 / Center position
 * @param radius 半径 / Radius
 * @param mat    材质 / Material
 * @return 对象索引 / Object index
 */
int xr_add_sphere(xr_vec3_t center, float radius, xr_material_t mat);

/**
 * @brief 添加一张桌子（由桌面和四条腿组成，以 AABB 表示）。
 * @brief Adds a table (composed of a top and four legs, represented as AABBs).
 * @param center   桌面中心位置 / Center of the table top
 * @param width    桌面宽度（X 方向） / Table width (X direction)
 * @param depth    桌面深度（Z 方向） / Table depth (Z direction)
 * @param height   桌面厚度（Y 方向） / Table thickness (Y direction)
 * @param leg_size 桌腿截面边长（正方形） / Leg cross‑section size (square)
 * @param mat      材质（实际未使用，桌子为木纹程序化纹理） / Material (not actually used, wood procedural texture)
 * @return 固定返回 -1（桌子不返回索引，由内部管理） / Always returns -1 (tables are managed internally)
 */
int xr_add_table(xr_vec3_t center, float width, float depth, float height, float leg_size, xr_material_t mat);

/**
 * @brief 添加一个半透明玻璃面板。
 * @brief Adds a translucent glass panel.
 * @param center            面板中心位置 / Panel center
 * @param normal            面板法线（单位向量） / Panel normal (unit vector)
 * @param half_width        面板半宽度（沿法线垂直方向） / Half‑width (perpendicular to normal)
 * @param half_height       面板半高度（沿法线垂直方向） / Half‑height (perpendicular to normal)
 * @param thickness         面板厚度 / Panel thickness
 * @param refractive_index  折射率（当前未使用） / Refractive index (currently unused)
 * @return 固定返回 -1（内部管理） / Always returns -1 (managed internally)
 */
int xr_add_glass_panel(xr_vec3_t center, xr_vec3_t normal, float half_width, float half_height, float thickness, float refractive_index);

/**
 * @brief 添加一个带有纹理贴图的平面。
 * @brief Adds a textured plane.
 * @param center 平面中心 / Plane center
 * @param normal 法线方向 / Normal direction
 * @param width  平面宽度 / Plane width
 * @param height 平面高度 / Plane height
 * @param tex    纹理对象（数据需保持有效） / Texture object (data must remain valid)
 * @param mat    材质（若纹理存在，材质粗糙度仍影响高光） / Material (roughness still affects specular if texture present)
 * @return 对象索引 / Object index
 */
int xr_add_textured_plane(xr_vec3_t center, xr_vec3_t normal, float width, float height,
                           xr_texture_t tex, xr_material_t mat);

/**
 * @brief 添加一个自定义三角网格。
 * @brief Adds a custom triangle mesh.
 * @param vertices         顶点数组 / Vertex array
 * @param indices          索引数组（每 3 个组成一个三角形） / Index array (3 per triangle)
 * @param num_vertices     顶点数 / Number of vertices
 * @param num_triangles    三角形数 / Number of triangles
 * @param position         网格位置（平移） / Mesh position (translation)
 * @param rotation_euler   欧拉旋转（弧度，顺序 ZYX） / Euler rotation (radians, order ZYX)
 * @param scale            缩放向量 / Scale vector
 * @param mat              材质 / Material
 * @return 固定返回 -1（内部管理），网格会参与 BVH 加速 / Always returns -1 (internal), mesh participates in BVH if enabled
 */
int xr_add_mesh(const xr_vec3_t* vertices, const uint16_t* indices,
                 int num_vertices, int num_triangles,
                 xr_vec3_t position, xr_vec3_t rotation_euler, xr_vec3_t scale,
                 xr_material_t mat);

/**
 * @brief 添加一个自定义三角形。
 * @brief Adds a custom triangle.
 * @param v0, v1, v2  三个顶点 / Three vertices
 * @param mat         材质 / Material
 * @return 对象索引 / Object index
 */
int xr_add_triangle(xr_vec3_t v0, xr_vec3_t v1, xr_vec3_t v2, xr_material_t mat);

/**
 * @brief 添加一个无限薄平面（有界矩形）。
 * @brief Adds an infinitesimally thin bounded plane (rectangle).
 * @param center 平面中心 / Plane center
 * @param normal 法线 / Normal
 * @param width  宽度 / Width
 * @param height 高度 / Height
 * @param mat    材质 / Material
 * @return 对象索引 / Object index
 */
int xr_add_plane(xr_vec3_t center, xr_vec3_t normal, float width, float height, xr_material_t mat);

/* ======================= 锁定组与变换 / Lock groups and transformations ======================= */

/**
 * @brief 创建一个锁定组，将多个对象绑定，以便同时旋转。
 * @brief Creates a lock group that binds multiple objects to rotate together.
 * @param first 第一个对象索引（必须 >=0），后续参数以 -1 结尾
 *              First object index (must be >=0), subsequent arguments end with -1
 * @return 锁定组 ID，可用于将来扩展；当前仅内部使用
 *         Lock group ID for future extension; currently only used internally
 * @note 例如：xr_lock_by_index(0, 2, 3, -1); 将索引 0,2,3 绑定。
 * @note Example: xr_lock_by_index(0, 2, 3, -1); binds indices 0,2,3.
 */
int xr_lock_by_index(int first, ...);

/**
 * @brief 旋转指定对象或锁定组中的所有对象。
 * @brief Rotates a specific object or all objects in its lock group.
 * @param idx 对象索引或组内任一对象的索引 / Object index or any index in the group
 * @param rx  绕 X 轴旋转量（弧度） / Rotation around X axis (radians)
 * @param ry  绕 Y 轴旋转量（弧度） / Rotation around Y axis (radians)
 * @param rz  绕 Z 轴旋转量（弧度） / Rotation around Z axis (radians)
 * @details 若 idx 属于某个锁定组，则组内所有对象同步旋转。
 * @details If idx belongs to a lock group, all objects in that group rotate synchronously.
 */
void xr_rotate_element(int idx, float rx, float ry, float rz);

/**
 * @brief 设置纹理平面的 UV 翻转标志。
 * @brief Sets UV flip flags for a textured plane.
 * @param idx    纹理平面对象索引 / Textured plane object index
 * @param flip_h 是否水平翻转（U 方向） / Whether to flip horizontally (U direction)
 * @param flip_v 是否垂直翻转（V 方向） / Whether to flip vertically (V direction)
 */
void xr_set_texture_flip(int idx, bool flip_h, bool flip_v);

/* ======================= 渲染函数 / Rendering functions ======================= */

/**
 * @brief 渲染整个帧缓冲。
 * @brief Renders the entire framebuffer.
 * @param framebuffer 输出帧缓冲（RGB565 格式，大小为 width*height）
 *                    Output framebuffer (RGB565 format, size width*height)
 */
void xr_render(uint16_t *framebuffer);

/**
 * @brief 渲染帧缓冲的指定矩形区域。
 * @brief Renders a specified rectangular region of the framebuffer.
 * @param start_x     区域起始 X 坐标（包含） / Region start X (inclusive)
 * @param start_y     区域起始 Y 坐标（包含） / Region start Y (inclusive)
 * @param w           区域宽度 / Region width
 * @param h           区域高度 / Region height
 * @param framebuffer 输出帧缓冲（RGB565） / Output framebuffer (RGB565)
 * @note 若区域超出边界，会自动裁剪。
 * @note Out‑of‑bounds regions are automatically clipped.
 */
void xr_render_region(int start_x, int start_y, int w, int h, uint16_t *framebuffer);

#ifdef __cplusplus
}
#endif

#endif /* _3DM_XRENDERER_H */
