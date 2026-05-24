#version 330 core

in vec2 v_world;
out vec4 FragColor;

uniform sampler2D u_polar;       // x = gate, y = radial (azimuth-sorted)
uniform sampler1D u_value_lut;
uniform sampler1D u_az_lookup;   // azimuth [0,1) -> continuous row coord, -1 in gaps
uniform float u_range_bin1;
uniform float u_gate_size;
uniform float u_max_range;
uniform int u_rows;
uniform int u_cols;
uniform float u_min_value;
uniform float u_max_value;
uniform float u_discard_below;
uniform vec2 u_storm_motion;

const float kSentinel = -9000.0;
const float kTwoPi = 6.28318530718;

// one bilinear tap. Skip out-of-range gates and sentinels so data edges
// don't smear into invalid values.
void accum(int gate, int row, float w, inout float vsum, inout float wsum) {
    if (gate < 0 || gate >= u_cols) return;
    float val = texelFetch(u_polar, ivec2(gate, row), 0).r;
    if (val <= kSentinel) return;
    vsum += w * val;
    wsum += w;
}

void main() {
    float r = length(v_world);
    if (r > u_max_range) discard;

    float az = atan(v_world.x, v_world.y);   // clockwise from north
    float az01 = az / kTwoPi;
    if (az01 < 0.0) az01 += 1.0;
    float row_coord = texture(u_az_lookup, az01).r;
    if (row_coord < 0.0) discard;

    // Gate centers sit at range_bin1 + (i + 0.5) * gate_size.
    float g = (r - u_range_bin1) / u_gate_size - 0.5;
    float gf = floor(g);
    float gfrac = g - gf;
    int g0 = int(gf);

    float rf = floor(row_coord);
    float rfrac = row_coord - rf;
    int r0 = int(rf) % u_rows;
    int r1 = (r0 + 1) % u_rows;   // azimuth wrap

    float vsum = 0.0;
    float wsum = 0.0;
    accum(g0,     r0, (1.0 - rfrac) * (1.0 - gfrac), vsum, wsum);
    accum(g0 + 1, r0, (1.0 - rfrac) * gfrac,         vsum, wsum);
    accum(g0,     r1, rfrac * (1.0 - gfrac),         vsum, wsum);
    accum(g0 + 1, r1, rfrac * gfrac,                 vsum, wsum);
    if (wsum < 0.05) discard;

    float v = vsum / wsum;
    v -= dot(vec2(sin(az), cos(az)), u_storm_motion);
    if (v < u_discard_below) discard;

    float t = clamp((v - u_min_value) / (u_max_value - u_min_value), 0.0, 1.0);
    FragColor = vec4(texture(u_value_lut, t).rgb, 1.0);
}
