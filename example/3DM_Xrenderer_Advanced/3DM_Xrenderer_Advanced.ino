/*
 * 3DM_Xrenderer_Advanced.ino
 * Advanced example: custom mesh (icosahedron), multiple materials,
 * transforms, and render configuration.
 */

#include "3DM_Xrenderer.h"

#define WIDTH 320
#define HEIGHT 320

uint16_t framebuffer[WIDTH * HEIGHT];

// ---------- Icosahedron vertices and indices ----------
// Vertex data for a unit icosahedron (radius ~1)
static const xr_vec3_t ico_vertices[] = {
  // 12 vertices
  { 0.000f, 1.000f, 0.000f },
  { 0.894f, 0.447f, 0.000f },
  { 0.276f, 0.447f, 0.851f },
  { -0.724f, 0.447f, 0.526f },
  { -0.724f, 0.447f, -0.526f },
  { 0.276f, 0.447f, -0.851f },
  { 0.724f, -0.447f, 0.000f },
  { -0.276f, -0.447f, 0.851f },
  { -0.894f, -0.447f, 0.000f },
  { -0.276f, -0.447f, -0.851f },
  { 0.724f, -0.447f, 0.000f },
  { 0.000f, 1.000f, 0.000f },
  { 0.894f, 0.447f, 0.000f },
  { 0.276f, 0.447f, 0.851f },
  { -0.724f, 0.447f, 0.526f },
  { -0.724f, 0.447f, -0.526f },
  { 0.276f, 0.447f, -0.851f },
  { 0.724f, -0.447f, 0.000f },
  { -0.276f, -0.447f, 0.851f },
  { -0.894f, -0.447f, 0.000f },
  { -0.276f, -0.447f, -0.851f },
  { 0.276f, -0.447f, -0.851f },
  { 0.276f, -0.447f, 0.851f }
};
// Indices (20 triangles)
static const uint16_t ico_indices[] = {
  0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 1,
  1, 6, 2, 2, 7, 3, 3, 8, 4, 4, 9, 5, 5, 10, 1,
  1, 10, 6, 2, 6, 7, 3, 7, 8, 4, 8, 9, 5, 9, 10,
  6, 11, 7, 7, 11, 8, 8, 11, 9, 9, 11, 10, 10, 11, 6
};
#define ICO_VERTS 12
#define ICO_TRIS 20

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("3DM_Xrenderer Advanced Example");

  // 1. Init renderer (width, height, max_bounces, aa_samples)
  xr_init(WIDTH, HEIGHT, 5, 1);
  // Configure rendering parameters
  xr_set_bias(0.001f);
  // xr_set_aa_samples(2);  // uncomment for AA
  // xr_set_max_bounces(3); // faster

  // 2. Set camera
  xr_vec3_t cam = { -45.0f, -15.0f, -55.0f };
  xr_vec3_t target = { 0.0f, -20.0f, 0.0f };
  xr_vec3_t up = { 0.0f, 1.0f, 0.0f };
  xr_set_camera(cam, target, up);

  // 3. Set light
  xr_vec3_t light_pos = { 0.0f, 50.0f, 0.0f };
  xr_vec3_t light_col = { 1.0f, 1.0f, 1.0f };
  xr_set_light(light_pos, light_col);

  // ----- Build scene -----

  // ---- 1. Custom icosahedron with gradient-like material (rainbow) ----
  // We'll create a material with high roughness and blue-green color
  xr_vec3_t ico_color = { 0.2f, 0.8f, 0.9f };                 // cyan
  xr_material_t mat_ico = xr_material_make(ico_color, 0.1f);  // shiny
  // Position, rotation, scale
  xr_vec3_t ico_pos = { -25.0f, -35.0f, 20.0f };
  xr_vec3_t ico_rot = { 0.3f, 0.8f, 0.1f };
  xr_vec3_t ico_scale = { 7.0f, 7.0f, 7.0f };
  xr_add_mesh(ico_vertices, ico_indices, ICO_VERTS, ICO_TRIS,
              ico_pos, ico_rot, ico_scale, mat_ico);

  // ---- 2. A gold cube with rotation ----
  xr_vec3_t cube_center = { 30.0f, -30.0f, 25.0f };
  xr_add_cube(cube_center, 10.0f, 0.5f, xr_mat_gold);

  // ---- 3. A red pyramid ----
  xr_vec3_t pyr_center = { -30.0f, -35.0f, -20.0f };
  xr_vec3_t red = { 1.0f, 0.2f, 0.2f };
  xr_material_t mat_red = xr_material_make(red, 0.4f);
  xr_add_pyramid(pyr_center, 12.0f, 0.2f, mat_red);

  // ---- 4. A wooden table ----
  xr_vec3_t table_center = { 0.0f, -42.0f, -18.0f };
  xr_add_table(table_center, 38.0f, 16.0f, 8.0f, 2.5f, xr_mat_wood);

  // ---- 5. Three glass panels (as before) ----
  xr_vec3_t g1 = { -25.0f, -5.0f, 0.0f };
  xr_add_glass_panel(g1, (xr_vec3_t){ 0, 0, 1 }, 25.0f, 45.0f, 4.0f, 1.2f);
  xr_vec3_t g2 = { 25.0f, -5.0f, 0.0f };
  xr_add_glass_panel(g2, (xr_vec3_t){ 0, 0, 1 }, 25.0f, 45.0f, 4.0f, 1.2f);
  xr_vec3_t g3 = { 0.0f, -5.0f, 25.0f };
  xr_add_glass_panel(g3, (xr_vec3_t){ 1, 0, 0 }, 25.0f, 45.0f, 4.0f, 1.2f);

  // ---- 6. Additional small spheres? Not directly, but we can add a small cube as a "sphere" approximation ----
  // We'll add a small bright green cube with high roughness
  xr_vec3_t green = { 0.0f, 1.0f, 0.0f };
  xr_material_t mat_green = xr_material_make(green, 0.9f);
  xr_vec3_t small_cube_pos = { 10.0f, -25.0f, 10.0f };
  xr_add_cube(small_cube_pos, 4.0f, 0.0f, mat_green);

  //xr_enable_shadow(true);

  //xr_enable_microsurface(true);

  //xr_enable_bvh(true);

  // 7. Render and measure
  Serial.println("Rendering started...");
  uint32_t start = micros();
  xr_render(framebuffer);
  uint32_t elapsed = micros() - start;
  float seconds = elapsed / 1000000.0f;
  Serial.printf("Rendering completed in %.3f s\n", seconds);

  // Optional: You could now display framebuffer on screen or save to SD (if you add back)
}

void loop() {
  delay(1000);
}