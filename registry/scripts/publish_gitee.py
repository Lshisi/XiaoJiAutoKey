"""Publish the same signed catalog or immutable plugin artifacts to Gitee."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
import uuid

from sources import download, repository


class NoRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args):
        return None


class Gitee:
    def __init__(self, address: str, token: str):
        host, owner, name = repository(address)
        if host != "gitee":
            raise ValueError("发布目标必须是 Gitee 仓库")
        self.base = f"https://gitee.com/api/v5/repos/{owner}/{name}"
        self.token = token
        self.opener = urllib.request.build_opener(NoRedirects())

    def request(self, path: str, method="GET", fields=None, file=None, missing=False):
        address = self.base + path
        values = {**(fields or {}), "access_token": self.token}
        headers = {"User-Agent": "XiaoJiPluginRegistry/1", "Accept": "application/json"}
        body = None
        if method in ("GET", "DELETE"):
            address += "?" + urllib.parse.urlencode(values)
        elif file is None:
            body = urllib.parse.urlencode(values).encode()
            headers["Content-Type"] = "application/x-www-form-urlencoded"
        else:
            boundary = "xiaoji-" + uuid.uuid4().hex
            parts = []
            for key, value in values.items():
                parts.append(f'--{boundary}\r\nContent-Disposition: form-data; name="{key}"\r\n\r\n{value}\r\n'.encode())
            parts.append(f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="{file.name}"\r\nContent-Type: application/octet-stream\r\n\r\n'.encode())
            parts.extend([file.read_bytes(), f"\r\n--{boundary}--\r\n".encode()])
            body = b"".join(parts)
            headers["Content-Type"] = "multipart/form-data; boundary=" + boundary
        try:
            with self.opener.open(urllib.request.Request(address, data=body, headers=headers, method=method), timeout=120) as response:
                payload = response.read(4 * 1024 * 1024 + 1)
                if len(payload) > 4 * 1024 * 1024:
                    raise ValueError("Gitee 发布响应超过限制")
                return json.loads(payload) if payload else None
        except urllib.error.HTTPError as error:
            if missing and error.code == 404:
                return None
            details = []
            try:
                document = json.loads(error.read(65536))
                if isinstance(document, dict):
                    for key in ("message", "error", "error_description"):
                        value = document.get(key)
                        if isinstance(value, str):
                            details.append(value.replace(self.token, "[redacted]")[:240])
            except (ValueError, OSError):
                pass
            explanation = "；".join(details)
            raise ValueError(f"Gitee 发布 API {method} {path} 返回 HTTP {error.code}" + ("：" + explanation if explanation else "；未输出认证参数")) from None
        except urllib.error.URLError:
            raise ValueError("Gitee 发布连接失败，GitHub 附件和本地文件仍保留") from None


def publish(args):
    token = os.environ.get("XIAOJI_GITEE_TOKEN", "")
    if not token:
        print("::notice::Gitee mirror skipped: configure XIAOJI_GITEE_TOKEN to enable dual-platform publication.")
        return
    if args.tag != "catalog" and not re.fullmatch(r"plugins/(?:driver-(?:dd|xiaoji|usb)|page-calendar|pet-xiaoji)/v\d+\.\d+\.\d+", args.tag):
        raise ValueError("不支持的官方插件发行 tag")
    if args.replace_catalog and args.tag != "catalog":
        raise ValueError("仅目录元数据允许替换，插件附件不可覆盖")
    files = [Path(name) for name in args.files]
    for file in files:
        if not file.is_file() or not re.fullmatch(r"[A-Za-z0-9._-]+", file.name) or file.stat().st_size > 64 * 1024 * 1024:
            raise ValueError("发布文件缺失、文件名无效或超过 64 MB")
        if args.tag == "catalog" and file.name not in ("catalog.json", "public-key.txt", "collection-errors.json"):
            raise ValueError("目录发行版只允许发布约定的元数据文件")
    api = Gitee(args.repository, token)
    release = None
    # Query by list so plugin tags containing '/' do not depend on encoded-slash routing.
    for page in range(1, 11):
        rows = api.request("/releases", fields={"page": page, "per_page": 100})
        if not isinstance(rows, list) or any(not isinstance(row, dict) for row in rows):
            raise ValueError("Gitee 发行版清单无效")
        release = next((row for row in rows if row.get("tag_name") == args.tag), None)
        if release is not None or len(rows) < 100:
            break
    else:
        raise ValueError("Gitee 发行版数量超出发布器处理范围")
    if release is None:
        release = api.request("/releases", "POST", {"tag_name": args.tag, "target_commitish": args.target,
            "name": args.tag, "body": "XiaoJi plugin artifacts. Review status is supplied by the signed catalog.", "prerelease": "false"})
    if not isinstance(release, dict) or release.get("tag_name") != args.tag or not isinstance(release.get("id"), int):
        raise ValueError("Gitee 未返回匹配的发行版")
    target = release.get("target_commitish", "")
    if args.tag != "catalog" and re.fullmatch(r"[0-9a-f]{40}", target) and re.fullmatch(r"[0-9a-f]{40}", args.target) and target != args.target:
        raise ValueError("Gitee 插件 tag 与本次发布提交不一致，请先同步源提交")
    path = f"/releases/{release['id']}/attach_files"
    attachments = []
    for page in range(1, 11):
        rows = api.request(path, fields={"page": page, "per_page": 100})
        if not isinstance(rows, list) or any(not isinstance(row, dict) for row in rows):
            raise ValueError("Gitee 附件清单无效")
        attachments.extend(rows)
        if len(rows) < 100:
            break
    else:
        raise ValueError("Gitee 附件数量超出发布器处理范围")
    for file in files:
        existing = next((row for row in attachments if row.get("name") == file.name), None)
        if existing:
            address = existing.get("browser_download_url", "")
            if address and hashlib.sha256(download(address, 64 * 1024 * 1024)).digest() == hashlib.sha256(file.read_bytes()).digest():
                print(f"Gitee kept identical artifact: {file.name}")
                continue
            if not args.replace_catalog:
                raise ValueError(f"Gitee 已存在不同内容的 {file.name}，拒绝覆盖同版本附件")
            api.request(path + "/" + str(existing["id"]), "DELETE")
        api.request(path, "POST", file=file)
        print(f"Gitee published: {args.tag}/{file.name}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", default="https://gitee.com/fourteen-fourteen/xiao-ji-auto-key")
    parser.add_argument("--tag", required=True)
    parser.add_argument("--target", default=os.environ.get("GITHUB_SHA", "master"))
    parser.add_argument("--replace-catalog", action="store_true")
    parser.add_argument("files", nargs="+")
    publish(parser.parse_args())


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
