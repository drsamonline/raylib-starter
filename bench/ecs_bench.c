/* bench/ecs_bench.c - does archetype storage pay at this scale?
 *
 * The shipped ECS keeps a pointer per component and a mask per entity, and a
 * system scans every entity testing the mask, then reads components straight off
 * the entity (O(1) - this benchmark is what found the earlier O(n) lookup). An archetype ECS groups entities
 * by their component set and stores the component data inline, so a system walks
 * only matching entities and their data is contiguous.
 *
 * The second is the textbook answer. The question is whether it is worth the
 * extra API at the sizes this starter allows. Measured, not assumed.
 *
 *   cc -std=c11 -O2 -I. -o ecs-bench bench/ecs_bench.c -lm && ./ecs-bench
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../main.c"

static double now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

/* ---------- A: the shipped ECS ---------- */
typedef struct { float x, y; } pos_t;
typedef struct { float vx, vy; } vel_t;

static float g_sink;

static void sys_a_integrate(gk_world *w, gk_entity *e) {
    (void)w;
    pos_t *p = (pos_t *)gk_ecs_comp(e, 0);
    vel_t *v = (vel_t *)gk_ecs_comp(e, 1);
    if (p && v) { p->x += v->vx * 0.016f; p->y += v->vy * 0.016f; }
}

/* ---------- B: archetype storage ---------- */
/* Components are inline in per-archetype arrays: an entity's position sits next
   to its neighbours' positions, and a system walks the archetype, not the world. */
#define ARCH_MAX 16
#define ARCH_CAP 8192

typedef struct {
    uint32_t mask;
    int count;
    uint32_t ids[ARCH_CAP];
    /* one array per component slot, ARCH_CAP entries of the component's size */
    void *cols[8];
} arch_t;

typedef struct {
    arch_t arch[ARCH_MAX];
    int narch;
    int comp_size[8];
    uint32_t next_id;
} arch_world;

static int arch_slot(arch_world *w, uint32_t mask) {
    for (int i = 0; i < w->narch; i++) if (w->arch[i].mask == mask) return i;
    if (w->narch >= ARCH_MAX) return -1;
    arch_t *a = &w->arch[w->narch];
    memset(a, 0, sizeof *a);
    a->mask = mask;
    for (int c = 0; c < 8; c++) {
        if ((mask & (1u << c)) && w->comp_size[c] > 0)
            a->cols[c] = calloc(ARCH_CAP, (size_t)w->comp_size[c]);
    }
    return w->narch++;
}

static void *arch_add(arch_world *w, int arch, int comp, const void *data) {
    arch_t *a = &w->arch[arch];
    if (a->count >= ARCH_CAP) return NULL;
    memcpy((char *)a->cols[comp] + (size_t)a->count * w->comp_size[comp], data, (size_t)w->comp_size[comp]);
    return (char *)a->cols[comp] + (size_t)a->count * w->comp_size[comp];
}

static void arch_system_integrate(arch_world *w, int arch) {
    arch_t *a = &w->arch[arch];
    pos_t *p = (pos_t *)a->cols[0];
    vel_t *v = (vel_t *)a->cols[1];
    for (int i = 0; i < a->count; i++) {
        p[i].x += v[i].vx * 0.016f;
        p[i].y += v[i].vy * 0.016f;
    }
}

int main(void) {
    printf("ecs benchmark: integrate pos += vel * dt\n\n");
    printf("%-8s %-12s %-14s %-14s %-8s\n", "entities", "iterations", "scan ns/ent", "arch ns/ent", "ratio");

    int sizes[] = { 256, 1024, 4096 };
    for (unsigned s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        int n = sizes[s];
        if (n > GK_MAX_ENTITIES) {
            printf("%-8d skipped (GK_MAX_ENTITIES=%d)\n", n, GK_MAX_ENTITIES);
            continue;
        }
        const int iters = 2000;

        /* --- A --- */
        gk_world w;
        gk_world_init(&w);
        pos_t *pa = calloc((size_t)n, sizeof *pa);
        vel_t *va = calloc((size_t)n, sizeof *va);
        for (int i = 0; i < n; i++) {
            pa[i] = (pos_t){ (float)i, (float)i };
            va[i] = (vel_t){ 1.0f, 0.5f };
            uint32_t id = gk_ecs_create(&w);
            gk_ecs_add(&w, id, 0, &pa[i]);
            gk_ecs_add(&w, id, 1, &va[i]);
        }
        gk_ecs_system(&w, (1u << 0) | (1u << 1), sys_a_integrate);
        double t0 = now_ns();
        for (int k = 0; k < iters; k++) gk_ecs_run(&w);
        double t1 = now_ns();
        g_sink += pa[0].x + pa[n - 1].y;

        /* --- B --- */
        arch_world aw;
        memset(&aw, 0, sizeof aw);
        aw.comp_size[0] = (int)sizeof(pos_t);
        aw.comp_size[1] = (int)sizeof(vel_t);
        int arch = arch_slot(&aw, (1u << 0) | (1u << 1));
        for (int i = 0; i < n; i++) {
            pos_t p = { (float)i, (float)i };
            vel_t v = { 1.0f, 0.5f };
            arch_add(&aw, arch, 0, &p);
            arch_add(&aw, arch, 1, &v);
            aw.arch[arch].ids[aw.arch[arch].count++] = ++aw.next_id;
        }
        double t2 = now_ns();
        for (int k = 0; k < iters; k++) {
            for (int a = 0; a < aw.narch; a++) {
                if ((aw.arch[a].mask & ((1u << 0) | (1u << 1))) == ((1u << 0) | (1u << 1)))
                    arch_system_integrate(&aw, a);
            }
        }
        double t3 = now_ns();
        g_sink += ((pos_t *)aw.arch[arch].cols[0])[0].x;

        double scan = (t1 - t0) / (double)(n * iters);
        double arch_ns = (t3 - t2) / (double)(n * iters);
        printf("%-8d %-12d %-14.3f %-14.3f %-8.2f\n", n, iters, scan, arch_ns, scan / arch_ns);

        for (int c = 0; c < 8; c++) free(aw.arch[arch].cols[c]);
        free(pa); free(va);
    }

    printf("\n(sink %f)\n", (double)g_sink);
    return 0;
}
