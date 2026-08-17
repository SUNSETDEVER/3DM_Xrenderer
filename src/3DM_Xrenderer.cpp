/**
 * @file 3DM_Xrenderer.c
 * @brief Implementation of 3DM_Xrenderer library.
 * @version 1.3  (Added mesh texture support with UV interpolation)
 */

#include "3DM_Xrenderer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

/* ---------- Constants ---------- */
#define EPSILON        1e-2f
#define LIGHT_DEC_FACTOR 0.4f
#define DEFAULT_MAX_BOUNCES 5
#define DEFAULT_AA_SAMPLES 1
#define DEFAULT_BIAS    1e-3f
#define BVH_LEAF_MAX_TRI 4
#define DEFAULT_TILE_SIZE 0

/* ---------- Predefined materials ---------- */
const xr_material_t xr_mat_gold          = {{1.0f, 0.76f, 0.33f}, 0.0f, 0.0f};
const xr_material_t xr_mat_wood          = {{0.55f, 0.27f, 0.07f}, 0.3f, 0.0f};
const xr_material_t xr_mat_glass         = {{1.0f, 1.0f, 1.0f}, 0.0f, 0.0f};
const xr_material_t xr_mat_wall          = {{0.8f, 0.8f, 0.8f}, 0.8f, 0.0f};
const xr_material_t xr_mat_floor         = {{0.8f, 0.8f, 0.8f}, 0.1f, 0.0f};
const xr_material_t xr_mat_red_plastic   = {{1.0f, 0.0f, 0.0f}, 0.6f, 0.0f};
const xr_material_t xr_mat_blue_plastic  = {{0.0f, 0.0f, 1.0f}, 0.6f, 0.0f};
const xr_material_t xr_mat_green_plastic = {{0.0f, 1.0f, 0.0f}, 0.6f, 0.0f};
const xr_material_t xr_mat_white_plastic = {{1.0f, 1.0f, 1.0f}, 0.5f, 0.0f};
const xr_material_t xr_mat_black_plastic = {{0.0f, 0.0f, 0.0f}, 0.8f, 0.0f};
const xr_material_t xr_mat_chrome        = {{0.8f, 0.8f, 0.9f}, 0.05f, 0.0f};
const xr_material_t xr_mat_brass         = {{0.8f, 0.6f, 0.2f}, 0.1f, 0.0f};

/* ---------- Internal structures ---------- */
typedef struct {
    xr_vec3_t min;
    xr_vec3_t max;
} aabb_t;

typedef struct {
    xr_vec3_t center;
    xr_vec3_t normal;
    float half_width;
    float half_height;
    float half_thickness;
} glass_panel_t;

typedef enum {
    OBJ_CUBE,
    OBJ_PYRAMID,
    OBJ_TABLE,
    OBJ_GLASS_PANEL,
    OBJ_TEXTURED_PLANE,
    OBJ_SPHERE,
    OBJ_TRIANGLE,
    OBJ_PLANE
} obj_type_t;

typedef struct {
    obj_type_t type;
    xr_material_t mat;
    xr_vec3_t center;
    float size;
    xr_vec3_t rot;
    float width, depth, height, leg_size;
    xr_vec3_t normal;
    float half_w, half_h, thickness, refr_idx;
    xr_vec3_t plane_normal;
    float plane_width, plane_height;
    xr_texture_t texture;
    bool tex_flip_h;
    bool tex_flip_v;
    xr_vec3_t tri[3];
    float tex_rotation;
} scene_object_t;

typedef struct bvh_node_t {
    xr_vec3_t min;
    xr_vec3_t max;
    int left;
    int right;
    int tri_start;
    int tri_count;
} bvh_node_t;

typedef struct {
    xr_vec3_t *vertices;
    xr_vec3_t *uvs;          // 顶点 UV 坐标 (u,v,0)
    uint16_t *indices;
    int num_vertices;
    int num_triangles;
    xr_material_t mat;
    xr_texture_t texture;    // 网格纹理（可为空）
    bvh_node_t *bvh_nodes;
    int bvh_root;
    int bvh_node_count;
    int bvh_node_capacity;
} mesh_t;

typedef struct {
    int *indices;
    int count;
    int capacity;
} lock_group_t;

/* ---------- Global state ---------- */
static int g_width, g_height;
static int g_max_bounces = DEFAULT_MAX_BOUNCES;
static int g_aa_samples = DEFAULT_AA_SAMPLES;
static float g_bias = DEFAULT_BIAS;
static int g_tile_size = DEFAULT_TILE_SIZE;

static xr_vec3_t g_cam_pos = {0, 0, 0};
static xr_vec3_t g_cam_target = {0, 0, 0};
static xr_vec3_t g_cam_up = {0, 1, 0};
static xr_vec3_t g_light_pos = {0, 50, 0};
static xr_vec3_t g_light_color = {1, 1, 1};

static bool g_shadow_enabled = false;
static bool g_microsurface_enabled = false;
static bool g_bvh_enabled = false;
static bool g_backface_culling_enabled = true;

static float g_boundary[6] = {-50.0f, 50.0f, -50.0f, 50.0f, -50.0f, 50.0f};
static bool g_wall_enabled[6] = {true, true, true, true, true, true};
static xr_material_t g_wall_materials[6] = {
    xr_mat_wall, xr_mat_wall, xr_mat_wall, xr_mat_wall, xr_mat_wall, xr_mat_wall
};
static xr_material_t g_floor_material = xr_mat_floor;
static bool g_floor_checker = true;
static xr_vec3_t g_light_center = {0.0f, 50.0f, 0.0f};
static float g_light_half_size = 10.0f;

static scene_object_t *g_objects = NULL;
static int g_obj_count = 0;
static int g_obj_capacity = 0;

static aabb_t *g_table_aabbs = NULL;
static int g_table_aabb_count = 0;

static glass_panel_t *g_glass_panels = NULL;
static int g_glass_count = 0;

static mesh_t *g_meshes = NULL;
static int g_mesh_count = 0;
static int g_mesh_capacity = 0;

static uint16_t *g_tile_buffer = NULL;
static int g_tile_buffer_size = 0;

static lock_group_t *g_lock_groups = NULL;
static int g_lock_group_count = 0;

/* ---------- Vector math ---------- */
static inline float fast_inv_sqrt(float x) {
    float xhalf = 0.5f * x;
    int i = *(int*)&x;
    i = 0x5f3759df - (i >> 1);
    x = *(float*)&i;
    x = x * (1.5f - xhalf * x * x);
    return x;
}
static inline xr_vec3_t v_add(xr_vec3_t a, xr_vec3_t b) {
    return (xr_vec3_t){a.x + b.x, a.y + b.y, a.z + b.z};
}
static inline xr_vec3_t v_sub(xr_vec3_t a, xr_vec3_t b) {
    return (xr_vec3_t){a.x - b.x, a.y - b.y, a.z - b.z};
}
static inline xr_vec3_t v_mul(xr_vec3_t a, float t) {
    return (xr_vec3_t){a.x * t, a.y * t, a.z * t};
}
static inline float v_dot(xr_vec3_t a, xr_vec3_t b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
static inline xr_vec3_t v_cross(xr_vec3_t a, xr_vec3_t b) {
    return (xr_vec3_t){a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
static inline xr_vec3_t v_norm(xr_vec3_t a) {
    float l2 = a.x*a.x + a.y*a.y + a.z*a.z;
    if (l2 > 0) {
        float inv = fast_inv_sqrt(l2);
        return (xr_vec3_t){a.x*inv, a.y*inv, a.z*inv};
    }
    return a;
}
static inline xr_vec3_t v_reflect(xr_vec3_t I, xr_vec3_t N) {
    return v_sub(I, v_mul(N, 2.0f * v_dot(N, I)));
}
static inline xr_vec3_t apply_transform(xr_vec3_t p, xr_vec3_t pos, xr_vec3_t rot, xr_vec3_t scale) {
    p.x *= scale.x;
    p.y *= scale.y;
    p.z *= scale.z;
    float cz = cosf(rot.z), sz = sinf(rot.z);
    float x = p.x * cz - p.y * sz;
    float y = p.x * sz + p.y * cz;
    p.x = x; p.y = y;
    float cy = cosf(rot.y), sy = sinf(rot.y);
    x = p.x * cy + p.z * sy;
    float z = -p.x * sy + p.z * cy;
    p.x = x; p.z = z;
    float cx = cosf(rot.x), sx = sinf(rot.x);
    y = p.y * cx - p.z * sx;
    z = p.y * sx + p.z * cx;
    p.y = y; p.z = z;
    p.x += pos.x;
    p.y += pos.y;
    p.z += pos.z;
    return p;
}
static inline xr_vec3_t apply_inverse_euler(xr_vec3_t p, xr_vec3_t rot) {
    float cz = cosf(-rot.z), sz = sinf(-rot.z);
    float x = p.x * cz - p.y * sz;
    float y = p.x * sz + p.y * cz;
    p.x = x; p.y = y;
    float cy = cosf(-rot.y), sy = sinf(-rot.y);
    x = p.x * cy + p.z * sy;
    float z = -p.x * sy + p.z * cy;
    p.x = x; p.z = z;
    float cx = cosf(-rot.x), sx = sinf(-rot.x);
    y = p.y * cx - p.z * sx;
    z = p.y * sx + p.z * cx;
    p.y = y; p.z = z;
    return p;
}

/* ---------- RGB565 ---------- */
static inline uint16_t float_to_rgb565(xr_vec3_t color) {
    uint8_t r = (uint8_t)(fminf(fmaxf(color.x, 0.0f), 1.0f) * 31.0f);
    uint8_t g = (uint8_t)(fminf(fmaxf(color.y, 0.0f), 1.0f) * 63.0f);
    uint8_t b = (uint8_t)(fminf(fmaxf(color.z, 0.0f), 1.0f) * 31.0f);
    return (r << 11) | (g << 5) | b;
}
static inline xr_vec3_t rgb565_to_vec(uint16_t c) {
    uint8_t r = (c >> 11) & 0x1F;
    uint8_t g = (c >> 5) & 0x3F;
    uint8_t b = c & 0x1F;
    return (xr_vec3_t){(float)r / 31.0f, (float)g / 63.0f, (float)b / 31.0f};
}

/* ---------- RNG ---------- */
static inline uint32_t splitmix32(uint32_t *state) {
    uint32_t z = (*state += 0x9e3779b9);
    z = (z ^ (z >> 16)) * 0x85ebca6b;
    z = (z ^ (z >> 13)) * 0xc2b2ae35;
    return z ^ (z >> 16);
}

/* ---------- 3x3 Matrix helpers ---------- */
static inline void mat3_identity(float m[3][3]) {
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m[i][j] = (i == j) ? 1.0f : 0.0f;
}

static inline void mat3_mul(float a[3][3], float b[3][3], float out[3][3]) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            out[i][j] = a[i][0]*b[0][j] + a[i][1]*b[1][j] + a[i][2]*b[2][j];
        }
    }
}

