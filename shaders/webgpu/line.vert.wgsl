// WGSL port of shaders/vulkan/line.vert (y-up clip space).
struct LineUniforms { p1: vec4f, p2: vec4f, color: vec4f, viewport: vec2f, stroke_w: f32, _pad: f32 };
@group(0) @binding(0) var<uniform> u: LineUniforms;

struct VSOut {
    @builtin(position) pos: vec4f,
    @location(0) color: vec4f,
    @location(1) px: vec2f,
    @location(2) p1: vec2f,
    @location(3) p2: vec2f,
    @location(4) half_w: f32,
};

@vertex fn main(@builtin(vertex_index) vi: u32) -> VSOut {
    var dir = u.p2.xy - u.p1.xy;
    let len = length(dir);
    if (len > 0.0001) { dir = dir / len; } else { dir = vec2f(1.0, 0.0); }
    let perp = vec2f(-dir.y, dir.x);

    let aa = 0.5;
    let half_w = u.stroke_w * 0.5;
    let perp_off = perp * (half_w + aa);
    let along_off = dir * aa;

    var idx = array<u32, 6>(0u, 1u, 3u, 1u, 2u, 3u);
    var corners = array<vec2f, 4>(
        u.p1.xy - perp_off - along_off,
        u.p1.xy + perp_off - along_off,
        u.p2.xy + perp_off + along_off,
        u.p2.xy - perp_off + along_off);

    let px = corners[idx[vi]];
    let ndc = (px / u.viewport) * 2.0 - 1.0;
    var out: VSOut;
    out.pos = vec4f(ndc.x, -ndc.y, 0.0, 1.0);
    out.color = u.color;
    out.px = px;
    out.p1 = u.p1.xy;
    out.p2 = u.p2.xy;
    out.half_w = half_w;
    return out;
}
