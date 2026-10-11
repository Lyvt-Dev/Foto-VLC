PAF-Photos 0.6.1 fixes the stray settings buttons and clipped sidebar text reported in 0.6.0.

- Settings and gallery controls now live inside a fixed native scrolling viewport. Its parent boundary prevents buttons from painting over the heading or toolbar, even while partially visible and moving.
- Collapsed navigation displays centered icons only. Expanded navigation still shows labels; accessible names and tooltips are retained in both modes.
- Scrolling avoids repeated updates to the search field and reuses drawing surfaces and control placement. Animation timers use the Windows minimum interval instead of a 16 ms request that can fall onto a slower timer cadence. Movement remains time based, follows Windows wheel preferences, and respects reduced motion.
- Shared control edges round consistently at fractional DPI scaling, including 125%, preventing a one-pixel overflow.

Download **setup.exe** for a per-user installation with codecs. Existing PAF-Photos 0.5.0 and newer installations can receive this release automatically: Settings → Check for updates → Install now, or close the app after the update finishes downloading. Preferences and original images are preserved.

**PAFPhotos-Windows-x64.zip** is the portable alternative; extract the complete ZIP. The standalone EXE includes fewer decoders. Builds are currently unsigned.

Validation includes displayed header pixels captured without a forced repaint, native viewport containment, icon-only navigation with accessible labels, real timer-driven scrolling, and rounded controls at 100/125/150/200 percent DPI. It also exercises five conversion encoders, codec images, installer migration and updater verification. Portable motion tests compare different frame intervals. Rendering time and timer cadence are measured on the test machine; perceived smoothness also depends on hardware and Windows scheduling.

The format guide lists decoding limits. The optional PAFPhotos-Codec-Sources.zip contains corresponding codec sources and licenses and is not required to run the app.