static inline void mat3_from_euler(float rx, float ry, float rz, float m[3][3]) {
    float cx = cosf(rx), sx = sinf(rx);
    float cy = cosf(ry), sy = sinf(ry);
    float cz = cosf(rz), sz = sinf(rz);
    // Rotation order: ZYX (m = Rz * Ry * Rx)
    m[0][0] = cy*cz;                     m[0][1] = sx*sy*cz - cx*sz;  m[0][2] = cx*sy*cz + sx*sz;
    m[1][0] = cy*sz;                     m[1][1] = sx*sy*sz + cx*cz;  m[1][2] = cx*sy*sz - sx*cz;
    m[2][0] = -sy;                       m[2][1] = sx*cy;             m[2][2] = cx*cy;
}

static inline void mat3_axis_angle(float ax, float ay, float az, float angle, float m[3][3]) {
    float len = sqrtf(ax*ax + ay*ay + az*az);
    if (len < 1e-6f) { mat3_identity(m); return; }
    float x = ax / len, y = ay / len, z = az / len;
    float c = cosf(angle), s = sinf(angle), t = 1.0f - c;
    m[0][0] = t*x*x + c;       m[0][1] = t*x*y - s*z;   m[0][2] = t*x*z + s*y;
    m[1][0] = t*x*y + s*z;     m[1][1] = t*y*y + c;     m[1][2] = t*y*z - s*x;
    m[2][0] = t*x*z - s*y;     m[2][1] = t*y*z + s*x;   m[2][2] = t*z*z + c;
}

static inline void euler_from_mat3(float m[3][3], float *rx, float *ry, float *rz) {
    *ry = asinf(-m[2][0]);
    float cos_y = cosf(*ry);
    if (fabsf(cos_y) > 1e-6f) {
        *rx = atan2f(m[2][1], m[2][2]);
        *rz = atan2f(m[1][0], m[0][0]);
    } else {
        // Gimbal lock: set rx = 0, rz = atan2(-m[0][1], m[1][1])
        *rx = 0.0f;
        *rz = atan2f(-m[0][1], m[1][1]);
    }
}

/* ---------- Intersection helpers ---------- */
static inline bool ray_aabb_intersect(xr_vec3_t origin, xr_vec3_t dir, aabb_t box, float *t_out, xr_vec3_t *n_out) {
    float t_min = -1e9f, t_max = 1e9f;
    for (int i = 0; i < 3; i++) {
        float o = (i == 0) ? origin.x : ((i == 1) ? origin.y : origin.z);
        float d = (i == 0) ? dir.x : ((i == 1) ? dir.y : dir.z);
        float min = (i == 0) ? box.min.x : ((i == 1) ? box.min.y : box.min.z);
        float max = (i == 0) ? box.max.x : ((i == 1) ? box.max.y : box.max.z);
        if (fabsf(d) < 1e-6f) {
            if (o < min || o > max) return false;
        } else {
            float t1 = (min - o) / d;
            float t2 = (max - o) / d;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > t_min) t_min = t1;
            if (t2 < t_max) t_max = t2;
        }
    }
    if (t_min < t_max && t_min > EPSILON) {
        *t_out = t_min;
        xr_vec3_t P = v_add(origin, v_mul(dir, t_min));
        float eps = 1e-3f;
        if (fabsf(P.x - box.min.x) < eps) *n_out = {-1,0,0};
        else if (fabsf(P.x - box.max.x) < eps) *n_out = {1,0,0};
        else if (fabsf(P.y - box.min.y) < eps) *n_out = {0,-1,0};
        else if (fabsf(P.y - box.max.y) < eps) *n_out = {0,1,0};
        else if (fabsf(P.z - box.min.z) < eps) *n_out = {0,0,-1};
        else if (fabsf(P.z - box.max.z) < eps) *n_out = {0,0,1};
        else *n_out = {0,1,0};
        return true;
    }
    return false;
}

// 修改 ray_triangle_intersect 以输出重心坐标 u, v
static inline bool ray_triangle_intersect(xr_vec3_t orig, xr_vec3_t dir,
                                          xr_vec3_t v0, xr_vec3_t v1, xr_vec3_t v2,
                                          float *t_out, xr_vec3_t *n_out,
                                          float *out_u, float *out_v) {
    const float EPS_TRI = 1e-6f;
    xr_vec3_t edge1 = v_sub(v1, v0);
    xr_vec3_t edge2 = v_sub(v2, v0);
    xr_vec3_t h = v_cross(dir, edge2);
    float a = v_dot(edge1, h);
    if (fabsf(a) < EPS_TRI) return false;
    float f = 1.0f / a;
    xr_vec3_t s = v_sub(orig, v0);
    float u = f * v_dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;
    xr_vec3_t q = v_cross(s, edge1);
    float v = f * v_dot(dir, q);
    if (v < 0.0f || u + v > 1.0f) return false;
    float t = f * v_dot(edge2, q);
    if (t > EPSILON) {
        *t_out = t;
        *n_out = v_norm(v_cross(edge1, edge2));
        if (out_u) *out_u = u;
        if (out_v) *out_v = v;
        return true;
    }
    return false;
}

static inline bool ray_sphere_intersect(xr_vec3_t origin, xr_vec3_t dir, xr_vec3_t center, float radius,
                                        float *t_out, xr_vec3_t *n_out) {
    xr_vec3_t oc = v_sub(origin, center);
    float a = v_dot(dir, dir);
    float b = 2.0f * v_dot(oc, dir);
    float c = v_dot(oc, oc) - radius * radius;
    float disc = b*b - 4*a*c;
    if (disc < 0) return false;
    float sqrt_disc = sqrtf(disc);
    float t = (-b - sqrt_disc) / (2*a);
    if (t < EPSILON) {
        t = (-b + sqrt_disc) / (2*a);
        if (t < EPSILON) return false;
    }
    *t_out = t;
    xr_vec3_t P = v_add(origin, v_mul(dir, t));
    *n_out = v_norm(v_sub(P, center));
    return true;
}

static inline xr_vec3_t sample_texture(xr_texture_t *tex, float u, float v) {
    if (!tex || !tex->data) return (xr_vec3_t){1.0f, 0.0f, 1.0f};
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    int px = (int)(u * (tex->width - 1));
    int py = (int)(v * (tex->height - 1));
    uint16_t pixel = tex->data[py * tex->width + px];
    return rgb565_to_vec(pixel);
}

/* ========================== BVH ========================== */
static mesh_t *g_sort_mesh = NULL;
static int g_sort_axis = 0;

static int cmp_tri(const void *a, const void *b) {
    int ia = *(int*)a;
    int ib = *(int*)b;
    uint16_t *indices = g_sort_mesh->indices;
    xr_vec3_t *verts = g_sort_mesh->vertices;
    int i0 = ia*3, i1 = ib*3;
    xr_vec3_t v0a = verts[indices[i0]];
    xr_vec3_t v1a = verts[indices[i0+1]];
    xr_vec3_t v2a = verts[indices[i0+2]];
    xr_vec3_t v0b = verts[indices[i1]];
    xr_vec3_t v1b = verts[indices[i1+1]];
    xr_vec3_t v2b = verts[indices[i1+2]];
    xr_vec3_t ca_vec = v_add(v_add(v0a, v1a), v2a);
    xr_vec3_t cb_vec = v_add(v_add(v0b, v1b), v2b);
    float ca, cb;
    if (g_sort_axis == 0) {
        ca = ca_vec.x; cb = cb_vec.x;
    } else if (g_sort_axis == 1) {
        ca = ca_vec.y; cb = cb_vec.y;
    } else {
        ca = ca_vec.z; cb = cb_vec.z;
    }
    if (ca < cb) return -1;
    if (ca > cb) return 1;
    return 0;
}

