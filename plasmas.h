#pragma once

const char* kernelSource1 = R"(
kernel void plasma_kernel(
    global uint* pixels, int w, int h, float t,
    float d_amp, float d_sx, float d_sy, float r_s,
    float s_bx, float s_by, float p_r, float p_g, float p_b,
    float s_ma, float s_msx, float s_msy,
    float w_b, float w_a, float w_s, float s_dm,
    float d_r, float d_g, float d_b,
    float t_c)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    float fx = (float)x / (float)w;
    float fy = (float)y / (float)h;

    if (t_c > 0.0f) {
        float cur_rel = (float)w / (float)h;
        fy = floor(fy * t_c) / t_c;
        fx = floor(fx * (t_c * cur_rel)) / (t_c * cur_rel);
    }

    float drift_x  = d_amp * sin(t * d_sx);
    float drift_y  = d_amp * cos(t * d_sy);
    float scale_x  = s_bx + s_ma * sin(t * s_msx);
    float scale_y  = s_by + s_ma * cos(t * s_msy);
    float rot_sin  = sin(t * r_s);
    float rot_cos  = cos(t * r_s);
    float warp_str = w_b + w_a * sin(t * w_s);

    float jitter = sin(fx * 200.0f) * cos(fy * 200.0f) * 0.01f;
    float curve_x = sin(fy * 10.0f + t) * 0.05f;
    float curve_y = cos(fx * 10.0f + t) * 0.05f;

    float warped_fx = fx + curve_x + jitter;
    float warped_fy = fy + curve_y + jitter;

    float cx = warped_fx - 0.5f;
    float cy = warped_fy - 0.5f;
    float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x * 0.1f;
    float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y * 0.1f;

    float dist = sqrt(cx * cx + cy * cy);
    float swirl_angle = dist * s_dm * warp_str;
    float sw_sin = sin(swirl_angle);
    float sw_cos = cos(swirl_angle);
    float wx = (rx - 0.5f) * sw_cos - (ry - 0.5f) * sw_sin + 0.5f;
    float wy = (rx - 0.5f) * sw_sin + (ry - 0.5f) * sw_cos + 0.5f;

    float v = 0.0f;
    v += sin(wx * scale_x + t);
    v += sin((wy * scale_y + t) * 0.7f);
    v += sin((wx * scale_x + wy * scale_y + t) * 0.5f);
    v += sin(sqrt(wx * wx * 100.0f + wy * wy * 100.0f) + t);
    v *= 0.25f;

    float num_bands = 6.0f;
    v = floor(v * num_bands) / num_bands;

    float r = 0.5f + 0.5f * cos(3.14159f * (v + p_r));
    float g = 0.5f + 0.5f * cos(3.14159f * (v + p_g));
    float b = 0.5f + 0.5f * cos(3.14159f * (v + p_b));

    r *= d_r;
    g *= d_g;
    b *= d_b;

    uint R = (uint)(r * 255.0f);
    uint G = (uint)(g * 255.0f);
    uint B = (uint)(b * 255.0f);

    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
})";

const char* kernelSource2 = R"(
kernel void plasma_kernel(
    global uint* pixels, int w, int h, float t,
    float d_amp, float d_sx, float d_sy, float r_s,
    float s_bx, float s_by, float p_r, float p_g, float p_b,
    float s_ma, float s_msx, float s_msy,
    float w_b, float w_a, float w_s, float s_dm,
    float d_r, float d_g, float d_b,
    float t_c)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    float fx = (float)x / (float)w;
    float fy = (float)y / (float)h;

    float jitter = sin(fx * 120.0f + t * 10.0f) * 0.02f;
    float wx = fx + jitter;
    float wy = fy + jitter;

    float v1 = 1.0f - fabs(sin(wx * s_bx + t));
    float v2 = 1.0f - fabs(sin(wy * s_by - t));
    float v = (v1 + v2) * 0.5f;

    float electric_v = v * v * v;

    float r = 0.5f + 0.5f * cos(3.14159f * (electric_v + p_r));
    float g = 0.5f + 0.5f * cos(3.14159f * (electric_v + p_g));
    float b = 0.5f + 0.5f * cos(3.14159f * (electric_v + p_b));

    uint R = (uint)(r * d_r * 255.0f);
    uint G = (uint)(g * d_g * 255.0f);
    uint B = (uint)(b * d_b * 255.0f);

    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
})";

