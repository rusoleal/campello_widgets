#include "android_text_rasterizer.hpp"
#include <campello_widgets/ui/rect.hpp>

#include <jni.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace systems::leal::campello_widgets
{

#define LOG_TAG "campello_widgets"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static JNIEnv* getAndroidJniEnv()
{
    JavaVM* vm = nullptr;
    jsize vmCount = 0;
    if (JNI_GetCreatedJavaVMs(&vm, 1, &vmCount) != JNI_OK || vmCount == 0)
        return nullptr;

    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK)
        return env;

    if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK)
        return env;

    return nullptr;
}

static int colorToArgb(const Color& c)
{
    int a = static_cast<int>(std::clamp(c.a, 0.0f, 1.0f) * 255.0f);
    int r = static_cast<int>(std::clamp(c.r, 0.0f, 1.0f) * 255.0f);
    int g = static_cast<int>(std::clamp(c.g, 0.0f, 1.0f) * 255.0f);
    int b = static_cast<int>(std::clamp(c.b, 0.0f, 1.0f) * 255.0f);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AndroidTextRasterizer::AndroidTextRasterizer()
{
    available_ = initializeJni();
}

AndroidTextRasterizer::~AndroidTextRasterizer()
{
    releaseJni();
}

// ---------------------------------------------------------------------------
// JNI caching
// ---------------------------------------------------------------------------

bool AndroidTextRasterizer::initializeJni()
{
    JNIEnv* env = getAndroidJniEnv();
    if (!env) {
        LOGE("AndroidTextRasterizer: failed to get JNI env");
        return false;
    }

    jni_.bitmap_cls = env->FindClass("android/graphics/Bitmap");
    jni_.bitmap_config_cls = env->FindClass("android/graphics/Bitmap$Config");
    jni_.canvas_cls = env->FindClass("android/graphics/Canvas");
    jni_.paint_cls = env->FindClass("android/graphics/Paint");
    jni_.paint_font_metrics_cls = env->FindClass("android/graphics/Paint$FontMetrics");

    if (!jni_.bitmap_cls || !jni_.bitmap_config_cls || !jni_.canvas_cls ||
        !jni_.paint_cls || !jni_.paint_font_metrics_cls)
    {
        LOGE("AndroidTextRasterizer: failed to find JNI classes");
        return false;
    }

    // Cache methods
    jni_.bitmap_create_ = env->GetStaticMethodID(
        jni_.bitmap_cls, "createBitmap",
        "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jni_.bitmap_copy_pixels_to_buffer_ = env->GetMethodID(
        jni_.bitmap_cls, "copyPixelsToBuffer", "(Ljava/nio/Buffer;)V");

    jni_.canvas_ctor_ = env->GetMethodID(
        jni_.canvas_cls, "<init>", "(Landroid/graphics/Bitmap;)V");
    jni_.canvas_draw_text_ = env->GetMethodID(
        jni_.canvas_cls, "drawText",
        "(Ljava/lang/String;FFLandroid/graphics/Paint;)V");

    jni_.paint_ctor_ = env->GetMethodID(
        jni_.paint_cls, "<init>", "()V");
    jni_.paint_set_color_ = env->GetMethodID(
        jni_.paint_cls, "setColor", "(I)V");
    jni_.paint_set_text_size_ = env->GetMethodID(
        jni_.paint_cls, "setTextSize", "(F)V");
    jni_.paint_set_antialias_ = env->GetMethodID(
        jni_.paint_cls, "setAntiAlias", "(Z)V");
    jni_.paint_measure_text_ = env->GetMethodID(
        jni_.paint_cls, "measureText", "(Ljava/lang/String;)F");
    jni_.paint_get_font_metrics_ = env->GetMethodID(
        jni_.paint_cls, "getFontMetrics", "()Landroid/graphics/Paint$FontMetrics;");

    jni_.argb_8888_field_ = env->GetStaticFieldID(
        jni_.bitmap_config_cls, "ARGB_8888", "Landroid/graphics/Bitmap$Config;");

    if (!jni_.bitmap_create_ || !jni_.bitmap_copy_pixels_to_buffer_ ||
        !jni_.canvas_ctor_ || !jni_.canvas_draw_text_ ||
        !jni_.paint_ctor_ || !jni_.paint_set_color_ ||
        !jni_.paint_set_text_size_ || !jni_.paint_set_antialias_ ||
        !jni_.paint_measure_text_ || !jni_.paint_get_font_metrics_ ||
        !jni_.argb_8888_field_)
    {
        LOGE("AndroidTextRasterizer: failed to cache JNI methods");
        return false;
    }

    return true;
}

void AndroidTextRasterizer::releaseJni()
{
    JNIEnv* env = getAndroidJniEnv();
    if (!env) return;

    if (jni_.bitmap_cls)             env->DeleteLocalRef(jni_.bitmap_cls);
    if (jni_.bitmap_config_cls)      env->DeleteLocalRef(jni_.bitmap_config_cls);
    if (jni_.canvas_cls)             env->DeleteLocalRef(jni_.canvas_cls);
    if (jni_.paint_cls)              env->DeleteLocalRef(jni_.paint_cls);
    if (jni_.paint_font_metrics_cls) env->DeleteLocalRef(jni_.paint_font_metrics_cls);

    jni_ = JniRefs{};
}

// ---------------------------------------------------------------------------
// measure
// ---------------------------------------------------------------------------

Size AndroidTextRasterizer::measure(const TextSpan& span)
{
    if (!available_ || span.text.empty()) {
        const float char_width  = span.style.font_size * 0.6f;
        const float line_height = span.style.font_size * 1.2f;
        return Size{ char_width * static_cast<float>(span.text.size()), line_height };
    }

    JNIEnv* env = getAndroidJniEnv();
    if (!env) {
        const float char_width  = span.style.font_size * 0.6f;
        const float line_height = span.style.font_size * 1.2f;
        return Size{ char_width * static_cast<float>(span.text.size()), line_height };
    }

    jobject paint = env->NewObject(jni_.paint_cls, jni_.paint_ctor_);
    if (!paint) return Size{};

    env->CallVoidMethod(paint, jni_.paint_set_text_size_, span.style.font_size);
    env->CallVoidMethod(paint, jni_.paint_set_antialias_, true);

    jstring jtext = env->NewStringUTF(span.text.c_str());
    float width = env->CallFloatMethod(paint, jni_.paint_measure_text_, jtext);
    env->DeleteLocalRef(jtext);

    jobject fm = env->CallObjectMethod(paint, jni_.paint_get_font_metrics_);
    float ascent = 0.0f, descent = 0.0f;
    if (fm) {
        jfieldID ascent_field  = env->GetFieldID(jni_.paint_font_metrics_cls, "ascent", "F");
        jfieldID descent_field = env->GetFieldID(jni_.paint_font_metrics_cls, "descent", "F");
        if (ascent_field)  ascent  = env->GetFloatField(fm, ascent_field);
        if (descent_field) descent = env->GetFloatField(fm, descent_field);
        env->DeleteLocalRef(fm);
    }
    float height = descent - ascent;
    if (height <= 0.0f) height = span.style.font_size * 1.2f;

    env->DeleteLocalRef(paint);

    return Size{ width, height };
}

// ---------------------------------------------------------------------------
// rasterize
// ---------------------------------------------------------------------------

AndroidTextRasterizer::Bitmap AndroidTextRasterizer::rasterize(const TextSpan& span)
{
    Bitmap bmp;
    if (!available_ || span.text.empty()) return bmp;

    JNIEnv* env = getAndroidJniEnv();
    if (!env) return bmp;

    // Measure first
    Size sz = measure(span);
    int w = static_cast<int>(std::ceil(sz.width));
    int h = static_cast<int>(std::ceil(sz.height));
    if (w <= 0 || h <= 0) return bmp;

    // Get ARGB_8888 config
    jobject config = env->GetStaticObjectField(
        jni_.bitmap_config_cls, jni_.argb_8888_field_);
    if (!config) return bmp;

    // Create bitmap
    jobject bitmap = env->CallStaticObjectMethod(
        jni_.bitmap_cls, jni_.bitmap_create_, w, h, config);
    env->DeleteLocalRef(config);
    if (!bitmap) return bmp;

    // Create canvas
    jobject canvas = env->NewObject(jni_.canvas_cls, jni_.canvas_ctor_, bitmap);
    if (!canvas) {
        env->DeleteLocalRef(bitmap);
        return bmp;
    }

    // Create paint
    jobject paint = env->NewObject(jni_.paint_cls, jni_.paint_ctor_);
    if (!paint) {
        env->DeleteLocalRef(canvas);
        env->DeleteLocalRef(bitmap);
        return bmp;
    }

    env->CallVoidMethod(paint, jni_.paint_set_color_, colorToArgb(span.style.color));
    env->CallVoidMethod(paint, jni_.paint_set_text_size_, span.style.font_size);
    env->CallVoidMethod(paint, jni_.paint_set_antialias_, true);

    // Get font metrics for baseline
    jobject fm = env->CallObjectMethod(paint, jni_.paint_get_font_metrics_);
    float ascent = 0.0f;
    if (fm) {
        jfieldID ascent_field = env->GetFieldID(jni_.paint_font_metrics_cls, "ascent", "F");
        if (ascent_field) ascent = env->GetFloatField(fm, ascent_field);
        env->DeleteLocalRef(fm);
    }

    // Draw text at baseline (y = -ascent places top of text at y=0)
    jstring jtext = env->NewStringUTF(span.text.c_str());
    env->CallVoidMethod(canvas, jni_.canvas_draw_text_, jtext, 0.0f, -ascent, paint);
    env->DeleteLocalRef(jtext);

    // Read pixels into C++ buffer
    bmp.width  = w;
    bmp.height = h;
    bmp.pixels.resize(static_cast<size_t>(w) * h * 4);

    jobject byte_buffer = env->NewDirectByteBuffer(
        bmp.pixels.data(), static_cast<jlong>(bmp.pixels.size()));
    if (byte_buffer) {
        env->CallVoidMethod(bitmap, jni_.bitmap_copy_pixels_to_buffer_, byte_buffer);
        env->DeleteLocalRef(byte_buffer);
    }

    env->DeleteLocalRef(paint);
    env->DeleteLocalRef(canvas);
    env->DeleteLocalRef(bitmap);

    return bmp;
}

} // namespace systems::leal::campello_widgets