static int build_bvh_recursive(mesh_t *mesh, int *tri_indices, int start, int count) {
    xr_vec3_t min = {1e9f,1e9f,1e9f};
    xr_vec3_t max = {-1e9f,-1e9f,-1e9f};
    for (int i = 0; i < count; i++) {
        int t = tri_indices[start + i];
        uint16_t *idx = &mesh->indices[t*3];
        xr_vec3_t v0 = mesh->vertices[idx[0]];
        xr_vec3_t v1 = mesh->vertices[idx[1]];
        xr_vec3_t v2 = mesh->vertices[idx[2]];
        if (v0.x < min.x) min.x = v0.x; if (v0.y < min.y) min.y = v0.y; if (v0.z < min.z) min.z = v0.z;
        if (v0.x > max.x) max.x = v0.x; if (v0.y > max.y) max.y = v0.y; if (v0.z > max.z) max.z = v0.z;
        if (v1.x < min.x) min.x = v1.x; if (v1.y < min.y) min.y = v1.y; if (v1.z < min.z) min.z = v1.z;
        if (v1.x > max.x) max.x = v1.x; if (v1.y > max.y) max.y = v1.y; if (v1.z > max.z) max.z = v1.z;
        if (v2.x < min.x) min.x = v2.x; if (v2.y < min.y) min.y = v2.y; if (v2.z < min.z) min.z = v2.z;
        if (v2.x > max.x) max.x = v2.x; if (v2.y > max.y) max.y = v2.y; if (v2.z > max.z) max.z = v2.z;
    }

    int node_idx = mesh->bvh_node_count++;
    if (node_idx >= mesh->bvh_node_capacity) {
        mesh->bvh_node_capacity = (mesh->bvh_node_capacity == 0) ? 4 : mesh->bvh_node_capacity * 2;
        mesh->bvh_nodes = (bvh_node_t*)realloc(mesh->bvh_nodes, sizeof(bvh_node_t) * mesh->bvh_node_capacity);
    }
    bvh_node_t *node = &mesh->bvh_nodes[node_idx];
    node->min = min;
    node->max = max;
    node->left = -1;
    node->right = -1;

    if (count <= BVH_LEAF_MAX_TRI) {
        node->tri_start = start;
        node->tri_count = count;
        return node_idx;
    }

    float ex = max.x - min.x;
    float ey = max.y - min.y;
    float ez = max.z - min.z;
    int axis = (ex >= ey && ex >= ez) ? 0 : (ey >= ez) ? 1 : 2;
    g_sort_mesh = mesh;
    g_sort_axis = axis;
    qsort(tri_indices + start, count, sizeof(int), cmp_tri);

    int mid = start + count/2;
    if (mid == start) mid++;
    if (mid == start + count) mid--;

    int left = build_bvh_recursive(mesh, tri_indices, start, mid - start);
    int right = build_bvh_recursive(mesh, tri_indices, mid, start + count - mid);
    node->left = left;
    node->right = right;
    node->tri_start = 0;
    node->tri_count = 0;
    return node_idx;
}

static void build_mesh_bvh(mesh_t *mesh) {
    if (mesh->num_triangles <= BVH_LEAF_MAX_TRI) {
        mesh->bvh_node_count = 1;
        mesh->bvh_nodes = (bvh_node_t*)malloc(sizeof(bvh_node_t));
        xr_vec3_t min = {1e9f,1e9f,1e9f};
        xr_vec3_t max = {-1e9f,-1e9f,-1e9f};
        for (int t = 0; t < mesh->num_triangles; t++) {
            uint16_t *idx = &mesh->indices[t*3];
            xr_vec3_t v0 = mesh->vertices[idx[0]];
            xr_vec3_t v1 = mesh->vertices[idx[1]];
            xr_vec3_t v2 = mesh->vertices[idx[2]];
            if (v0.x < min.x) min.x = v0.x; if (v0.y < min.y) min.y = v0.y; if (v0.z < min.z) min.z = v0.z;
            if (v0.x > max.x) max.x = v0.x; if (v0.y > max.y) max.y = v0.y; if (v0.z > max.z) max.z = v0.z;
            if (v1.x < min.x) min.x = v1.x; if (v1.y < min.y) min.y = v1.y; if (v1.z < min.z) min.z = v1.z;
            if (v1.x > max.x) max.x = v1.x; if (v1.y > max.y) max.y = v1.y; if (v1.z > max.z) max.z = v1.z;
            if (v2.x < min.x) min.x = v2.x; if (v2.y < min.y) min.y = v2.y; if (v2.z < min.z) min.z = v2.z;
            if (v2.x > max.x) max.x = v2.x; if (v2.y > max.y) max.y = v2.y; if (v2.z > max.z) max.z = v2.z;
        }
        mesh->bvh_nodes[0].min = min;
        mesh->bvh_nodes[0].max = max;
        mesh->bvh_nodes[0].left = -1;
        mesh->bvh_nodes[0].right = -1;
        mesh->bvh_nodes[0].tri_start = 0;
        mesh->bvh_nodes[0].tri_count = mesh->num_triangles;
        mesh->bvh_root = 0;
        return;
    }

    int *tri_indices = (int*)malloc(sizeof(int) * mesh->num_triangles);
    for (int i = 0; i < mesh->num_triangles; i++) tri_indices[i] = i;

    mesh->bvh_node_count = 0;
    mesh->bvh_node_capacity = 0;
    mesh->bvh_nodes = NULL;
    mesh->bvh_root = build_bvh_recursive(mesh, tri_indices, 0, mesh->num_triangles);

    uint16_t *new_indices = (uint16_t*)malloc(sizeof(uint16_t) * mesh->num_triangles * 3);
    for (int i = 0; i < mesh->num_triangles; i++) {
        int src = tri_indices[i] * 3;
        new_indices[i*3] = mesh->indices[src];
        new_indices[i*3+1] = mesh->indices[src+1];
        new_indices[i*3+2] = mesh->indices[src+2];
    }
    free(mesh->indices);
    mesh->indices = new_indices;
    free(tri_indices);
}

static bool intersect_bvh(mesh_t *mesh, xr_vec3_t orig, xr_vec3_t dir,
                          float *t_out, xr_vec3_t *n_out,
                          float *out_u, float *out_v) {
    if (mesh->bvh_root < 0) return false;
    #define STACK_SIZE 64
    int stack[STACK_SIZE];
    int top = 0;
    stack[top++] = mesh->bvh_root;
    float t_min = 1e9f;
    bool hit = false;
    float best_u = 0.0f, best_v = 0.0f;

    while (top > 0) {
        int node_idx = stack[--top];
        bvh_node_t *node = &mesh->bvh_nodes[node_idx];
        float t_box;
        xr_vec3_t n_dummy;
        if (!ray_aabb_intersect(orig, dir, (aabb_t){node->min, node->max}, &t_box, &n_dummy))
            continue;
        if (t_box > t_min) continue;

        if (node->left == -1) {
            for (int i = 0; i < node->tri_count; i++) {
                int tri = node->tri_start + i;
                uint16_t *idx = &mesh->indices[tri*3];
                xr_vec3_t v0 = mesh->vertices[idx[0]];
                xr_vec3_t v1 = mesh->vertices[idx[1]];
                xr_vec3_t v2 = mesh->vertices[idx[2]];
                float t_tri; xr_vec3_t n_tri;
                float u, v;
                if (ray_triangle_intersect(orig, dir, v0, v1, v2, &t_tri, &n_tri, &u, &v)) {
                    if (t_tri > EPSILON && t_tri < t_min) {
                        if (g_backface_culling_enabled && v_dot(n_tri, dir) >= 0.0f) {
                            continue;
                        }
                        t_min = t_tri;
                        *n_out = n_tri;
                        best_u = u;
                        best_v = v;
                        hit = true;
                    }
                }
            }
        } else {
            stack[top++] = node->right;
            stack[top++] = node->left;
        }
    }
    if (hit) {
        *t_out = t_min;
        if (out_u) *out_u = best_u;
        if (out_v) *out_v = best_v;
    }
    return hit;
}

