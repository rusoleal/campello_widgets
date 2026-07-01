#!/bin/bash
LD_LIBRARY_PATH=/home/ruben/repos/campello_widgets/build/linux-asan:/home/ruben/repos/campello_widgets/build/linux-asan/_deps/campello_gpu-build:/home/ruben/repos/campello_widgets/build/linux-asan/_deps/campello_image-build:/usr/lib/gcc/x86_64-linux-gnu/15 \
ASAN_OPTIONS=halt_on_error=1:symbolize=1:detect_leaks=0 \
/home/ruben/repos/campello_widgets/build/linux-asan/campello_widgets_gallery 2>&1 | tee /tmp/asan_crash.txt