const char* kernelSource3 = R"(
kernel void plasma_kernel(
    global uint* pixels, int w, int h, float t,
    float d_amp, float d_sx, float d_sy, float r_s,
    float s_bx, float s_by, float p_r, float p_g, float p_b,
    float s_ma, float s_msx, float s_msy,
    float w_b, float w_a, float w_s, float s_dm,
    float d_r, float d_g, float d_b,
    float t_c)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    float fx = (float)x / (float)w;
    float fy = (float)y / (float)h;

    float sX = (s_bx == 0.0f) ? 10.0f : s_bx;
    float sY = (s_by == 0.0f) ? 10.0f : s_by;

    float jitter = sin(fx * 40.0f + fy * 40.0f + t * 3.0f) * 0.01f;
    float wx = fx + jitter;
    float wy = fy + jitter;

    float v = 0.0f;
    v += 1.0f - fabs(sin(wx * sX + t));
    v += 1.0f - fabs(sin(wy * sY - t));
    v += 1.0f - fabs(sin((wx + wy) * (sX * 0.5f) + t * 0.5f));
    v /= 3.0f;

    v = (v > 0.5f) ? (v - 0.5f) * 2.0f : 0.0f;

    float r_f = 0.5f + 0.5f * cos(3.14159f * (v + p_r));
    float g_f = 0.5f + 0.5f * cos(3.14159f * (v + p_g));
    float b_f = 0.5f + 0.5f * cos(3.14159f * (v + p_b));

    uint R = (uint)(r_f * v * d_r * 255.0f);
    uint G = (uint)(g_f * v * d_g * 255.0f);
    uint B = (uint)(b_f * v * d_b * 255.0f);

    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
})";

const char* kernelSource4 = R"(
kernel void plasma_kernel(
    global uint* pixels, int w, int h, float t,
    float d_amp, float d_sx, float d_sy, float r_s,
    float s_bx, float s_by, float p_r, float p_g, float p_b,
    float s_ma, float s_msx, float s_msy,
    float w_b, float w_a, float w_s, float s_dm,
    float d_r, float d_g, float d_b,
    float t_c)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    float fx = (float)x / (float)w;
    float fy = (float)y / (float)h;

    float drift_x  = d_amp * sin(t * d_sx);
    float drift_y  = d_amp * cos(t * d_sy);
    float s_x      = s_bx + s_ma * sin(t * s_msx);
    float s_y      = s_by + s_ma * cos(t * s_msy);
    float rot_sin  = sin(t * r_s);
    float rot_cos  = cos(t * r_s);

    float cx = fx - 0.5f;
    float cy = fy - 0.5f;
    float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
    float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

    float warp_noise = sin(rx * 15.0f + t) * cos(ry * 15.0f - t);
    float wx = rx + warp_noise * 0.05f;
    float wy = ry + warp_noise * 0.05f;

    float v = 0.0f;
    v += 1.0f - fabs(sin(wx * s_x + t));
    v += 1.0f - fabs(sin(wy * s_y * 1.2f - t * 0.8f));
    v += 1.0f - fabs(sin((wx + wy) * s_x * 0.5f + t));
    float dist = sqrt((wx-0.5f)*(wx-0.5f) + (wy-0.5f)*(wy-0.5f));
    v += 1.0f - fabs(sin(dist * 20.0f - t * 2.0f));
    v /= 4.0f;

    v = pow(v, 3.5f);

    float r = 0.5f + 0.5f * cos(3.14159f * (v + p_r));
    float g = 0.5f + 0.5f * cos(3.14159f * (v + p_g));
    float b = 0.5f + 0.5f * cos(3.14159f * (v + p_b));

    float brightness_boost = 1.8f;
    uint R = (uint)clamp(r * v * brightness_boost * 255.0f, 0.0f, 255.0f);
    uint G = (uint)clamp(g * v * brightness_boost * 255.0f, 0.0f, 255.0f);
    uint B = (uint)clamp(b * v * brightness_boost * 255.0f, 0.0f, 255.0f);

    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
})";

