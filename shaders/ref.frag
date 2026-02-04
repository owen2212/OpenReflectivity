#version 330 core

in float v_gate;
out vec4 FragColor;

void main() {
    if (v_gate <= -9999.0) {
        discard;
    }

    vec3 color;
    if (v_gate < 0.0) {
        color = vec3(0.1, 0.3, 0.9); // blue
    } else if (v_gate < 30.0) {
        color = vec3(0.1, 0.8, 0.2); // green
    } else {
        color = vec3(0.9, 0.1, 0.1); // red
    }

    FragColor = vec4(color, 1.0);
}
