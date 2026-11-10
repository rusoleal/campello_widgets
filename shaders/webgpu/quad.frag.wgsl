// WGSL port of shaders/vulkan/quad.frag.
@group(0) @binding(1) var u_texture: texture_2d<f32>;
@group(0) @binding(2) var u_sampler: sampler;

@fragment fn main(@location(0) uv: vec2f, @location(1) opacity: f32) -> @location(0) vec4f {
    return textureSample(u_texture, u_sampler, uv) * opacity;
}
