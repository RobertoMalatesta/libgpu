#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "frame.h"

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

#define SWAP(a, b) do { tmp = (a); (a) = (b); (b) = (tmp); } while (0);

color_t white = {0xFF, 0xFF, 0xFF, 0xFF};

bool is_backward(vertex_t *tri, int index) {
    pos_t *a = &tri->pos[index+0];
    pos_t *b = &tri->pos[index+1];
    pos_t *c = &tri->pos[index+2];
    return ((b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x)) <= 0;
}

static inline color_t *gpu_pixel_at(gpu_frame *frame, int x, int y) {
    if (x < 0 || x >= frame->width || y < 0 || y >= frame->height) return NULL;
    return &(frame->buf[x + y * frame->width]);
}

void gpu_pixel(gpu_frame *frame, int x, int y) {
    color_t *pixel = gpu_pixel_at(frame, x, y);
    if (pixel == NULL) return;
    *pixel = white;
}

void gpu_line(gpu_frame *frame, pos_t *a, pos_t *b) {
    float x1, y1, x2, y2;
    float tmp;
    x1 = a->x, y1 = a->y;
    x2 = b->x, y2 = b->y;
    float max = frame->width;
    bool steep = ABS(y2 - y1) > ABS(x2 - x1);
    if (steep) {
        max = frame->height;
        SWAP(x1, y1);
        SWAP(x2, y2);
    }
    if (x1 > x2) {
        SWAP(x1, x2);
        SWAP(y1, y2);
    }
    float slope = (y2 - y1) / (x2 - x1);
    for (float x = MAX(x1, 0); x < MIN(x2, max); x++) {
        float y = slope * (x - x1) + y1;
        if (steep) {
            gpu_pixel(frame, y, x);
        } else {
            gpu_pixel(frame, x, y);
        }
    }
}

void gpu_triangle_fill(gpu_frame *frame, vertex_t *v, int index) {
    pos_t *v1 = &v->pos[index+0], *v2 = &v->pos[index+1], *v3 = &v->pos[index+2];
    pos_t *tmp;
    // sort vertices
    if (v2->y < v1->y) {
        SWAP(v2, v1);
    }
    if (v3->y < v1->y) {
        SWAP(v3, v2);
        SWAP(v2, v1);
    } else if (v3->y < v2->y) {
        SWAP(v3, v2);
    }
    pos_t *top = v3, *lmid = v2, *bot = v1;
    pos_t *rmid, rmidb;
    if (top->y == lmid->y) {
        rmid = top;
    } else {
        float rslope = (top->x - bot->x) / (top->y - bot->y);
        float tmid[3] = {
            (lmid->y - bot->y) * rslope + bot->x,
            lmid->y,
            0, // tbd
        };
        memcpy(&rmidb, tmid, sizeof(pos_t));
        rmid = &rmidb;
    }
    if (rmid->x < lmid->x) {
        SWAP(rmid, lmid);
    }
    // top half
    float ldx, ldy, rdx, rdy, div, lx, rx;
    ldx = top->x - lmid->x;
    ldy = top->y - lmid->y;
    rdx = top->x - rmid->x;
    rdy = top->y - rmid->y;
    div = ldy + 1;
    if (div != 0) {
        div = 1.0 / div;
        ldx *= div;
        ldy *= div;
        rdx *= div;
        rdy *= div;
        lx = lmid->x;
        rx = rmid->x;
        for (int y = lmid->y; y < top->y; y += 1) {
            float tlx = MAX(0, MIN(lx, frame->width));
            float trx = MAX(0, MIN(rx, frame->width));
            color_t *pixel = gpu_pixel_at(frame, tlx, y);
            for (int x = tlx; x < trx; x++) {
                *pixel++ = white;
            }
            lx += ldx;
            rx += rdx;
        }
    }
    // bottom half
    ldx = lmid->x - bot->x;
    ldy = lmid->y - bot->y;
    rdx = rmid->x - bot->x;
    rdy = rmid->y - bot->y;
    div = ldy + 1;
    if (div != 0) {
        div = 1.0 / div;
        ldx *= div;
        ldy *= div;
        rdx *= div;
        rdy *= div;
        lx = bot->x;
        rx = bot->x;
        for (int y = bot->y; y < lmid->y; y += 1) {
            float tlx = MAX(0, MIN(lx, frame->width));
            float trx = MAX(0, MIN(rx, frame->width));
            color_t *pixel = gpu_pixel_at(frame, tlx, y);
            for (int x = tlx; x < trx; x++) {
                *pixel++ = white;
            }
            lx += ldx;
            rx += rdx;
        }
    }
}

void gpu_triangle(gpu_frame *frame, vertex_t *verts, int index, bool wire) {
    if (is_backward(verts, index)) {
        return;
    }
    if (wire) {
        for (int i = index; i < index + 3; i++) {
            int next = index + (i + 1) % 3;
            gpu_line(frame, &verts->pos[i], &verts->pos[next]);
        }
    } else {
        gpu_triangle_fill(frame, verts, index);
    }
}
