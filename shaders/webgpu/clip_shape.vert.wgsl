// WGSL port of shaders/vulkan/clip_shape.vert (y-up clip space).
struct ClipShapeUniforms { rect_size: vec2f, viewport: vec2f, corner_r: f32, kind: f32, _pad: vec2f };
@group(1) @binding(0) var<uniform> u: ClipShapeUniforms;

struct VSOut {
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
    @location(1) @interpolate(flat) rect_size: vec2f,
    @location(2) @interpolate(flat) corner_r: f32,
    @location(3) @interpolate(flat) kind: f32,
};

@vertex fn main(@location(0) posw: vec3f, @location(1) uv: vec2f) -> VSOut {
    var out: VSOut;
    out.pos = vec4f(2.0 * posw.x / u.viewport.x - posw.z,
                    posw.z - 2.0 * posw.y / u.viewport.y, 0.0, posw.z);
    out.uv = uv;
    out.rect_size = u.rect_size;
    out.corner_r = u.corner_r;
    out.kind = u.kind;
    return out;
}
