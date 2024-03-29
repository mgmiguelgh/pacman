/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <gl/gl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "../platform.h"

#include "../glfuncs.h"
#include "../game.c"

#define WIN_CHECK_CREATION_ERROR(expr, msg) \
    do { \
        if(!(expr)) { \
            MessageBoxA(NULL, msg, "Error", MB_ICONERROR); \
            exit(-1); \
        } \
    } while(0)

HGLRC win_setup_opengl(HDC device_context);

volatile struct {
    HANDLE semaphore;
    uint32_t input;
    bool running;
} game_env_data = {
    .semaphore = NULL,
    .input = 0,
    .running = true
};

static DWORD get_window_style(void) {
    return WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX | WS_SYSMENU | WS_VISIBLE;
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {

#define SET_INPUT(cond, action) if((cond)) { game_env_data.input |= action; }
#define UNSET_INPUT(cond, action) if((cond)) { game_env_data.input &= ~action; }

    switch(message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_KEYDOWN:
            SET_INPUT(w_param == 'W' || w_param == VK_UP, INPUT_UP);
            SET_INPUT(w_param == 'S' || w_param == VK_DOWN, INPUT_DOWN);
            SET_INPUT(w_param == 'A' || w_param == VK_LEFT, INPUT_LEFT);
            SET_INPUT(w_param == 'D' || w_param == VK_RIGHT, INPUT_RIGHT);
            SET_INPUT(w_param == 'M' || w_param == 'Z' || w_param == VK_RETURN, INPUT_CONFIRM);
            SET_INPUT(w_param == VK_ESCAPE, INPUT_MENU);
            break;
        case WM_KEYUP:
            UNSET_INPUT(w_param == 'W' || w_param == VK_UP, INPUT_UP);
            UNSET_INPUT(w_param == 'S' || w_param == VK_DOWN, INPUT_DOWN);
            UNSET_INPUT(w_param == 'A' || w_param == VK_LEFT, INPUT_LEFT);
            UNSET_INPUT(w_param == 'D' || w_param == VK_RIGHT, INPUT_RIGHT);
            UNSET_INPUT(w_param == 'M' || w_param == 'Z' || w_param == VK_RETURN, INPUT_CONFIRM);
            UNSET_INPUT(w_param == VK_ESCAPE, INPUT_MENU);
            break;
        case WM_SIZE: {
            signal_window_resize(LOWORD(l_param), HIWORD(l_param));
            break;
        }
        case WM_SIZING: {
            RECT *r = (RECT *)l_param;
            RECT window_rect = { 0 };
            AdjustWindowRectEx(&window_rect, get_window_style(), FALSE, 0);

            int32_t xoff = window_rect.right - window_rect.left;
            int32_t yoff = window_rect.bottom - window_rect.top;

            if((r->right - r->left - xoff) < DEFAULT_WINDOW_WIDTH) {
                r->right = r->left + (DEFAULT_WINDOW_WIDTH + xoff);
            }

            if((r->bottom - r->top - yoff) < DEFAULT_WINDOW_HEIGHT) {
                r->bottom = r->top + (DEFAULT_WINDOW_HEIGHT + yoff);
            }

            break;
        }
        default:
            return DefWindowProcW(window, message, w_param, l_param);
    }

#undef SET_INPUT
#undef UNSET_INPUT

    return (LRESULT)0;
}

DWORD WINAPI win_game_init_and_run(LPVOID parameter) {
    HWND window = parameter;

    HDC device_context = GetDC(window);
    HGLRC rendering_context = win_setup_opengl(device_context);

    double elapsed_time = 0.0;
    LARGE_INTEGER perf_freq;
    LARGE_INTEGER current_time;
    LARGE_INTEGER prev_time;
    QueryPerformanceFrequency(&perf_freq);
    QueryPerformanceCounter(&current_time);

    while(game_env_data.running) {
        game_env_data.running = update_loop((float)elapsed_time, game_env_data.input);

        render_loop((float)elapsed_time);
        SwapBuffers(device_context);

        prev_time = current_time;
        QueryPerformanceCounter(&current_time);
        elapsed_time = (double)(current_time.QuadPart - prev_time.QuadPart) /
            (double)perf_freq.QuadPart;

        elapsed_time = (elapsed_time < 1.0f) ? elapsed_time : 1.0f;

        ReleaseSemaphore(game_env_data.semaphore, 1, NULL);
    }

    close_game();

    ReleaseDC(window, device_context);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rendering_context);

    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR args, int cmd_show) {
    srand((unsigned int)time(NULL));

    IGNORED_VARIABLE(prev);
    IGNORED_VARIABLE(args);
    IGNORED_VARIABLE(cmd_show);

    WNDCLASSEXW window_class = {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = window_proc,
        .hInstance = instance,
        .hIcon = LoadIconW(NULL, IDI_APPLICATION),
        .hCursor = LoadCursorW(NULL, IDC_ARROW),
        .lpszClassName = L"pacman_window_class"
    };

    RegisterClassExW(&window_class);

    RECT client_rect = {
        .left = 0,
        .top = 0,
        .right = DEFAULT_WINDOW_WIDTH,
        .bottom = DEFAULT_WINDOW_HEIGHT
    };

    DWORD window_style = get_window_style();
    AdjustWindowRectEx(&client_rect, window_style, FALSE, 0);

    HWND window = CreateWindowExW(0,
                                  window_class.lpszClassName,
                                  L"Pacman Clone",
                                  window_style,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  client_rect.right - client_rect.left,
                                  client_rect.bottom - client_rect.top,
                                  NULL,
                                  NULL,
                                  instance,
                                  NULL);

    WIN_CHECK_CREATION_ERROR(window, "Could not create window!\n");

    GetClientRect(window, &client_rect);

    // We run the game on a different thread, so it doesn't freeze when dragging the window around
    game_env_data.semaphore = CreateSemaphoreW(NULL, 0, 1, NULL);
    HANDLE game_thread = CreateThread(NULL, 0, win_game_init_and_run, window, 0, NULL);

    MSG message;
    while(game_env_data.running) {
        while(PeekMessageW(&message, NULL, 0, 0, PM_REMOVE) != 0) {
            if(message.message == WM_QUIT) {
                game_env_data.running = false;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        WaitForSingleObject(game_env_data.semaphore, INFINITE);
    }

    WaitForSingleObject(game_thread, INFINITE);
    CloseHandle(game_thread);
    CloseHandle(game_env_data.semaphore);

    DestroyWindow(window);

    return 0;
}

HGLRC win_setup_opengl(HDC device_context) {
    PIXELFORMATDESCRIPTOR format_descriptor = {
        .nSize = sizeof(PIXELFORMATDESCRIPTOR),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |
            PFD_GENERIC_ACCELERATED | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 24,
        .cAlphaBits = 8,
        .cDepthBits = 0,
        .cStencilBits = 0
    };
    int format_index = ChoosePixelFormat(device_context, &format_descriptor);
    WIN_CHECK_CREATION_ERROR(format_index != 0, "Could not choose a valid OpenGL pixel format!\n");

    SetPixelFormat(device_context, format_index, &format_descriptor);

    HGLRC rendering_context = wglCreateContext(device_context);
    WIN_CHECK_CREATION_ERROR(rendering_context, "Could not create an OpenGL rendering context!\n");

    wglMakeCurrent(device_context, rendering_context);

    load_all_gl_extensions();

    int ext_attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_FULL_ACCELERATION_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_COLOR_BITS_ARB, 24,
        WGL_ALPHA_BITS_ARB, 8,
        0
    };

    int ext_format_index;
    UINT num_formats;
    BOOL format_is_valid = wglChoosePixelFormatARB(device_context, ext_attribs, NULL, 1,
                                                   &ext_format_index, &num_formats);
    WIN_CHECK_CREATION_ERROR(format_is_valid, "Could not obtain a valid CORE OpenGL pixel format!\n");

    SetPixelFormat(device_context, ext_format_index, &format_descriptor);
    wglMakeCurrent(device_context, NULL);
    wglDeleteContext(rendering_context);

    int context_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };
    rendering_context = wglCreateContextAttribsARB(device_context, NULL, context_attribs);
    WIN_CHECK_CREATION_ERROR(rendering_context, "Could not create a valid CORE OpenGL rendering context!\n");

    wglMakeCurrent(device_context, rendering_context);

    // Check if we obtained the correct OpenGL version
    GLint opengl_version[2] = { 0 };
    glGetIntegerv(GL_MAJOR_VERSION, &opengl_version[0]);
    glGetIntegerv(GL_MINOR_VERSION, &opengl_version[1]);
    WIN_CHECK_CREATION_ERROR(opengl_version[0] > 3 || (opengl_version[0] == 3 && opengl_version[1] >= 3),
                             "Could not obtain an OpenGL 3.3 or newer context!\n");

    wglSwapIntervalEXT(1);

    initialize_game();

    return rendering_context;
}

void init_level_names(LevelFileData *data) {
    static const char *level_dir = "data/level/*.csv";
    WIN32_FIND_DATAA file_data;
    HANDLE level_dir_handle = FindFirstFileA(level_dir, &file_data);
    if(level_dir_handle) {
        do {
            data->count++;
        } while(FindNextFileA(level_dir_handle, &file_data));
        FindClose(level_dir_handle);

        data->names = calloc(1, (MAX_PATH + 1) * data->count);

        int32_t i = 0;
        level_dir_handle = FindFirstFileA(level_dir, &file_data);
        do {
            memcpy(&data->names[i * (MAX_PATH + 1)], file_data.cFileName, MAX_PATH);
            i++;
        } while(FindNextFileA(level_dir_handle, &file_data));
        FindClose(level_dir_handle);

        qsort(data->names, data->count, MAX_PATH + 1, level_name_compare);
    } else {
        WIN_CHECK_CREATION_ERROR(0, "Failed to load any level data!\n");
    }
}

void destroy_level_names(LevelFileData *data) {
    free(data->names);
}
