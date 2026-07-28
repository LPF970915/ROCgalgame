#define GLFW_INCLUDE_ES2
#include <GLFW/glfw3.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <cstdio>

int main() {
#if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_WAYLAND)
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
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
    glfwWindowHint(GLFW_ALPHA_BITS, 0);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    GLFWwindow* window = glfwCreateWindow(
        1600, 1440, "glfw-egl-surface-probe", monitor, nullptr);
    if (!window) {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 2;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    std::fprintf(stderr, "renderer=%s\n", glGetString(GL_RENDERER));
    for (int frame = 0; frame < 600 && !glfwWindowShouldClose(window); ++frame) {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        const float phase = (frame / 120) % 2 ? 0.12f : 0.88f;
        glViewport(0, 0, width, height);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(phase, 0.08f, 1.0f - phase, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (frame == 0) {
            unsigned char pixel[4] = {};
            glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixel);
            std::fprintf(stderr,
                         "framebuffer=%dx%d pixel=%u,%u,%u,%u error=0x%04X\n",
                         width, height, pixel[0], pixel[1], pixel[2], pixel[3],
                         glGetError());
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