/* ========================== Scene intersection ========================== */
static bool scene_intersect(xr_vec3_t origin, xr_vec3_t dir, float *t_out, xr_vec3_t *n_out,
                            bool *is_light, bool *is_glass, bool *is_edge, bool *is_table,
                            xr_material_t **out_mat, xr_texture_t **out_tex, float *out_u, float *out_v) {
    float t_min = 1e9f;
    bool hit = false;
    *is_light = false; *is_glass = false; *is_edge = false; *is_table = false;
    xr_vec3_t N = {0,0,0};
    xr_material_t *mat_ptr = NULL;
    xr_texture_t *tex_ptr = NULL;
    float u_uv = 0.0f, v_uv = 0.0f;

    float xmin = g_boundary[0], xmax = g_boundary[1];
    float ymin = g_boundary[2], ymax = g_boundary[3];
    float zmin = g_boundary[4], zmax = g_boundary[5];

    float t;
    if (g_wall_enabled[0] && dir.x > 1e-5f) {
        t = (xmax - origin.x) / dir.x;
        if (t > EPSILON && t < t_min) {
            xr_vec3_t N_tmp = {-1,0,0};
            if (!(g_backface_culling_enabled && v_dot(N_tmp, dir) >= 0.0f)) {
                t_min = t; N = N_tmp; hit = true; mat_ptr = &g_wall_materials[0];
            }
        }
    }
    if (g_wall_enabled[1] && dir.x < -1e-5f) {
        t = (xmin - origin.x) / dir.x;
        if (t > EPSILON && t < t_min) {
            xr_vec3_t N_tmp = {1,0,0};
            if (!(g_backface_culling_enabled && v_dot(N_tmp, dir) >= 0.0f)) {
                t_min = t; N = N_tmp; hit = true; mat_ptr = &g_wall_materials[1];
            }
        }
    }
    if (g_wall_enabled[2] && dir.y > 1e-5f) {
        t = (ymax - origin.y) / dir.y;
        if (t > EPSILON && t < t_min) {
            xr_vec3_t N_tmp = {0,-1,0};
            if (!(g_backface_culling_enabled && v_dot(N_tmp, dir) >= 0.0f)) {
                xr_vec3_t P = v_add(origin, v_mul(dir, t));
                float half = g_light_half_size;
                float cx = g_light_center.x, cz = g_light_center.z;
                *is_light = (P.x >= cx - half && P.x <= cx + half &&
                             P.z >= cz - half && P.z <= cz + half);
                t_min = t; N = N_tmp; hit = true; mat_ptr = &g_wall_materials[2];
            }
        }
    }
    if (g_wall_enabled[3] && dir.y < -1e-5f) {
        t = (ymin - origin.y) / dir.y;
        if (t > EPSILON && t < t_min) {
            xr_vec3_t N_tmp = {0,1,0};
            if (!(g_backface_culling_enabled && v_dot(N_tmp, dir) >= 0.0f)) {
                t_min = t; N = N_tmp; hit = true; mat_ptr = &g_floor_material;
            }
        }
    }
    if (g_wall_enabled[4] && dir.z > 1e-5f) {
        t = (zmax - origin.z) / dir.z;
        if (t > EPSILON && t < t_min) {
            xr_vec3_t N_tmp = {0,0,-1};
            if (!(g_backface_culling_enabled && v_dot(N_tmp, dir) >= 0.0f)) {
                t_min = t; N = N_tmp; hit = true; mat_ptr = &g_wall_materials[4];
            }
        }
    }
    if (g_wall_enabled[5] && dir.z < -1e-5f) {
        t = (zmin - origin.z) / dir.z;
        if (t > EPSILON && t < t_min) {
            xr_vec3_t N_tmp = {0,0,1};
            if (!(g_backface_culling_enabled && v_dot(N_tmp, dir) >= 0.0f)) {
                t_min = t; N = N_tmp; hit = true; mat_ptr = &g_wall_materials[5];
            }
        }
    }

    for (int i = 0; i < g_table_aabb_count; i++) {
        float t_box; xr_vec3_t n_box;
        if (ray_aabb_intersect(origin, dir, g_table_aabbs[i], &t_box, &n_box)) {
            if (t_box < t_min) {
                if (g_backface_culling_enabled && v_dot(n_box, dir) >= 0.0f) {
                    continue;
                }
                t_min = t_box; N = n_box; hit = true;
                *is_table = true;
                *is_light = false; *is_glass = false; *is_edge = false;
                mat_ptr = NULL; tex_ptr = NULL;
            }
        }
    }

    for (int i = 0; i < g_obj_count; i++) {
        scene_object_t *obj = &g_objects[i];
        switch (obj->type) {
            case OBJ_CUBE: {
                xr_vec3_t rel = v_sub(origin, obj->center);
                xr_vec3_t local_orig = apply_inverse_euler(rel, obj->rot);
                xr_vec3_t local_dir = apply_inverse_euler(dir, obj->rot);
                float half = obj->size;
                float t_near = -1e9f, t_far = 1e9f;
                int hit_axis = -1, hit_sign = 0;
                for (int j = 0; j < 3; j++) {
                    float o = (j==0)?local_orig.x:((j==1)?local_orig.y:local_orig.z);
                    float d = (j==0)?local_dir.x:((j==1)?local_dir.y:local_dir.z);
                    if (fabsf(d) < 1e-5f) {
                        if (o < -half || o > half) { t_near = 1e9f; break; }
                    } else {
                        float t1 = (-half - o) / d;
                        float t2 = (half - o) / d;
                        if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
                        if (t1 > t_near) { t_near = t1; hit_axis = j; hit_sign = (d > 0) ? -1 : 1; }
                        if (t2 < t_far) t_far = t2;
                    }
                }
                if (t_near < t_far && t_near > EPSILON && t_near < t_min) {
                    xr_vec3_t local_n = {0,0,0};
                    if (hit_axis == 0) local_n.x = hit_sign;
                    else if (hit_axis == 1) local_n.y = hit_sign;
                    else local_n.z = hit_sign;
                    N = apply_transform(local_n, (xr_vec3_t){0,0,0}, obj->rot, (xr_vec3_t){1,1,1});
                    N = v_norm(N);
                    if (g_backface_culling_enabled && v_dot(N, dir) >= 0.0f) {
                        break;
                    }
                    t_min = t_near; hit = true;
                    *is_light = false; *is_glass = false; *is_edge = false; *is_table = false;
                    mat_ptr = &obj->mat; tex_ptr = NULL;
                }
                break;
            }
            case OBJ_PYRAMID: {
                xr_vec3_t rel = v_sub(origin, obj->center);
                xr_vec3_t local_orig = apply_inverse_euler(rel, obj->rot);
                xr_vec3_t local_dir = apply_inverse_euler(dir, obj->rot);
                static const xr_vec3_t pyr_normals[4] = {
                    {-0.57735f,-0.57735f,-0.57735f},
                    {-0.57735f, 0.57735f, 0.57735f},
                    { 0.57735f,-0.57735f, 0.57735f},
                    { 0.57735f, 0.57735f,-0.57735f}
                };
                float pyr_d = 7.5f;
                float t_near = -1e9f, t_far = 1e9f;
                int hit_face = -1;
                for (int j = 0; j < 4; j++) {
                    float NdotD = v_dot(pyr_normals[j], local_dir);
                    float NdotO = v_dot(pyr_normals[j], local_orig);
                    if (fabsf(NdotD) < 1e-5f) {
                        if (NdotO > pyr_d) { t_near = 1e9f; break; }
                    } else {
                        float t_plane = (pyr_d - NdotO) / NdotD;
                        if (NdotD > 0.0f) {
                            if (t_plane < t_far) t_far = t_plane;
                        } else {
                            if (t_plane > t_near) { t_near = t_plane; hit_face = j; }
                        }
                    }
                }
                if (t_near < t_far && t_near > EPSILON && t_near < t_min) {
                    xr_vec3_t local_n = pyr_normals[hit_face];
                    N = apply_transform(local_n, (xr_vec3_t){0,0,0}, obj->rot, (xr_vec3_t){1,1,1});
                    N = v_norm(N);
                    if (g_backface_culling_enabled && v_dot(N, dir) >= 0.0f) {
                        break;
                    }
                    t_min = t_near; hit = true;
                    *is_light = false; *is_glass = false; *is_edge = false; *is_table = false;
                    mat_ptr = &obj->mat; tex_ptr = NULL;
                }
                break;
            }
            case OBJ_TEXTURED_PLANE: {
                xr_vec3_t plane_norm = apply_transform(obj->plane_normal, (xr_vec3_t){0,0,0}, obj->rot, (xr_vec3_t){1,1,1});
                plane_norm = v_norm(plane_norm);
                float denom = v_dot(plane_norm, dir);
                if (fabsf(denom) < 1e-6f) break;
                float t_plane = v_dot(v_sub(obj->center, origin), plane_norm) / denom;
                if (t_plane < EPSILON || t_plane >= t_min) break;
                xr_vec3_t P = v_add(origin, v_mul(dir, t_plane));
                xr_vec3_t local = v_sub(P, obj->center);
                xr_vec3_t ref = (fabsf(plane_norm.y) < 0.9f) ? (xr_vec3_t){0,1,0} : (xr_vec3_t){1,0,0};
                xr_vec3_t u_axis = v_norm(v_cross(ref, plane_norm));
                xr_vec3_t v_axis = v_cross(plane_norm, u_axis);
                float lx = v_dot(local, u_axis);
                float ly = v_dot(local, v_axis);
                if (fabsf(lx) <= obj->plane_width/2.0f && fabsf(ly) <= obj->plane_height/2.0f) {
                    N = plane_norm;
                    if (g_backface_culling_enabled && v_dot(N, dir) >= 0.0f) {
                        break;
                    }
                    t_min = t_plane; hit = true;
                    *is_light = false; *is_glass = false; *is_edge = false; *is_table = false;
                    mat_ptr = &obj->mat;
                    tex_ptr = &obj->texture;
                    u_uv = (lx / obj->plane_width) + 0.5f;
                    v_uv = (ly / obj->plane_height) + 0.5f;

                    // Texture UV rotation with aspect-ratio correction
                    if (obj->tex_rotation != 0.0f && tex_ptr != NULL && tex_ptr->data != NULL) {
                        float aspect = (float)tex_ptr->width / (float)tex_ptr->height;
                        if (aspect > 0.0f) {
                            float u_cent = u_uv - 0.5f;
                            float v_cent = v_uv - 0.5f;
                            u_cent *= aspect;
                            float cosA = cosf(obj->tex_rotation);
                            float sinA = sinf(obj->tex_rotation);
                            float u_rot = u_cent * cosA - v_cent * sinA;
                            float v_rot = u_cent * sinA + v_cent * cosA;
                            u_rot /= aspect;
                            u_uv = u_rot + 0.5f;
                            v_uv = v_rot + 0.5f;
                        }
                    }
                    if (obj->tex_flip_h) u_uv = 1.0f - u_uv;
                    if (obj->tex_flip_v) v_uv = 1.0f - v_uv;
                }
                break;
            }
            case OBJ_SPHERE: {
                float t_sph; xr_vec3_t n_sph;
                if (ray_sphere_intersect(origin, dir, obj->center, obj->size, &t_sph, &n_sph)) {
                    if (t_sph > EPSILON && t_sph < t_min) {
                        if (g_backface_culling_enabled && v_dot(n_sph, dir) >= 0.0f) {
                            break;
                        }
                        t_min = t_sph;
                        N = n_sph;
                        hit = true;
                        *is_light = false; *is_glass = false; *is_edge = false; *is_table = false;
                        mat_ptr = &obj->mat; tex_ptr = NULL;
                    }
                }
                break;
            }
            case OBJ_TRIANGLE: {
                float t_tri; xr_vec3_t n_tri;
                if (ray_triangle_intersect(origin, dir, obj->tri[0], obj->tri[1], obj->tri[2], &t_tri, &n_tri, NULL, NULL)) {
                    if (t_tri > EPSILON && t_tri < t_min) {
                        if (g_backface_culling_enabled && v_dot(n_tri, dir) >= 0.0f) {
                            break;
                        }
                        t_min = t_tri;
                        N = n_tri;
                        hit = true;
                        *is_light = false; *is_glass = false; *is_edge = false; *is_table = false;
                        mat_ptr = &obj->mat; tex_ptr = NULL;
                    }
                }
                break;
            }
            case OBJ_PLANE: {
                xr_vec3_t plane_norm = apply_transform(obj->plane_normal, (xr_vec3_t){0,0,0}, obj->rot, (xr_vec3_t){1,1,1});
                plane_norm = v_norm(plane_norm);
                float denom = v_dot(plane_norm, dir);
                if (fabsf(denom) < 1e-6f) break;
                float t_plane = v_dot(v_sub(obj->center, origin), plane_norm) / denom;
                if (t_plane < EPSILON || t_plane >= t_min) break;
                xr_vec3_t P = v_add(origin, v_mul(dir, t_plane));
                xr_vec3_t local = v_sub(P, obj->center);
                xr_vec3_t ref = (fabsf(plane_norm.y) < 0.9f) ? (xr_vec3_t){0,1,0} : (xr_vec3_t){1,0,0};
                xr_vec3_t u_axis = v_norm(v_cross(ref, plane_norm));
                xr_vec3_t v_axis = v_cross(plane_norm, u_axis);
                float lx = v_dot(local, u_axis);
                float ly = v_dot(local, v_axis);
                if (fabsf(lx) <= obj->plane_width/2.0f && fabsf(ly) <= obj->plane_height/2.0f) {
                    N = plane_norm;
                    if (g_backface_culling_enabled && v_dot(N, dir) >= 0.0f) {
                        break;
                    }
                    t_min = t_plane; hit = true;
                    *is_light = false; *is_glass = false; *is_edge = false; *is_table = false;
                    mat_ptr = &obj->mat; tex_ptr = NULL;
                }
                break;
            }
            default: break;
        }
    }

    for (int i = 0; i < g_glass_count; i++) {
        glass_panel_t *panel = &g_glass_panels[i];
        for (int side = -1; side <= 1; side += 2) {
            xr_vec3_t surf_norm = v_mul(panel->normal, (float)side);
            xr_vec3_t surf_center = v_add(panel->center, v_mul(panel->normal, side * panel->half_thickness));
            float denom = v_dot(surf_norm, dir);
            if (fabsf(denom) < 1e-6f) continue;
            float t_plane = v_dot(v_sub(surf_center, origin), surf_norm) / denom;
            if (t_plane < EPSILON || t_plane >= t_min) continue;
            xr_vec3_t P = v_add(origin, v_mul(dir, t_plane));
            xr_vec3_t local = v_sub(P, panel->center);
            bool inside = false;
            if (fabsf(panel->normal.x) > 0.99f) {
                inside = (fabsf(local.y) <= panel->half_height && fabsf(local.z) <= panel->half_width);
            } else {
                inside = (fabsf(local.x) <= panel->half_width && fabsf(local.y) <= panel->half_height);
            }
            if (inside) {
                t_min = t_plane; hit = true;
                N = surf_norm;
                *is_glass = true;
                *is_light = false; *is_edge = false; *is_table = false;
                mat_ptr = NULL; tex_ptr = NULL;
                if (v_dot(surf_norm, dir) < 0) {
                    float border = 1.2f;
                    if (fabsf(panel->normal.x) > 0.99f) {
                        *is_edge = (fabsf(local.y) > panel->half_height - border || fabsf(local.z) > panel->half_width - border);
                    } else {
                        *is_edge = (fabsf(local.x) > panel->half_width - border || fabsf(local.y) > panel->half_height - border);
                    }
                } else {
                    *is_edge = false;
                }
            }
        }
    }

    // 遍历网格
    for (int i = 0; i < g_mesh_count; i++) {
        mesh_t *mesh = &g_meshes[i];
        if (g_bvh_enabled && mesh->bvh_root >= 0) {
            float t_tri; xr_vec3_t n_tri;
            float u, v;
            if (intersect_bvh(mesh, origin, dir, &t_tri, &n_tri, &u, &v)) {
                if (t_tri > EPSILON && t_tri < t_min) {
                    t_min = t_tri;
                    N = n_tri;
                    hit = true;
                    *is_light = false; *is_glass = false; *is_edge = false; *is_table = false;
                    mat_ptr = &mesh->mat;
                    // 处理纹理
                    if (mesh->texture.data != NULL) {
                        tex_ptr = &mesh->texture;
                        u_uv = u;
                        v_uv = v;
                    } else {
                        tex_ptr = NULL;
                    }
                }
            }
        } else {
            for (int t_idx = 0; t_idx < mesh->num_triangles; t_idx++) {
                uint16_t i0 = mesh->indices[t_idx*3];
                uint16_t i1 = mesh->indices[t_idx*3+1];
                uint16_t i2 = mesh->indices[t_idx*3+2];
                xr_vec3_t v0 = mesh->vertices[i0];
                xr_vec3_t v1 = mesh->vertices[i1];
                xr_vec3_t v2 = mesh->vertices[i2];
                float t_tri; xr_vec3_t n_tri;
                float u, v;
                if (ray_triangle_intersect(origin, dir, v0, v1, v2, &t_tri, &n_tri, &u, &v)) {
                    if (t_tri > EPSILON && t_tri < t_min) {
                        if (g_backface_culling_enabled && v_dot(n_tri, dir) >= 0.0f) {
                            continue;
                        }
                        t_min = t_tri;
                        N = n_tri;
                        hit = true;
                        *is_light = false; *is_glass = false; *is_edge = false; *is_table = false;
                        mat_ptr = &mesh->mat;
                        if (mesh->texture.data != NULL) {
                            tex_ptr = &mesh->texture;
                            u_uv = u;
                            v_uv = v;
                        } else {
                            tex_ptr = NULL;
                        }
                    }
                }
            }
        }
    }

    *t_out = t_min;
    *n_out = N;
    if (out_mat) *out_mat = mat_ptr;
    if (out_tex) *out_tex = tex_ptr;
    if (out_u) *out_u = u_uv;
    if (out_v) *out_v = v_uv;
    return hit;
}

