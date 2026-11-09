// WGSL port of shaders/vulkan/icon.vert (y-up clip space).
struct IconUniforms { viewport: vec2f, opacity: f32, _pad: f32, tint: vec4f };
@group(1) @binding(0) var<uniform> u: IconUniforms;

struct VSOut {
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
    @location(1) opacity: f32,
    @location(2) tint: vec4f,
};

@vertex fn main(@location(0) posw: vec3f, @location(1) uv: vec2f) -> VSOut {
    var out: VSOut;
    out.pos = vec4f(2.0 * posw.x / u.viewport.x - posw.z,
                    posw.z - 2.0 * posw.y / u.viewport.y, 0.0, posw.z);
    out.uv = uv;
    out.opacity = u.opacity;
    out.tint = u.tint;
    return out;
}
