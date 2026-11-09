// WGSL port of shaders/vulkan/shader_mask.vert (y-up clip space).
struct ShaderMaskUniforms {
    viewport: vec2f, gradient_type: f32, tile_mode: f32,
    gradient_p1: vec4f, gradient_p2: vec4f, blend_mode: f32,
};
@group(1) @binding(0) var<uniform> u: ShaderMaskUniforms;

struct VSOut {
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
    @location(1) @interpolate(flat) gradient_type: f32,
    @location(2) @interpolate(flat) gradient_p1: vec4f,
    @location(3) @interpolate(flat) gradient_p2: vec4f,
    @location(4) @interpolate(flat) blend_mode: f32,
    @location(5) @interpolate(flat) tile_mode: f32,
};

@vertex fn main(@location(0) posw: vec3f, @location(1) uv: vec2f) -> VSOut {
    var out: VSOut;
    out.pos = vec4f(2.0 * posw.x / u.viewport.x - posw.z,
                    posw.z - 2.0 * posw.y / u.viewport.y, 0.0, posw.z);
    out.uv = uv;
    out.gradient_type = u.gradient_type;
    out.gradient_p1 = u.gradient_p1;
    out.gradient_p2 = u.gradient_p2;
    out.blend_mode = u.blend_mode;
    out.tile_mode = u.tile_mode;
    return out;
}
