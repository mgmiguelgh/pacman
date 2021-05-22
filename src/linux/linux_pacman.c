/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "../platform.h"

#include "../glfuncs.h"
#include "../game.c"

#define LINUX_CHECK_CREATION_ERROR(expr, msg) \
    do { \
        if(!(expr)) { \
            fprintf(stderr, msg); \
            exit(-1); \
        } \
    } while(0)

#define MAX_PATH 260 // Defined here to be the same as Windows

static struct {
    char *names;
    uint32_t count;
    uint32_t current;
} level_files = { 0 };

int level_name_compare(const void *lhs, const void *rhs) {
    return strcmp(lhs, rhs) > 0;
}

static int select_best_fbx(Display *display, GLXFBConfig *framebuffers, int count) {
    int idx = -1;
    int best_total_bits = -1;
    for(int i = 0; i < count; i++) {
        XVisualInfo *visual = glXGetVisualFromFBConfig(display, framebuffers[i]);
        if(visual) {
            int double_buffering;

            glXGetFBConfigAttrib(display, framebuffers[i], GLX_DOUBLEBUFFER, &double_buffering);
            if(!double_buffering) {
                continue;
            }

            int rgba[4];
            glXGetFBConfigAttrib(display, framebuffers[i], GLX_RED_SIZE, &rgba[0]);
            glXGetFBConfigAttrib(display, framebuffers[i], GLX_GREEN_SIZE, &rgba[1]);
            glXGetFBConfigAttrib(display, framebuffers[i], GLX_BLUE_SIZE, &rgba[2]);
            glXGetFBConfigAttrib(display, framebuffers[i], GLX_ALPHA_SIZE, &rgba[3]);

            int total_bits = rgba[0] + rgba[1] + rgba[2] + rgba[3];
            if(best_total_bits < total_bits) {
                best_total_bits = total_bits;
                idx = i;
            }

            XFree(visual);
        }
    }

    return idx;
}

static void query_all_level_names(void) {
    DIR *d = opendir("data/level");
    struct dirent *dir;
    if(d) {
        while((dir = readdir(d)) != NULL) {
            if(dir->d_type == DT_REG) {
                level_files.count++;
            }
        }
        rewinddir(d);

        level_files.names = calloc(1, (MAX_PATH + 1) * level_files.count);

        int32_t index = 0;
        while((dir = readdir(d)) != NULL) {
            if(dir->d_type == DT_REG) {
                memcpy(&level_files.names[index * (MAX_PATH + 1)], dir->d_name, MAX_PATH);
                index++;
            }
        }

        qsort(level_files.names, level_files.count, MAX_PATH + 1, level_name_compare);
        closedir(d);
    }
}

