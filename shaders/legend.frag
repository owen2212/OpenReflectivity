#version 330 core

in vec2 v_uv;
out vec4 FragColor;

uniform sampler1D u_dbz_lut;
uniform vec2 u_pixel_size;   // legend bar size in pixels (for border thickness)
uniform float u_min_dbz;
uniform float u_max_dbz;

void main() {
    // 1px border in pixel space.
    vec2 edge = 1.0 / u_pixel_size;
    if (v_uv.x < edge.x || v_uv.x > 1.0 - edge.x ||
        v_uv.y < edge.y || v_uv.y > 1.0 - edge.y) {
        FragColor = vec4(0.85, 0.85, 0.88, 1.0);
        return;
    }

    // Tick marks every 10 dBZ from min to max along the bar.
    float dbz = mix(u_min_dbz, u_max_dbz, v_uv.y);
    float tick_period = 10.0;
    float dist_to_tick = abs(fract(dbz / tick_period) - 0.5);
    // half-width of a tick in dbz units, derived from one-pixel height.
    float pixel_dbz = (u_max_dbz - u_min_dbz) / u_pixel_size.y;
    bool on_tick = dist_to_tick > 0.5 - (pixel_dbz / tick_period) * 0.5;
    bool tick_zone = v_uv.x < 0.18 || v_uv.x > 0.82;
    if (on_tick && tick_zone) {
        FragColor = vec4(0.95, 0.95, 0.95, 1.0);
        return;
    }

    vec3 color = texture(u_dbz_lut, v_uv.y).rgb;
    FragColor = vec4(color, 1.0);
}
