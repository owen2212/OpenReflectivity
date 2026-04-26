#version 330 core

layout(location = 0) in vec2 in_world; // meters

uniform vec2 u_view_scale;
uniform vec2 u_view_offset;

void main() {
    vec2 ndc = in_world * u_view_scale + u_view_offset;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
