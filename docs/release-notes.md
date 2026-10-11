PAF-Photos 0.6.0 improves button rendering and scrolling throughout the gallery and settings.

- Subtle raised button faces with an outer shadow and upper rim replace the recessed settings buttons. Rounded keyboard-focus outlines follow the chosen radius; complete background fills prevent visible rectangular corners at fractional DPI sizes.
- Pixel-based scrolling replaces gallery row jumps and abrupt settings movement. A time-based, damped movement preserves momentum on repeated wheel input and settles gently. Small touchpad deltas, Windows scroll-line/page preferences and reduced-motion settings are respected.
- Partial cards and their native buttons remain visible at viewport edges. The scrollbar uses the actual content range and dragging stops pending movement immediately.
- Drawing buffers and fonts are reused, only visible settings are drawn, and scrolling invalidates the content area. Native child surfaces are composed together to reduce tearing.

Download **setup.exe** for a per-user installation with codecs. Existing PAF-Photos 0.5.0 installations can receive this release automatically; use Settings → Check for updates → Install now, or close the app after the update finishes downloading. Settings and originals are preserved. Versions before 0.5.0 need the setup once.

**PAFPhotos-Windows-x64.zip** is the portable alternative; extract the complete ZIP. The standalone EXE includes fewer decoders. Builds are currently unsigned.

Validation includes Windows controls and pixels at 100/150/200 percent DPI, partial viewport clipping, precision wheel input, reduced motion, scrollbar bounds, drawing-resource reuse, five actual conversion encoders, codec images, installer migration and updater verification. Portable motion tests compare behavior across different frame intervals. The Windows report records rendering time on the test machine; display smoothness also depends on hardware and Windows scheduling.

The format guide lists decoding limits. The optional PAFPhotos-Codec-Sources.zip contains corresponding codec sources and licenses and is not required to run the app.
