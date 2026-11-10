// Browser glue for campello_widgets on the web (src/web/). Linked with
// --js-library by web.cmake.

addToLibrary({
  $CW: {
    textCanvas: null,
    textContext: null,
    context2d: function () {
      if (!CW.textContext) {
        CW.textCanvas = new OffscreenCanvas(1, 1);
        CW.textContext = CW.textCanvas.getContext('2d', { willReadFrequently: true });
      }
      return CW.textContext;
    },
  },

  // Resizes the canvas backing store to its CSS box times devicePixelRatio.
  // Writes [width, height] (physical pixels), the ratio and the canvas's
  // viewport origin [left, top] (CSS pixels); returns 1 if the size changed.
  cw_web_sync_canvas__deps: ['$CW'],
  cw_web_sync_canvas: function (selectorPtr, outSizePtr, outDprPtr, outOriginPtr) {
    var canvas = document.querySelector(UTF8ToString(selectorPtr));
    if (!canvas) return 0;
    var dpr = window.devicePixelRatio || 1;
    var rect = canvas.getBoundingClientRect();
    var w = Math.max(1, Math.round(rect.width * dpr));
    var h = Math.max(1, Math.round(rect.height * dpr));
    var changed = canvas.width !== w || canvas.height !== h;
    if (changed) {
      canvas.width = w;
      canvas.height = h;
    }
    HEAP32[outSizePtr >> 2] = w;
    HEAP32[(outSizePtr >> 2) + 1] = h;
    HEAPF32[outDprPtr >> 2] = dpr;
    HEAPF32[outOriginPtr >> 2] = rect.left;
    HEAPF32[(outOriginPtr >> 2) + 1] = rect.top;
    return changed ? 1 : 0;
  },

  // The frame loop: requestAnimationFrame calling cw_web_frame(). If a frame
  // yields (an ASYNCIFY sleep in a blocking campello_gpu call), the next
  // frame is requested once it has finished, instead of the loop ending.
  cw_web_start_frame_loop__deps: ['cw_web_frame'],
  cw_web_start_frame_loop: function () {
    function tick() {
      _cw_web_frame();
#if ASYNCIFY == 1
      if (Asyncify.currData) {
        Asyncify.whenDone().then(function () { requestAnimationFrame(tick); });
        return;
      }
#endif
      requestAnimationFrame(tick);
    }
    requestAnimationFrame(tick);
  },

  cw_web_set_title: function (titlePtr) {
    document.title = UTF8ToString(titlePtr);
  },

  cw_web_set_cursor: function (selectorPtr, cursorPtr) {
    var canvas = document.querySelector(UTF8ToString(selectorPtr));
    if (canvas) canvas.style.cursor = UTF8ToString(cursorPtr);
  },

  cw_web_install_paste_listener: function () {
    document.addEventListener('paste', function (e) {
      var text = (e.clipboardData && e.clipboardData.getData('text/plain')) || '';
      e.preventDefault();
      var len = lengthBytesUTF8(text) + 1;
      var ptr = _malloc(len);
      stringToUTF8(text, ptr, len);
      _cw_web_on_paste(ptr);
      _free(ptr);
    });
  },
  cw_web_install_paste_listener__deps: ['malloc', 'free', 'cw_web_on_paste'],

  cw_web_prefers_dark: function () {
    return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches ? 1 : 0;
  },

  // Text: Canvas2D measureText. Writes [advance, ascent, descent].
  cw_web_text_measure__deps: ['$CW'],
  cw_web_text_measure: function (textPtr, fontPtr, outPtr) {
    var ctx = CW.context2d();
    ctx.font = UTF8ToString(fontPtr);
    var m = ctx.measureText(UTF8ToString(textPtr));
    var i = outPtr >> 2;
    HEAPF32[i] = m.width;
    HEAPF32[i + 1] = m.fontBoundingBoxAscent;
    HEAPF32[i + 2] = m.fontBoundingBoxDescent;
  },

  // Draws the text with its baseline at `baseline` into a w x h bitmap and
  // writes it as premultiplied BGRA8 to outPtr.
  cw_web_text_rasterize__deps: ['$CW'],
  cw_web_text_rasterize: function (textPtr, fontPtr, r, g, b, a, w, h, baseline, outPtr) {
    var ctx = CW.context2d();
    if (CW.textCanvas.width < w) CW.textCanvas.width = w;
    if (CW.textCanvas.height < h) CW.textCanvas.height = h;
    ctx.clearRect(0, 0, w, h);
    ctx.font = UTF8ToString(fontPtr);
    ctx.textBaseline = 'alphabetic';
    ctx.textAlign = 'left';
    ctx.fillStyle = 'rgba(' + Math.round(r * 255) + ',' + Math.round(g * 255) + ',' +
                    Math.round(b * 255) + ',' + a + ')';
    ctx.fillText(UTF8ToString(textPtr), 0, baseline);
    var src = ctx.getImageData(0, 0, w, h).data;
    var n = w * h * 4;
    for (var p = 0; p < n; p += 4) {
      var alpha = src[p + 3];
      var k = alpha / 255;
      HEAPU8[outPtr + p] = Math.round(src[p + 2] * k);
      HEAPU8[outPtr + p + 1] = Math.round(src[p + 1] * k);
      HEAPU8[outPtr + p + 2] = Math.round(src[p] * k);
      HEAPU8[outPtr + p + 3] = alpha;
    }
  },
});
