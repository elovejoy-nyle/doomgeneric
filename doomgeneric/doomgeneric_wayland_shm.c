#include "doomkeys.h"
#include "doomgeneric.h"

#include <wayland-client.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WIN_W 480
#define WIN_H 272
#define NUM_BUFFERS 2
#define KEYQUEUE_SIZE 64

static struct wl_display *display;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct wl_shell *shell;
static struct wl_seat *seat;
static struct wl_keyboard *keyboard;
static struct wl_surface *surface;
static struct wl_shell_surface *shell_surface;

static int stride;
static int size;

static struct wl_buffer *buffers[NUM_BUFFERS];
static uint32_t *pixel_buffers[NUM_BUFFERS];
static int buffer_busy[NUM_BUFFERS];
static int current_buffer;

static int xmap[WIN_W];
static int ymap[WIN_H];
static unsigned short key_queue[KEYQUEUE_SIZE];
static unsigned int key_write_index;
static unsigned int key_read_index;

static int create_tmpfile(size_t size)
{
    char template[] = "/tmp/doom-wlshm-XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0) return -1;

    unlink(template);

    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void build_scaling_maps(void)
{
    int scaled_w = WIN_W;
    int scaled_h = (DOOMGENERIC_RESY * WIN_W) / DOOMGENERIC_RESX;

    if (scaled_h > WIN_H) {
        scaled_h = WIN_H;
        scaled_w = (DOOMGENERIC_RESX * WIN_H) / DOOMGENERIC_RESY;
    }

    int offset_x = (WIN_W - scaled_w) / 2;
    int offset_y = (WIN_H - scaled_h) / 2;

    for (int x = 0; x < WIN_W; x++) {
        if (x < offset_x || x >= offset_x + scaled_w)
            xmap[x] = -1;
        else
            xmap[x] = ((x - offset_x) * DOOMGENERIC_RESX) / scaled_w;
    }

    for (int y = 0; y < WIN_H; y++) {
        if (y < offset_y || y >= offset_y + scaled_h)
            ymap[y] = -1;
        else
            ymap[y] = ((y - offset_y) * DOOMGENERIC_RESY) / scaled_h;
    }

    printf("Scaling: Doom %dx%d -> %dx%d, offset %d,%d\n",
           DOOMGENERIC_RESX, DOOMGENERIC_RESY,
           scaled_w, scaled_h,
           offset_x, offset_y);
}

static void queue_key(int pressed, unsigned char doom_key)
{
    unsigned int next = (key_write_index + 1) % KEYQUEUE_SIZE;
    if (next == key_read_index) {
        return;
    }
    key_queue[key_write_index] = ((pressed & 1) << 8) | doom_key;
    key_write_index = next;
}

static unsigned char evdev_to_doom_key(uint32_t key)
{
    switch (key) {
        case 1:   return KEY_ESCAPE;
        case 28:  return KEY_ENTER;
        case 29:  return KEY_FIRE;
        case 56:  return KEY_LALT;
        case 57:  return KEY_FIRE;
        case 103: return KEY_UPARROW;
        case 105: return KEY_LEFTARROW;
        case 106: return KEY_RIGHTARROW;
        case 108: return KEY_DOWNARROW;
        default:
            return 0;
    }
}

static void buffer_release(void *data, struct wl_buffer *wl_buffer)
{
    int i = (int)(intptr_t)data;
    (void)wl_buffer;
    if (i >= 0 && i < NUM_BUFFERS) {
        buffer_busy[i] = 0;
    }
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release
};

static void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard_obj,
                            uint32_t format, int fd, uint32_t size_bytes)
{
    (void)data;
    (void)wl_keyboard_obj;
    (void)format;
    (void)size_bytes;
    if (fd >= 0) {
        close(fd);
    }
}

static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard_obj,
                           uint32_t serial, struct wl_surface *surface_obj,
                           struct wl_array *keys)
{
    (void)data;
    (void)wl_keyboard_obj;
    (void)serial;
    (void)surface_obj;
    (void)keys;
}

static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard_obj,
                           uint32_t serial, struct wl_surface *surface_obj)
{
    (void)data;
    (void)wl_keyboard_obj;
    (void)serial;
    (void)surface_obj;
}

static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard_obj,
                         uint32_t serial, uint32_t time, uint32_t key,
                         uint32_t state)
{
    (void)data;
    (void)wl_keyboard_obj;
    (void)serial;
    (void)time;

    unsigned char doom_key = evdev_to_doom_key(key);
    if (doom_key == 0) {
        return;
    }
    queue_key(state == WL_KEYBOARD_KEY_STATE_PRESSED, doom_key);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard_obj,
                               uint32_t serial, uint32_t mods_depressed,
                               uint32_t mods_latched, uint32_t mods_locked,
                               uint32_t group)
{
    (void)data;
    (void)wl_keyboard_obj;
    (void)serial;
    (void)mods_depressed;
    (void)mods_latched;
    (void)mods_locked;
    (void)group;
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard_obj,
                                 int32_t rate, int32_t delay)
{
    (void)data;
    (void)wl_keyboard_obj;
    (void)rate;
    (void)delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info
};

