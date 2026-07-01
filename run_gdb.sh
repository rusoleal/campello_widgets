#!/bin/bash
LD_LIBRARY_PATH=/home/ruben/repos/campello_widgets/build/linux-debug:/home/ruben/repos/campello_widgets/build/linux-debug/_deps/campello_gpu-build:/home/ruben/repos/campello_widgets/build/linux-debug/_deps/campello_image-build \
gdb -batch \
    -ex "set pagination off" \
    -ex "run" \
    -ex "backtrace full" \
    -ex "info threads" \
    -ex "thread apply all bt full" \
    --args /home/ruben/repos/campello_widgets/build/linux-debug/campello_widgets_gallery \
    2>&1 | tee /tmp/gdb_crash.txt