/* ========================== Rendering ========================== */
static void render_tile(int start_x, int start_y, int w, int h, uint16_t *tile_buf, uint16_t *framebuffer) {
    xr_vec3_t forward = v_norm(v_sub(g_cam_target, g_cam_pos));
    xr_vec3_t right = v_norm(v_cross(g_cam_up, forward));
    xr_vec3_t up_actual = v_norm(v_cross(forward, right));

    float fov_scale = 1.5f;
    uint32_t rng_state = 0;
    int end_x = start_x + w;
    int end_y = start_y + h;

    for (int y = start_y; y < end_y; y++) {
        float v_base = ((g_height - 2.0f * y) / g_height) * fov_scale;
        for (int x = start_x; x < end_x; x++) {
            float u_base = ((2.0f * x - g_width) / g_width) * fov_scale;
            xr_vec3_t total_color = {0,0,0};

            for (int s = 0; s < g_aa_samples; s++) {
                float u = u_base;
                float v = v_base;
                xr_vec3_t D = v_norm(v_add(v_add(forward, v_mul(right, u)), v_mul(up_actual, v)));
                xr_vec3_t color = {0,0,0};
                xr_vec3_t ray_origin = g_cam_pos;
                xr_vec3_t ray_dir = D;
                float reflection_factor = 1.0f;

                for (int bounce = 0; bounce < g_max_bounces; bounce++) {
                    float t_hit;
                    xr_vec3_t normal;
                    bool is_light = false, is_glass = false, is_edge = false, is_table = false;
                    xr_material_t *mat = NULL;
                    xr_texture_t *tex = NULL;
                    float uv_u = 0.0f, uv_v = 0.0f;
                    if (!scene_intersect(ray_origin, ray_dir, &t_hit, &normal, &is_light, &is_glass, &is_edge, &is_table,
                                         &mat, &tex, &uv_u, &uv_v))
                        break;

                    xr_vec3_t P = v_add(ray_origin, v_mul(ray_dir, t_hit));

                    if (is_light) {
                        color = v_add(color, v_mul((xr_vec3_t){1,1,1}, reflection_factor));
                        break;
                    } else if (is_glass) {
                        if (is_edge && bounce == 0) {
                            xr_vec3_t L = v_norm(v_sub(g_light_pos, P));
                            float diff = fmaxf(v_dot(normal, L), 0.0f);
                            float ambient = 0.7f;
                            float intensity = ambient + (1.0f - ambient) * diff;
                            color = v_add(color, v_mul((xr_vec3_t){1,1,1}, intensity));
                            break;
                        }
                        float dist = 2.0f * 4.0f + g_bias;
                        float attenuation = 0.9f;
                        reflection_factor *= attenuation;
                        xr_vec3_t new_dir = ray_dir;
                        ray_origin = v_add(P, v_mul(new_dir, dist));
                        ray_dir = new_dir;
                        continue;
                    } else {
                        xr_vec3_t L = v_norm(v_sub(g_light_pos, P));
                        float dist_to_light = sqrtf(v_dot(v_sub(g_light_pos, P), v_sub(g_light_pos, P)));

                        bool in_shadow = false;
                        if (g_shadow_enabled) {
                            xr_vec3_t shadow_origin = v_add(P, v_mul(normal, g_bias));
                            xr_vec3_t shadow_dir = L;
                            float remaining_dist = dist_to_light;
                            bool table_dummy = false;
                            while (remaining_dist > EPSILON) {
                                float t_shad;
                                xr_vec3_t n_dummy;
                                bool g_dummy = false, l_dummy = false, glass_dummy = false, edge_dummy = false;
                                if (!scene_intersect(shadow_origin, shadow_dir, &t_shad, &n_dummy,
                                                     &l_dummy, &glass_dummy, &edge_dummy, &table_dummy,
                                                     NULL, NULL, NULL, NULL))
                                    break;
                                if (l_dummy) break;
                                if (t_shad > remaining_dist + g_bias) break;
                                if (glass_dummy) {
                                    if (!edge_dummy) {
                                        shadow_origin = v_add(shadow_origin, v_mul(shadow_dir, t_shad + g_bias));
                                        remaining_dist -= t_shad;
                                        continue;
                                    } else {
                                        in_shadow = true;
                                        break;
                                    }
                                } else {
                                    in_shadow = true;
                                    break;
                                }
                            }
                        }

                        xr_vec3_t albedo;
                        float roughness;

                        if (is_table) {
                            float scale = 0.5f;
                            float u_tex = P.x * scale;
                            float v_tex = P.z * scale;
                            float ring = sinf(u_tex*20.0f + v_tex*10.0f)*0.5f + 0.5f;
                            float grain = sinf(u_tex*50.0f + v_tex*30.0f)*0.1f;
                            float wood = fminf(fmaxf(ring + grain, 0.0f), 1.0f);
                            xr_vec3_t light_wood = {0.55f,0.27f,0.07f};
                            xr_vec3_t dark_wood = {0.3f,0.15f,0.04f};
                            albedo.x = light_wood.x*wood + dark_wood.x*(1.0f-wood);
                            albedo.y = light_wood.y*wood + dark_wood.y*(1.0f-wood);
                            albedo.z = light_wood.z*wood + dark_wood.z*(1.0f-wood);
                            roughness = 0.3f;
                        } else if (tex) {
                            albedo = sample_texture(tex, uv_u, uv_v);
                            float alpha = mat ? fmaxf(0.0f, fminf(1.0f, mat->roughness)) : 1.0f;
                            if (alpha < 1.0f) {
                                float diff = fmaxf(v_dot(normal, L), 0.0f);
                                float intensity = 0.2f + 0.6f * diff;
                                if (in_shadow) intensity = 0.2f;
                                xr_vec3_t light_color = v_mul(albedo, intensity);
                                color = v_add(v_mul(color, 1.0f - alpha), v_mul(light_color, alpha));
                                ray_origin = v_add(P, v_mul(ray_dir, g_bias));
                                continue;
                            } else {
                                roughness = 1.0f;
                            }
                        } else if (mat) {
                            albedo = mat->albedo;
                            roughness = mat->roughness;
                        } else {
                            albedo = (xr_vec3_t){0.8f,0.8f,0.8f};
                            roughness = 0.8f;
                        }

                        if (!tex && mat == &g_floor_material && g_floor_checker) {
                            float tile_size = 5.0f;
                            int ix = (int)floorf(P.x / tile_size);
                            int iz = (int)floorf(P.z / tile_size);
                            if ((ix + iz) % 2 == 0) albedo = (xr_vec3_t){0.2f,0.2f,0.2f};
                            else albedo = (xr_vec3_t){0.8f,0.8f,0.8f};
                        }

                        float intensity = 0.2f;
                        if (!in_shadow) {
                            intensity += 0.6f * fmaxf(v_dot(normal, L), 0.0f);
                            float spec_exp = 5.0f + (1.0f - roughness) * 128.0f;
                            float spec_strength = (1.0f - roughness);
                            xr_vec3_t V = v_norm(v_sub(g_cam_pos, P));
                            xr_vec3_t H = v_norm(v_add(L, V));
                            intensity += 0.6f * spec_strength * powf(fmaxf(v_dot(normal, H), 0.0f), spec_exp);
                        }
                        color = v_add(color, v_mul(albedo, intensity * reflection_factor));

                        xr_vec3_t new_dir = v_reflect(ray_dir, normal);
                        if (g_microsurface_enabled && roughness > 0.01f) {
                            float alpha = roughness * roughness;
                            float r1 = (float)splitmix32(&rng_state) / (float)0xFFFFFFFF;
                            float r2 = (float)splitmix32(&rng_state) / (float)0xFFFFFFFF;
                            float denom = 1.0f - r1;
                            if (denom < 1e-6f) denom = 1e-6f;
                            float theta = atanf(alpha * sqrtf(r1) / sqrtf(denom));
                            float phi = 2.0f * M_PI * r2;
                            xr_vec3_t up_vec = (fabsf(normal.y) < 0.999f) ? (xr_vec3_t){0,1,0} : (xr_vec3_t){1,0,0};
                            xr_vec3_t tangent = v_norm(v_cross(up_vec, normal));
                            xr_vec3_t bitangent = v_cross(normal, tangent);
                            xr_vec3_t h = v_add(v_add(v_mul(tangent, sinf(theta) * cosf(phi)),
                                                      v_mul(bitangent, sinf(theta) * sinf(phi))),
                                                v_mul(normal, cosf(theta)));
                            h = v_norm(h);
                            new_dir = v_reflect(ray_dir, h);
                            new_dir = v_norm(new_dir);
                        }

                        ray_dir = new_dir;
                        ray_origin = v_add(P, v_mul(normal, g_bias));
                        float lightdec = (mat) ? 0.7f : LIGHT_DEC_FACTOR;
                        reflection_factor *= lightdec * (1.0f - roughness * 0.8f);
                    }
                }
                total_color = v_add(total_color, color);
            }
            total_color.x /= g_aa_samples;
            total_color.y /= g_aa_samples;
            total_color.z /= g_aa_samples;
            int local_x = x - start_x;
            int local_y = y - start_y;
            tile_buf[local_y * w + local_x] = float_to_rgb565(total_color);
        }
    }
}

