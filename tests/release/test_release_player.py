"""No-network regression checks for production release integrity."""
from copy import deepcopy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from zipfile import ZipFile, ZipInfo

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("release_player", ROOT / "scripts/release_player.py")
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)
SHA = "a" * 40
REPO = "owner/player"


def sample():
    runs, checks = [], []
    for index, (name, path) in enumerate(r.REQUIRED.items(), start=1):
        runs.append(dict(id=index, path=path, head_sha=SHA, head_branch="main", event="push",
                         head_repository={"id": 42}, status="completed", conclusion="success", run_attempt=1))
        checks.append(dict(id=100+index, name=name, head_sha=SHA, app={"slug": "github-actions"},
                           html_url=f"https://github.com/{REPO}/actions/runs/{index}/job/{100+index}",
                           status="completed", conclusion="success"))
    return runs, checks


class ReleasePolicyTests(unittest.TestCase):
    def test_exact_main_success(self):
        runs, checks = sample()
        self.assertEqual(set(r.select_runs(runs, checks, SHA, 42)), set(r.REQUIRED))

    def test_required_run_scopes(self):
        for field, value in [("head_sha", "b"*40), ("head_branch", "feature"), ("event", "pull_request"),
                             ("head_repository", {"id": 99}), ("path", ".github/workflows/spoof.yml")]:
            with self.subTest(field=field):
                runs, checks = sample()
                runs[0][field] = value
                with self.assertRaises(r.ReleaseError): r.select_runs(runs, checks, SHA, 42)

    def test_newer_failed_or_pending_run_cannot_fall_back_to_green(self):
        for state in ["failure", None, "cancelled"]:
            runs, checks = sample()
            newer = {**runs[-1], "id": 99, "conclusion": state}
            runs.append(newer)
            with self.assertRaises(r.ReleaseError): r.select_runs(runs, checks, SHA, 42)

    def test_latest_check_and_run_must_agree(self):
        runs, checks = sample()
        for replacement in [{"id":999,"conclusion":"failure"}, {"id":999,"html_url":"https://github.com/x/actions/runs/99/job/123"}]:
            with self.assertRaises(r.ReleaseError):
                r.select_runs(runs, checks+[{**checks[-1], **replacement}], SHA, 42)

    def test_third_party_success_check_is_not_trusted(self):
        runs, checks = sample()
        checks[0]["app"]["slug"]="spoof"
        with self.assertRaises(r.ReleaseError): r.select_runs(runs, checks, SHA, 42)

    def test_artifact_identity_and_digest_required(self):
        run = sample()[0][-1]
        good = dict(id=7, name="vlc-player-and-evidence", expired=False, size_in_bytes=123,
                    digest="sha256:"+"b"*64, workflow_run=dict(id=run["id"], head_sha=SHA, head_branch="main"))
        self.assertEqual(r.choose_artifact([good],run,SHA)["id"],7)
        for patch in [{"expired":True}, {"digest":""}, {"workflow_run":{"id":0}}, {"size_in_bytes":0}]:
            with self.assertRaises(r.ReleaseError): r.choose_artifact([{**good, **patch}],run,SHA)
        with self.assertRaises(r.ReleaseError): r.choose_artifact([good,good],run,SHA)

    def test_archive_traversal_links_duplicates_and_budget(self):
        with tempfile.TemporaryDirectory() as temp:
            archive=Path(temp)/"a.zip"
            for name in ["../outside", "/absolute", "C:/drive", "nested\\file"]:
                with ZipFile(archive,"w") as z: z.writestr(name,b"data")
                with self.assertRaises(r.ReleaseError): r.unpack(archive,Path(temp)/"out")
            link=ZipInfo("link");link.external_attr=(0o120777<<16)
            with ZipFile(archive,"w") as z:z.writestr(link,"target")
            with self.assertRaises(r.ReleaseError):r.unpack(archive,Path(temp)/"out")
            with ZipFile(archive,"w") as z:
                z.writestr("File",b"a");z.writestr("file",b"b")
            with self.assertRaises(r.ReleaseError):r.unpack(archive,Path(temp)/"out")
            with ZipFile(archive,"w") as z:
                for i in range(r.MAX_ARCHIVE_FILES+1):z.writestr(f"f{i}",b"")
            with self.assertRaises(r.ReleaseError):r.unpack(archive,Path(temp)/"out")

    def test_checksums_reject_altered_installer_or_unsafe_list(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);p=folder/"Setup.exe";p.write_bytes(b"MZfixture")
            sums=folder/"SHA256SUMS.txt"
            sums.write_text(r.digest(p)+"  Setup.exe\n")
            self.assertEqual(set(r.checksums(folder)), {"Setup.exe"})
            p.write_bytes(b"MZchanged")
            with self.assertRaises(r.ReleaseError):r.checksums(folder)
            for name in ["../outside", "SHA256SUMS.txt", "/absolute"]:
                sums.write_text("a"*64+"  "+name+"\n")
                with self.assertRaises(r.ReleaseError):r.checksums(folder)

    def test_complete_bundle_provenance_and_portable_contents(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            run = sample()[0][-1]
            version = "0.7.0"
            names = [f"ShinyPlayer-{version}-Windows-x64-Setup.exe", f"ShinyPlayer-{version}-Source.zip",
                     "OpenDLSS-NR-Pinned-Source.zip", "playback-report.json", "model-guard-report.json",
                     "ui-workbench-report.json", "installer-report.json", "player-desktop.png",
                     "player-research-mode.png", "player-neural-workbench.png", "player-cinema.png"]
            for name in names:
                data = b"MZfixture" if name.endswith(".exe") else b"\x89PNG\r\n\x1a\nfixture" if name.endswith(".png") else b"{}"
                (folder / name).write_bytes(data)
            info = dict(schema=1, version=version, source_sha=SHA, repository=REPO,
                        workflow_run_id=run["id"], workflow_run_attempt=run["run_attempt"])
            (folder / "build-info.json").write_text(json.dumps(info))
            portable = folder / f"ShinyPlayer-{version}-Windows-x64-Portable.zip"
            content = {"ShinyVlcPlayer.exe": b"MZplayer", "nr/ShinyNrWorker.exe": b"MZworker"}
            inner = "".join(hashlib.sha256(data).hexdigest()+"  "+name+"\n" for name, data in content.items())
            with ZipFile(portable, "w") as z:
                for name, data in content.items(): z.writestr(name, data)
                z.writestr("SHA256SUMS.txt", inner)
            def write_sums():
                (folder / "SHA256SUMS.txt").write_text("".join(r.digest(p)+"  "+p.name+"\n"
                    for p in sorted(folder.iterdir()) if p.name != "SHA256SUMS.txt"))
            write_sums()
            verified = r.verify_bundle(folder, version, SHA, run, REPO)
            self.assertIn("SHA256SUMS.txt", verified)
            for patch in [{"source_sha": "f"*40}, {"workflow_run_id": 99}, {"workflow_run_attempt": 2}, {"version": "0.6.0"}]:
                (folder / "build-info.json").write_text(json.dumps({**info, **patch})); write_sums()
                with self.assertRaises(r.ReleaseError): r.verify_bundle(folder, version, SHA, run, REPO)
            (folder / "build-info.json").write_text(json.dumps(info)); write_sums()
            with ZipFile(portable, "a") as z: z.writestr("unlisted.exe", b"MZunexpected")
            write_sums()
            with self.assertRaises(r.ReleaseError): r.verify_bundle(folder, version, SHA, run, REPO)

    def test_remote_digests_and_complete_public_release(self):
        expected={"Setup.exe":"a"*64,"SHA256SUMS.txt":"b"*64}
        release={"assets":[{"name":"Setup.exe","digest":"sha256:"+"a"*64}]}
        self.assertEqual(r.remote_assets(release,expected,False), {"Setup.exe"})
        with self.assertRaises(r.ReleaseError):r.remote_assets(release,expected,True)
        release["assets"].append({"name":"SHA256SUMS.txt","digest":"sha256:"+"b"*64})
        r.remote_assets(release,expected,True)
        release["assets"][0]["digest"]="sha256:"+"c"*64
        with self.assertRaises(r.ReleaseError):r.remote_assets(release,expected,True)


if __name__ == "__main__":
    unittest.main()