int main(int argc, char **argv) {
    srand((unsigned int)time(NULL));

    Display *display = XOpenDisplay(NULL);
    LINUX_CHECK_CREATION_ERROR(display, "Failed to connect to the X Server\n");

    int screen = DefaultScreen(display);

    load_all_gl_extensions();

    int gl_attribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DOUBLEBUFFER, True,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        None
    };

    int num_elements;
    GLXFBConfig *glx_framebuffers = glXChooseFBConfig(display, screen, gl_attribs, &num_elements);
    LINUX_CHECK_CREATION_ERROR(glx_framebuffers, "Could not select a valid GLX Frame Buffer!\n");

    int fbx_idx = select_best_fbx(display, glx_framebuffers, num_elements);
    fbx_idx = (fbx_idx != -1) ? fbx_idx : 0;
    GLXFBConfig selected_fb = glx_framebuffers[fbx_idx];
    XFree(glx_framebuffers);

    XVisualInfo *visual_info = glXGetVisualFromFBConfig(display, selected_fb);
    XSetWindowAttributes set_window_attributes = { 0 };
    set_window_attributes.colormap = XCreateColormap(display, RootWindow(display, screen),
                                                     visual_info->visual, AllocNone);
    set_window_attributes.background_pixmap = None;
    set_window_attributes.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask;

    Window window = XCreateWindow(display, RootWindow(display, screen),
                                  0, 0,
                                  DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                                  0, visual_info->depth,
                                  InputOutput,
                                  visual_info->visual,
                                  CWBorderPixel | CWColormap | CWEventMask, &set_window_attributes);
    LINUX_CHECK_CREATION_ERROR(window, "Could not create a X11 Window!\n");

    XStoreName(display, window, "Pacman Clone");
    XMapWindow(display, window);

    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };
    GLXContext gl_context = glXCreateContextAttribsARB(display, selected_fb, NULL, true, context_attribs);
    LINUX_CHECK_CREATION_ERROR(gl_context, "Could not create a valid OpenGL context!\n");

    glXMakeCurrent(display, window, gl_context);

    // Check if we obtained the correct OpenGL version
    GLint opengl_version[2] = { 0 };
    glGetIntegerv(GL_MAJOR_VERSION, &opengl_version[0]);
    glGetIntegerv(GL_MINOR_VERSION, &opengl_version[1]);
    LINUX_CHECK_CREATION_ERROR(opengl_version[0] > 3 || (opengl_version[0] == 3 && opengl_version[1] >= 3),
                               "Could not obtain an OpenGL 3.3 or newer context!\n");

    query_all_level_names();

    glXSwapIntervalEXT(display, window, 1);
    initialize_game();

    struct timespec current, previous;
    clock_gettime(CLOCK_MONOTONIC, &current);
    double elapsed_time = 0.0;

    XEvent event;
    uint32_t input = 0;
    bool running = true;
    while(running) {
        bool received_resize_event = false;
        while(XPending(display)) {
            XNextEvent(display, &event);

#define SET_INPUT(sym, action) if(event.xkey.keycode == XKeysymToKeycode(display, (sym))) { input |= action; }
#define UNSET_INPUT(sym, action) if(event.xkey.keycode == XKeysymToKeycode(display, (sym))) { input &= ~action; }

            switch(event.type) {
                case KeyPress:
                    SET_INPUT(XK_w, INPUT_UP);
                    SET_INPUT(XK_Up, INPUT_UP);
                    SET_INPUT(XK_s, INPUT_DOWN);
                    SET_INPUT(XK_Down, INPUT_DOWN);
                    SET_INPUT(XK_a, INPUT_LEFT);
                    SET_INPUT(XK_Left, INPUT_LEFT);
                    SET_INPUT(XK_d, INPUT_RIGHT);
                    SET_INPUT(XK_Right, INPUT_RIGHT);
                    SET_INPUT(XK_Return, INPUT_CONFIRM);
                    SET_INPUT(XK_m, INPUT_CONFIRM);
                    SET_INPUT(XK_z, INPUT_CONFIRM);
                    SET_INPUT(XK_Escape, INPUT_MENU);
                    break;
                case KeyRelease:
                    UNSET_INPUT(XK_w, INPUT_UP);
                    UNSET_INPUT(XK_Up, INPUT_UP);
                    UNSET_INPUT(XK_s, INPUT_DOWN);
                    UNSET_INPUT(XK_Down, INPUT_DOWN);
                    UNSET_INPUT(XK_a, INPUT_LEFT);
                    UNSET_INPUT(XK_Left, INPUT_LEFT);
                    UNSET_INPUT(XK_d, INPUT_RIGHT);
                    UNSET_INPUT(XK_Right, INPUT_RIGHT);
                    UNSET_INPUT(XK_Return, INPUT_CONFIRM);
                    UNSET_INPUT(XK_m, INPUT_CONFIRM);
                    UNSET_INPUT(XK_z, INPUT_CONFIRM);
                    UNSET_INPUT(XK_Escape, INPUT_MENU);
                    break;
                case ConfigureNotify: {
                    XConfigureEvent config_event = event.xconfigure;
                    signal_window_resize(config_event.width, config_event.height);
                    received_resize_event = true;
                    break;
                }
                default:
                    break;
            }

#undef SET_INPUT
#undef UNSET_INPUT
        }

        if(received_resize_event) {
            XWindowAttributes attributes;
            XGetWindowAttributes(display, window, &attributes);
            int32_t width = attributes.width;
            int32_t height = attributes.height;

            if(width < DEFAULT_WINDOW_WIDTH || height < DEFAULT_WINDOW_HEIGHT) {
                XResizeWindow(display, window,
                              MAX(DEFAULT_WINDOW_WIDTH, width),
                              MAX(DEFAULT_WINDOW_HEIGHT, height));
            }
        }

        running = update_loop((float)elapsed_time, input);
        render_loop((float)elapsed_time);
        glXSwapBuffers(display, window);

        previous = current;
        clock_gettime(CLOCK_MONOTONIC, &current);
        uint64_t ticks_current = (current.tv_sec * 1000000000) + current.tv_nsec;
        uint64_t ticks_prev = (previous.tv_sec * 1000000000) + previous.tv_nsec;
        elapsed_time = (double)(ticks_current - ticks_prev) / (double)1000000000;

    }

    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, gl_context);
    XFree(visual_info);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    free(level_files.names);

    return 0;
}

const char * get_next_level_name(void) {
    const char *name = &level_files.names[level_files.current * (MAX_PATH + 1)];
    level_files.current = (level_files.current + 1) % level_files.count;
    return name;
}

void set_next_level_index(uint32_t index) {
    level_files.current = index % level_files.count;
}
