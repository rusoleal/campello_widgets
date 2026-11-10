// WGSL port of shaders/vulkan/rect.vert. Push constants: campello_gpu's
// WebGPU emulation binds them in the group after the layout's own (here 0).
// WebGPU clip space is y-up (Vulkan's is y-down): pixel y flips sign.
struct RectUniforms { rect: vec4f, color: vec4f, viewport: vec2f, _pad: vec2f };
@group(0) @binding(0) var<uniform> u: RectUniforms;

struct VSOut { @builtin(position) pos: vec4f, @location(0) color: vec4f };

@vertex fn main(@builtin(vertex_index) vi: u32) -> VSOut {
    var corners = array<vec2f, 6>(vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
                                  vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
    let t = corners[vi];
    let px = u.rect.xy + t * u.rect.zw;
    let ndc = (px / u.viewport) * 2.0 - 1.0;
    var out: VSOut;
    out.pos = vec4f(ndc.x, -ndc.y, 0.0, 1.0);
    out.color = u.color;
    return out;
}
