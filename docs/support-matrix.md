# Support matrix — 0.7.0

| Feature | Current scope |
|---|---|
| Native VLC player | Windows x64; compatible official libVLC3.0.24+ in the 3.0 series. No VLC4 ABI support |
| Native model research | Opt-in validated local data, 71-block pinned format, explicit acknowledgment; no bundled model or trained-model certification |
| Neural preview | Independent muted decoder, opaque SDR RGBA,33–512 pixel dimensions, CPU transfer. Not audio synchronized, temporal, HDR or4K |
| RTX VSR | Optional VLC driver request; not DLSS and not proof of driver execution |
| Ordinary video features | Depend on input and installed VLC plugins; full original VLC is launched separately for omitted advanced controls |
| Installer | Unsigned per-user Windows setup and portable ZIP; optional verified online VLC prerequisite or matching cached archive |
| Other native OS | No macOS/Linux installers; portable test compilation does not imply application support |
| Browser extension | Chrome/Edge MV3, explicit supported media/capture. Browser model approval policy unchanged |
| Browser graphics | Actual software-rendered fixtures in CI are not physical-GPU performance tests |
| UI | DPI-aware native layout, controls reflow on narrow windows; real Windows and browser test reports accompany releases |
| Desktop capture | Existing separate SDR preview companion, not a DLSS desktop overlay |

Read the exact release reports. No performance number, trained-model parity, complete codec matrix, real audio-device, long-session, HDR or production signing certification is claimed without corresponding evidence.
