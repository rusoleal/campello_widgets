#include <campello_widgets/ui/clipboard.hpp>
#include "android_clipboard.hpp"

#include <jni.h>

namespace systems::leal::campello_widgets
{

    static JavaVM* g_java_vm = nullptr;
    static jobject g_activity = nullptr; // global ref, owned here

    void setAndroidClipboardContext(JavaVM* vm, jobject activity)
    {
        // Drop any previously held global ref before replacing it (activity
        // recreation, e.g. on rotation, hands us a new jobject).
        if (g_activity)
        {
            JNIEnv* env = nullptr;
            if (g_java_vm &&
                g_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK && env)
            {
                env->DeleteGlobalRef(g_activity);
            }
            g_activity = nullptr;
        }

        g_java_vm = vm;

        if (vm && activity)
        {
            JNIEnv* env = nullptr;
            if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK && env)
                g_activity = env->NewGlobalRef(activity);
        }
    }

    // Mirrors run_app.cpp's getJniEnv(android_app*, bool*) -- same
    // GetEnv-then-AttachCurrentThread fallback, just against g_java_vm
    // instead of an android_app* (not available in this TU).
    static JNIEnv* getJniEnv(bool* out_attached)
    {
        *out_attached = false;
        if (!g_java_vm) return nullptr;

        JNIEnv* env = nullptr;
        if (g_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK)
            return env;
        if (g_java_vm->AttachCurrentThread(&env, nullptr) == JNI_OK)
        {
            *out_attached = true;
            return env;
        }
        return nullptr;
    }

    static jobject getClipboardManager(JNIEnv* env)
    {
        jclass activityClass = env->FindClass("android/app/NativeActivity");
        jmethodID getSystemService = env->GetMethodID(
            activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
        jstring serviceName = env->NewStringUTF("clipboard");
        jobject clipboardManager = env->CallObjectMethod(g_activity, getSystemService, serviceName);
        env->DeleteLocalRef(serviceName);
        return clipboardManager;
    }

    void Clipboard::setText(const std::string& text)
    {
        if (!g_activity) return;

        bool attached = false;
        JNIEnv* env = getJniEnv(&attached);
        if (!env) return;

        jobject clipboardManager = getClipboardManager(env);

        jclass clipDataClass = env->FindClass("android/content/ClipData");
        jmethodID newPlainText = env->GetStaticMethodID(
            clipDataClass, "newPlainText",
            "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");

        jstring label = env->NewStringUTF("campello");
        jstring value = env->NewStringUTF(text.c_str());
        jobject clipData = env->CallStaticObjectMethod(clipDataClass, newPlainText, label, value);

        jclass cmClass = env->FindClass("android/content/ClipboardManager");
        jmethodID setPrimaryClip = env->GetMethodID(
            cmClass, "setPrimaryClip", "(Landroid/content/ClipData;)V");
        env->CallVoidMethod(clipboardManager, setPrimaryClip, clipData);

        env->DeleteLocalRef(clipData);
        env->DeleteLocalRef(value);
        env->DeleteLocalRef(label);
        env->DeleteLocalRef(clipboardManager);

        if (attached)
            g_java_vm->DetachCurrentThread();
    }

    std::string Clipboard::getText()
    {
        if (!g_activity) return {};

        bool attached = false;
        JNIEnv* env = getJniEnv(&attached);
        if (!env) return {};

        std::string result;

        jobject clipboardManager = getClipboardManager(env);
        jclass cmClass = env->FindClass("android/content/ClipboardManager");

        jmethodID hasPrimaryClip = env->GetMethodID(cmClass, "hasPrimaryClip", "()Z");
        if (env->CallBooleanMethod(clipboardManager, hasPrimaryClip))
        {
            jmethodID getPrimaryClip = env->GetMethodID(
                cmClass, "getPrimaryClip", "()Landroid/content/ClipData;");
            jobject clipData = env->CallObjectMethod(clipboardManager, getPrimaryClip);

            if (clipData)
            {
                jclass clipDataClass = env->FindClass("android/content/ClipData");
                jmethodID getItemCount = env->GetMethodID(clipDataClass, "getItemCount", "()I");

                if (env->CallIntMethod(clipData, getItemCount) > 0)
                {
                    jmethodID getItemAt = env->GetMethodID(
                        clipDataClass, "getItemAt", "(I)Landroid/content/ClipData$Item;");
                    jobject item = env->CallObjectMethod(clipData, getItemAt, 0);

                    jclass itemClass = env->FindClass("android/content/ClipData$Item");
                    jmethodID getText = env->GetMethodID(
                        itemClass, "getText", "()Ljava/lang/CharSequence;");
                    jobject charSeq = env->CallObjectMethod(item, getText);

                    if (charSeq)
                    {
                        // CharSequence -> String via Object.toString(), same
                        // as any CharSequence implementation (String,
                        // SpannableString, ...) the system clipboard might
                        // hand back.
                        jclass objClass = env->FindClass("java/lang/Object");
                        jmethodID toString = env->GetMethodID(
                            objClass, "toString", "()Ljava/lang/String;");
                        jstring jstr = static_cast<jstring>(env->CallObjectMethod(charSeq, toString));

                        if (jstr)
                        {
                            const char* chars = env->GetStringUTFChars(jstr, nullptr);
                            if (chars)
                            {
                                result = chars;
                                env->ReleaseStringUTFChars(jstr, chars);
                            }
                            env->DeleteLocalRef(jstr);
                        }
                        env->DeleteLocalRef(charSeq);
                    }
                    env->DeleteLocalRef(item);
                }
                env->DeleteLocalRef(clipData);
            }
        }

        env->DeleteLocalRef(clipboardManager);

        if (attached)
            g_java_vm->DetachCurrentThread();

        return result;
    }

} // namespace systems::leal::campello_widgets
