#version 450
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 outFragColor;

layout(set=3, binding=0) uniform PlasmaParams {
    vec4 t_d_amp_d_sx_d_sy;
    vec4 r_s_s_bx_s_by_p_r;
    vec4 p_g_p_b_s_ma_s_msx;
    vec4 s_msy_w_b_w_a_w_s;
    vec4 s_dm_d_r_d_g_d_b;
    vec4 t_c_w_h_iPlasmaIDX;
} params;

void main() {
    float t = params.t_d_amp_d_sx_d_sy.x;
    float d_amp = params.t_d_amp_d_sx_d_sy.y;
    float d_sx = params.t_d_amp_d_sx_d_sy.z;
    float d_sy = params.t_d_amp_d_sx_d_sy.w;

    float r_s = params.r_s_s_bx_s_by_p_r.x;
    float s_bx = params.r_s_s_bx_s_by_p_r.y;
    float s_by = params.r_s_s_bx_s_by_p_r.z;
    float p_r = params.r_s_s_bx_s_by_p_r.w;

    float p_g = params.p_g_p_b_s_ma_s_msx.x;
    float p_b = params.p_g_p_b_s_ma_s_msx.y;
    float s_ma = params.p_g_p_b_s_ma_s_msx.z;
    float s_msx = params.p_g_p_b_s_ma_s_msx.w;

    float s_msy = params.s_msy_w_b_w_a_w_s.x;
    float w_b = params.s_msy_w_b_w_a_w_s.y;
    float w_a = params.s_msy_w_b_w_a_w_s.z;
    float w_s = params.s_msy_w_b_w_a_w_s.w;

    float s_dm = params.s_dm_d_r_d_g_d_b.x;
    float d_r = params.s_dm_d_r_d_g_d_b.y;
    float d_g = params.s_dm_d_r_d_g_d_b.z;
    float d_b = params.s_dm_d_r_d_g_d_b.w;

    float t_c = params.t_c_w_h_iPlasmaIDX.x;
    float w = params.t_c_w_h_iPlasmaIDX.y;
    float h = params.t_c_w_h_iPlasmaIDX.z;
    int idx = int(params.t_c_w_h_iPlasmaIDX.w);

    float fx = inUV.x;
    float fy = inUV.y;
    
    float R = 0.0, G = 0.0, B = 0.0;

    if (idx == 0) {
        if (t_c > 0.0f) {
            float cur_rel = w / h;
            fy = floor(fy * t_c) / t_c;
            fx = floor(fx * (t_c * cur_rel)) / (t_c * cur_rel);
        }
        float dx = d_amp * sin(t * d_sx);
        float dy = d_amp * cos(t * d_sy);
        float rs = sin(t * r_s), rc = cos(t * r_s);
        float jitter = sin(fx * 140.0 + t * 15.0) * cos(fy * 240.0 - t * 10.0) * 0.02;
        float cx = (fx + jitter) - 0.5;
        float cy = (fy + jitter) - 0.5;
        float rx = cx * rc - cy * rs + 0.5 + dx;
        float ry = cx * rs + cy * rc + 0.5 + dy;
        float dist = sqrt(cx * cx + cy * cy);
        float warp_str = w_b + w_a * sin(t * w_s);
        float swirl_angle = dist * s_dm * warp_str;
        float sw_sin = sin(swirl_angle), sw_cos = cos(swirl_angle);
        float wx = (rx - 0.5) * sw_cos - (ry - 0.5) * sw_sin + 0.5;
        float wy = (rx - 0.5) * sw_sin + (ry - 0.5) * sw_cos + 0.5;
        float scale_x = s_bx + s_ma * sin(t * s_msx);
        float scale_y = s_by + s_ma * cos(t * s_msy);
        float v = (sin(wx * scale_x + t) + sin(wy * scale_y - t * 0.5) + 
                   sin((wx + wy) * (scale_x * 0.5) + t) + 
                   sin(sqrt(wx * wx + wy * wy) * 10.0 + t)) * 0.25;
        R = (0.5 + 0.5 * cos(6.28318 * (v + p_r))) * d_r;
        G = (0.5 + 0.5 * cos(6.28318 * (v + p_g))) * d_g;
        B = (0.5 + 0.5 * cos(6.28318 * (v + p_b))) * d_b;
    } else if (idx == 1) {
        if (t_c > 0.0f) {
            float cur_rel = w / h;
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
        float jitter = sin(fx * 200.0) * cos(fy * 200.0) * 0.01;
        float curve_x = sin(fy * 10.0 + t) * 0.05;
        float curve_y = cos(fx * 10.0 + t) * 0.05;
        float warped_fx = fx + curve_x + jitter;
        float warped_fy = fy + curve_y + jitter;
        float cx = warped_fx - 0.5;
        float cy = warped_fy - 0.5;
        float rx = cx * rot_cos - cy * rot_sin + 0.5 + drift_x * 0.1;
        float ry = cx * rot_sin + cy * rot_cos + 0.5 + drift_y * 0.1;
        float dist = sqrt(cx * cx + cy * cy);
        float swirl_angle = dist * s_dm * warp_str;
        float sw_sin = sin(swirl_angle);
        float sw_cos = cos(swirl_angle);
        float wx = (rx - 0.5) * sw_cos - (ry - 0.5) * sw_sin + 0.5;
        float wy = (rx - 0.5) * sw_sin + (ry - 0.5) * sw_cos + 0.5;
        float v = 0.0;
        v += sin(wx * scale_x + t);
        v += sin((wy * scale_y + t) * 0.7);
        v += sin((wx * scale_x + wy * scale_y + t) * 0.5);
        v += sin(sqrt(wx * wx * 100.0 + wy * wy * 100.0) + t);
        v *= 0.25;
        float num_bands = 6.0;
        v = floor(v * num_bands) / num_bands;
        R = (0.5 + 0.5 * cos(3.14159 * (v + p_r))) * d_r;
        G = (0.5 + 0.5 * cos(3.14159 * (v + p_g))) * d_g;
        B = (0.5 + 0.5 * cos(3.14159 * (v + p_b))) * d_b;
    } else if (idx == 2) {
        float jitter = sin(fx * 120.0 + t * 10.0) * 0.02;
        float wx = fx + jitter;
        float wy = fy + jitter;
        float v1 = 1.0 - abs(sin(wx * s_bx + t));
        float v2 = 1.0 - abs(sin(wy * s_by - t));
        float v = (v1 + v2) * 0.5;
        float electric_v = v * v * v;
        R = (0.5 + 0.5 * cos(3.14159 * (electric_v + p_r))) * d_r;
        G = (0.5 + 0.5 * cos(3.14159 * (electric_v + p_g))) * d_g;
        B = (0.5 + 0.5 * cos(3.14159 * (electric_v + p_b))) * d_b;
    } else if (idx == 3) {
        float sX = (s_bx == 0.0) ? 10.0 : s_bx;
        float sY = (s_by == 0.0) ? 10.0 : s_by;
        float jitter = sin(fx * 40.0 + fy * 40.0 + t * 3.0) * 0.01;
        float wx = fx + jitter;
        float wy = fy + jitter;
        float v = 0.0;
        v += 1.0 - abs(sin(wx * sX + t));
        v += 1.0 - abs(sin(wy * sY - t));
        v += 1.0 - abs(sin((wx + wy) * (sX * 0.5) + t * 0.5));
        v /= 3.0;
        v = (v > 0.5) ? (v - 0.5) * 2.0 : 0.0;
        R = (0.5 + 0.5 * cos(3.14159 * (v + p_r))) * v * d_r;
        G = (0.5 + 0.5 * cos(3.14159 * (v + p_g))) * v * d_g;
        B = (0.5 + 0.5 * cos(3.14159 * (v + p_b))) * v * d_b;
    } else if (idx == 4) {
        float drift_x  = d_amp * sin(t * d_sx);
        float drift_y  = d_amp * cos(t * d_sy);
        float s_x      = s_bx + s_ma * sin(t * s_msx);
        float s_y      = s_by + s_ma * cos(t * s_msy);
        float rot_sin  = sin(t * r_s);
        float rot_cos  = cos(t * r_s);
        float cx = fx - 0.5;
        float cy = fy - 0.5;
        float rx = cx * rot_cos - cy * rot_sin + 0.5 + drift_x;
        float ry = cx * rot_sin + cy * rot_cos + 0.5 + drift_y;
        float warp_noise = sin(rx * 15.0 + t) * cos(ry * 15.0 - t);
        float wx = rx + warp_noise * 0.05;
        float wy = ry + warp_noise * 0.05;
        float v = 0.0;
        v += 1.0 - abs(sin(wx * s_x + t));
        v += 1.0 - abs(sin(wy * s_y * 1.2 - t * 0.8));
        v += 1.0 - abs(sin((wx + wy) * s_x * 0.5 + t));
        float dist = sqrt((wx-0.5)*(wx-0.5) + (wy-0.5)*(wy-0.5));
        v += 1.0 - abs(sin(dist * 20.0 - t * 2.0));
        v /= 4.0;
        v = pow(v, 3.5);
        R = clamp((0.5 + 0.5 * cos(3.14159 * (v + p_r))) * v * d_r, 0.0, 1.0);
        G = clamp((0.5 + 0.5 * cos(3.14159 * (v + p_g))) * v * d_g, 0.0, 1.0);
        B = clamp((0.5 + 0.5 * cos(3.14159 * (v + p_b))) * v * d_b, 0.0, 1.0);
    } else if (idx == 5) {
        float jitter = sin(fx * 150.0 + t * 20.0) * 0.012;
        float wx = fx + jitter;
        float wy = fy + jitter;
        float v1 = 1.0 - abs(sin(wx * s_bx + t));
        float v2 = 1.0 - abs(sin(wy * s_by - t * 1.3));
        float v3 = 1.0 - abs(sin((wx + wy) * (s_bx * 0.4) + t));
        float v = (v1 + v2 + v3) / 3.0;
        float electric_v = max(0.0, v - 0.4) * 1.8;
        R = (0.5 + 0.5 * cos(3.14159 * (electric_v + p_r))) * electric_v * d_r;
        G = (0.5 + 0.5 * cos(3.14159 * (electric_v + p_g))) * electric_v * d_g;
        B = (0.5 + 0.5 * cos(3.14159 * (electric_v + p_b))) * electric_v * d_b;
    } else if (idx == 6) {
        float drift_x  = d_amp * sin(t * d_sx);
        float drift_y  = d_amp * cos(t * d_sy);
        float rot_sin  = sin(t * r_s);
        float rot_cos  = cos(t * r_s);
        float cx = fx - 0.5;
        float cy = fy - 0.5;
        float rx = cx * rot_cos - cy * rot_sin + 0.5 + drift_x;
        float ry = cx * rot_sin + cy * rot_cos + 0.5 + drift_y;
        float jitter = sin(rx * 50.0 + t * 2.0) * 0.02;
        float wx = rx + jitter;
        float wy = ry + jitter;
        float v1 = 1.0 - abs(sin(wx * s_bx + t));
        float v2 = 1.0 - abs(sin(wy * s_by - t * 0.5));
        float v3 = 1.0 - abs(sin((wx + wy) * (s_bx * 0.5) + t));
        float v = (v1 + v2 + v3) / 3.0;
        v = max(0.0, v - 0.4) * 1.6;
        R = min(1.0, (0.5 + 0.5 * cos(3.14159 * (v + p_r))) * v * d_r);
        G = min(1.0, (0.5 + 0.5 * cos(3.14159 * (v + p_g))) * v * d_g);
        B = min(1.0, (0.5 + 0.5 * cos(3.14159 * (v + p_b))) * v * d_b);
    } else if (idx == 7) {
        float drift_x = d_amp * sin(t * d_sx);
        float drift_y = d_amp * cos(t * d_sy);
        float rot_sin = sin(t * r_s);
        float rot_cos = cos(t * r_s);
        float cx = fx - 0.5;
        float cy = fy - 0.5;
        float rx = cx * rot_cos - cy * rot_sin + 0.5 + drift_x;
        float ry = cx * rot_sin + cy * rot_cos + 0.5 + drift_y;
        float buzz = sin(rx * 130.0 + t * 15.0) * cos(ry * 210.0 - t * 10.0);
        float wx = rx + buzz * 0.015;
        float wy = ry + buzz * 0.015;
        float v1 = 1.0 - abs(sin(wx * s_bx + t));
        float v2 = 1.0 - abs(sin(wy * s_by - t * 0.5));
        float v3 = 1.0 - abs(sin((wx + wy) * (s_bx * 0.5) + t));
        float v = (v1 + v2 + v3) / 3.0;
        float electric_v = max(0.0, v - 0.5) * 2.0;
        electric_v *= electric_v;
        R = min(1.0, (0.5 + 0.5 * cos(3.14159 * (electric_v + p_r))) * electric_v * d_r);
        G = min(1.0, (0.5 + 0.5 * cos(3.14159 * (electric_v + p_g))) * electric_v * d_g);
        B = min(1.0, (0.5 + 0.5 * cos(3.14159 * (electric_v + p_b))) * electric_v * d_b);
    } else if (idx == 8) {
        float jitter = sin(fx * 140.0 + t * 15.0) * cos(fy * 240.0 - t * 10.0) * 0.02;
        float cx = (fx + jitter) - 0.5;
        float cy = (fy + jitter) - 0.5;
        float drift_x = d_amp * sin(t * d_sx);
        float drift_y = d_amp * cos(t * d_sy);
        float rot_sin = sin(t * r_s);
        float rot_cos = cos(t * r_s);
        float rx = cx * rot_cos - cy * rot_sin + 0.5 + drift_x;
        float ry = cx * rot_sin + cy * rot_cos + 0.5 + drift_y;
        float v = 0.0;
        v += sin(rx * s_bx + t);
        v += sin(ry * s_by - t * 0.5);
        v += sin((rx + ry) * (s_bx * 0.5) + t);
        v += sin(sqrt(rx * rx + ry * ry) * 10.0 + t);
        v *= 0.25;
        R = (0.5 + 0.5 * cos(6.28318 * d_r * (v + p_r)));
        G = (0.5 + 0.5 * cos(6.28318 * d_g * (v + p_g)));
        B = (0.5 + 0.5 * cos(6.28318 * d_b * (v + p_b)));
    }

    outFragColor = vec4(R, G, B, 1.0) * inColor;
}
