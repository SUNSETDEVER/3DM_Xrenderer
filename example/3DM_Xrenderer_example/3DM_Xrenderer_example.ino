/**
 * @file 3DM_Xrenderer_example.ino
 * @brief Example usage of the 3DM_Xrenderer library.
 *        Demonstrates scene construction, configuration, and rendering.
 */

#include "3DM_Xrenderer.h"

#define WIDTH 320
#define HEIGHT 320

uint16_t framebuffer[WIDTH * HEIGHT];

void setup() {
  Serial.begin(115200);
  delay(100);

  // ----- 1. Initialize renderer -----
  // Parameters: width, height, max_bounces, aa_samples
  xr_init(WIDTH, HEIGHT, 5, 1);

  // ----- 2. (Optional) Tweak rendering parameters -----
  // Set ray intersection bias (avoid self‑shadowing artifacts)
  xr_set_bias(0.001f);  // default is 1e-3

  // Enable 2× anti‑aliasing (smoother edges, double render time)
  // xr_set_aa_samples(2);

  // Limit maximum bounces for faster rendering (reduces global illumination)
  // xr_set_max_bounces(3);

  // ----- 3. Set camera -----
  xr_vec3_t cam = { -40.0f, -20.0f, -45.0f };
  xr_vec3_t target = { 0.0f, -25.0f, 0.0f };
  xr_vec3_t up = { 0.0f, 1.0f, 0.0f };
  xr_set_camera(cam, target, up);

  // ----- 4. Set light source -----
  xr_vec3_t light_pos = { 0.0f, 50.0f, 0.0f };
  xr_vec3_t light_col = { 1.0f, 1.0f, 1.0f };
  xr_set_light(light_pos, light_col);

  // ----- 5. Build scene -----
  // Gold cube
  xr_vec3_t cube_center = { 30.0f, -30.0f, 25.0f };
  xr_add_cube(cube_center, 10.0f, 0.0f, xr_mat_gold);

  // Gold pyramid
  xr_vec3_t pyr_center = { -30.0f, -30.0f, 25.0f };
  xr_add_pyramid(pyr_center, 10.0f, 0.0f, xr_mat_gold);

  // Wooden table
  xr_vec3_t table_center = { 0.0f, -40.0f, -20.0f };
  xr_add_table(table_center, 36.0f, 14.0f, 8.0f, 2.0f, xr_mat_wood);

  // Three glass panels
  xr_vec3_t g1 = { -25.0f, -5.0f, 0.0f };
  xr_add_glass_panel(g1, (xr_vec3_t){ 0, 0, 1 }, 25.0f, 45.0f, 4.0f, 1.2f);

  xr_vec3_t g2 = { 25.0f, -5.0f, 0.0f };
  xr_add_glass_panel(g2, (xr_vec3_t){ 0, 0, 1 }, 25.0f, 45.0f, 4.0f, 1.2f);

  xr_vec3_t g3 = { 0.0f, -5.0f, 25.0f };
  xr_add_glass_panel(g3, (xr_vec3_t){ 1, 0, 0 }, 25.0f, 45.0f, 4.0f, 1.2f);

  //xr_enable_shadow(true);

  //xr_enable_microsurface(true);

  //xr_enable_bvh(true);

  // ----- 6. Render and measure time -----
  Serial.println("Rendering started...");
  uint32_t start = micros();

  xr_render(framebuffer);

  uint32_t elapsed = micros() - start;
  float seconds = elapsed / 1000000.0f;
  Serial.printf("Rendering complete in %.2f s\n", seconds);

  // Optional: display framebuffer on a screen or save to SD (not included)
}

void loop() {
  // Nothing to do here – render once on startup.
  delay(1000);
}