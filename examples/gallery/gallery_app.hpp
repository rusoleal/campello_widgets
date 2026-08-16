#pragma once

#include <campello_widgets/widgets/widget.hpp>
#include <memory>
#include <string>

namespace systems::leal::campello_widgets
{
    std::shared_ptr<Widget> buildGalleryApp();

    /**
     * @brief Sets the file path to the gallery's sample video clip,
     * consumed by the Video tab.
     *
     * Must be called before buildGalleryApp() constructs the widget tree
     * (VideoSectionState::initState() reads it on first build). Each
     * platform's main.mm/main.cpp resolves this differently — macOS from a
     * compile-time CAMPELLO_GALLERY_ASSETS_DIR path (this example always
     * runs directly out of the build tree, never redistributed, so an
     * absolute host path is fine there), iOS via NSBundle (the app is
     * sandboxed — no access to an arbitrary host filesystem path, on a
     * real device it wouldn't even exist) — keeping that resolution out of
     * gallery_app.cpp is what keeps it portable, platform-agnostic C++.
     */
    void setSampleVideoPath(std::string path);
}
