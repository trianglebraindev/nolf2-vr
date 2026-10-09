DirectMusic headers (dmusici.h and companions)
==============================================

NOLF2's dynamic music runs on DirectMusic. Microsoft removed the DirectMusic
performance-layer headers (dmusici.h, dmplugin.h) from the DirectX SDK after
2007 and from the Windows SDK, but the DirectMusic runtime itself still ships
with Windows, so the engine's original music code still works once it can
compile.

These headers come from the Wine project (https://www.winehq.org), which
re-declares the same public COM interfaces. They are licensed under the
GNU LGPL 2.1 (see each file's header). The whole set is used together so
the declarations are consistent with each other.

Wine source: https://github.com/wine-mirror/wine/tree/master/include
Wine commit: 20315474eb2a129234d13a9d91376ec7ea752d4c (Thu Oct 8 22:30:24 2026 +0200)
