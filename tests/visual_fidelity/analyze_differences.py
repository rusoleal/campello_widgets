#!/usr/bin/env python3
"""
Analyze differences between Flutter and C++ rendered images.
"""

import sys
from PIL import Image
import os

def analyze_image_difference(flutter_path, cpp_path, output_dir):
    """Analyze differences between two images."""
    
    # Load images
    flutter_img = Image.open(flutter_path).convert('RGBA')
    cpp_img = Image.open(cpp_path).convert('RGBA')
    
    print(f"\nAnalyzing: {os.path.basename(flutter_path)}")
    print(f"  Flutter: {flutter_img.size} mode={flutter_img.mode}")
    print(f"  C++:     {cpp_img.size} mode={cpp_img.mode}")
    
    # Check sizes match
    if flutter_img.size != cpp_img.size:
        print(f"  ERROR: Size mismatch! Flutter={flutter_img.size}, C++={cpp_img.size}")
        return
    
    width, height = flutter_img.size
    
    # Compare pixels
    flutter_data = list(flutter_img.getdata())
    cpp_data = list(cpp_img.getdata())
    
    total_pixels = len(flutter_data)
    diff_pixels = 0
    max_diff = 0
    total_diff = 0
    
    diff_channels = {'r': 0, 'g': 0, 'b': 0, 'a': 0}
    
    for i, (f_px, c_px) in enumerate(zip(flutter_data, cpp_data)):
        # Calculate per-channel differences
        diff_r = abs(f_px[0] - c_px[0])
        diff_g = abs(f_px[1] - c_px[1])
        diff_b = abs(f_px[2] - c_px[2])
        diff_a = abs(f_px[3] - c_px[3])
        
        pixel_diff = max(diff_r, diff_g, diff_b, diff_a)
        
        if pixel_diff > 0:
            diff_pixels += 1
            total_diff += pixel_diff
            max_diff = max(max_diff, pixel_diff)
            diff_channels['r'] += diff_r
            diff_channels['g'] += diff_g
            diff_channels['b'] += diff_b
            diff_channels['a'] += diff_a
    
    diff_percent = (diff_pixels / total_pixels) * 100
    avg_diff = total_diff / diff_pixels if diff_pixels > 0 else 0
    
    print(f"\n  Difference Statistics:")
    print(f"    Different pixels: {diff_pixels} / {total_pixels} ({diff_percent:.2f}%)")
    print(f"    Maximum difference: {max_diff}")
    print(f"    Average difference: {avg_diff:.2f}")
    
    if diff_pixels > 0:
        print(f"\n  Per-channel average differences:")
        print(f"    Red:   {diff_channels['r'] / diff_pixels:.2f}")
        print(f"    Green: {diff_channels['g'] / diff_pixels:.2f}")
        print(f"    Blue:  {diff_channels['b'] / diff_pixels:.2f}")
        print(f"    Alpha: {diff_channels['a'] / diff_pixels:.2f}")
    
    # Check if differences are concentrated in specific areas
    # Create a simple heatmap
    if diff_pixels > 0:
        diff_img = Image.new('RGBA', (width, height), (0, 0, 0, 0))
        diff_pixels_list = []
        
        for y in range(height):
            for x in range(width):
                idx = y * width + x
                f_px = flutter_data[idx]
                c_px = cpp_data[idx]
                
                diff = max(abs(f_px[i] - c_px[i]) for i in range(4))
                if diff > 0:
                    # Red intensity based on difference
                    intensity = int((diff / 255) * 255)
                    diff_pixels_list.append((x, y, (intensity, 0, 0, 128)))
        
        # Draw differences
        for x, y, color in diff_pixels_list:
            diff_img.putpixel((x, y), color)
        
        basename = os.path.splitext(os.path.basename(flutter_path))[0]
        diff_output = os.path.join(output_dir, f"{basename}_analysis.png")
        diff_img.save(diff_output)
        print(f"\n  Diff heatmap saved: {diff_output}")

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    flutter_dir = os.path.join(base_dir, 'flutter_goldens')
    cpp_dir = os.path.join(base_dir, 'cpp_output')
    output_dir = os.path.join(base_dir, 'analysis')
    
    os.makedirs(output_dir, exist_ok=True)
    
    # Find real_image files
    test_names = [
        'real_image_explicit_size.png',
        'real_image_fit_contain.png',
        'real_image_fit_cover.png',
        'real_image_gallery.png',
        'real_image_with_opacity.png',
    ]
    
    for name in test_names:
        flutter_path = os.path.join(flutter_dir, name)
        cpp_path = os.path.join(cpp_dir, name)
        
        if os.path.exists(flutter_path) and os.path.exists(cpp_path):
            analyze_image_difference(flutter_path, cpp_path, output_dir)
        else:
            print(f"\nSkipping {name} - file not found")

if __name__ == '__main__':
    main()
