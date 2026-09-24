# Library manager — third-party notices

Shiny manager, reference backends and integration code are original MIT-licensed code. The two packaged `shiny_spatial.dll` variants implement conventional spatial sharpening; they contain no NVIDIA runtime, neural network or model weights. DLSS Swapper was an architecture reference only; no GPL application code was copied.

## SQLite 3.53.4

SQLite is in the public domain: https://sqlite.org/copyright.html . Official amalgamation source: https://sqlite.org/2026/sqlite-amalgamation-3530400.zip . The build authenticates SHA3-256 `628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e` before extracting only `sqlite3.c` and `sqlite3.h`.

Windows statically links SQLite with JSON1, defensive configuration, extension loading omitted, parameterized statements and explicit size limits. System SQLite/OpenSSL are used only by the portable Linux test harness and are not redistributed in the Windows package. Windows hashing uses BCrypt, not copied cryptographic code.

The integrated player still has its separate libVLC/OpenDLSS-NR/volk/Vulkan notices. The manager does not change those terms or grant rights to imported proprietary libraries. User-imported candidates are not included in application distribution. A valid file signature is not a license or compatibility determination.