void xr_render(uint16_t *framebuffer) {
    xr_render_region(0, 0, g_width, g_height, framebuffer);
}

void xr_render_region(int start_x, int start_y, int w, int h, uint16_t *framebuffer) {
    if (!framebuffer) return;

    if (start_x < 0) { w += start_x; start_x = 0; }
    if (start_y < 0) { h += start_y; start_y = 0; }
    if (start_x + w > g_width) w = g_width - start_x;
    if (start_y + h > g_height) h = g_height - start_y;
    if (w <= 0 || h <= 0) return;

    if (g_bvh_enabled) {
        for (int i = 0; i < g_mesh_count; i++) {
            mesh_t *mesh = &g_meshes[i];
            if (mesh->bvh_root == -1) {
                build_mesh_bvh(mesh);
            }
        }
    }

    uint16_t *tile_buf = g_tile_buffer;
    int tile_w = g_tile_buffer_size;
    int tile_h = g_tile_buffer_size;

    if (tile_buf == NULL || tile_w <= 0 || tile_w >= w || tile_h >= h) {
        render_tile(start_x, start_y, w, h, framebuffer + start_y * g_width + start_x, framebuffer);
        return;
    }

    for (int ty = start_y; ty < start_y + h; ty += tile_h) {
        int cur_h = (ty + tile_h > start_y + h) ? (start_y + h - ty) : tile_h;
        for (int tx = start_x; tx < start_x + w; tx += tile_w) {
            int cur_w = (tx + tile_w > start_x + w) ? (start_x + w - tx) : tile_w;
            render_tile(tx, ty, cur_w, cur_h, tile_buf, framebuffer);
            for (int y = 0; y < cur_h; y++) {
                memcpy(framebuffer + (ty + y) * g_width + tx,
                       tile_buf + y * tile_w,
                       cur_w * sizeof(uint16_t));
            }
        }
    }
}

