// Non-macOS stub — GPU visual renderer is not available on this platform.
#include "gpu_visual_renderer.hpp"

namespace systems::leal::campello_widgets::testing
{

struct GpuVisualRenderer::Impl {};

GpuVisualRenderer::GpuVisualRenderer(int, int) : impl_(nullptr) {}
GpuVisualRenderer::~GpuVisualRenderer() = default;

bool GpuVisualRenderer::isValid()  const                           { return false; }
void GpuVisualRenderer::setClearColor(const Color&)                {}
bool GpuVisualRenderer::renderDrawList(const DrawList&)            { return false; }
bool GpuVisualRenderer::saveToPng(const std::string&)              { return false; }
int  GpuVisualRenderer::width()    const                           { return 0; }
int  GpuVisualRenderer::height()   const                           { return 0; }

} // namespace systems::leal::campello_widgets::testing
