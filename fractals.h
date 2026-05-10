#ifndef FRACTALS_H
#define FRACTALS_H

// OpenCL kernel sources for several fractal types.
// Each kernel uses the same parameter list:
//   global uint* pixels, int w, int h,
//   double x_off, double y_off, double zoom, int max_iter,
//   float p_r, float p_g, float p_b, float c_s
//
// These are raw C string literals you can pass to your OpenCL compiler.

//mandelbrot_kernel
/*
static const char* fractal1 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s, float transparency)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    double aspect = (double)w / (double)h;
    double scale = 2.0 / zoom;

    double cx = x_off + ((double)x - (double)w * 0.5) / (double)w * scale * aspect;
    double cy = y_off + ((double)y - (double)h * 0.5) / (double)h * scale;

    double zx = 0.0, zy = 0.0;
    int iter = 0;
    while (zx*zx + zy*zy < 4.0 && iter < max_iter) {
        double xt = zx*zx - zy*zy + cx;
        zy = 2.0*zx*zy + cy;
        zx = xt;
        iter++;
    }

    uint R=0u, G=0u, B=0u;
    uint A=255u;
    if (iter == max_iter || iter < (int)transparency) {
        A = 0u;
    } else {
        double v = (double)iter / (double)max_iter * (double)c_s;
        R = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_r))) * 255.0) & 0xFFu;
        G = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_g))) * 255.0) & 0xFFu;
        B = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_b))) * 255.0) & 0xFFu;
    }
    pixels[y * w + x] = (A << 24) | (R << 16) | (G << 8) | B;
}
)";
*/
//julia_kernel
static const char* fractal2 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// Julia set: uses (zx, zy) initialised from pixel, constant (cx, cy) from x_off,y_off
kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s, float transparency)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    double aspect = (double)w / (double)h;
    double scale = 2.0 / zoom;

    double zx = x_off + ((double)x - (double)w * 0.5) / (double)w * scale * aspect;
    double zy = y_off + ((double)y - (double)h * 0.5) / (double)h * scale;

    double cx = x_off; // treat offsets as the complex constant
    double cy = y_off;

    int iter = 0;
    while (zx*zx + zy*zy < 4.0 && iter < max_iter) {
        double xt = zx*zx - zy*zy + cx;
        zy = 2.0*zx*zy + cy;
        zx = xt;
        iter++;
    }

    uint R=0u, G=0u, B=0u;
    uint A=255u;
    if (iter == max_iter || iter < (int)transparency) {
        A = 0u;
    } else {
        double v = (double)iter / (double)max_iter * (double)c_s;
        R = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_r))) * 255.0) & 0xFFu;
        G = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_g))) * 255.0) & 0xFFu;
        B = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_b))) * 255.0) & 0xFFu;
    }
    pixels[y * w + x] = (A << 24) | (R << 16) | (G << 8) | B;
}
)";

//burning_ship_kernel
static const char* fractal3 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// Burning Ship fractal: uses absolute values in iterative formula
kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s, float transparency)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    double aspect = (double)w / (double)h;
    double scale = 2.0 / zoom;

    double cx = x_off + ((double)x - (double)w * 0.5) / (double)w * scale * aspect;
    double cy = y_off + ((double)y - (double)h * 0.5) / (double)h * scale;

    double zx = 0.0, zy = 0.0;
    int iter = 0;
    while (zx*zx + zy*zy < 4.0 && iter < max_iter) {
        double xabs = fabs(zx);
        double yabs = fabs(zy);
        double xt = xabs*xabs - yabs*yabs + cx;
        zy = 2.0*xabs*yabs + cy;
        zx = xt;
        iter++;
    }

    uint R=0u, G=0u, B=0u;
    uint A=255u;
    if (iter == max_iter || iter < (int)transparency) {
        A = 0u;
    } else {
        double v = (double)iter / (double)max_iter * (double)c_s;
        R = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_r))) * 255.0) & 0xFFu;
        G = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_g))) * 255.0) & 0xFFu;
        B = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_b))) * 255.0) & 0xFFu;
    }
    pixels[y * w + x] = (A << 24) | (R << 16) | (G << 8) | B;
}
)";

//tricorn_kernel
static const char* fractal4 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// Tricorn (a.k.a. Mandelbar) fractal: conjugate in iteration
kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s, float transparency)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    double aspect = (double)w / (double)h;
    double scale = 2.0 / zoom;

    double cx = x_off + ((double)x - (double)w * 0.5) / (double)w * scale * aspect;
    double cy = y_off + ((double)y - (double)h * 0.5) / (double)h * scale;

    double zx = 0.0, zy = 0.0;
    int iter = 0;
    while (zx*zx + zy*zy < 4.0 && iter < max_iter) {
        // use conjugate: z -> conjugate(z)^2 + c  => (zx - i*zy)^2
        double xt = zx*zx - zy*zy + cx;
        zy = -2.0*zx*zy + cy;
        zx = xt;
        iter++;
    }

    uint R=0u, G=0u, B=0u;
    uint A=255u;
    if (iter == max_iter || iter < (int)transparency) {
        A = 0u;
    } else {
        double v = (double)iter / (double)max_iter * (double)c_s;
        R = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_r))) * 255.0) & 0xFFu;
        G = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_g))) * 255.0) & 0xFFu;
        B = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_b))) * 255.0) & 0xFFu;
    }
    pixels[y * w + x] = (A << 24) | (R << 16) | (G << 8) | B;
}
)";

