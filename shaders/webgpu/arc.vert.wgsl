// WGSL port of shaders/vulkan/arc.vert (y-up clip space).
struct ArcUniforms {
    rect: vec4f, color: vec4f, viewport: vec2f,
    stroke_w: f32, start_angle: f32, sweep_angle: f32,
};
@group(0) @binding(0) var<uniform> u: ArcUniforms;

struct VSOut { @builtin(position) pos: vec4f, @location(0) px: vec2f };

@vertex fn main(@builtin(vertex_index) vi: u32) -> VSOut {
    var corners = array<vec2f, 6>(vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
                                  vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
    let aa = 0.5;
    var inflate = 0.0;
    if (u.stroke_w > 0.0) { inflate = u.stroke_w * 0.5 + aa; }
    let t = corners[vi];
    let px = (u.rect.xy - inflate) + t * (u.rect.zw + inflate * 2.0);
    let ndc = (px / u.viewport) * 2.0 - 1.0;
    var out: VSOut;
    out.pos = vec4f(ndc.x, -ndc.y, 0.0, 1.0);
    out.px = px;
    return out;
}
