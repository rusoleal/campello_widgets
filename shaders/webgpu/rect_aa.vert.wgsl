// WGSL port of shaders/vulkan/rect_aa.vert (y-up clip space).
struct RectAAUniforms { rect: vec4f, color: vec4f, viewport: vec2f, _pad: vec2f };
@group(0) @binding(0) var<uniform> u: RectAAUniforms;

struct VSOut {
    @builtin(position) pos: vec4f,
    @location(0) color: vec4f,
    @location(1) alpha: f32,
};

@vertex fn main(@location(0) posw_a: vec4f) -> VSOut {
    var out: VSOut;
    out.pos = vec4f(2.0 * posw_a.x / u.viewport.x - posw_a.z,
                    posw_a.z - 2.0 * posw_a.y / u.viewport.y, 0.0, posw_a.z);
    out.color = u.color;
    out.alpha = posw_a.w;
    return out;
}
