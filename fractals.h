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
static const char* fractal1 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s)
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
    if (iter != max_iter) {
        double v = (double)iter / (double)max_iter * (double)c_s;
        R = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_r))) * 255.0) & 0xFFu;
        G = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_g))) * 255.0) & 0xFFu;
        B = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_b))) * 255.0) & 0xFFu;
    }
    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
}
)";

//julia_kernel
static const char* fractal2 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// Julia set: uses (zx, zy) initialised from pixel, constant (cx, cy) from x_off,y_off
kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s)
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
    if (iter != max_iter) {
        double v = (double)iter / (double)max_iter * (double)c_s;
        R = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_r))) * 255.0) & 0xFFu;
        G = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_g))) * 255.0) & 0xFFu;
        B = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_b))) * 255.0) & 0xFFu;
    }
    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
}
)";

//burning_ship_kernel
static const char* fractal3 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// Burning Ship fractal: uses absolute values in iterative formula
kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s)
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
    if (iter != max_iter) {
        double v = (double)iter / (double)max_iter * (double)c_s;
        R = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_r))) * 255.0) & 0xFFu;
        G = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_g))) * 255.0) & 0xFFu;
        B = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_b))) * 255.0) & 0xFFu;
    }
    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
}
)";

//tricorn_kernel
static const char* fractal4 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// Tricorn (a.k.a. Mandelbar) fractal: conjugate in iteration
kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s)
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
    if (iter != max_iter) {
        double v = (double)iter / (double)max_iter * (double)c_s;
        R = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_r))) * 255.0) & 0xFFu;
        G = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_g))) * 255.0) & 0xFFu;
        B = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_b))) * 255.0) & 0xFFu;
    }
    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
}
)";

//multibrot_kernel
static const char* fractal5 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// Multibrot (power-3) fractal: z = z^power + c, here power = 3
kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s)
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
    if (iter != max_iter) {
        double v = (double)iter / (double)max_iter * (double)c_s;
        R = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_r))) * 255.0) & 0xFFu;
        G = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_g))) * 255.0) & 0xFFu;
        B = (uint)((0.5 + 0.5 * cos(6.283185307179586 * (v + (double)p_b))) * 255.0) & 0xFFu;
    }
    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
}
)";

#endif // FRACTALS_H