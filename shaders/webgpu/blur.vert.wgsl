// WGSL port of shaders/vulkan/blur.vert (y-up clip space).
struct BlurUniforms {
    dstRect: vec4f, srcRect: vec4f, viewport: vec2f,
    sigma: f32, horizontal: f32, tex_size: vec2f, _pad: vec2f,
};
@group(1) @binding(0) var<uniform> u: BlurUniforms;

struct VSOut {
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
    @location(1) @interpolate(flat) sigma: f32,
    @location(2) @interpolate(flat) horizontal: f32,
    @location(3) @interpolate(flat) tex_size: vec2f,
};

@vertex fn main(@builtin(vertex_index) vi: u32) -> VSOut {
    var corners = array<vec2f, 6>(vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
                                  vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
    let t = corners[vi];
    let px = u.dstRect.xy + t * u.dstRect.zw;
    let ndc = (px / u.viewport) * 2.0 - 1.0;
    var out: VSOut;
    out.pos = vec4f(ndc.x, -ndc.y, 0.0, 1.0);
    out.uv = u.srcRect.xy + t * (u.srcRect.zw - u.srcRect.xy);
    out.sigma = u.sigma;
    out.horizontal = u.horizontal;
    out.tex_size = u.tex_size;
    return out;
}
