#include <glad/glad.h>
#include <GLFW/glfw3.h>

extern "C" {
#include "rsl.h"
}

#include <cstdio>
#include <cstdlib>


int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Boilerplate Test", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "Failed to initialize GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::printf("OpenGL Vendor  : %s\n", glGetString(GL_VENDOR));
    std::printf("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
    std::printf("OpenGL Version : %s\n", glGetString(GL_VERSION));

    // Check RSL
    const char* level2_path = "examples/KTLX20130520_000122_V06";
    const char* site_id = "KTLX";

    Radar* radar = RSL_wsr88d_to_radar(const_cast<char*>(level2_path),
                                       const_cast<char*>(site_id));
    if (!radar) {
        std::fprintf(stderr, "Failed to load Level II file: %s\n", level2_path);
    } else {
        std::printf("Loaded radar site: %.4s\n", radar->h.name);
        std::printf("Volumes: %d\n", radar->h.nvolumes);
        RSL_free_radar(radar);
    }

    while (!glfwWindowShouldClose(window)) {
        glViewport(0, 0, 800, 600);
        glClearColor(0.08f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
