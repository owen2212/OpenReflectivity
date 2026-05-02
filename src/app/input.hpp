#ifndef INPUT_HPP
#define INPUT_HPP

struct GLFWwindow;
struct AppState;

// Sets the window user pointer to `app` and installs all GLFW callbacks
// (which chain into the ImGui GLFW backend).
void install_input_callbacks(GLFWwindow *window, AppState *app);

#endif