/* ========================== API ========================== */

void xr_init(int width, int height, int max_bounces, int aa_samples) {
    g_width = width;
    g_height = height;
    g_max_bounces = max_bounces;
    g_aa_samples = (aa_samples > 0) ? aa_samples : 1;

    if (g_objects) free(g_objects);
    if (g_table_aabbs) free(g_table_aabbs);
    if (g_glass_panels) free(g_glass_panels);
    if (g_meshes) {
        for (int i = 0; i < g_mesh_count; i++) {
            free(g_meshes[i].vertices);
            free(g_meshes[i].uvs);
            free(g_meshes[i].indices);
            if (g_meshes[i].bvh_nodes) free(g_meshes[i].bvh_nodes);
        }
        free(g_meshes);
    }
    for (int i = 0; i < g_lock_group_count; i++) free(g_lock_groups[i].indices);
    free(g_lock_groups);

    g_objects = NULL;
    g_obj_count = 0;
    g_obj_capacity = 0;
    g_table_aabbs = NULL;
    g_table_aabb_count = 0;
    g_glass_panels = NULL;
    g_glass_count = 0;
    g_meshes = NULL;
    g_mesh_count = 0;
    g_mesh_capacity = 0;
    g_lock_groups = NULL;
    g_lock_group_count = 0;

    g_boundary[0] = -50.0f; g_boundary[1] = 50.0f;
    g_boundary[2] = -50.0f; g_boundary[3] = 50.0f;
    g_boundary[4] = -50.0f; g_boundary[5] = 50.0f;
    for (int i = 0; i < 6; i++) {
        g_wall_enabled[i] = true;
        g_wall_materials[i] = xr_mat_wall;
    }
    g_floor_material = xr_mat_floor;
    g_floor_checker = true;
    g_light_center = (xr_vec3_t){0.0f, 50.0f, 0.0f};
    g_light_half_size = 10.0f;

    g_cam_pos = {-40.0f, -20.0f, -45.0f};
    g_cam_target = {0.0f, -25.0f, 0.0f};
    g_cam_up = {0.0f, 1.0f, 0.0f};
    g_light_pos = {0.0f, 50.0f, 0.0f};
    g_light_color = {1.0f, 1.0f, 1.0f};
    g_bias = DEFAULT_BIAS;
    g_tile_size = DEFAULT_TILE_SIZE;
    g_shadow_enabled = false;
    g_microsurface_enabled = false;
    g_bvh_enabled = false;
    g_backface_culling_enabled = false;
}

void xr_set_boundary(float xmin, float xmax, float ymin, float ymax, float zmin, float zmax) {
    g_boundary[0] = xmin;
    g_boundary[1] = xmax;
    g_boundary[2] = ymin;
    g_boundary[3] = ymax;
    g_boundary[4] = zmin;
    g_boundary[5] = zmax;
}

void xr_set_wall_material(int face, xr_material_t mat) {
    if (face < 0 || face > 5) return;
    g_wall_materials[face] = mat;
}

void xr_enable_wall(int face, bool enable) {
    if (face < 0 || face > 5) return;
    g_wall_enabled[face] = enable;
}

void xr_set_floor_material(xr_material_t mat) {
    g_floor_material = mat;
}

void xr_set_floor_checker(bool enable) {
    g_floor_checker = enable;
}

void xr_set_light_region(xr_vec3_t center, float half_size) {
    g_light_center = center;
    g_light_half_size = half_size;
}

void xr_set_tile_buffer(uint16_t* buffer, int size) {
    g_tile_buffer = buffer;
    g_tile_buffer_size = size;
}

void xr_set_camera(xr_vec3_t pos, xr_vec3_t target, xr_vec3_t up) {
    g_cam_pos = pos;
    g_cam_target = target;
    g_cam_up = up;
}

void xr_set_light(xr_vec3_t pos, xr_vec3_t color) {
    g_light_pos = pos;
    g_light_color = color;
}

void xr_set_bias(float bias) { g_bias = bias; }
void xr_set_aa_samples(int samples) { g_aa_samples = (samples > 0) ? samples : 1; }
void xr_set_max_bounces(int bounces) { g_max_bounces = (bounces >= 0) ? bounces : 0; }

void xr_enable_shadow(bool enable) { g_shadow_enabled = enable; }
void xr_enable_microsurface(bool enable) { g_microsurface_enabled = enable; }

void xr_enable_bvh(bool enable) {
    g_bvh_enabled = enable;
    if (!enable) {
        for (int i = 0; i < g_mesh_count; i++) {
            mesh_t *mesh = &g_meshes[i];
            if (mesh->bvh_nodes) {
                free(mesh->bvh_nodes);
                mesh->bvh_nodes = NULL;
                mesh->bvh_root = -1;
                mesh->bvh_node_count = 0;
                mesh->bvh_node_capacity = 0;
            }
        }
    }
}

void xr_enable_backface_culling(bool enable) {
    g_backface_culling_enabled = enable;
}

xr_vec3_t xr_conv_angle_to_normal(float ax, float ay, float az) {
    xr_vec3_t p = {0.0f, 0.0f, 1.0f};
    float cz = cosf(az), sz = sinf(az);
    float x = p.x * cz - p.y * sz;
    float y = p.x * sz + p.y * cz;
    p.x = x; p.y = y;
    float cy = cosf(ay), sy = sinf(ay);
    x = p.x * cy + p.z * sy;
    float z = -p.x * sy + p.z * cy;
    p.x = x; p.z = z;
    float cx = cosf(ax), sx = sinf(ax);
    y = p.y * cx - p.z * sx;
    z = p.y * sx + p.z * cx;
    p.y = y; p.z = z;
    return v_norm(p);
}

static void ensure_object_capacity(int extra) {
    if (g_obj_count + extra > g_obj_capacity) {
        int new_cap = g_obj_capacity * 2 + 4;
        if (new_cap < g_obj_count + extra) new_cap = g_obj_count + extra;
        scene_object_t *new_objs = (scene_object_t*)realloc(g_objects, sizeof(scene_object_t) * new_cap);
        if (new_objs) {
            g_objects = new_objs;
            g_obj_capacity = new_cap;
        }
    }
}

int xr_add_cube(xr_vec3_t center, float size, float angle_rad, xr_material_t mat) {
    ensure_object_capacity(1);
    scene_object_t obj = { OBJ_CUBE, mat, center, size, {0, angle_rad, 0}, 0,0,0,0, {0,0,0},0,0,0,0, {0,0,0}, 0,0, {NULL,0,0}, false, false, {{0,0,0},{0,0,0},{0,0,0}}, 0.0f };
    g_objects[g_obj_count] = obj;
    return g_obj_count++;
}

int xr_add_pyramid(xr_vec3_t center, float size, float angle_rad, xr_material_t mat) {
    ensure_object_capacity(1);
    scene_object_t obj = { OBJ_PYRAMID, mat, center, size, {0, angle_rad, 0}, 0,0,0,0, {0,0,0},0,0,0,0, {0,0,0}, 0,0, {NULL,0,0}, false, false, {{0,0,0},{0,0,0},{0,0,0}}, 0.0f };
    g_objects[g_obj_count] = obj;
    return g_obj_count++;
}

int xr_add_sphere(xr_vec3_t center, float radius, xr_material_t mat) {
    ensure_object_capacity(1);
    scene_object_t obj = { OBJ_SPHERE, mat, center, radius, {0,0,0}, 0,0,0,0, {0,0,0},0,0,0,0, {0,0,0}, 0,0, {NULL,0,0}, false, false, {{0,0,0},{0,0,0},{0,0,0}}, 0.0f };
    g_objects[g_obj_count] = obj;
    return g_obj_count++;
}

int xr_add_table(xr_vec3_t center, float width, float depth, float height, float leg_size, xr_material_t mat) {
    float top_y_min = center.y - height/2.0f;
    float top_y_max = center.y + height/2.0f;
    float half_w = width/2.0f;
    float half_d = depth/2.0f;
    aabb_t top = { {center.x - half_w, top_y_min, center.z - half_d},
                   {center.x + half_w, top_y_max, center.z + half_d} };
    float leg_off_x = half_w - leg_size/2.0f;
    float leg_off_z = half_d - leg_size/2.0f;
    float leg_y_min = center.y - height - 18.0f;
    float leg_y_max = top_y_min;
    aabb_t legs[4] = {
        {{center.x - leg_off_x, leg_y_min, center.z - leg_off_z}, {center.x - leg_off_x + leg_size, leg_y_max, center.z - leg_off_z + leg_size}},
        {{center.x + leg_off_x - leg_size, leg_y_min, center.z - leg_off_z}, {center.x + leg_off_x, leg_y_max, center.z - leg_off_z + leg_size}},
        {{center.x - leg_off_x, leg_y_min, center.z + leg_off_z - leg_size}, {center.x - leg_off_x + leg_size, leg_y_max, center.z + leg_off_z}},
        {{center.x + leg_off_x - leg_size, leg_y_min, center.z + leg_off_z - leg_size}, {center.x + leg_off_x, leg_y_max, center.z + leg_off_z}}
    };
    int old_count = g_table_aabb_count;
    int new_count = old_count + 5;
    aabb_t *new_aabbs = (aabb_t*)realloc(g_table_aabbs, sizeof(aabb_t) * new_count);
    if (new_aabbs) {
        g_table_aabbs = new_aabbs;
        g_table_aabbs[old_count] = top;
        for (int i = 0; i < 4; i++) g_table_aabbs[old_count + 1 + i] = legs[i];
        g_table_aabb_count = new_count;
    }
    return -1;
}