static void seat_capabilities(void *data, struct wl_seat *seat_obj, uint32_t caps)
{
    (void)data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard == NULL) {
        keyboard = wl_seat_get_keyboard(seat_obj);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard != NULL) {
        wl_keyboard_destroy(keyboard);
        keyboard = NULL;
    }
}

static void seat_name(void *data, struct wl_seat *seat_obj, const char *name)
{
    (void)data;
    (void)seat_obj;
    (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name
};

static void registry_global(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version)
{
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_shm") == 0) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, "wl_shell") == 0) {
        shell = wl_registry_bind(registry, name, &wl_shell_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    }
}

static void registry_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    registry_remove
};

void DG_Init()
{
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Wayland: impossible de se connecter au display\n");
        exit(1);
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !shm || !shell) {
        fprintf(stderr, "Wayland: compositor=%p shm=%p shell=%p seat=%p\n",
                compositor, shm, shell, seat);
        exit(1);
    }
    if (seat) {
        wl_seat_add_listener(seat, &seat_listener, NULL);
    }

    surface = wl_compositor_create_surface(compositor);
    shell_surface = wl_shell_get_shell_surface(shell, surface);
    wl_shell_surface_set_toplevel(shell_surface);

    stride = WIN_W * 4;
    size = stride * WIN_H;

    for (int i = 0; i < NUM_BUFFERS; i++) {
        int fd = create_tmpfile(size);
        if (fd < 0) {
            fprintf(stderr, "Wayland: impossible de créer le fichier SHM\n");
            exit(1);
        }

        pixel_buffers[i] = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (pixel_buffers[i] == MAP_FAILED) {
            fprintf(stderr, "Wayland: mmap failed\n");
            close(fd);
            exit(1);
        }

        struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
        buffers[i] = wl_shm_pool_create_buffer(
            pool,
            0,
            WIN_W,
            WIN_H,
            stride,
            WL_SHM_FORMAT_XRGB8888
        );
        wl_buffer_add_listener(buffers[i], &buffer_listener, (void *)(intptr_t)i);
        wl_shm_pool_destroy(pool);
        close(fd);

        memset(pixel_buffers[i], 0, size);
        buffer_busy[i] = 0;
    }
    build_scaling_maps();

    wl_display_roundtrip(display);
    wl_surface_attach(surface, buffers[0], 0, 0);
    wl_surface_damage(surface, 0, 0, WIN_W, WIN_H);
    wl_surface_commit(surface);
    buffer_busy[0] = 1;
    current_buffer = 1;
    wl_display_flush(display);

    printf("Wayland SHM init OK: %dx%d\n", WIN_W, WIN_H);
}

void DG_DrawFrame()
{
    while (buffer_busy[current_buffer]) {
        wl_display_dispatch(display);
    }

    uint32_t *pixels = pixel_buffers[current_buffer];
    memset(pixels, 0, size);

    for (int y = 0; y < WIN_H; y++) {
        int sy = ymap[y];
        if (sy < 0) continue;

        uint32_t *dst = pixels + y * WIN_W;
        uint32_t *src = (uint32_t *)DG_ScreenBuffer + sy * DOOMGENERIC_RESX;

        for (int x = 0; x < WIN_W; x++) {
            int sx = xmap[x];
            if (sx >= 0)
                dst[x] = src[sx];
        }
    }

    wl_surface_attach(surface, buffers[current_buffer], 0, 0);
    wl_surface_damage(surface, 0, 0, WIN_W, WIN_H);
    wl_surface_commit(surface);
    buffer_busy[current_buffer] = 1;
    current_buffer = (current_buffer + 1) % NUM_BUFFERS;

    wl_display_dispatch_pending(display);
    wl_display_flush(display);
}

void DG_SleepMs(uint32_t ms)
{
    usleep(ms * 1000);
}

uint32_t DG_GetTicksMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
    wl_display_dispatch_pending(display);
    if (key_read_index == key_write_index) {
        return 0;
    }

    unsigned short key_data = key_queue[key_read_index];
    key_read_index = (key_read_index + 1) % KEYQUEUE_SIZE;

    *pressed = (key_data >> 8) & 1;
    *doomKey = key_data & 0xff;
    return 1;
}

void DG_SetWindowTitle(const char *title)
{
}

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);

    while (1) {
        doomgeneric_Tick();
    }

    return 0;
}