//multibrot_kernel
static const char* fractal5 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// Multibrot (power-3) fractal: z = z^power + c, here power = 3
kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s, float transparency)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    double aspect = (double)w / (double)h;
    double scale = 2.0 / zoom;

    double cx = x_off + ((double)x - (double)w * 0.5) / (double)w * scale * aspect;
    double cy = y_off + ((double)y - (double)h * 0.5) / (double)h * scale;

    double zx = 0.0, zy = 0.0;
    int iter = 0;
    while (zx*zx + zy*zy < 4.0 && iter < max_iter) {
        // (zx + i zy)^3 = zx^3 + 3 zx^2 i zy + 3 zx (i zy)^2 + (i zy)^3
        // compute with real arithmetic:
        double zx2 = zx*zx;
        double zy2 = zy*zy;
        double xt = zx*(zx2 - 3.0*zy2) + cx;
        double yt = zy*(3.0*zx2 - zy2) + cy;
        zx = xt;
        zy = yt;
        iter++;
    }

    uint R=0u, G=0u, B=0u;
    uint A=255u;
    if (iter == max_iter || iter < (int)transparency) {
        A = 0u;
    } else {
        double v = (double)iter / (double)max_iter * (double)c_s;
        R = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_r))) * 255.0) & 0xFFu;
        G = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_g))) * 255.0) & 0xFFu;
        B = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_b))) * 255.0) & 0xFFu;
    }
    pixels[y * w + x] = (A << 24) | (R << 16) | (G << 8) | B;
}
)";

//mandelbulb_kernel
static const char* fractal1 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// Helper for Mandelbulb distance estimation
double get_mandelbulb_de(double3 p, double* trap) {
    double3 z = p;
    double dr = 1.0;
    double r = 0.0;
    for (int i = 0; i < 8; i++) {
        r = length(z);
        if (r > 2.0) break;
        if (trap) *trap = min(*trap, r);
        
        double theta = acos(z.y / r);
        double phi = atan2(z.z, z.x);
        dr = pow(r, 7.0) * 8.0 * dr + 1.0;
        
        double zr = pow(r, 8.0);
        theta *= 8.0;
        phi *= 8.0;
        
        z = zr * (double3)(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));
        z += p;
    }
    return 0.5 * log(r) * r / dr;
}

kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s, float transparency)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    double aspect = (double)w / (double)h;
    double uv_x = ((double)x / (double)w - 0.5) * aspect;
    double uv_y = (double)y / (double)h - 0.5;

    // Camera setup
    double dist_cam = 3.2 / (zoom + 0.001);
    double3 ro = (double3)(
        dist_cam * cos(x_off) * cos(y_off),
        dist_cam * sin(y_off),
        dist_cam * sin(x_off) * cos(y_off)
    );

    double3 target = (double3)(0.0, 0.0, 0.0);
    double3 forward = normalize(target - ro);
    double3 right = normalize(cross((double3)(0.0, 1.0, 0.0), forward));
    double3 up = cross(forward, right);
    double3 rd = normalize(forward * 1.5 + right * uv_x + up * uv_y);

    // Ray marching
    double t = 0.0;
    double trap = 1e10;
    bool hit = false;
    int steps = 0;
    for (steps = 0; steps < 128; steps++) {
        double d = get_mandelbulb_de(ro + rd * t, &trap);
        if (d < 0.0003) { hit = true; break; }
        t += d;
        if (t > 8.0) break;
    }

    uint R=0, G=0, B=0;
    if (hit) {
        double3 p = ro + rd * t;
        // Calculate normal using finite differences
        double eps = 0.001;
        double3 n = normalize((double3)(
            get_mandelbulb_de(p + (double3)(eps, 0, 0), 0) - get_mandelbulb_de(p - (double3)(eps, 0, 0), 0),
            get_mandelbulb_de(p + (double3)(0, eps, 0), 0) - get_mandelbulb_de(p - (double3)(0, eps, 0), 0),
            get_mandelbulb_de(p + (double3)(0, 0, eps), 0) - get_mandelbulb_de(p - (double3)(0, 0, eps), 0)
        ));

        // Fixed light source
        double3 lightDir = normalize((double3)(1.0, 1.0, -1.0));
        double diff = max(0.05, dot(n, lightDir));
        // Fake Ambient Occlusion based on step count
        double ao = clamp(1.0 - (double)steps / 128.0, 0.0, 1.0);
        
        // Color based on orbit trap (min distance to origin)
        double v = trap * (double)c_s;
        double3 baseCol = (double3)(
            (0.5 + 0.5 * cos(6.283185 * (v + (double)p_r))),
            (0.5 + 0.5 * cos(6.283185 * (v + (double)p_g))),
            (0.5 + 0.5 * cos(6.283185 * (v + (double)p_b)))
        );

        double3 finalCol = baseCol * diff * ao;
        R = (uint)(clamp(finalCol.x, 0.0, 1.0) * 255.0);
        G = (uint)(clamp(finalCol.y, 0.0, 1.0) * 255.0);
        B = (uint)(clamp(finalCol.z, 0.0, 1.0) * 255.0);
    }
    
    uint A = hit ? 255u : 0u;
    pixels[y * w + x] = (A << 24) | (R << 16) | (G << 8) | B;
}
)";

#endif // FRACTALS_H