int xr_add_glass_panel(xr_vec3_t center, xr_vec3_t normal, float half_width, float half_height, float thickness, float refractive_index) {
    int old_count = g_glass_count;
    int new_count = old_count + 1;
    glass_panel_t *new_panels = (glass_panel_t*)realloc(g_glass_panels, sizeof(glass_panel_t) * new_count);
    if (new_panels) {
        g_glass_panels = new_panels;
        g_glass_panels[old_count] = glass_panel_t{ center, v_norm(normal), half_width, half_height, thickness/2.0f };
        g_glass_count = new_count;
    }
    return -1;
}

int xr_add_textured_plane(xr_vec3_t center, xr_vec3_t normal, float width, float height,
                           xr_texture_t tex, xr_material_t mat) {
    ensure_object_capacity(1);
    scene_object_t obj = { OBJ_TEXTURED_PLANE, mat, center, 0, {0,0,0}, 0,0,0,0, {0,0,0},0,0,0,0, v_norm(normal), width, height, tex, false, false, {{0,0,0},{0,0,0},{0,0,0}}, 0.0f };
    g_objects[g_obj_count] = obj;
    return g_obj_count++;
}

// 修改后的 xr_add_mesh 支持 UV 和纹理
int xr_add_mesh(const xr_vec3_t* vertices, const xr_vec3_t* uvs, const uint16_t* indices,
                 int num_vertices, int num_triangles,
                 xr_vec3_t position, xr_vec3_t rotation_euler, xr_vec3_t scale,
                 xr_material_t mat, xr_texture_t tex) {
    if (g_mesh_count >= g_mesh_capacity) {
        int new_cap = g_mesh_capacity * 2 + 4;
        mesh_t *new_meshes = (mesh_t*)realloc(g_meshes, sizeof(mesh_t) * new_cap);
        if (!new_meshes) return -1;
        g_meshes = new_meshes;
        g_mesh_capacity = new_cap;
    }
    mesh_t *mesh = &g_meshes[g_mesh_count++];
    mesh->num_vertices = num_vertices;
    mesh->num_triangles = num_triangles;
    mesh->mat = mat;
    mesh->texture = tex;
    mesh->bvh_nodes = NULL;
    mesh->bvh_root = -1;
    mesh->bvh_node_count = 0;
    mesh->bvh_node_capacity = 0;

    mesh->vertices = (xr_vec3_t*)malloc(sizeof(xr_vec3_t) * num_vertices);
    mesh->indices = (uint16_t*)malloc(sizeof(uint16_t) * num_triangles * 3);
    if (uvs) {
        mesh->uvs = (xr_vec3_t*)malloc(sizeof(xr_vec3_t) * num_vertices);
    } else {
        mesh->uvs = NULL;
    }
    if (!mesh->vertices || !mesh->indices || (uvs && !mesh->uvs)) {
        free(mesh->vertices); free(mesh->indices); free(mesh->uvs);
        g_mesh_count--;
        return -1;
    }
    memcpy(mesh->indices, indices, sizeof(uint16_t) * num_triangles * 3);
    for (int i = 0; i < num_vertices; i++) {
        mesh->vertices[i] = apply_transform(vertices[i], position, rotation_euler, scale);
        if (uvs) {
            mesh->uvs[i] = uvs[i]; // 只复制 u,v，忽略 z
        }
    }
    return -1;
}

int xr_add_triangle(xr_vec3_t v0, xr_vec3_t v1, xr_vec3_t v2, xr_material_t mat) {
    ensure_object_capacity(1);
    scene_object_t obj = { OBJ_TRIANGLE, mat, {0,0,0}, 0, {0,0,0}, 0,0,0,0, {0,0,0},0,0,0,0, {0,0,0}, 0,0, {NULL,0,0}, false, false, {v0, v1, v2}, 0.0f };
    g_objects[g_obj_count] = obj;
    return g_obj_count++;
}

int xr_add_plane(xr_vec3_t center, xr_vec3_t normal, float width, float height, xr_material_t mat) {
    ensure_object_capacity(1);
    scene_object_t obj = { OBJ_PLANE, mat, center, 0, {0,0,0}, 0,0,0,0, {0,0,0},0,0,0,0, v_norm(normal), width, height, {NULL,0,0}, false, false, {{0,0,0},{0,0,0},{0,0,0}}, 0.0f };
    g_objects[g_obj_count] = obj;
    return g_obj_count++;
}

/* ---------- Texture rotation API ---------- */
void xr_set_texture_rotation(int idx, float angle) {
    if (idx < 0 || idx >= g_obj_count) return;
    scene_object_t *obj = &g_objects[idx];
    if (obj->type != OBJ_TEXTURED_PLANE) return;
    obj->tex_rotation = angle;
}

int xr_lock_by_index(int first, ...) {
    if (first < 0) return -1;
    va_list args;
    va_start(args, first);
    int count = 0;
    int idx = first;
    while (idx >= 0) {
        count++;
        idx = va_arg(args, int);
    }
    va_end(args);

    if (count == 0) return -1;

    int group_id = g_lock_group_count++;
    g_lock_groups = (lock_group_t*)realloc(g_lock_groups, sizeof(lock_group_t) * g_lock_group_count);
    lock_group_t *group = &g_lock_groups[group_id];
    group->count = count;
    group->capacity = count;
    group->indices = (int*)malloc(sizeof(int) * count);

    va_start(args, first);
    idx = first;
    for (int i = 0; i < count; i++) {
        group->indices[i] = idx;
        idx = va_arg(args, int);
    }
    va_end(args);

    return group_id;
}

static void rotate_single_object(scene_object_t *obj, float rx, float ry, float rz) {
    obj->rot.x += rx;
    obj->rot.y += ry;
    obj->rot.z += rz;
}

void xr_rotate_element(int idx, float rx, float ry, float rz) {
    for (int g = 0; g < g_lock_group_count; g++) {
        lock_group_t *group = &g_lock_groups[g];
        for (int i = 0; i < group->count; i++) {
            if (group->indices[i] == idx) {
                for (int j = 0; j < group->count; j++) {
                    int obj_idx = group->indices[j];
                    if (obj_idx >= 0 && obj_idx < g_obj_count) {
                        rotate_single_object(&g_objects[obj_idx], rx, ry, rz);
                    }
                }
                return;
            }
        }
    }
    if (idx >= 0 && idx < g_obj_count) {
        rotate_single_object(&g_objects[idx], rx, ry, rz);
    }
}

void xr_set_texture_flip(int idx, bool flip_h, bool flip_v) {
    if (idx < 0 || idx >= g_obj_count) return;
    scene_object_t *obj = &g_objects[idx];
    if (obj->type != OBJ_TEXTURED_PLANE) return;
    obj->tex_flip_h = flip_h;
    obj->tex_flip_v = flip_v;
}

/* ---------- Rotate around arbitrary fixed axis ---------- */
void xr_rotate_axis(int idx, xr_vec3_t p1, xr_vec3_t p2, float arc) {
    xr_vec3_t d = v_sub(p2, p1);
    float len = sqrtf(d.x*d.x + d.y*d.y + d.z*d.z);
    if (len < 1e-6f || fabsf(arc) < 1e-6f) return;
    d.x /= len; d.y /= len; d.z /= len;
    xr_vec3_t A = p1;

    float R_axis[3][3];
    mat3_axis_angle(d.x, d.y, d.z, arc, R_axis);

    int targets[256];
    int target_count = 0;
    bool in_group = false;
    for (int g = 0; g < g_lock_group_count; g++) {
        lock_group_t *group = &g_lock_groups[g];
        for (int i = 0; i < group->count; i++) {
            if (group->indices[i] == idx) {
                for (int j = 0; j < group->count; j++) {
                    int obj_idx = group->indices[j];
                    if (obj_idx >= 0 && obj_idx < g_obj_count) {
                        targets[target_count++] = obj_idx;
                    }
                }
                in_group = true;
                break;
            }
        }
        if (in_group) break;
    }
    if (!in_group) {
        if (idx >= 0 && idx < g_obj_count) {
            targets[target_count++] = idx;
        }
    }

    for (int t = 0; t < target_count; t++) {
        scene_object_t *obj = &g_objects[targets[t]];

        xr_vec3_t rel = v_sub(obj->center, A);
        xr_vec3_t rel_rot;
        rel_rot.x = R_axis[0][0]*rel.x + R_axis[0][1]*rel.y + R_axis[0][2]*rel.z;
        rel_rot.y = R_axis[1][0]*rel.x + R_axis[1][1]*rel.y + R_axis[1][2]*rel.z;
        rel_rot.z = R_axis[2][0]*rel.x + R_axis[2][1]*rel.y + R_axis[2][2]*rel.z;
        obj->center = v_add(A, rel_rot);

        float R_old[3][3];
        mat3_from_euler(obj->rot.x, obj->rot.y, obj->rot.z, R_old);
        float R_new[3][3];
        mat3_mul(R_axis, R_old, R_new);
        float rx, ry, rz;
        euler_from_mat3(R_new, &rx, &ry, &rz);
        obj->rot.x = rx;
        obj->rot.y = ry;
        obj->rot.z = rz;
    }
}
