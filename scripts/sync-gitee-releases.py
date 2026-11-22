"""Copy published Gitee releases to GitHub without rebuilding the application."""

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlencode, urlsplit
from urllib.request import Request, urlopen
import zipfile


GITEE_REPOSITORY = "fourteen-fourteen/xiao-ji-auto-key"
GITHUB_REPOSITORY = "Lshisi/XiaoJiAutoKey"
USER_AGENT = "XiaoJiAutoKey-release-sync"
TAG_PATTERN = re.compile(r"v\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?")


def request(url, *, token=None, method="GET", data=None, content_type=None):
    headers = {"User-Agent": USER_AGENT, "Accept": "application/json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
        headers["X-GitHub-Api-Version"] = "2022-11-28"
    if content_type:
        headers["Content-Type"] = content_type
    for attempt in range(3):
        try:
            return urlopen(
                Request(url, data=data, headers=headers, method=method), timeout=60
            )
        except HTTPError as error:
            if method != "GET" or error.code not in (429, 500, 502, 503, 504):
                raise
            if attempt == 2:
                raise
            error.close()
        except URLError:
            if method != "GET" or attempt == 2:
                raise
        time.sleep(2 ** attempt)


def github(path, token, *, method="GET", payload=None, allow_missing=False):
    data = None if payload is None else json.dumps(payload).encode("utf-8")
    try:
        with request(
            f"https://api.github.com/repos/{GITHUB_REPOSITORY}/{path}",
            token=token,
            method=method,
            data=data,
            content_type="application/json" if data is not None else None,
        ) as response:
            return json.load(response)
    except HTTPError as error:
        if allow_missing and error.code == 404:
            error.close()
            return None
        raise


def list_releases():
    releases = []
    page = 1
    while True:
        url = (
            f"https://gitee.com/api/v5/repos/{GITEE_REPOSITORY}/releases"
            f"?per_page=100&page={page}"
        )
        with request(url) as response:
            batch = json.load(response)
        if not isinstance(batch, list):
            raise ValueError("Gitee did not return a release list.")
        releases.extend(batch)
        if len(batch) < 100:
            return releases
        page += 1


def find_github_release(tag, token):
    # Listing includes drafts, so an interrupted first upload can be resumed.
    page = 1
    while True:
        batch = github(f"releases?per_page=100&page={page}", token)
        for release in batch:
            if release["tag_name"] == tag:
                return release
        if len(batch) < 100:
            return None
        page += 1


def release_assets(release):
    assets = []
    expected_path = f"/{GITEE_REPOSITORY}/releases/download/{release['tag_name']}/"
    for asset in release.get("assets") or []:
        url = asset.get("browser_download_url", "")
        parsed = urlsplit(url)
        # Gitee also lists source archives as assets; only copy uploaded zip files.
        if (
            parsed.scheme == "https"
            and parsed.netloc == "gitee.com"
            and parsed.path.startswith(expected_path)
            and parsed.path.lower().endswith(".zip")
        ):
            name = asset.get("name", "")
            if not name or re.search(r'[\\/:\x00-\x1f]', name) or name in (".", ".."):
                raise ValueError("A release attachment has an invalid filename.")
            assets.append((name, url))
    return assets


def verify_tag(release, token):
    tag = release["tag_name"]
    reference = github(f"git/ref/tags/{quote(tag, safe='')}", token, allow_missing=True)
    if reference is None:
        raise ValueError(f"Push tag {tag} to GitHub before synchronizing its release.")
    target = reference["object"]
    for _ in range(8):
        if target["type"] == "commit":
            break
        if target["type"] != "tag":
            raise ValueError(f"GitHub tag {tag} does not point to a commit.")
        target = github(f"git/tags/{target['sha']}", token)["object"]
    else:
        raise ValueError(f"GitHub tag {tag} contains too many nested tags.")
    source_commit = release.get("target_commitish") or ""
    if re.fullmatch(r"[0-9a-fA-F]{40}", source_commit):
        if source_commit.lower() != target["sha"].lower():
            raise ValueError(f"Gitee and GitHub disagree on the commit for {tag}.")


def download_package(url, destination):
    with request(url) as response, destination.open("wb") as output:
        shutil.copyfileobj(response, output)
    with zipfile.ZipFile(destination) as archive:
        filenames = [entry.filename.replace("\\", "/").rsplit("/", 1)[-1].lower()
                     for entry in archive.infolist() if not entry.is_dir()]
        if "config.json" in filenames:
            raise ValueError("The release zip contains config.json and will not be published.")
        if "xiaojiautokey.exe" not in filenames:
            raise ValueError("The release zip does not contain XiaoJiAutoKey.exe.")
    with destination.open("rb") as package:
        return hashlib.file_digest(package, "sha256").hexdigest()


def upload_package(release_id, name, package, digest, token):
    query = urlencode({"name": name})
    url = (
        f"https://uploads.github.com/repos/{GITHUB_REPOSITORY}"
        f"/releases/{release_id}/assets?{query}"
    )
    with request(
        url, token=token, method="POST", data=package.read_bytes(),
        content_type="application/zip",
    ) as response:
        uploaded = json.load(response)
    if uploaded["size"] != package.stat().st_size:
        raise ValueError(f"GitHub reported an unexpected size for {name}.")
    if uploaded.get("digest") and uploaded["digest"] != f"sha256:{digest}":
        raise ValueError(f"GitHub reported an unexpected SHA-256 for {name}.")
    print(f"Uploaded {name} (SHA-256: {digest})")


def sync_release(source, assets, token):
    tag = source["tag_name"]
    verify_tag(source, token)
    metadata = {
        "tag_name": tag,
        "name": source.get("name") or tag,
        "body": source.get("body") or "",
        "prerelease": bool(source.get("prerelease")),
    }
    target = find_github_release(tag, token)
    if target is None:
        # Keep a new release private until all its packages have been uploaded.
        target = github("releases", token, method="POST", payload={**metadata, "draft": True})
    existing_assets = []
    page = 1
    while True:
        batch = github(f"releases/{target['id']}/assets?per_page=100&page={page}", token)
        existing_assets.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    asset_names = {asset["name"] for asset in existing_assets if asset.get("state") == "uploaded"}
    with tempfile.TemporaryDirectory(prefix="xiaoji-release-sync-") as temporary:
        for name, url in assets:
            if name in asset_names:
                continue
            package = Path(temporary) / name
            digest = download_package(url, package)
            upload_package(target["id"], name, package, digest, token)
            asset_names.add(name)
    if target.get("draft") or any(target.get(key) != value for key, value in metadata.items()):
        published_metadata = {**metadata, "draft": False}
        if not metadata["prerelease"]:
            published_metadata["make_latest"] = "legacy"
        target = github(
            f"releases/{target['id']}", token, method="PATCH",
            payload=published_metadata,
        )
    print(f"Synchronized {tag}: {target['html_url']}")
    return f"- [{tag}]({target['html_url']})"


def main():
    token = os.environ.get("GITHUB_TOKEN", "")
    if not token:
        raise ValueError("GITHUB_TOKEN is required; run this script in GitHub Actions.")
    if os.environ.get("GITHUB_REPOSITORY", GITHUB_REPOSITORY) != GITHUB_REPOSITORY:
        raise ValueError("This workflow only synchronizes Lshisi/XiaoJiAutoKey.")
    selected_tag = os.environ.get("SYNC_TAG", "").strip()
    if selected_tag and not TAG_PATTERN.fullmatch(selected_tag):
        raise ValueError("SYNC_TAG must be a version tag such as vx.y.z.")
    releases = [release for release in list_releases()
                if not release.get("draft")
                and TAG_PATTERN.fullmatch(release.get("tag_name") or "")
                and (not selected_tag or release["tag_name"] == selected_tag)]
    if selected_tag and not releases:
        raise ValueError(f"Gitee has no published release for {selected_tag}.")
    releases.sort(key=lambda release: tuple(map(int, release["tag_name"][1:].split("-")[0].split("."))))
    summary = []
    for release in releases:
        assets = release_assets(release)
        if not assets:
            if selected_tag:
                raise ValueError(f"{selected_tag} has no uploaded zip attachment on Gitee.")
            print(f"Skipped {release['tag_name']}: no uploaded zip attachment.")
            continue
        summary.append(sync_release(release, assets, token))
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary_path:
        with open(summary_path, "a", encoding="utf-8") as output:
            output.write("### Gitee releases synchronized to GitHub\n\n")
            output.write("\n".join(summary) if summary else "No published zip packages to synchronize.")
            output.write("\n")


if __name__ == "__main__":
    try:
        main()
    except (HTTPError, URLError, ValueError, KeyError, OSError, zipfile.BadZipFile) as error:
        print(f"Release synchronization failed: {error}", file=sys.stderr)
        sys.exit(1)
