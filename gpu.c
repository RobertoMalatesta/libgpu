#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "matrix.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b) ? (a) : (b)))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b) ? (a) : (b)))
#endif

#ifndef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))
#endif

#ifndef FLOOR
#define FLOOR(a) (a - (a - (int)a))
#endif

bool is_backward(float *tri) {
    float *a = &tri[0];
    float *b = &tri[3];
    float *c = &tri[6];
    return ((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) <= 0;
}

void gpu_pixel(uint8_t *frame, int x, int y) {
    if (x < 0 || x >= 640 || y < 0 || y >= 480) return;
    uint8_t *pixel = &frame[x * 4 + y * 640 * 4];
    pixel[0] = 0xFF;
    pixel[1] = 0xFF;
    pixel[2] = 0xFF;
    pixel[3] = 0xFF;
}

static void swapf(float *a, float *b) {
    float tmp = *a;
    *a = *b;
    *b = tmp;
}

void gpu_line(uint8_t *frame, float a[2], float b[2]) {
    float x1, y1, x2, y2;
    x1 = a[0], y1 = a[1];
    x2 = b[0], y2 = b[1];
    bool steep = ABS(b[1] - a[1]) > ABS(b[0] - a[0]);
    if (steep) {
        swapf(&x1, &y1);
        swapf(&x2, &y2);
    }
    if (x1 > x2) {
        swapf(&x1, &x2);
        swapf(&y1, &y2);
    }
    if (y1 == y2) {
        for (int x = x1; x < x2; x++) {
            if (steep) {
                gpu_pixel(frame, y1, x);
            } else {
                gpu_pixel(frame, x, y1);
            }
        }
        return;
    }
    float slope = (y2 - y1) / (x2 - x1);
    for (float x = MAX(x1, 0); x < MIN(x2, 640); x++) {
        float y = slope * (x - x1) + y1;
        if (steep) {
            gpu_pixel(frame, y, x);
        } else {
            gpu_pixel(frame, x, y);
        }
    }
}

static void swapfv(float **a, float **b) {
    float *tmp;
    tmp = *a;
    *a = *b;
    *b = tmp;
}

void gpu_2d_triangle_fill(uint8_t *frame, float *t) {
    float *v1 = &t[0], *v2 = &t[3], *v3 = &t[6];
    float *tmp;
    if (v2[1] < v1[1]) {
        swapfv(&v2, &v1);
    }
    if (v3[1] < v1[1]) {
        swapfv(&v3, &v2);
        swapfv(&v2, &v1);
    } else if (v3[1] < v2[1]) {
        swapfv(&v3, &v2);
    }
    float *top = v3, *lmid = v2, *bot = v1;
    float *rmid;
    if (top[1] == lmid[1]) {
        rmid = top;
    } else {
        float rslope = (top[0] - bot[0]) / (top[1] - bot[1]);
        float tmp[3] = {
            (lmid[1] - bot[1]) * rslope + bot[0],
            lmid[1],
            0, // tbd
        };
        rmid = tmp;
    }
    if (rmid[0] < lmid[0]) {
        swapfv(&rmid, &lmid);
    }
    /*
    printf("top: %.2f, %.2f, %.2f\n", top[0], top[1], top[2]);
    printf("midl %.2f, %.2f, %.2f\n", lmid[0], lmid[1], lmid[2]);
    printf("midr %.2f, %.2f, %.2f\n", rmid[0], rmid[1], rmid[2]);
    printf("bot: %.2f, %.2f, %.2f\n", bot[0], bot[1], bot[2]);
    printf("\n");
    */
    // top half
    float ldx, ldy, rdx, rdy, div, lx, rx;
    ldx = top[0] - lmid[0];
    ldy = top[1] - lmid[1];
    rdx = top[0] - rmid[0];
    rdy = top[1] - rmid[1];
    div = ldy + 1;
    if (div != 0) {
        div = 1.0 / div;
        ldx *= div;
        ldy *= div;
        rdx *= div;
        rdy *= div;
        lx = lmid[0];
        rx = rmid[0];
        for (int y = lmid[1]; y < top[1]; y += 1) {
            float p1[] = {lx, y};
            float p2[] = {rx, y};
            gpu_line(frame, p1, p2);
            lx += ldx;
            rx += rdx;
        }
    }
    // bottom half
    ldx = lmid[0] - bot[0];
    ldy = lmid[1] - bot[1];
    rdx = rmid[0] - bot[0];
    rdy = rmid[1] - bot[1];
    div = ldy + 1;
    if (div != 0) {
        div = 1.0 / div;
        ldx *= div;
        ldy *= div;
        rdx *= div;
        rdy *= div;
        lx = bot[0];
        rx = bot[0];
        for (int y = bot[1]; y < lmid[1]; y += 1) {
            float p1[] = {lx, y};
            float p2[] = {rx, y};
            gpu_line(frame, p1, p2);
            lx += ldx;
            rx += rdx;
        }
    }
}

void gpu_triangle(uint8_t *frame, float *verts, float rotate, float offset, bool fill) {
    mat4 *viewport = mat4_new();
    mat4_translate(viewport, (640 - 0.5f) / 2.0f, (480 - 0.5f) / 2.0f, -1.0f);
    mat4_scale(viewport, (640 - 0.5f) / 2.0f, -(480 - 0.5f) / 2.0f, 1.0f);

    mat4 *model = mat4_new();
    mat4_translate(model, offset + 3.0f, 0, 10.0f);
    mat4_rotate(model, rotate, 1.0f, 0, 0);

    mat4 *view = mat4_new();
    mat4_perspective(view, 45.0f, 640.0f / 480.0f, 0.1f, 100.0f);

    for (int i = 0; i < 3; i++) {
        float *v = &verts[i * 3];
        mat4_mul_vec3(model, v, v);
        mat4_mul_vec3(view, v, v);
        mat4_mul_vec3(viewport, v, v);
    }
    if (is_backward(verts)) {
        return;
    }
    if (!fill) {
        for (int i = 0; i < 3; i++) {
            int next = (i + 1) % 3;
            gpu_line(frame, &verts[i * 3], &verts[next * 3]);
        }
    } else {
        gpu_2d_triangle_fill(frame, verts);
    }
}

void gpu_frame(uint8_t *frame, int counter) {
    for (int y = 0; y < 480; y++) {
        for (int x = 0; x < 640; x++) {
            uint8_t *pixel = &frame[y * 640 * 4 + x * 4];
            pixel[0] = 0x00;
            pixel[1] = 0x00;
            pixel[2] = 0x00;
            pixel[3] = 0xFF;
        }
    }
    #include "shapes.h"

    /*
    float tri2d1[9] = {
        0, 0, 0,
        1, 1, 0,
        1, -1, 0,
    };
    gpu_triangle(frame, tri2d1, 0, 0, 0);
    float tri2d2[9] = {
        0, 0, 0,
        1, 1, 0,
        1, -1, 0,
    };
    gpu_triangle(frame, tri2d2, 0, 2, 1);
    */

    for (int i = 0; i < 4; i++) {
//        gpu_triangle(frame, &tri3d[i * 9], counter / 2);
    }
    for (int i = 0; i < 12; i++) {
        #include "shapes.h"
        gpu_triangle(frame, &cube3d[i * 9], counter / 10, 0, 1);
    }
    for (int i = 0; i < 12; i++) {
        #include "shapes.h"
        gpu_triangle(frame, &cube3d[i * 9], counter / 10, -6, 0);
    }
}
