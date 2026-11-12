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
    // A canvas by selector: a popup's canvas (registered in
    // specialHTMLTargets, which emscripten's own lookups also check first)
    // or an element of this document.
    find: function (selector) {
      return specialHTMLTargets[selector] || document.querySelector(selector);
    },
    onPaste: function (e) {
      var text = (e.clipboardData && e.clipboardData.getData('text/plain')) || '';
      e.preventDefault();
      var len = lengthBytesUTF8(text) + 1;
      var ptr = _malloc(len);
      stringToUTF8(text, ptr, len);
      _cw_web_on_paste(ptr);
      _free(ptr);
    },
    popups: {},  // selector -> popup Window
  },
  $CW__deps: ['$specialHTMLTargets', 'malloc', 'free', 'cw_web_on_paste'],

  // Resizes the canvas backing store to its CSS box times devicePixelRatio.
  // Writes [width, height] (physical pixels), the ratio and the canvas's
  // viewport origin [left, top] (CSS pixels); returns 1 if the size changed.
  cw_web_sync_canvas__deps: ['$CW'],
  cw_web_sync_canvas: function (selectorPtr, outSizePtr, outDprPtr, outOriginPtr) {
    var canvas = CW.find(UTF8ToString(selectorPtr));
    if (!canvas) return 0;
    var view = canvas.ownerDocument.defaultView || window;  // a popup has its own ratio
    var dpr = view.devicePixelRatio || 1;
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

  // 1 if the canvas matching the selector is the topmost element at the
  // client point (so hover goes to a dialog canvas, not the one under it).
  cw_web_is_topmost_at__deps: ['$CW'],
  cw_web_is_topmost_at: function (selectorPtr, x, y) {
    var canvas = CW.find(UTF8ToString(selectorPtr));
    return canvas && canvas.ownerDocument.elementFromPoint(x, y) === canvas ? 1 : 0;
  },

  // --- Canvases opened by WindowController (src/web/platform_windowing.cpp).
  // A new, hidden <canvas id=...> of w x h CSS pixels. Placement: inside the
  // page's [data-cw-windows] element if there is one (the page lays it out),
  // otherwise floating (position: fixed) at (x, y), or cascaded when
  // hasPosition is 0. Dialogs float above everything.
  cw_web_create_canvas: function (idPtr, titlePtr, w, h, hasPosition, x, y, dialog) {
    var canvas = document.createElement('canvas');
    canvas.id = UTF8ToString(idPtr);
    canvas.tabIndex = 0;
    canvas.className = dialog ? 'cw-window cw-dialog' : 'cw-window';
    canvas.setAttribute('aria-label', UTF8ToString(titlePtr));
    canvas.oncontextmenu = function (e) { e.preventDefault(); };
    var s = canvas.style;
    s.display = 'none';
    s.width = w + 'px';
    s.height = h + 'px';
    s.outline = 'none';
    var host = dialog ? null : document.querySelector('[data-cw-windows]');
    if (host) {
      host.appendChild(canvas);
    } else {
      CW.cascade = ((CW.cascade || 0) % 8) + 1;
      s.position = 'fixed';
      s.left = (hasPosition ? x : 24 + 32 * CW.cascade) + 'px';
      s.top = (hasPosition ? y : 24 + 32 * CW.cascade) + 'px';
      s.zIndex = dialog ? '1001' : '1000';
      s.boxShadow = '0 8px 32px rgba(0,0,0,0.25)';
      s.borderRadius = '8px';
      document.body.appendChild(canvas);
    }
  },
  cw_web_create_canvas__deps: ['$CW'],

  cw_web_remove_canvas__deps: ['$CW'],
  cw_web_remove_canvas: function (selectorPtr) {
    var canvas = CW.find(UTF8ToString(selectorPtr));
    if (canvas) canvas.remove();
  },

  // Shows the canvas; a floating dialog is centred on `parentPtr`'s canvas
  // (or the viewport) the first time.
  cw_web_show_canvas__deps: ['$CW'],
  cw_web_show_canvas: function (selectorPtr, parentPtr, centre) {
    var canvas = CW.find(UTF8ToString(selectorPtr));
    if (!canvas) return;
    canvas.style.display = 'block';
    if (centre && canvas.style.position === 'fixed') {
      var parent = parentPtr ? CW.find(UTF8ToString(parentPtr)) : null;
      var p = parent ? parent.getBoundingClientRect()
                     : { left: 0, top: 0, width: window.innerWidth, height: window.innerHeight };
      var r = canvas.getBoundingClientRect();
      canvas.style.left = Math.round(p.left + (p.width - r.width) / 2) + 'px';
      canvas.style.top = Math.round(p.top + (p.height - r.height) / 2) + 'px';
    }
    canvas.focus({ preventScroll: true });
  },

  cw_web_set_canvas_size__deps: ['$CW'],
  cw_web_set_canvas_size: function (selectorPtr, w, h) {
    var canvas = CW.find(UTF8ToString(selectorPtr));
    if (!canvas) return;
    canvas.style.width = w + 'px';
    canvas.style.height = h + 'px';
  },

  cw_web_set_canvas_title__deps: ['$CW'],
  cw_web_set_canvas_title: function (selectorPtr, titlePtr) {
    var canvas = CW.find(UTF8ToString(selectorPtr));
    if (canvas) canvas.setAttribute('aria-label', UTF8ToString(titlePtr));
  },

  // --- Popup windows (WindowOptions::web_popup, experimental). A same-origin
  // window.open() whose canvas is drawn with the opener's GPUDevice. The
  // canvas is registered as specialHTMLTargets[selector] and the popup's
  // root element as specialHTMLTargets[selector + '-window'], so emscripten's event
  // and canvas lookups (and WebGPU surface creation) find them. Returns 0 if
  // the browser blocked the popup.
  cw_web_open_popup__deps: ['$CW'],
  cw_web_open_popup: function (selectorPtr, titlePtr, w, h, hasPosition, x, y) {
    var selector = UTF8ToString(selectorPtr);
    var features = 'popup,width=' + w + ',height=' + h +
                   (hasPosition ? ',left=' + x + ',top=' + y : '');
    var win = window.open('', selector.replace(/[^a-zA-Z0-9_-]/g, '_'), features);
    if (!win) return 0;
    var doc = win.document;
    doc.title = UTF8ToString(titlePtr);
    var style = doc.createElement('style');
    style.textContent = 'html,body{margin:0;height:100%;overflow:hidden;background:#fff}' +
                        'canvas{display:block;width:100vw;height:100vh;outline:none}';
    doc.head.appendChild(style);
    var canvas = doc.createElement('canvas');
    canvas.tabIndex = 0;
    canvas.oncontextmenu = function (e) { e.preventDefault(); };
    doc.body.appendChild(canvas);
    doc.addEventListener('paste', CW.onPaste);
    specialHTMLTargets[selector] = canvas;
    // The popup's own events: its root element, not its Window, because
    // emscripten's mouse events read getBoundingClientRect() of the target.
    specialHTMLTargets[selector + '-window'] = doc.documentElement;
    CW.popups[selector] = win;
    return 1;
  },

  // 1 once the user (or the browser) has closed the popup.
  cw_web_popup_closed__deps: ['$CW'],
  cw_web_popup_closed: function (selectorPtr) {
    var win = CW.popups[UTF8ToString(selectorPtr)];
    return !win || win.closed ? 1 : 0;
  },

  cw_web_close_popup__deps: ['$CW'],
  cw_web_close_popup: function (selectorPtr) {
    var selector = UTF8ToString(selectorPtr);
    var win = CW.popups[selector];
    delete CW.popups[selector];
    delete specialHTMLTargets[selector];
    delete specialHTMLTargets[selector + '-window'];
    if (win && !win.closed) win.close();
  },

  cw_web_popup_command__deps: ['$CW'],
  // 0 = focus, 1 = set title (str), 2 = resize content to (w, h).
  cw_web_popup_command: function (selectorPtr, command, strPtr, w, h) {
    var win = CW.popups[UTF8ToString(selectorPtr)];
    if (!win || win.closed) return;
    if (command === 0) win.focus();
    else if (command === 1) win.document.title = UTF8ToString(strPtr);
    else if (command === 2) win.resizeBy(w - win.innerWidth, h - win.innerHeight);
  },

  cw_web_set_title: function (titlePtr) {
    document.title = UTF8ToString(titlePtr);
  },

  cw_web_set_cursor__deps: ['$CW'],
  cw_web_set_cursor: function (selectorPtr, cursorPtr) {
    var canvas = CW.find(UTF8ToString(selectorPtr));
    if (canvas) canvas.style.cursor = UTF8ToString(cursorPtr);
  },

  cw_web_install_paste_listener__deps: ['$CW'],
  cw_web_install_paste_listener: function () {
    document.addEventListener('paste', CW.onPaste);
  },

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
