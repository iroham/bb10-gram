#include "cam.h"

#include <camera/camera_api.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

struct glance_cam {
    camera_handle_t  handle;
    glance_frame_cb  cb;
    void            *user;
    uint8_t         *gray;      // packed Y buffer, reused across frames
    int              gray_cap;  // allocated bytes
    int              started;
};

// Pack the NV12 Y plane (which may be strided) into a contiguous width*height
// grayscale buffer and hand it to the user callback.
static void vf_callback(camera_handle_t h, camera_buffer_t *buf, void *arg)
{
    (void)h;
    struct glance_cam *c = (struct glance_cam *)arg;
    if (!c || !c->cb) return;
    if (buf->frametype != CAMERA_FRAMETYPE_NV12) return;

    const camera_frame_nv12_t *d = &buf->framedesc.nv12;
    int w = (int)d->width, hgt = (int)d->height, stride = (int)d->stride;
    if (w <= 0 || hgt <= 0 || stride < w || !buf->framebuf) return;

    int need = w * hgt;
    if (c->gray_cap < need) {
        uint8_t *n = (uint8_t *)realloc(c->gray, (size_t)need);
        if (!n) return;
        c->gray = n;
        c->gray_cap = need;
    }
    if (stride == w) {
        memcpy(c->gray, buf->framebuf, (size_t)need);
    } else {
        const uint8_t *src = buf->framebuf;
        uint8_t *dst = c->gray;
        for (int y = 0; y < hgt; ++y) {
            memcpy(dst, src, (size_t)w);
            src += stride;
            dst += w;
        }
    }

    glance_frame_t f;
    f.width = w;
    f.height = hgt;
    f.gray = c->gray;
    f.timestamp = buf->frametimestamp;
    f.orientation = buf->frameorientation;
    c->cb(&f, c->user);
}

static void status_callback(camera_handle_t h, camera_devstatus_t status,
                            uint16_t extra, void *arg)
{
    (void)h; (void)status; (void)extra; (void)arg;
    // Device status events (powerdown, etc.) — ignored for now; the UI layer
    // will subscribe to these later to blank the preview on interruption.
}

glance_cam *glance_cam_open_front(int want_w, int want_h, char *err, int errlen)
{
    return glance_cam_open_front_win(want_w, want_h, NULL, NULL, err, errlen);
}

glance_cam *glance_cam_open_front_win(int want_w, int want_h,
                                      const char *win_group, const char *win_id,
                                      char *err, int errlen)
{
    struct glance_cam *c = (struct glance_cam *)calloc(1, sizeof(*c));
    if (!c) { if (err) snprintf(err, errlen, "out of memory"); return NULL; }
    c->handle = CAMERA_HANDLE_INVALID;

    camera_error_t e = camera_open(CAMERA_UNIT_FRONT, CAMERA_MODE_RW, &c->handle);
    if (e != CAMERA_EOK) {
        if (err) snprintf(err, errlen, "camera_open(front) failed: %d", (int)e);
        free(c);
        return NULL;
    }

    e = camera_set_vf_mode(c->handle, CAMERA_VFMODE_VIDEO);
    if (e != CAMERA_EOK) {
        if (err) snprintf(err, errlen, "set_vf_mode(video) failed: %d", (int)e);
        camera_close(c->handle);
        free(c);
        return NULL;
    }

    // Grayscale is carried by NV12's Y plane; we ask for NV12 because a GRAY8
    // viewfinder is not advertised on every unit. CREATEWINDOW=0 is headless
    // (CLI / background); CREATEWINDOW=1 renders into a window in the caller's
    // Cascades window group so a ForeignWindowControl can show the preview.
    // The WIN_GROUPID / WIN_ID properties take string values.
    int windowed = (win_group && win_id);
    if (windowed && want_w > 0 && want_h > 0) {
        e = camera_set_vf_property(c->handle,
                                   CAMERA_IMGPROP_CREATEWINDOW, 1,
                                   CAMERA_IMGPROP_WIN_GROUPID, win_group,
                                   CAMERA_IMGPROP_WIN_ID, win_id,
                                   CAMERA_IMGPROP_FORMAT, CAMERA_FRAMETYPE_NV12,
                                   CAMERA_IMGPROP_WIDTH, want_w,
                                   CAMERA_IMGPROP_HEIGHT, want_h);
    } else if (windowed) {
        e = camera_set_vf_property(c->handle,
                                   CAMERA_IMGPROP_CREATEWINDOW, 1,
                                   CAMERA_IMGPROP_WIN_GROUPID, win_group,
                                   CAMERA_IMGPROP_WIN_ID, win_id,
                                   CAMERA_IMGPROP_FORMAT, CAMERA_FRAMETYPE_NV12);
    } else if (want_w > 0 && want_h > 0) {
        e = camera_set_vf_property(c->handle,
                                   CAMERA_IMGPROP_CREATEWINDOW, 0,
                                   CAMERA_IMGPROP_FORMAT, CAMERA_FRAMETYPE_NV12,
                                   CAMERA_IMGPROP_WIDTH, want_w,
                                   CAMERA_IMGPROP_HEIGHT, want_h);
    } else {
        e = camera_set_vf_property(c->handle,
                                   CAMERA_IMGPROP_CREATEWINDOW, 0,
                                   CAMERA_IMGPROP_FORMAT, CAMERA_FRAMETYPE_NV12);
    }
    if (e != CAMERA_EOK) {
        if (err) snprintf(err, errlen, "set_vf_property failed: %d", (int)e);
        camera_close(c->handle);
        free(c);
        return NULL;
    }

    return c;
}

int glance_cam_start(glance_cam *c, glance_frame_cb cb, void *user)
{
    if (!c) return -1;
    c->cb = cb;
    c->user = user;
    camera_error_t e = camera_start_viewfinder(c->handle, vf_callback,
                                                status_callback, c);
    if (e != CAMERA_EOK) return (int)e;
    c->started = 1;
    return 0;
}

void glance_cam_stop(glance_cam *c)
{
    if (!c || !c->started) return;
    camera_stop_viewfinder(c->handle);
    c->started = 0;
}

void glance_cam_close(glance_cam *c)
{
    if (!c) return;
    glance_cam_stop(c);
    if (c->handle != CAMERA_HANDLE_INVALID) camera_close(c->handle);
    free(c->gray);
    free(c);
}
