#!/usr/bin/env bash
# Download Roboto font from Google Fonts
set -e

echo "Downloading Roboto font..."

cd "$(dirname "$0")/fonts"

# Roboto regular
if [ ! -f "Roboto-Regular.ttf" ]; then
    curl -L -o Roboto-Regular.ttf "https://github.com/googlefonts/roboto/raw/main/src/hinted/Roboto-Regular.ttf" 2>/dev/null || \
    curl -L -o Roboto-Regular.ttf "https://raw.githubusercontent.com/google/fonts/main/apache/roboto/Roboto%5Bwdth%2Cwght%5D.ttf" 2>/dev/null || \
    echo "Warning: Could not download Roboto-Regular.ttf"
fi

# Roboto bold
if [ ! -f "Roboto-Bold.ttf" ]; then
    curl -L -o Roboto-Bold.ttf "https://github.com/googlefonts/roboto/raw/main/src/hinted/Roboto-Bold.ttf" 2>/dev/null || \
    curl -L -o Roboto-Bold.ttf "https://raw.githubusercontent.com/google/fonts/main/apache/roboto/Roboto-Bold.ttf" 2>/dev/null || \
    echo "Warning: Could not download Roboto-Bold.ttf"
fi

# Use Roboto as Helvetica Neue fallback
if [ ! -f "helvetica_neue.ttf" ]; then
    ln -sf Roboto-Regular.ttf helvetica_neue.ttf 2>/dev/null || true
fi

echo "Font download complete."
ls -la