const char* kernelSource5 = R"(
kernel void plasma_kernel(
    global uint* pixels, int w, int h, float t,
    float d_amp, float d_sx, float d_sy, float r_s,
    float s_bx, float s_by, float p_r, float p_g, float p_b,
    float s_ma, float s_msx, float s_msy,
    float w_b, float w_a, float w_s, float s_dm,
    float d_r, float d_g, float d_b,
    float t_c)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    float fx = (float)x / (float)w;
    float fy = (float)y / (float)h;

    float jitter = sin(fx * 150.0f + t * 20.0f) * 0.012f;
    float wx = fx + jitter;
    float wy = fy + jitter;

    float v1 = 1.0f - fabs(sin(wx * s_bx + t));
    float v2 = 1.0f - fabs(sin(wy * s_by - t * 1.3f));
    float v3 = 1.0f - fabs(sin((wx + wy) * (s_bx * 0.4f) + t));
    float v = (v1 + v2 + v3) / 3.0f;

    float electric_v = max(0.0f, v - 0.4f) * 1.8f;

    float r_val = 0.5f + 0.5f * cos(3.14159f * (electric_v + p_r));
    float g_val = 0.5f + 0.5f * cos(3.14159f * (electric_v + p_g));
    float b_val = 0.5f + 0.5f * cos(3.14159f * (electric_v + p_b));

    uint R = (uint)(r_val * electric_v * 255.0f);
    uint G = (uint)(g_val * electric_v * 255.0f);
    uint B = (uint)(b_val * electric_v * 255.0f);

    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
})";

const char* kernelSource6 = R"(
kernel void plasma_kernel(
    global uint* pixels, int w, int h, float t,
    float d_amp, float d_sx, float d_sy, float r_s,
    float s_bx, float s_by, float p_r, float p_g, float p_b,
    float s_ma, float s_msx, float s_msy,
    float w_b, float w_a, float w_s, float s_dm,
    float d_r, float d_g, float d_b,
    float t_c)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    float fx = (float)x / (float)w;
    float fy = (float)y / (float)h;

    float drift_x  = d_amp * sin(t * d_sx);
    float drift_y  = d_amp * cos(t * d_sy);
    float rot_sin  = sin(t * r_s);
    float rot_cos  = cos(t * r_s);

    float cx = fx - 0.5f;
    float cy = fy - 0.5f;
    float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
    float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

    float jitter = sin(rx * 50.0f + t * 2.0f) * 0.02f;
    float wx = rx + jitter;
    float wy = ry + jitter;

    float v1 = 1.0f - fabs(sin(wx * s_bx + t));
    float v2 = 1.0f - fabs(sin(wy * s_by - t * 0.5f));
    float v3 = 1.0f - fabs(sin((wx + wy) * (s_bx * 0.5f) + t));
    float v = (v1 + v2 + v3) / 3.0f;

    v = max(0.0f, v - 0.4f) * 1.6f;

    float r_val = 0.5f + 0.5f * cos(3.14159f * (v + p_r));
    float g_val = 0.5f + 0.5f * cos(3.14159f * (v + p_g));
    float b_val = 0.5f + 0.5f * cos(3.14159f * (v + p_b));

    uint R = (uint)min(255.0f, r_val * v * 255.0f);
    uint G = (uint)min(255.0f, g_val * v * 255.0f);
    uint B = (uint)min(255.0f, b_val * v * 255.0f);

    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
})";

