// Front-camera capture for Glance (org.bq.faceunlock).
//
// Opens the BB10 front camera headless (no screen window) and delivers a stream
// of GRAYSCALE frames via a callback — the Y plane of the NV12 viewfinder, which
// is exactly what the face detector/recogniser wants. No colour, no display
// surface; this runs in the background while the Cascades UI (later) shows a
// separate preview if it wants one.
//
// Thread note: the frame callback fires on the camera's own viewfinder thread,
// not the caller's. Keep work in it bounded; copy out what you need.
#ifndef GLANCE_CAM_H
#define GLANCE_CAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int             width;      // pixels
    int             height;     // pixels
    const uint8_t  *gray;       // width*height, packed (row stride == width)
    int64_t         timestamp;  // frame timestamp from the camera, ns
    int             orientation;// frameorientation reported by the camera, degrees
} glance_frame_t;

// Called once per viewfinder frame, on the camera thread. The gray pointer is
// valid only for the duration of the call.
typedef void (*glance_frame_cb)(const glance_frame_t *f, void *user);

typedef struct glance_cam glance_cam;

// Open the front unit headless. want_w/want_h are hints (0 = let the driver
// pick); the real size is reported per frame. On failure returns NULL and, if
// err is non-NULL, writes a message. 'detail' from camera_* errors is included.
glance_cam *glance_cam_open_front(int want_w, int want_h, char *err, int errlen);

// As above, but the viewfinder also renders into an on-screen window so a
// Cascades ForeignWindowControl can display the live preview. win_group and
// win_id are the control's window-group name and window id (both strings); the
// camera creates its viewfinder window in that group with that id, and the
// frame callback still fires with grayscale frames for detection. If win_group
// or win_id is NULL this is identical to glance_cam_open_front (headless).
glance_cam *glance_cam_open_front_win(int want_w, int want_h,
                                      const char *win_group, const char *win_id,
                                      char *err, int errlen);

// Start the viewfinder; cb fires per frame until glance_cam_stop(). Returns 0 ok.
int  glance_cam_start(glance_cam *c, glance_frame_cb cb, void *user);

// Stop the viewfinder (safe to call if not started).
void glance_cam_stop(glance_cam *c);

// Stop (if needed) and release the camera.
void glance_cam_close(glance_cam *c);

#ifdef __cplusplus
}
#endif

#endif
