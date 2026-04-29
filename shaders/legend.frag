#version 330 core

in vec2 v_uv;
out vec4 FragColor;

uniform sampler1D u_value_lut;
uniform vec2 u_pixel_size;   // legend bar size in pixels (for border thickness)
uniform float u_min_value;
uniform float u_max_value;
uniform float u_tick_period;

void main() {
    // 1px border in pixel space.
    vec2 edge = 1.0 / u_pixel_size;
    if (v_uv.x < edge.x || v_uv.x > 1.0 - edge.x ||
        v_uv.y < edge.y || v_uv.y > 1.0 - edge.y) {
        FragColor = vec4(0.85, 0.85, 0.88, 1.0);
        return;
    }

    // Tick marks every u_tick_period units along the bar.
    float value = mix(u_min_value, u_max_value, v_uv.y);
    float dist_to_tick = abs(fract(value / u_tick_period) - 0.5);
    // half-width of a tick in value units, derived from one-pixel height.
    float pixel_value = (u_max_value - u_min_value) / u_pixel_size.y;
    bool on_tick = dist_to_tick > 0.5 - (pixel_value / u_tick_period) * 0.5;
    bool tick_zone = v_uv.x < 0.18 || v_uv.x > 0.82;
    if (on_tick && tick_zone) {
        FragColor = vec4(0.95, 0.95, 0.95, 1.0);
        return;
    }

    vec3 color = texture(u_value_lut, v_uv.y).rgb;
    FragColor = vec4(color, 1.0);
}
