# README sample media

The README hero shows **the released Shiny Player 0.10.0 running on Windows**, playing a silent still-image demonstration of **“Cosmic Cliffs” in the Carina Nebula (NIRCam Image)**. The image's blue starscape and amber clouds replace the flat integration-test colors for presentation only. The application interface is captured as rendered, not recreated or composited over a stock image.

## Image credit and source

**Image: NASA, ESA, CSA, STScI.**

- [Original image and description](https://science.nasa.gov/asset/webb/cosmic-cliffs-in-the-carina-nebula-nircam-image/), released July 12, 2022.
- [NASA images and media usage guidelines](https://www.nasa.gov/nasa-brand-center/images-and-media/).

This is a published infrared composite, not natural-color footage. It is used as attributed sample media in application documentation. No affiliation or endorsement by NASA, ESA, CSA, STScI, VideoLAN or NVIDIA is implied. The repository's MIT license does not relicense agency names, marks or third-party media.

The 2000 × 1158 source is resized to 1280 × 742 with bicubic interpolation, saved at JPEG quality 96 and repeated for a ten-second silent MJPEG clip. No additional color grading, sharpening or AI enhancement is applied to the sample. Repetition is solely to exercise normal video decoding; it is not a telescope time-lapse.

## Capture provenance

`readme-cosmic-cliffs.json` records the exact released-player source commit, verified package digest, source image URL/digest, screenshot digest and native decoder diagnostics. The unmodified application's `--ui-smoke` path captures its Windows client area. Window size is set to 1440 × 900 before capture; Windows borders are not included. The captured pixels are not edited afterward.

The six original versioned release-test screenshots remain linked in the root README. The color-pattern test fixture, test assertions, release reports, installers and application source are unchanged by this media update. The sample is **not** before/after enhancement evidence, DLSS output, or hardware/perceptual-quality certification.

## Reproduce on Windows

Requires PowerShell, Python 3 and network access to the fixed GitHub release, VideoLAN archive and NASA image. Use a new temporary working directory; downloads of executable packages are checked against pinned SHA-256 values before extraction. Source media is recorded by digest and must be visually reviewed if regenerated.

```powershell
./docs/media/capture-readme.ps1 -Work "$env:TEMP/shiny-readme-capture" -Output "$PWD/docs/media"
```

Keep the credit with the image whenever reusing this screenshot. Inspect the new image and provenance before committing a replacement.
