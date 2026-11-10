// WGSL port of shaders/vulkan/blur.frag. The tap loop's bound varies per
// fragment (non-uniform control flow), so it samples with an explicit LOD.
@group(0) @binding(1) var u_texture: texture_2d<f32>;
@group(0) @binding(2) var u_sampler: sampler;

@fragment fn main(@location(0) uv: vec2f,
                  @location(1) @interpolate(flat) sigma_in: f32,
                  @location(2) @interpolate(flat) horizontal: f32,
                  @location(3) @interpolate(flat) tex_size: vec2f) -> @location(0) vec4f {
    let sigma = max(sigma_in, 0.5);
    let radius = min(i32(ceil(2.5 * sigma)), 12);
    var step = vec2f(0.0, 1.0 / tex_size.y);
    if (horizontal > 0.5) { step = vec2f(1.0 / tex_size.x, 0.0); }

    var color = vec4f(0.0);
    var wsum = 0.0;
    for (var i = -radius; i <= radius; i++) {
        let fi = f32(i);
        let weight = exp(-0.5 * (fi / sigma) * (fi / sigma));
        let suv = clamp(uv + fi * step, vec2f(0.0), vec2f(1.0));
        color += textureSampleLevel(u_texture, u_sampler, suv, 0.0) * weight;
        wsum += weight;
    }
    return color / wsum;
}
