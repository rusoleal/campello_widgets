#pragma once

#include <jni.h>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Supplies the JavaVM* and a NativeActivity ref used by Clipboard
     *        on Android (there's no shared getter for either elsewhere in
     *        this codebase -- android_text_rasterizer.cpp's g_java_vm and
     *        run_app.cpp's activity jobject are both TU-local).
     *
     * Must be called (e.g. from run_app.cpp) before any clipboard access.
     * Safe to call again on activity recreation -- replaces any previously
     * held global ref.
     */
    void setAndroidClipboardContext(JavaVM* vm, jobject activity);

} // namespace systems::leal::campello_widgets
