// WGSL port of shaders/vulkan/quad_aa.vert (y-up clip space).
struct QuadAAUniforms { viewport: vec2f, opacity: f32, _pad: f32, pos: vec4f, uv: vec4f };
@group(1) @binding(0) var<uniform> u: QuadAAUniforms;

struct VSOut { @builtin(position) pos: vec4f, @location(0) uv: vec2f, @location(1) opacity: f32 };

@vertex fn main(@builtin(vertex_index) vi: u32) -> VSOut {
    var corners = array<vec2f, 6>(vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
                                  vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
    let t = corners[vi];
    let px = u.pos.xy + t * u.pos.zw;
    let ndc = (px / u.viewport) * 2.0 - 1.0;
    var out: VSOut;
    out.pos = vec4f(ndc.x, -ndc.y, 0.0, 1.0);
    out.uv = u.uv.xy + t * (u.uv.zw - u.uv.xy);
    out.opacity = u.opacity;
    return out;
}
