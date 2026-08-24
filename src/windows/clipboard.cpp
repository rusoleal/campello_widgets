#include <campello_widgets/ui/clipboard.hpp>

#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace systems::leal::campello_widgets
{
    namespace
    {
        // Two-pass MultiByteToWideChar/WideCharToMultiByte + CP_UTF8, same
        // idiom as run_app.cpp's utf16ToUtf8() and d3d_draw_backend.cpp's
        // utf8ToUtf16() -- neither is exposed outside its own TU, so this
        // mirrors the pattern locally rather than reusing either.
        std::wstring utf8ToUtf16(const std::string& s)
        {
            if (s.empty()) return {};
            int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
            if (len <= 0) return {};
            std::wstring w(static_cast<size_t>(len), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), len);
            return w;
        }

        std::string utf16ToUtf8(const wchar_t* w, int wlen)
        {
            if (!w || wlen <= 0) return {};
            int len = WideCharToMultiByte(CP_UTF8, 0, w, wlen, nullptr, 0, nullptr, nullptr);
            if (len <= 0) return {};
            std::string s(static_cast<size_t>(len), '\0');
            WideCharToMultiByte(CP_UTF8, 0, w, wlen, s.data(), len, nullptr, nullptr);
            return s;
        }
    }

    void Clipboard::setText(const std::string& text)
    {
        // OpenClipboard(NULL) is explicitly valid per the Win32 docs and
        // associates ownership with the current task -- there's no HWND
        // accessor exposed anywhere in this codebase's Windows backend to
        // pass a real one instead (see run_app.cpp's file-local
        // gWindowState).
        if (!OpenClipboard(nullptr)) return;

        if (!EmptyClipboard())
        {
            CloseClipboard();
            return;
        }

        std::wstring wide = utf8ToUtf16(text);
        // Still need to allocate a valid (if empty) buffer for an empty
        // string, so the clipboard ends up genuinely cleared rather than
        // left holding whatever EmptyClipboard() didn't touch.
        SIZE_T bytes = (wide.size() + 1) * sizeof(wchar_t);

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!hMem)
        {
            CloseClipboard();
            return;
        }

        void* dst = GlobalLock(hMem);
        if (!dst)
        {
            GlobalFree(hMem);
            CloseClipboard();
            return;
        }
        memcpy(dst, wide.c_str(), bytes);
        GlobalUnlock(hMem);

        // On success SetClipboardData() takes ownership of hMem -- must not
        // free it ourselves in that case. On failure, we still own it.
        if (!SetClipboardData(CF_UNICODETEXT, hMem))
            GlobalFree(hMem);

        CloseClipboard();
    }

    std::string Clipboard::getText()
    {
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return {};
        if (!OpenClipboard(nullptr)) return {};

        std::string result;
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData)
        {
            const wchar_t* wide = static_cast<const wchar_t*>(GlobalLock(hData));
            if (wide)
            {
                result = utf16ToUtf8(wide, static_cast<int>(wcslen(wide)));
                GlobalUnlock(hData);
            }
        }

        CloseClipboard();
        return result;
    }

} // namespace systems::leal::campello_widgets
