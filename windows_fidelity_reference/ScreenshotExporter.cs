using System.Runtime.InteropServices.WindowsRuntime;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media.Imaging;
using Windows.Graphics.Imaging;
using Windows.Storage.Streams;

namespace FidelityReference;

/// <summary>
/// Captures a XAML element to a PNG file via RenderTargetBitmap — the
/// WinUI 3 equivalent of the C++ side's captureRenderBoxToPng() (offscreen
/// GPU-backed raster → PNG). Renders at the element's actual composited
/// device-pixel resolution (already DPI-scaled by WinUI's own layout system
/// — no manual scaling needed here), matching kFluentPhysicalWidth/Height
/// on the C++ side for a fixed-size element.
/// </summary>
public static class ScreenshotExporter
{
    public static async Task CaptureToPngAsync(FrameworkElement element, string outPath)
    {
        var rtb = new RenderTargetBitmap();
        await rtb.RenderAsync(element);

        var pixelBuffer = await rtb.GetPixelsAsync();

        using var memStream = new InMemoryRandomAccessStream();
        var encoder = await BitmapEncoder.CreateAsync(BitmapEncoder.PngEncoderId, memStream);
        // GetPixelsAsync() returns BGRA8 premultiplied-alpha pixel data —
        // see its doc comment.
        encoder.SetPixelData(
            BitmapPixelFormat.Bgra8,
            BitmapAlphaMode.Premultiplied,
            (uint)rtb.PixelWidth,
            (uint)rtb.PixelHeight,
            96.0, 96.0,
            pixelBuffer.ToArray());
        await encoder.FlushAsync();

        var bytes = new byte[memStream.Size];
        memStream.Seek(0);
        await memStream.ReadAsync(bytes.AsBuffer(), (uint)bytes.Length, InputStreamOptions.None);

        var dir = Path.GetDirectoryName(outPath);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        await File.WriteAllBytesAsync(outPath, bytes);
    }
}
