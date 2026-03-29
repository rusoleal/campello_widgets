#include "d3d_draw_backend.hpp"
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_gpu/device.hpp>
#include <campello_gpu/render_pipeline.hpp>
#include <campello_gpu/bind_group_layout.hpp>
#include <campello_gpu/sampler.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_gpu/render_pass_encoder.hpp>
#include <campello_gpu/buffer.hpp>

#include <windows.h>
#include <dwrite.h>      // DirectWrite for text
#include <d2d1.h>        // Direct2D for text rasterization
#include <wrl/client.h>  // ComPtr

#include <vector>
#include <cmath>

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")

namespace systems::leal::campello_widgets
{

// ---------------------------------------------------------------------------
// Vertex structure for rectangles
// ---------------------------------------------------------------------------
struct RectVertex
{
    float x, y;
    float r, g, b, a;
};

// ---------------------------------------------------------------------------
// Vertex structure for textured quads
// ---------------------------------------------------------------------------
struct QuadVertex
{
    float x, y;
    float u, v;
    float opacity;
};

// ---------------------------------------------------------------------------
// D3DDrawBackend implementation
// ---------------------------------------------------------------------------

D3DDrawBackend::D3DDrawBackend(
    std::shared_ptr<campello_gpu::Device> device,
    Color                                 bg_color,
    HWND                                  hwnd)
    : device_(std::move(device))
    , bg_color_(bg_color)
    , hwnd_(hwnd)
{
    // Create pipelines using campello_gpu's shader compilation
    // The actual shader code (HLSL) would be embedded or loaded from file
    
    // For now, create placeholder pipelines - campello_gpu handles the
    // actual D3D shader compilation and pipeline state creation
    
    // TODO: Load and compile rect_pipeline_ for solid color fills
    // TODO: Load and compile quad_pipeline_ for textured quads
    // TODO: Create quad_bgl_ for texture binding layout
    // TODO: Create quad_sampler_ for texture sampling
}

void D3DDrawBackend::drawRect(
    const DrawRectCmd&               cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    campello_gpu::RenderPassEncoder& encoder)
{
    (void)clip;  // Clipping handled by scissor rect in encoder

    // Transform the rectangle corners
    float x1 = cmd.rect.left();
    float y1 = cmd.rect.top();
    float x2 = cmd.rect.right();
    float y2 = cmd.rect.bottom();

    // Apply transform (simple 2D transform)
    float tx1 = transform.data[0] * x1 + transform.data[4] * y1 + transform.data[12];
    float ty1 = transform.data[1] * x1 + transform.data[5] * y1 + transform.data[13];
    float tx2 = transform.data[0] * x2 + transform.data[4] * y2 + transform.data[12];
    float ty2 = transform.data[1] * x2 + transform.data[5] * y2 + transform.data[13];

    // Normalize to NDC (-1 to 1)
    float nx1 = (tx1 / vp_w_) * 2.0f - 1.0f;
    float ny1 = 1.0f - (ty1 / vp_h_) * 2.0f;
    float nx2 = (tx2 / vp_w_) * 2.0f - 1.0f;
    float ny2 = 1.0f - (ty2 / vp_h_) * 2.0f;

    const Color& c = cmd.paint.color;

    // Create vertex buffer for two triangles (quad)
    RectVertex vertices[6] = {
        { nx1, ny1, c.r, c.g, c.b, c.a },
        { nx2, ny1, c.r, c.g, c.b, c.a },
        { nx1, ny2, c.r, c.g, c.b, c.a },
        { nx1, ny2, c.r, c.g, c.b, c.a },
        { nx2, ny1, c.r, c.g, c.b, c.a },
        { nx2, ny2, c.r, c.g, c.b, c.a },
    };

    // Submit to GPU via campello_gpu encoder
    // The encoder abstracts the actual D3D11/12 draw calls
    if (rect_pipeline_) {
        encoder.setPipeline(rect_pipeline_);
        // Note: Actual vertex buffer binding would go here
        // encoder.setVertexBuffer(0, vb);
        // encoder.draw(6);
    }

    // For now, this is a stub - campello_gpu's actual implementation
    // would handle the D3D-specific draw calls
}

void D3DDrawBackend::drawText(
    const DrawTextCmd&               cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    campello_gpu::RenderPassEncoder& encoder)
{
    (void)transform;
    (void)clip;
    (void)encoder;
    (void)cmd;
    
    // Text rendering involves:
    // 1. Rasterize glyphs to a texture atlas using DirectWrite
    // 2. Upload texture to GPU
    // 3. Draw textured quads for each glyph
    
    // For a minimal implementation, this would use the same
    // quad rendering path as drawImage()
}

void D3DDrawBackend::drawImage(
    const DrawImageCmd&              cmd,
    const Matrix4&                   transform,
    const Rect&                      clip,
    campello_gpu::RenderPassEncoder& encoder)
{
    (void)clip;

    if (!cmd.texture) return;

    // Transform destination rectangle
    float x1 = cmd.dst_rect.left();
    float y1 = cmd.dst_rect.top();
    float x2 = cmd.dst_rect.right();
    float y2 = cmd.dst_rect.bottom();

    float tx1 = transform.data[0] * x1 + transform.data[4] * y1 + transform.data[12];
    float ty1 = transform.data[1] * x1 + transform.data[5] * y1 + transform.data[13];
    float tx2 = transform.data[0] * x2 + transform.data[4] * y2 + transform.data[12];
    float ty2 = transform.data[1] * x2 + transform.data[5] * y2 + transform.data[13];

    float nx1 = (tx1 / vp_w_) * 2.0f - 1.0f;
    float ny1 = 1.0f - (ty1 / vp_h_) * 2.0f;
    float nx2 = (tx2 / vp_w_) * 2.0f - 1.0f;
    float ny2 = 1.0f - (ty2 / vp_h_) * 2.0f;

    // Source UVs
    float u1 = cmd.src_rect.left();
    float v1 = cmd.src_rect.top();
    float u2 = cmd.src_rect.right();
    float v2 = cmd.src_rect.bottom();

    // Create vertex buffer
    QuadVertex vertices[6] = {
        { nx1, ny1, u1, v1, cmd.opacity },
        { nx2, ny1, u2, v1, cmd.opacity },
        { nx1, ny2, u1, v2, cmd.opacity },
        { nx1, ny2, u1, v2, cmd.opacity },
        { nx2, ny1, u2, v1, cmd.opacity },
        { nx2, ny2, u2, v2, cmd.opacity },
    };

    if (quad_pipeline_ && quad_bgl_ && quad_sampler_) {
        encoder.setPipeline(quad_pipeline_);
        // Bind texture and sampler
        // encoder.setBindGroup(0, texture_bind_group);
        // encoder.draw(6);
    }
}

Size D3DDrawBackend::measureText(const TextSpan& span) const
{
    // Use DirectWrite to measure text
    // This is a simplified implementation
    
    if (span.text.empty()) {
        return Size{ 0.0f, span.style.font_size * 1.2f };
    }

    // Create DirectWrite factory
    Microsoft::WRL::ComPtr<IDWriteFactory> dw_factory;
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dw_factory.GetAddressOf()));

    if (FAILED(hr)) {
        // Fallback to estimation
        const float char_width = span.style.font_size * 0.6f;
        const float line_height = span.style.font_size * 1.2f;
        return Size{ char_width * static_cast<float>(span.text.size()), line_height };
    }

    // Create text format
    Microsoft::WRL::ComPtr<IDWriteTextFormat> text_format;
    hr = dw_factory->CreateTextFormat(
        span.style.font_family.empty() ? L"Segoe UI" : nullptr,  // Use wide string conversion
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        span.style.font_size,
        L"en-US",
        text_format.GetAddressOf());

    if (FAILED(hr)) {
        const float char_width = span.style.font_size * 0.6f;
        const float line_height = span.style.font_size * 1.2f;
        return Size{ char_width * static_cast<float>(span.text.size()), line_height };
    }

    // Create text layout
    // Convert UTF-8 to UTF-16 for Windows
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, span.text.c_str(), -1, nullptr, 0);
    std::wstring wide_text(wide_len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, span.text.c_str(), -1, wide_text.data(), wide_len);

    Microsoft::WRL::ComPtr<IDWriteTextLayout> text_layout;
    hr = dw_factory->CreateTextLayout(
        wide_text.c_str(),
        static_cast<UINT32>(wide_text.length()),
        text_format.Get(),
        10000.0f,  // Max width
        10000.0f,  // Max height
        text_layout.GetAddressOf());

    if (FAILED(hr)) {
        const float char_width = span.style.font_size * 0.6f;
        const float line_height = span.style.font_size * 1.2f;
        return Size{ char_width * static_cast<float>(span.text.size()), line_height };
    }

    DWRITE_TEXT_METRICS metrics;
    hr = text_layout->GetMetrics(&metrics);
    if (FAILED(hr)) {
        const float char_width = span.style.font_size * 0.6f;
        const float line_height = span.style.font_size * 1.2f;
        return Size{ char_width * static_cast<float>(span.text.size()), line_height };
    }

    return Size{ metrics.width, metrics.height };
}