const char* kernelSource7 = R"(
kernel void plasma_kernel(
    global uint* pixels, int w, int h, float t,
    float d_amp, float d_sx, float d_sy, float r_s,
    float s_bx, float s_by, float p_r, float p_g, float p_b,
    float s_ma, float s_msx, float s_msy,
    float w_b, float w_a, float w_s, float s_dm,
    float d_r, float d_g, float d_b,
    float t_c)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    float fx = (float)x / (float)w;
    float fy = (float)y / (float)h;

    float drift_x = d_amp * sin(t * d_sx);
    float drift_y = d_amp * cos(t * d_sy);
    float rot_sin = sin(t * r_s);
    float rot_cos = cos(t * r_s);

    float cx = fx - 0.5f;
    float cy = fy - 0.5f;
    float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
    float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

    float buzz = sin(rx * 130.0f + t * 15.0f) * cos(ry * 210.0f - t * 10.0f);
    float wx = rx + buzz * 0.015f;
    float wy = ry + buzz * 0.015f;

    float v1 = 1.0f - fabs(sin(wx * s_bx + t));
    float v2 = 1.0f - fabs(sin(wy * s_by - t * 0.5f));
    float v3 = 1.0f - fabs(sin((wx + wy) * (s_bx * 0.5f) + t));
    float v = (v1 + v2 + v3) / 3.0f;

    float electric_v = max(0.0f, v - 0.5f) * 2.0f;
    electric_v *= electric_v;

    float r_val = 0.5f + 0.5f * cos(3.14159f * (electric_v + p_r));
    float g_val = 0.5f + 0.5f * cos(3.14159f * (electric_v + p_g));
    float b_val = 0.5f + 0.5f * cos(3.14159f * (electric_v + p_b));

    uint R = (uint)min(255.0f, r_val * electric_v * 255.0f);
    uint G = (uint)min(255.0f, g_val * electric_v * 255.0f);
    uint B = (uint)min(255.0f, b_val * electric_v * 255.0f);

    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
})";

const char* kernelSource8 = R"(
kernel void plasma_kernel(
    global uint* pixels, int w, int h, float t,
    float d_amp, float d_sx, float d_sy, float r_s,
    float s_bx, float s_by, float p_r, float p_g, float p_b,
    float s_ma, float s_msx, float s_msy,
    float w_b, float w_a, float w_s, float s_dm,
    float d_r, float d_g, float d_b,
    float t_c)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    float fx = (float)x / (float)w;
    float fy = (float)y / (float)h;

    float jitter = sin(fx * 140.0f + t * 15.0f) * cos(fy * 240.0f - t * 10.0f) * 0.02f;
    float cx = (fx + jitter) - 0.5f;
    float cy = (fy + jitter) - 0.5f;

    float drift_x = d_amp * sin(t * d_sx);
    float drift_y = d_amp * cos(t * d_sy);
    float rot_sin = sin(t * r_s);
    float rot_cos = cos(t * r_s);

    float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
    float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

    float v = 0.0f;
    v += sin(rx * s_bx + t);
    v += sin(ry * s_by - t * 0.5f);
    v += sin((rx + ry) * (s_bx * 0.5f) + t);
    v += sin(sqrt(rx * rx + ry * ry) * 10.0f + t);
    v *= 0.25f;

    float r_f = 0.5f + 0.5f * cos(6.28318f * (v + p_r));
    float g_f = 0.5f + 0.5f * cos(6.28318f * (v + p_g));
    float b_f = 0.5f + 0.5f * cos(6.28318f * (v + p_b));

    uint R = (uint)(r_f * 255.0f);
    uint G = (uint)(g_f * 255.0f);
    uint B = (uint)(b_f * 255.0f);

    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
})";
