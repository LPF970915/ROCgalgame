#define GLFW_INCLUDE_ES2
#include <GLFW/glfw3.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main() {
#if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_WAYLAND)
    const char *backend = std::getenv("ROCGALGAME_PROBE_BACKEND");
    if (backend && std::strcmp(backend, "wayland") == 0) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
        glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
    }
#ifdef GLFW_PLATFORM_X11
    else if (backend && std::strcmp(backend, "x11") == 0) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    }
#endif
#endif
    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    const char* alpha = std::getenv("ROCGALGAME_PROBE_ALPHA_BITS");
    glfwWindowHint(GLFW_ALPHA_BITS, alpha ? std::atoi(alpha) : 0);
    const char* transparent = std::getenv("ROCGALGAME_PROBE_TRANSPARENT");
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER,
                   transparent && std::strcmp(transparent, "0") != 0);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    const char* fullscreen = std::getenv("ROCGALGAME_PROBE_FULLSCREEN");
    GLFWmonitor* monitor = !fullscreen || std::strcmp(fullscreen, "0") != 0
        ? glfwGetPrimaryMonitor()
        : nullptr;
    const char* requestedWidth = std::getenv("ROCGALGAME_PROBE_WIDTH");
    const char* requestedHeight = std::getenv("ROCGALGAME_PROBE_HEIGHT");
    const int windowWidth = requestedWidth ? std::atoi(requestedWidth) : 1600;
    const int windowHeight = requestedHeight ? std::atoi(requestedHeight) : 1440;
    GLFWwindow* window = glfwCreateWindow(
        windowWidth, windowHeight, "glfw-egl-surface-probe", monitor, nullptr);
    if (!window) {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 2;
    }

    glfwMakeContextCurrent(window);
    const char *vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    if (!vendor || !renderer || !version) {
        std::fprintf(stderr, "GL context unavailable egl_error=0x%04X\n",
                     eglGetError());
        glfwDestroyWindow(window);
        glfwTerminate();
        return 3;
    }
    glfwSwapInterval(1);
    EGLDisplay display = eglGetCurrentDisplay();
    EGLContext context = eglGetCurrentContext();
    EGLint configId = -1;
    eglQueryContext(display, context, EGL_CONFIG_ID, &configId);
    EGLConfig configs[256];
    EGLint configCount = 0;
    EGLConfig selectedConfig = nullptr;
    if (eglGetConfigs(display, configs, 256, &configCount)) {
        for (EGLint i = 0; i < configCount; ++i) {
            EGLint candidateId = -1;
            eglGetConfigAttrib(display, configs[i], EGL_CONFIG_ID, &candidateId);
            if (candidateId == configId) {
                selectedConfig = configs[i];
                break;
            }
        }
    }
    const auto configAttribute = [display, selectedConfig](EGLint attribute) {
        EGLint value = -1;
        if (selectedConfig)
            eglGetConfigAttrib(display, selectedConfig, attribute, &value);
        return value;
    };
    std::fprintf(stderr, "vendor=%s\nrenderer=%s\nversion=%s\n",
                 vendor, renderer, version);
    std::fprintf(stderr, "config=%d rgba=%d/%d/%d/%d buffer=%d depth=%d stencil=%d\n",
                 configId, configAttribute(EGL_RED_SIZE),
                 configAttribute(EGL_GREEN_SIZE), configAttribute(EGL_BLUE_SIZE),
                 configAttribute(EGL_ALPHA_SIZE), configAttribute(EGL_BUFFER_SIZE),
                 configAttribute(EGL_DEPTH_SIZE), configAttribute(EGL_STENCIL_SIZE));
    for (int frame = 0; frame < 600 && !glfwWindowShouldClose(window); ++frame) {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width <= 0 || height <= 0) {
            std::fprintf(stderr, "invalid framebuffer=%dx%d egl_error=0x%04X\n",
                         width, height, eglGetError());
            glfwDestroyWindow(window);
            glfwTerminate();
            return 4;
        }
        const float phase = (frame / 120) % 2 ? 0.12f : 0.88f;
        glViewport(0, 0, width, height);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(phase, 0.08f, 1.0f - phase, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (frame == 0) {
            const GLenum framebuffer_status =
                glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
                std::fprintf(stderr,
                             "incomplete framebuffer=0x%04X gl_error=0x%04X\n",
                             framebuffer_status, glGetError());
                glfwDestroyWindow(window);
                glfwTerminate();
                return 6;
            }
            unsigned char pixel[4] = {};
            glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixel);
            const GLenum read_error = glGetError();
            if (read_error != GL_NO_ERROR) {
                std::fprintf(stderr, "glReadPixels failed error=0x%04X\n",
                             read_error);
                glfwDestroyWindow(window);
                glfwTerminate();
                return 5;
            }
            std::fprintf(stderr,
                         "framebuffer=%dx%d fbo=0x%04X pixel=%u,%u,%u,%u error=0x%04X\n",
                         width, height, framebuffer_status, pixel[0], pixel[1],
                         pixel[2], pixel[3], read_error);
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
