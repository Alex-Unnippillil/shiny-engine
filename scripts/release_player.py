"""Publish only verified artifacts from the exact current main revision.

No runtime dependencies beyond Python and the authenticated GitHub CLI. The
workflow checks out trusted main before invoking this script. Tests exercise the
pure policy functions without GitHub credentials or network access.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tempfile
from typing import Any
from zipfile import ZipFile

REQUIRED = {
    "browser-build": ".github/workflows/web.yml",
    "windows-build": ".github/workflows/native.yml",
    "vlc-player": ".github/workflows/vlc-player.yml",
    "library-audit (ubuntu-latest)": ".github/workflows/library-audit.yml",
    "library-audit (windows-latest)": ".github/workflows/library-audit.yml",
}
MAX_ARCHIVE_BYTES = 128 * 1024 * 1024
MAX_ARCHIVE_FILES = 512


class ReleaseError(RuntimeError):
    """A release precondition failed. Do not publish or overwrite artifacts."""


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise ReleaseError(reason)


def select_runs(runs: list[dict], checks: list[dict], sha: str, repository_id: int) -> dict[str, dict]:
    """Select latest exact-main runs, never an older green run after a failure."""
    selected: dict[str, dict] = {}
    for name, path in REQUIRED.items():
        candidates = [r for r in runs if r.get("path") == path and r.get("head_sha") == sha
                      and r.get("head_branch") == "main" and r.get("event") == "push"
                      and r.get("head_repository", {}).get("id") == repository_id]
        require(bool(candidates), f"Missing exact-main workflow: {name}")
        run = max(candidates, key=lambda r: r["id"])
        require(run.get("status") == "completed" and run.get("conclusion") == "success",
                f"Latest {name} workflow is not successful")
        matching = [c for c in checks if c.get("name") == name and c.get("head_sha") == sha
                    and c.get("app", {}).get("slug") == "github-actions"]
        require(bool(matching), f"Missing GitHub Actions check: {name}")
        check = max(matching, key=lambda c: c["id"])
        require(check.get("status") == "completed" and check.get("conclusion") == "success",
                f"Latest {name} check is not successful")
        require(f"/actions/runs/{run['id']}/job/" in check.get("html_url", ""),
                f"{name} check is not from the selected workflow run")
        selected[name] = run
    return selected


def choose_artifact(artifacts: list[dict], run: dict, sha: str) -> dict:
    found = [a for a in artifacts if a.get("name") == "vlc-player-and-evidence"]
    require(len(found) == 1, "Require one unambiguous native artifact")
    artifact = found[0]
    metadata = artifact.get("workflow_run", {})
    require(not artifact.get("expired", True) and metadata.get("id") == run["id"]
            and metadata.get("head_sha") == sha and metadata.get("head_branch") == "main",
            "Artifact does not belong to the selected current-main run")
    require(bool(re.fullmatch(r"sha256:[0-9a-f]{64}", artifact.get("digest", ""))),
            "GitHub artifact SHA-256 digest is required")
    require(0 < artifact.get("size_in_bytes", 0) <= MAX_ARCHIVE_BYTES, "Artifact size is invalid")
    return artifact


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def unpack(archive: Path, destination: Path) -> None:
    """Reject escaping paths, duplicate names, links and decompression bombs."""
    destination.mkdir(parents=True, exist_ok=True)
    with ZipFile(archive) as zipped:
        entries = zipped.infolist()
        require(len(entries) <= MAX_ARCHIVE_FILES, "Too many archive members")
        require(sum(i.file_size for i in entries) <= MAX_ARCHIVE_BYTES, "Archive expansion exceeds budget")
        names: set[str] = set()
        for item in entries:
            p = PurePosixPath(item.filename)
            require(bool(p.parts) and not p.is_absolute() and ".." not in p.parts
                    and "\\" not in item.filename and ":" not in item.filename
                    and not any(ord(c) < 32 for c in item.filename), "Unsafe archive path")
            require(item.filename.casefold() not in names, "Duplicate archive member")
            names.add(item.filename.casefold())
            mode = item.external_attr >> 16
            require(not stat.S_ISLNK(mode), "Archive links are forbidden")
            require((destination / item.filename).resolve().is_relative_to(destination.resolve()), "Archive escaped destination")
        zipped.extractall(destination)


def checksums(folder: Path, filename: str = "SHA256SUMS.txt") -> dict[str, str]:
    path = folder / filename
    require(path.is_file() and path.stat().st_size < 128 * 1024, "Checksum list missing or oversized")
    result: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9][A-Za-z0-9._/-]*)", line)
        require(match is not None, "Malformed checksum entry")
        assert match is not None
        expected, name = match.groups()
        p = PurePosixPath(name)
        require(".." not in p.parts and not p.is_absolute() and name != filename, "Unsafe checksum path")
        require(name not in result, "Duplicate checksum entry")
        target = folder / name
        require(target.is_file() and not target.is_symlink() and digest(target) == expected,
                f"Missing or modified checksummed file: {name}")
        result[name] = expected
    require(bool(result), "Empty checksum list")
    return result


def verify_bundle(folder: Path, version: str, sha: str, run: dict, repository: str) -> dict[str, str]:
    sums = checksums(folder)
    mandatory = {
        f"ShinyPlayer-{version}-Windows-x64-Setup.exe",
        f"ShinyPlayer-{version}-Windows-x64-Portable.zip",
        f"ShinyPlayer-{version}-Source.zip", "OpenDLSS-NR-Pinned-Source.zip",
        "build-info.json", "playback-report.json", "model-guard-report.json",
        "ui-workbench-report.json", "installer-report.json", "player-desktop.png",
        "player-research-mode.png", "player-neural-workbench.png", "player-cinema.png",
    }
    if tuple(map(int, version.split('.'))) >= (0, 8, 0):
        mandatory.add("managed-playback-report.json")
    require(mandatory <= sums.keys(), "Release bundle lacks required binaries or evidence")
    payloads = {p.name for p in folder.iterdir() if p.is_file() and p.suffix in {".exe", ".zip", ".png", ".json"}}
    require(payloads == sums.keys(), "All release payloads must be checksummed; no unexpected paths")
    info = json.loads((folder / "build-info.json").read_text(encoding="utf-8-sig"))
    require(info.get("schema") == 1 and info.get("version") == version and info.get("source_sha") == sha
            and info.get("repository") == repository and info.get("workflow_run_id") == run["id"]
            and info.get("workflow_run_attempt") == run["run_attempt"], "Build provenance does not match selected run")
    for name in payloads:
        if name.endswith(".png"):
            require((folder / name).read_bytes().startswith(b"\x89PNG\r\n\x1a\n"), f"Invalid screenshot: {name}")
        elif name.endswith(".exe"):
            require((folder / name).read_bytes().startswith(b"MZ"), "Installer is not a Windows executable")
        elif name.endswith(".json"):
            json.loads((folder / name).read_text(encoding="utf-8-sig"))
    with tempfile.TemporaryDirectory() as temp:
        portable = Path(temp) / "portable"
        unpack(folder / f"ShinyPlayer-{version}-Windows-x64-Portable.zip", portable)
        internal = checksums(portable)
        actual = {p.relative_to(portable).as_posix() for p in portable.rglob("*") if p.is_file()}
        require(actual == internal.keys() | {"SHA256SUMS.txt"}, "Portable package checksum coverage is incomplete")
        require({"ShinyVlcPlayer.exe", "nr/ShinyNrWorker.exe"} <= internal.keys(), "Player or native worker missing")
        if tuple(map(int, version.split('.'))) >= (0, 8, 0):
            require({"ShinyLibraryManager.exe", "ShinyLibraryManagerCli.exe", "ShinyEnhancementWorker.exe",
                     "library-bundles/1.0.0/shiny_spatial.dll", "library-bundles/1.1.0/shiny_spatial.dll"} <= internal.keys(),
                    "Managed enhancement package incomplete")
            managed = json.loads((folder / "managed-playback-report.json").read_text())
            require(managed.get("versionsProcessed") == 2 and managed.get("originalPlaybackPreserved") is True
                    and managed.get("rollback") is True and managed.get("dlss") is False,
                    "Managed reference playback evidence missing or misleading")
    return {**sums, "SHA256SUMS.txt": digest(folder / "SHA256SUMS.txt")}


def gh(*args: str) -> str:
    return subprocess.run(["gh", *args], text=True, capture_output=True, check=True).stdout


def api(path: str, *args: str, optional: bool = False) -> Any:
    try:
        return json.loads(gh("api", path, *args))
    except subprocess.CalledProcessError as error:
        if optional and "HTTP 404" in error.stderr:
            return None
        raise


def paged(path: str, key: str) -> list[dict]:
    pages = api(path + ("&" if "?" in path else "?") + "per_page=100", "--paginate", "--slurp")
    return [entry for page in pages for entry in page[key]]


def find_release(repository: str, tag: str) -> dict | None:
    # The tag endpoint returns published releases only. Authenticated list
    # releases includes drafts and supports safe resumption after interruption.
    pages = api(f"repos/{repository}/releases?per_page=100", "--paginate", "--slurp")
    matches = [release for page in pages for release in page if release.get("tag_name") == tag]
    require(len(matches) <= 1, "Ambiguous versioned release")
    return matches[0] if matches else None


def remote_assets(release: dict, expected: dict[str, str], complete: bool) -> set[str]:
    assets = release.get("assets", [])
    require(len(assets) == len({a["name"] for a in assets}), "Duplicate remote release asset")
    actual = {a["name"]: a.get("digest") for a in assets}
    require(actual.keys() <= expected.keys(), "Unexpected assets in versioned release")
    for name, value in actual.items():
        require(value == "sha256:" + expected[name], f"Remote asset differs from tested file: {name}")
    if complete:
        require(actual.keys() == expected.keys(), "Release is missing tested assets")
    return set(actual)


def publish() -> None:
    repo, sha = os.environ["GH_REPO"], os.environ["RELEASE_SHA"]
    require(bool(re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo)), "Invalid repository")
    require(bool(re.fullmatch(r"[0-9a-f]{40}", sha)), "Invalid commit")
    require(gh("api", f"repos/{repo}/git/ref/heads/main", "--jq", ".object.sha").strip() == sha, "Main has moved")
    require(subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip() == sha, "Checkout differs from release revision")
    version = json.loads(Path("package.json").read_text())["version"]
    require(bool(re.fullmatch(r"\d+\.\d+\.\d+", version)), "Invalid version")
    repository_id = api(f"repos/{repo}")["id"]

    def current_runs() -> dict[str, dict]:
        require(api(f"repos/{repo}/git/ref/heads/main")["object"]["sha"] == sha, "Main moved before publication")
        return select_runs(paged(f"repos/{repo}/actions/runs?head_sha={sha}&event=push", "workflow_runs"),
                           paged(f"repos/{repo}/commits/{sha}/check-runs", "check_runs"), sha, repository_id)

    # workflow_run fires once per completing suite. Earlier completions are a
    # safe no-op; the final successful completion will attempt publication.
    try:
        selected = current_runs()
    except ReleaseError as error:
        print(f"Not ready; no release changes made: {error}")
        return
    run = selected["vlc-player"]
    artifact = choose_artifact(paged(f"repos/{repo}/actions/runs/{run['id']}/artifacts", "artifacts"), run, sha)
    with tempfile.TemporaryDirectory(prefix="shiny-release-") as temp:
        archive, folder = Path(temp) / "artifact.zip", Path(temp) / "assets"
        with archive.open("wb") as out:
            subprocess.run(["gh", "api", f"repos/{repo}/actions/artifacts/{artifact['id']}/zip"], stdout=out, check=True)
        require(digest(archive) == artifact["digest"].removeprefix("sha256:"), "Downloaded artifact differs from GitHub digest")
        unpack(archive, folder)
        sums = verify_bundle(folder, version, sha, run, repo)
        tag = f"v{version}"
        release = find_release(repo, tag)
        if release:
            require(release.get("target_commitish") == sha and release.get("prerelease") is True,
                    "Existing version belongs to a different revision or release channel")
            if not release["draft"]:
                remote_assets(release, sums, complete=True)
                print(f"{tag} is already published with identical verified assets; no overwrite.")
                return
        else:
            tag_ref = api(f"repos/{repo}/git/ref/tags/{tag}", optional=True)
            if tag_ref:
                require(tag_ref["object"].get("type") == "commit" and tag_ref["object"].get("sha") == sha,
                        "Existing tag conflicts with this release")
            notes = Path(temp) / "notes.md"
            notes.write_text(Path(f"docs/release-{version.rsplit('.', 1)[0]}.md").read_text()
                             + f"\nValidated main commit: `{sha}`\n", encoding="utf-8")
            gh("release", "create", tag, "--target", sha, "--draft", "--prerelease", "--repo", repo,
               "--title", f"Shiny Player {version} — Windows player and local NR research", "--notes-file", str(notes))
            release = find_release(repo, tag)
            require(release is not None and release.get("draft") is True, "New draft release could not be resolved")
        present = remote_assets(release, sums, complete=False)
        for name in sorted(sums.keys() - present):
            gh("release", "upload", tag, str(folder / name), "--repo", repo)
        release = api(f"repos/{repo}/releases/{release['id']}")
        remote_assets(release, sums, complete=True)
        latest = current_runs()["vlc-player"]
        require((latest["id"], latest["run_attempt"]) == (run["id"], run["run_attempt"]),
                "Native run changed during publication; draft retained")
        api(f"repos/{repo}/releases/{release['id']}", "--method", "PATCH", "-F", "draft=false",
            "-F", "prerelease=true", "-f", "make_latest=false")
        remote_assets(api(f"repos/{repo}/releases/{release['id']}"), sums, complete=True)
        print(f"Published {tag} from {sha}: {len(sums)} verified assets.")


if __name__ == "__main__":
    publish()
