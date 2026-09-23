# Verified Windows release publication

Release automation is part of the product's supply chain. It is tested independently from media decoding and does not certify trained-model output, hardware performance or signing.

## Selection and evidence

Only a completed successful push workflow from this repository's current `main` can start publication. The release script requires the latest `browser-build`, `windows-build` and `vlc-player` checks and binds each to its actual workflow-run ID. A queued/failed newer run, a mismatching check, a fork run, an old revision or a differently named workflow cannot supply the release.

The native job includes `build-info.json`: source commit/tree, repository, version, workflow ID and attempt, and explicit unsigned/unvalidated-model flags. It is checksummed along with the installer, portable archive, source archives, reports and screenshots.

The publisher downloads the exact native artifact archive and verifies GitHub's SHA-256 digest before extraction. Extraction rejects traversal, duplicate paths, links and oversized archives. It then validates the build identity, every listed checksum, complete release-payload coverage and the checksums of every file inside the portable package. Both the player and the fixed native worker must be present.

## Publication transaction

Files are uploaded to a **draft prerelease** first. Every remote asset digest must match the tested local file. The publisher rechecks the current main revision and workflow attempt immediately before making the release visible. Existing published versions are never overwritten. An interrupted draft can be resumed only for the same revision and identical already-uploaded content; conflicting content stops the job for review.

The standalone Windows installer remains unsigned. Its optional VLC prerequisite is retrieved from VideoLAN and checked separately by the installer. This project neither redistributes NVIDIA model data nor turns educational acknowledgment into vendor or model approval.

## Tests

```sh
python -m unittest discover -s tests/release -v
```

The tests cover stale/failed runs, forged check origins, run/check mismatch, expired/ambiguous artifacts, missing digests, archive traversal/links/duplicates, changed installer bytes, wrong build provenance, portable-package checksum gaps and changed/incomplete remote assets. They run in the required browser-build job; they do not contact GitHub or mutate a release.
