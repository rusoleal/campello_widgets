#pragma once

#include <jni.h>
#include "../gpu/vulkan/text_rasterizer.hpp"
#include <campello_widgets/ui/color.hpp>
#include <cstdint>
#include <string>

namespace systems::leal::campello_widgets
{

/** @brief Supply the JavaVM* used by AndroidTextRasterizer instead of JNI_GetCreatedJavaVMs.
 *         Must be called (e.g. from run_app.cpp) before any text rasterisation. */
void setAndroidJavaVM(JavaVM* vm);

// ---------------------------------------------------------------------------
// AndroidTextRasterizer
//
// CPU text rasterisation using Android's Canvas / Paint via JNI.
// Delegates to the same Skia-based text stack Flutter uses under the hood.
//
//   measure()    → bounding size of a text span
//   rasterize()  → BGRA8 premultiplied bitmap (native-endian int[] layout)
//
// One texture per draw call — no glyph atlas (same pattern as Metal/Linux).
// ---------------------------------------------------------------------------
class AndroidTextRasterizer final : public ITextRasterizer
{
public:
    AndroidTextRasterizer();
    ~AndroidTextRasterizer() override;

    /** @brief Returns true if JNI refs were successfully cached. */
    bool isAvailable() const noexcept override { return available_; }

    /** @brief Measures the bounding box of @p span. */
    Size measure(const TextSpan& span) override;

    /** @brief Rasterises @p span into a CPU bitmap. */
    Bitmap rasterize(const TextSpan& span) override;

private:
    bool initializeJni();
    void releaseJni();

    bool available_ = false;

    // Cached JNI classes / methods
    struct JniRefs
    {
        jclass bitmap_cls             = nullptr;
        jclass bitmap_config_cls      = nullptr;
        jclass canvas_cls             = nullptr;
        jclass paint_cls              = nullptr;
        jclass paint_font_metrics_cls = nullptr;

        jmethodID bitmap_create_              = nullptr;
        jmethodID bitmap_copy_pixels_to_buffer_ = nullptr;
        jmethodID canvas_ctor_                = nullptr;
        jmethodID canvas_draw_text_           = nullptr;
        jmethodID paint_ctor_                 = nullptr;
        jmethodID paint_set_color_            = nullptr;
        jmethodID paint_set_text_size_        = nullptr;
        jmethodID paint_set_antialias_        = nullptr;
        jmethodID paint_measure_text_         = nullptr;
        jmethodID paint_get_font_metrics_     = nullptr;

        jfieldID argb_8888_field_             = nullptr;
        jfieldID ascent_field_                = nullptr;
        jfieldID descent_field_               = nullptr;
    };

    JniRefs jni_;
};

} // namespace systems::leal::campello_widgets
