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
#include <time.h>

#include "../glfuncs.h"
#include "../game.c"

#define WIN_CHECK_CREATION_ERROR(expr, msg) \
    do { \
        if(!(expr)) { \
            OutputDebugStringA(msg); \
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

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {

#define SET_INPUT(cond, dir) if((cond)) { game_env_data.input |= dir; }
#define UNSET_INPUT(cond, dir) if((cond)) { game_env_data.input &= ~dir; }

    switch(message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_KEYDOWN:
            SET_INPUT(w_param == 'W' || w_param == VK_UP, INPUT_UP);
            SET_INPUT(w_param == 'S' || w_param == VK_DOWN, INPUT_DOWN);
            SET_INPUT(w_param == 'A' || w_param == VK_LEFT, INPUT_LEFT);
            SET_INPUT(w_param == 'D' || w_param == VK_RIGHT, INPUT_RIGHT);
            SET_INPUT(w_param == 'N' || w_param == 'Z', INPUT_CONFIRM);
            SET_INPUT(w_param == 'M' || w_param == 'X', INPUT_CANCEL);

            if(w_param == VK_ESCAPE) {
                game_env_data.running = false;
            }

            break;
        case WM_KEYUP:
            UNSET_INPUT(w_param == 'W' || w_param == VK_UP, INPUT_UP);
            UNSET_INPUT(w_param == 'S' || w_param == VK_DOWN, INPUT_DOWN);
            UNSET_INPUT(w_param == 'A' || w_param == VK_LEFT, INPUT_LEFT);
            UNSET_INPUT(w_param == 'D' || w_param == VK_RIGHT, INPUT_RIGHT);
            UNSET_INPUT(w_param == 'N' || w_param == 'Z', INPUT_CONFIRM);
            UNSET_INPUT(w_param == 'M' || w_param == 'X', INPUT_CANCEL);
            break;
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

    initialize_game();

    float elapsed_time = 0.0f;
    LARGE_INTEGER perf_freq;
    LARGE_INTEGER current_time;
    LARGE_INTEGER prev_time;
    QueryPerformanceFrequency(&perf_freq);
    QueryPerformanceCounter(&current_time);

    uint32_t *fb = get_framebuffer();

    while(game_env_data.running) {
        update_loop(elapsed_time, game_env_data.input);

        memset(fb, 0, sizeof(fb[0]) * DEFAULT_FRAMEBUFFER_WIDTH * DEFAULT_FRAMEBUFFER_HEIGHT);

        render_loop();
        SwapBuffers(device_context);

        prev_time = current_time;
        QueryPerformanceCounter(&current_time);
        elapsed_time = (float)(current_time.QuadPart - prev_time.QuadPart) /
            (float)perf_freq.QuadPart;

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

    DWORD window_style = WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_VISIBLE;
    DWORD extended_window_style = 0;

    AdjustWindowRectEx(&client_rect, window_style, FALSE, extended_window_style);

    HWND window = CreateWindowExW(extended_window_style,
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

static inline void check_gl_status(GLuint id, GLenum parameter) {
    GLint status;
    static char error_log[512];

    if(parameter == GL_COMPILE_STATUS) {
        glGetShaderiv(id, parameter, &status);
        if(status != GL_TRUE) {
            glGetShaderInfoLog(id, sizeof(error_log), NULL, error_log);
            WIN_CHECK_CREATION_ERROR(status == 1, "Could not compile the shader!\n");
        }
    } else if(parameter == GL_LINK_STATUS) {
        glGetProgramiv(id, parameter, &status);
        if(status != GL_TRUE) {
            glGetProgramInfoLog(id, sizeof(error_log), NULL, error_log);
            WIN_CHECK_CREATION_ERROR(status == 1, "Could not link the GL program!\n");
        }
    }
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
    GLint opengl_version[2];
    glGetIntegerv(GL_MAJOR_VERSION, &opengl_version[0]);
    glGetIntegerv(GL_MINOR_VERSION, &opengl_version[1]);
    WIN_CHECK_CREATION_ERROR(opengl_version[0] > 3 || (opengl_version[0] == 3 && opengl_version[1] >= 3),
                             "Could not obtain an OpenGL 3.3 or newer context!\n");

    wglSwapIntervalEXT(1);

    // TODO: This should be moved to the game instead
    // Setup core OpenGL
    static const char *vertex_shader_source = {
        "#version 330 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "layout(location = 1) in vec2 uvs;\n"
        "out vec2 texcoords;\n"
        "void main(void) {\n"
        "    texcoords = vec2(uvs.s, 1.0 - uvs.t);\n"
        "    gl_Position = vec4(pos, 0.0, 1.0);\n"
        "}"
    };
    static const char *fragment_shader_source = {
        "#version 330 core\n"
        "uniform sampler2D sampler;\n"
        "in vec2 texcoords;\n"
        "out vec4 out_color;\n"
        "void main(void) {\n"
        "    out_color = texture(sampler, texcoords);\n"
        "}"
    };
    GLuint program = glCreateProgram();
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);

    glCompileShader(vertex_shader);
    check_gl_status(vertex_shader, GL_COMPILE_STATUS);
    glCompileShader(fragment_shader);
    check_gl_status(fragment_shader, GL_COMPILE_STATUS);

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);

    glLinkProgram(program);
    check_gl_status(program, GL_LINK_STATUS);

    glUseProgram(program);

    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    static const GLfloat vertex_buffer_data[] = {
        // Position       // UV coordinates
        -1.0f, -1.0f,     0.0f, 0.0f,
         1.0f, -1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,     1.0f, 1.0f,

         1.0f,  1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,     0.0f, 1.0f,
        -1.0f, -1.0f,     0.0f, 0.0f,
    };

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, NULL);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (const void *)(2 * sizeof(GLfloat)));

    glEnable(GL_TEXTURE_2D);
    GLuint framebuffer_texture;
    glGenTextures(1, &framebuffer_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, framebuffer_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, get_framebuffer());

    float aspect_ratio = (float)DEFAULT_FRAMEBUFFER_WIDTH / (float)DEFAULT_FRAMEBUFFER_HEIGHT;
    float w = DEFAULT_WINDOW_WIDTH;
    float h = w / aspect_ratio;

    if(h > DEFAULT_WINDOW_HEIGHT) {
        h = DEFAULT_WINDOW_HEIGHT;
        w = h * aspect_ratio;
    }

    GLint x = (DEFAULT_WINDOW_WIDTH - (GLint)w) / 2;
    GLint y = (DEFAULT_WINDOW_HEIGHT - (GLint)h) / 2;

    glViewport(x, y, (GLsizei)w, (GLsizei)h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    return rendering_context;
}