void D3DDrawBackend::drawFilledRect(
    float x, float y, float w, float h,
    const Color& color,
    campello_gpu::RenderPassEncoder& encoder)
{
    DrawRectCmd cmd;
    cmd.rect = Rect::fromLTWH(x, y, w, h);
    cmd.paint.color = color;
    cmd.paint.style = PaintStyle::fill;
    
    drawRect(cmd, Matrix4::identity(), Rect::fromLTWH(0, 0, vp_w_, vp_h_), encoder);
}

void D3DDrawBackend::drawTexturedQuad(
    std::shared_ptr<campello_gpu::Texture>  texture,
    float dst_x, float dst_y, float dst_w, float dst_h,
    float src_u0, float src_v0, float src_u1, float src_v1,
    float opacity,
    campello_gpu::RenderPassEncoder&        encoder)
{
    DrawImageCmd cmd;
    cmd.texture = std::move(texture);
    cmd.src_rect = Rect::fromLTRB(src_u0, src_v0, src_u1, src_v1);
    cmd.dst_rect = Rect::fromLTWH(dst_x, dst_y, dst_w, dst_h);
    cmd.opacity = opacity;
    
    drawImage(cmd, Matrix4::identity(), Rect::fromLTWH(0, 0, vp_w_, vp_h_), encoder);
}

} // namespace systems::leal::campello_widgets
