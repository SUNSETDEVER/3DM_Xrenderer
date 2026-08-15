# 3DM_Xrenderer

**Lightweight Ray Tracing Renderer for Embedded Systems**

`3DM_Xrenderer` is a compact, performance-oriented ray tracing library designed for resource-constrained embedded platforms (ESP32, STM32H7). It outputs RGB565 framebuffers suitable for direct display on TFT/LCD panels, with optional LVGL integration.

---

## Key Features

- **Scene Construction** – Add cubes, pyramids, spheres, textured planes, glass panels, tables, and custom triangle meshes.
- **Configurable Materials** – Predefined materials (gold, wood, glass, plastics, chrome, brass) and user‑defined albedo/roughness.
- **Texture Mapping** – RGB565 textures with horizontal/vertical flip control.
- **Advanced Lighting** – Point light source with soft shadows (toggleable) and GGX microsurface (toggleable).
- **BVH Acceleration** – Median‑split BVH for mesh triangles (toggleable).
- **Tile‑Based Rendering** – External tile buffer to reduce SPIRAM pressure (user‑allocated, no internal malloc).
- **Camera & Rotation** – Free camera positioning; 3‑axis Euler rotation for objects.
- **Scene Boundaries** – Customizable room size, per‑wall materials, enable/disable individual walls, floor checkerboard toggle.
- **Flexible Output** – Full‑frame or region‑only rendering; double‑buffering ready.

---

## API Overview

### Initialization & Configuration

| Function | Description |
|----------|-------------|
| `xr_init(width, height, max_bounces, aa_samples)` | Initialize renderer with resolution, ray depth, and anti‑aliasing samples. |
| `xr_set_camera(pos, target, up)` | Set eye position, look‑at target, and up vector. |
| `xr_set_light(pos, color)` | Set light source position and RGB colour. |
| `xr_set_bias(bias)` | Intersection bias to avoid self‑shadowing (default 1e‑3). |
| `xr_set_aa_samples(n)` | Adjust anti‑aliasing samples (1 = off). |
| `xr_set_max_bounces(n)` | Max ray bounces for reflections/refractions. |
| `xr_enable_shadow(bool)` | Enable/disable shadow rays. |
| `xr_enable_microsurface(bool)` | Enable/disable GGX microsurface (roughness‑based reflection). |
| `xr_enable_bvh(bool)` | Enable/disable BVH acceleration for meshes. |
| `xr_set_tile_buffer(buffer, size)` | Provide external tile buffer (`uint16_t[size*size]`) for tiled rendering. |

### Scene Editing

| Function | Description |
|----------|-------------|
| `xr_set_boundary(xmin, xmax, ymin, ymax, zmin, zmax)` | Set world bounding box. |
| `xr_set_wall_material(face, mat)` | Set material for a specific wall (0..5: +X, -X, +Y, -Y, +Z, -Z). |
| `xr_enable_wall(face, bool)` | Enable/disable a wall. |
| `xr_set_floor_material(mat)` | Material used when checkerboard is off. |
| `xr_set_floor_checker(bool)` | Enable/disable checkerboard pattern on floor. |
| `xr_set_light_region(center, half_size)` | Define the emissive area on the ceiling (light source). |

### Object Adders (all return an index for later manipulation)

| Function | Returned Object |
|----------|-----------------|
| `xr_add_cube(center, half_size, y_angle, mat)` | Cube (axis‑aligned, initial Y rotation). |
| `xr_add_pyramid(center, scale, y_angle, mat)` | Triangular pyramid. |
| `xr_add_sphere(center, radius, mat)` | Sphere. |
| `xr_add_table(center, width, depth, thickness, leg_size, mat)` | Table with wooden texture. |
| `xr_add_glass_panel(center, normal, half_w, half_h, thickness, ior)` | Glass sheet. |
| `xr_add_textured_plane(center, normal, width, height, tex, mat)` | Textured plane (UV‑mapped). |
| `xr_add_mesh(vertices, indices, numVerts, numTris, pos, rot, scale, mat)` | Custom triangle mesh (pre‑transformed). |

### Object Manipulation

| Function | Description |
|----------|-------------|
| `xr_rotate_element(idx, rx, ry, rz)` | Add Euler angles (radians) to the object’s local rotation (ZYX order). |
| `xr_set_texture_flip(idx, flip_h, flip_v)` | Flip texture horizontally/vertically for a textured plane. |

### Rendering

| Function | Description |
|----------|-------------|
| `xr_render(framebuffer)` | Render entire scene into RGB565 framebuffer. |
| `xr_render_region(x, y, w, h, framebuffer)` | Render only a rectangular region (useful for partial updates). |

---

## Material System

Predefined materials (can be used directly):
- `xr_mat_gold`, `xr_mat_wood`, `xr_mat_glass`
- `xr_mat_wall`, `xr_mat_floor`
- `xr_mat_red_plastic`, `xr_mat_blue_plastic`, `xr_mat_green_plastic`
- `xr_mat_white_plastic`, `xr_mat_black_plastic`
- `xr_mat_chrome`, `xr_mat_brass`

Custom material helper:
```c
xr_material_t my_mat = xr_material_make((xr_vec3_t){0.2, 0.8, 0.3}, 0.4f);
xr\_material\_t my\_mat = xr\_material\_make((xr\_vec3\_t){0.2, 0.8, 0.3}, 0.4f);
```
---

Tile‑Based Rendering
To reduce SPIRAM pressure, the library supports external tile buffers:

```c
#define TILE_SIZE 32
static uint16_t tile_buffer[TILE_SIZE * TILE_SIZE];
xr_set_tile_buffer(tile_buffer, TILE_SIZE);
```
If no buffer is provided, the library renders directly (no tiling). The tile buffer must be large enough to hold size * size pixels (uint16_t each).

---

**Performance Notes**
- **BVH** significantly speeds up mesh intersection, but adds build time on first render.

- **Microsurface** improves realism but adds sampling overhead.

- **Shadows** increase ray count; disable if not needed.

- **Tile size** should match your cache hierarchy (16–64 recommended).

- **AA samples** >1 linearly increase render time.

---

**Quick Start**
```c
#include "3DM_Xrenderer.h"

#define W 320
#define H 320
uint16_t fb[W * H];

void setup() {
    xr_init(W, H, 5, 1);
    xr_set_camera((xr_vec3_t){-40,-20,-45}, (xr_vec3_t){0,-25,0}, (xr_vec3_t){0,1,0});
    xr_set_light((xr_vec3_t){0,50,0}, (xr_vec3_t){1,1,1});
    xr_set_boundary(-100, 100, -50, 50, -100, 100);
    
    xr_add_cube((xr_vec3_t){30, -30, 25}, 10, 0, xr_mat_gold);
    xr_add_sphere((xr_vec3_t){-30, -30, 20}, 8, xr_mat_red_plastic);

    // external tile buffer
    #define TS 32
    static uint16_t tile[TS*TS];
    xr_set_tile_buffer(tile, TS);

    xr_render(fb);
}
```

---

-Standard **Cpp** library

-**Arduino** framework (or bare‑metal with printf for warnings)

-No external **GPU** or **OS** required (FreeRTOS optional)

Author:**SUNSET**
