"""Read public releases and publish a signed catalog. Never execute plugin code."""

from __future__ import annotations

import argparse
import base64
import hashlib
import io
import json
import os
from pathlib import Path
import re
import sys
import time
import zipfile

from sources import SourceHTTPError, VERSION, descriptor, discover, download, repository, validate_identity

ROOT = Path(__file__).resolve().parents[1]
MAX_PACKAGE_BYTES = 64 * 1024 * 1024
MAX_UNPACKED_BYTES = 256 * 1024 * 1024
MAX_DOCUMENT_BYTES = 4 * 1024 * 1024
PERMISSIONS = {"driver.provide", "device.identify", "device.serial", "ui.page", "engine.read",
    "engine.control", "settings.own", "log.write", "system.driver.manage", "runtime.background", "ui.window", "messaging.own"}


def string_list(value, maximum: int) -> bool:
    return isinstance(value, list) and len(value) <= maximum and all(isinstance(item, str) and 0 < len(item) <= 256 for item in value)


def relative_path(value) -> str:
    if not isinstance(value, str) or not value or len(value) > 256 or any(char in value for char in ':*?"<>|\0'):
        raise ValueError("包内路径无效")
    name = value.replace("\\", "/")
    for piece in name.split("/"):
        if piece in ("", ".", "..") or piece.endswith((".", " ")) or re.fullmatch(r"(?:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\..*)?", piece, re.IGNORECASE):
            raise ValueError("包内路径无效")
    return name


def read_json(path: Path, default=None):
    if not path.exists() and default is not None:
        return default
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".pending")
    temporary.write_text(json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def check_package(body: bytes, expected_id: str) -> dict:
    with zipfile.ZipFile(io.BytesIO(body)) as archive:
        entries = archive.infolist()
        if not entries or len(entries) > 4096 or sum(item.file_size for item in entries) > MAX_UNPACKED_BYTES:
            raise ValueError("包内容为空或超过解压限制")
        files = {}
        folded = set()
        for entry in entries:
            name = relative_path(entry.filename.rstrip("/\\"))
            if entry.file_size > MAX_PACKAGE_BYTES or entry.flag_bits & 1:
                raise ValueError("包包含过大或加密的文件")
            if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError("插件包不允许符号链接")
            if entry.is_dir():
                continue
            key = name.casefold()
            if key in folded:
                raise ValueError("包内有大小写冲突的重复路径")
            folded.add(key)
            files[name] = entry
        manifest_entry = files.get("plugin.json")
        if not manifest_entry or manifest_entry.file_size > 128 * 1024:
            raise ValueError("包根目录需要 plugin.json")
        manifest = json.loads(archive.read(manifest_entry).decode("utf-8-sig"))
        if not isinstance(manifest, dict) or manifest.get("id") != expected_id:
            raise ValueError("插件 ID 与登记不一致")
        validate_identity(manifest["id"])
        if manifest.get("schemaVersion") != 1 or manifest.get("apiVersion") != 1:
            raise ValueError("不支持的清单或 API 版本")
        if not isinstance(manifest.get("version"), str) or not VERSION.fullmatch(manifest["version"]):
            raise ValueError("需要正式的三段版本号")
        if not isinstance(manifest.get("minHostVersion"), str) or not VERSION.fullmatch(manifest["minHostVersion"]):
            raise ValueError("需要最低宿主版本")
        maximum = manifest.get("maxHostVersion", "")
        if not isinstance(maximum, str) or maximum and (not VERSION.fullmatch(maximum) or tuple(map(int, maximum.split("."))) < tuple(map(int, manifest["minHostVersion"].split(".")))):
            raise ValueError("最高宿主版本无效")
        for field, maximum in (("name", 80), ("author", 120), ("description", 2000)):
            if not isinstance(manifest.get(field), str) or not manifest[field].strip() or len(manifest[field]) > maximum:
                raise ValueError(f"{field} 格式无效")
        if not string_list(manifest.get("platforms"), 8) or "windows-x64" not in manifest["platforms"]:
            raise ValueError("需要声明 Windows x64 平台")
        permissions = manifest.get("permissions", [])
        if not string_list(permissions, 32) or any(item not in PERMISSIONS for item in permissions):
            raise ValueError("能力声明格式无效")
        resources = manifest.get("resources", [])
        if not string_list(resources, 32):
            raise ValueError("资源清单格式无效")
        for resource in resources:
            resource = relative_path(resource)
            if resource not in files and not any(name.startswith(resource + "/") for name in files):
                raise ValueError("资源不在包内")
        contributions = 0
        for field, maximum in (("drivers", 16), ("pages", 8)):
            rows = manifest.get(field, [])
            if not isinstance(rows, list) or len(rows) > maximum:
                raise ValueError(f"{field} 格式无效")
            known_ids = set()
            for row in rows:
                if not isinstance(row, dict) or not isinstance(row.get("id"), str) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,47}", row["id"]) or row["id"] in known_ids:
                    raise ValueError("提供方或页面标识无效")
                known_ids.add(row["id"])
                title = row.get("title" if field == "pages" else "name")
                if not isinstance(title, str) or not title.strip() or len(title) > 80:
                    raise ValueError("提供方或页面名称无效")
                if field == "pages":
                    page = relative_path(row.get("file", ""))
                    if page not in files or not page.lower().endswith(".qml"):
                        raise ValueError("页面入口不在包内")
                    icon = row.get("icon", "puzzle")
                    if not isinstance(icon, str) or not icon or len(icon) > 256:
                        raise ValueError("页面图标声明无效")
                    if icon not in {"puzzle", "keyboard", "repeat", "monitor", "settings", "info", "bar-chart", "music"}:
                        icon = relative_path(icon)
                        if icon not in files or not icon.lower().endswith((".svg", ".png")):
                            raise ValueError("页面图标须为内置图标名或包内 SVG、PNG 文件")
                else:
                    tokens = row.get("deviceTokens", [])
                    heartbeat = row.get("heartbeatMs", 0)
                    if not string_list(tokens, 24) or any(len(item.strip()) < 5 for item in tokens) or tokens and "device.identify" not in permissions:
                        raise ValueError("设备身份声明无效")
                    if type(heartbeat) is not int or heartbeat and not 250 <= heartbeat <= 60000:
                        raise ValueError("心跳配置无效")
                contributions += 1
        native = manifest.get("native", "")
        if not isinstance(native, str):
            raise ValueError("原生入口格式无效")
        if native:
            native = relative_path(native)
        if manifest.get("drivers") and (not native or "driver.provide" not in manifest.get("permissions", [])):
            raise ValueError("驱动需要原生模块和 driver.provide")
        if native and (native not in files or not native.lower().endswith(".dll")):
            raise ValueError("原生模块入口不在包内")
        if manifest.get("pages") and "ui.page" not in manifest.get("permissions", []):
            raise ValueError("页面需要 ui.page")
        background = manifest.get("background")
        if background is not None:
            if not isinstance(background, dict) or not isinstance(background.get("file", ""), str) or type(background.get("native", False)) is not bool:
                raise ValueError("后台声明格式无效")
            background_file = background.get("file", "")
            background_native = background.get("native", False)
            if "file" in background and not background_file:
                raise ValueError("后台 QML 路径不能为空")
            if not background_file and not background_native or "runtime.background" not in permissions or background_native and not native:
                raise ValueError("后台需要有效入口和 runtime.background")
            if background_file:
                background_file = relative_path(background_file)
                if background_file not in files or not background_file.lower().endswith(".qml"):
                    raise ValueError("后台 QML 不在包内")
            contributions += 1
        else:
            background_file = ""
        windows = manifest.get("windows", [])
        if not isinstance(windows, list) or len(windows) > 8 or windows and "ui.window" not in permissions:
            raise ValueError("窗口声明格式或 ui.window 能力无效")
        window_ids = set()
        for window in windows:
            if not isinstance(window, dict) or not isinstance(window.get("id"), str) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,47}", window["id"]) or window["id"] in window_ids:
                raise ValueError("窗口标识无效")
            window_ids.add(window["id"])
            title = window.get("title")
            file = relative_path(window.get("file", ""))
            if not isinstance(title, str) or not title.strip() or len(title) > 80 or file not in files or not file.lower().endswith(".qml"):
                raise ValueError("窗口名称或 QML 入口无效")
            for field, default in (("width", 320), ("height", 240)):
                if type(window.get(field, default)) is not int or not 32 <= window.get(field, default) <= 2048:
                    raise ValueError("窗口尺寸无效")
            if any(type(window.get(field, False)) is not bool for field in ("transparent", "frameless", "topmost")):
                raise ValueError("窗口选项无效")
            contributions += 1
        commands = manifest.get("commands", [])
        if not isinstance(commands, list) or len(commands) > 32 or commands and "messaging.own" not in permissions:
            raise ValueError("命令声明格式或 messaging.own 能力无效")
        command_ids = set()
        for command in commands:
            if not isinstance(command, dict) or not isinstance(command.get("id"), str) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,47}", command["id"]) or command["id"] in command_ids:
                raise ValueError("命令标识无效")
            command_ids.add(command["id"])
            title, target = command.get("title"), command.get("target", "background")
            if not isinstance(title, str) or not title.strip() or len(title) > 80 or target not in {"background", "native"}:
                raise ValueError("命令名称或目标无效")
            if target == "background" and not background_file or target == "native" and not native:
                raise ValueError("命令目标没有对应后台入口")
        if not contributions:
            raise ValueError("插件未提供驱动、页面、后台或窗口")
        management = manifest.get("management", {})
        if not isinstance(management, dict) or any(operation not in {"install", "repair", "uninstall"} for operation in management):
            raise ValueError("管理操作声明无效")
        if management and "system.driver.manage" not in permissions:
            raise ValueError("管理操作需要 system.driver.manage")
        for command in management.values():
            if not isinstance(command, dict):
                raise ValueError("管理命令格式无效")
            program = relative_path(command.get("file", ""))
            confirmation = command.get("confirmation", "")
            if program not in files or not program.lower().endswith(".exe") or not isinstance(confirmation, str) or not confirmation.strip() or len(confirmation) > 1500 or not string_list(command.get("arguments", []), 24):
                raise ValueError("管理工具、参数或说明无效")
        return manifest


def releases(source: dict) -> list[dict]:
    host, owner, name = repository(source["repository"])
    if host == "github":
        address = f"https://api.github.com/repos/{owner}/{name}/releases?per_page=30"
    else:
        address = f"https://gitee.com/api/v5/repos/{owner}/{name}/releases?page=1&per_page=30"
    rows = json.loads(download(address, MAX_DOCUMENT_BYTES))
    if not isinstance(rows, list):
        raise ValueError("发行版本列表无效")
    result = []
    for row in rows:
        if not isinstance(row, dict):
            continue
        if row.get("draft") or row.get("prerelease"):
            continue
        if source.get("tagPrefix") and not str(row.get("tag_name", "")).startswith(source["tagPrefix"]):
            continue
        assets = row.get("assets", [])
        if isinstance(assets, dict):
            assets = assets.get("links", [])
        if not isinstance(assets, list):
            continue
        for asset in assets:
            if not isinstance(asset, dict):
                continue
            label = asset.get("name", "")
            if not isinstance(label, str):
                continue
            if source.get("assetPrefix") and not label.startswith(source["assetPrefix"]):
                continue
            if source.get("asset") and label != source["asset"]:
                continue
            url = asset.get("browser_download_url") or asset.get("direct_asset_url") or asset.get("url", "")
            if not isinstance(url, str) or not label.lower().endswith(".zip"):
                continue
            if "/releases/download/" not in url and "api.github.com/repos/" not in url:
                continue
            result.append({"url": url, "notes": str(row.get("body", ""))[:8000], "publishedAt": row.get("published_at", row.get("created_at", ""))})
            if len(result) >= 128:
                return result
    return result


def register_issue(path: Path):
    event = read_json(path)
    issue = event.get("issue", {})
    body = issue.get("body") or ""
    match = re.search(r"```json\s*(\{.*?\})\s*```", body, re.DOTALL)
    if not match:
        raise ValueError("登记内容需要 fenced JSON 对象")
    source = json.loads(match[1])
    if not isinstance(source, dict):
        raise ValueError("登记信息必须是 JSON 对象")
    validate_identity(source.get("id"))
    host, owner, name = repository(source.get("repository", ""))
    publisher = f"{host}:{owner.lower()}"
    registry = read_json(ROOT / "registry.json")
    if len(registry["sources"]) >= 2048 and not any(item["id"] == source["id"] for item in registry["sources"]):
        raise ValueError("登记来源超过目录容量")
    if source["id"].startswith("xiaoji.") and publisher not in registry.get("officialPublishers", []):
        raise ValueError("xiaoji 命名空间保留给官方发布者")
    if not source["id"].startswith("xiaoji.") and not source["id"].startswith(owner.lower() + "."):
        raise ValueError("第三方插件 ID 必须以仓库所有者的小写名称加点开头")
    submitted = source
    source = {"id": submitted["id"], "repository": f"https://{host}.com/{owner}/{name}", "publisher": publisher}
    for field in ("tagPrefix", "assetPrefix"):
        value = submitted.get(field, "")
        if not isinstance(value, str) or len(value) > 120 or "\n" in value:
            raise ValueError(f"{field} 格式无效")
        if value:
            source[field] = value
    existing = next((item for item in registry["sources"] if item["id"] == source["id"]), None)
    if existing and any(existing.get(key, "") != source.get(key, "") for key in ("id", "repository", "publisher", "tagPrefix", "assetPrefix")):
        raise ValueError("已登记插件不能通过普通登记修改来源")
    candidates = releases(source)
    if not candidates:
        raise ValueError("尚未找到插件 zip 发行附件")
    checked = None
    for candidate in candidates:
        try:
            checked = check_package(download(candidate["url"], MAX_PACKAGE_BYTES), source["id"])
            break
        except (ValueError, zipfile.BadZipFile):
            continue
    if checked is None:
        raise ValueError("发行附件中没有匹配登记 ID 的有效插件包")
    source["name"] = checked["name"]
    if not existing:
        registry["sources"].append(source)
        registry["sources"].sort(key=lambda item: item["id"])
        write_json(ROOT / "registry.json", registry)
    print(f"已补充来源 {source['id']}；自动发现无需此步骤，收录默认为未审核。")


def collect_sources(registry: dict, old: dict, errors: list[dict]) -> tuple[dict, set[str]]:
    sources = {}
    active = set()
    for entry in registry["sources"]:
        source = descriptor(entry["repository"], entry, registry.get("officialPublishers", []))
        mirrors = entry.get("mirrors", [])
        if not isinstance(mirrors, list) or len(mirrors) > 4:
            raise ValueError("镜像仓库清单无效")
        for mirror in mirrors:
            repository(mirror)
        source["mirrors"] = mirrors
        sources[source["id"]] = source
        active.add(source["id"])
    fresh, complete = discover(registry, errors)
    for plugin_id, row in old.items():
        prior = row.get("distribution")
        if plugin_id in sources or not isinstance(prior, dict) or not prior.get("discovered"):
            continue
        try:
            source = descriptor(prior["repository"], prior, registry.get("officialPublishers", []))
            source["discovered"] = True
            sources[plugin_id] = source
            if not complete[repository(source["repository"])[0]]:
                active.add(plugin_id)
        except (KeyError, ValueError):
            errors.append({"id": plugin_id, "error": "此前发现来源格式无效"})
    for source in fresh:
        plugin_id = source["id"]
        prior = sources.get(plugin_id)
        if prior:
            allowed = [prior["repository"], *prior.get("mirrors", [])]
            if source["repository"].lower() not in {address.lower() for address in allowed}:
                candidates = prior.setdefault("candidateMirrors", [])
                if len(candidates) < 4 and all(item["repository"].lower() != source["repository"].lower() for item in candidates):
                    candidates.append(source)
                continue
            if prior.get("discovered"):
                if prior.get("candidateMirrors"):
                    source["candidateMirrors"] = prior["candidateMirrors"]
                sources[plugin_id] = source
        elif len(sources) < 2048:
            sources[plugin_id] = source
        else:
            errors.append({"repository": source["repository"], "error": "插件目录达到来源数量上限"})
            continue
        active.add(plugin_id)
    return sources, active


def validate_history(catalog: dict) -> dict:
    if (not isinstance(catalog, dict) or catalog.get("format") != 1
            or type(catalog.get("revision")) is not int or catalog["revision"] <= 0
            or not isinstance(catalog.get("plugins"), list)):
        raise ValueError("历史目录格式无效")
    identities = set()
    for row in catalog["plugins"]:
        if not isinstance(row, dict):
            raise ValueError("历史插件记录无效")
        validate_identity(row.get("id"))
        if row["id"] in identities or not isinstance(row.get("versions"), list) or not row["versions"]:
            raise ValueError("历史插件版本记录无效")
        identities.add(row["id"])
        for version in row["versions"]:
            if (not isinstance(version, dict) or not isinstance(version.get("version"), str)
                    or not VERSION.fullmatch(version["version"]) or not isinstance(version.get("sha256"), str)
                    or not re.fullmatch(r"[0-9a-fA-F]{64}", version["sha256"])):
                raise ValueError("历史版本或包哈希无效")
    return catalog


def previous_catalog(registry: dict, errors: list[dict], bootstrap: bool = False) -> dict:
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature

    cache = ROOT / ".cache" / "catalog.json"
    previous = None
    if cache.exists():
        try:
            previous = validate_history(read_json(cache))
        except (OSError, ValueError, KeyError, TypeError) as error:
            errors.append({"cache": "catalog.json", "error": "历史缓存不可用：" + type(error).__name__})
    secret = os.environ.get("XIAOJI_REGISTRY_SIGNING_KEY_PEM", "")
    if not secret:
        raise ValueError("尚未配置目录签名密钥")
    key = serialization.load_pem_private_key(secret.encode(), password=None)
    if not isinstance(key, ec.EllipticCurvePrivateKey) or not isinstance(key.curve, ec.SECP256R1):
        raise ValueError("目录密钥必须是 ECDSA P-256")
    addresses = registry.get("catalogUrls", [])
    if not isinstance(addresses, list) or not addresses or len(addresses) > 4:
        raise ValueError("已发布目录地址无效")
    missing = 0
    for address in addresses:
        try:
            envelope = json.loads(download(address, 8 * 1024 * 1024))
            if envelope.get("format") != 1 or envelope.get("algorithm") != "ecdsa-p256-sha256":
                raise ValueError("已发布目录封装无效")
            payload = base64.b64decode(envelope["payload"], validate=True)
            signature = base64.b64decode(envelope["signature"], validate=True)
            if len(payload) > MAX_DOCUMENT_BYTES or len(signature) != 64:
                raise ValueError("已发布目录大小或签名无效")
            der = encode_dss_signature(int.from_bytes(signature[:32], "big"), int.from_bytes(signature[32:], "big"))
            key.public_key().verify(der, payload, ec.ECDSA(hashes.SHA256()))
            candidate = validate_history(json.loads(payload))
            if previous is None or candidate["revision"] > previous["revision"]:
                previous = candidate
        except Exception as error:
            if isinstance(error, SourceHTTPError) and error.status == 404:
                missing += 1
            errors.append({"catalog": address, "error": "历史目录未读取：" + type(error).__name__})
    if previous is None:
        if bootstrap and not cache.exists() and missing == len(addresses):
            return {"format": 1, "revision": 0, "plugins": []}
        write_json(ROOT / "dist" / "collection-errors.json", errors)
        raise ValueError("无法恢复历史目录，本次停止收录和发布，保留线上目录。首次发布需显式初始化，且所有目录地址须确认返回 404。")
    return previous


def collect(bootstrap: bool = False):
    registry = read_json(ROOT / "registry.json")
    if len(registry["sources"]) > 2048:
        raise ValueError("登记来源超过目录容量")
    reviews = read_json(ROOT / "reviews.json")["reviews"]
    policy = read_json(ROOT / "policy.json")
    errors = []
    previous = previous_catalog(registry, errors, bootstrap)
    old = {row["id"]: row for row in previous["plugins"]}
    result = []
    sources, active = collect_sources(registry, old, errors)
    for plugin_id, source in sources.items():
        validate_identity(plugin_id)
        known = {row["version"]: row for row in old.get(plugin_id, {}).get("versions", [])}
        manifest = None
        locations = [(source, False)]
        locations.extend(({**source, "repository": address}, False) for address in source.get("mirrors", []))
        locations.extend((candidate, True) for candidate in source.get("candidateMirrors", []))
        for location, mirror_only in (locations if plugin_id in active else []):
            address = location["repository"]
            try:
                for release in releases(location):
                    package = download(release["url"], MAX_PACKAGE_BYTES)
                    try:
                        candidate = check_package(package, plugin_id)
                    except (ValueError, zipfile.BadZipFile) as error:
                        errors.append({"id": plugin_id, "error": str(error)})
                        continue
                    digest = hashlib.sha256(package).hexdigest()
                    version = candidate["version"]
                    prior = known.get(version)
                    if mirror_only and prior is None:
                        errors.append({"id": plugin_id, "repository": address, "error": "不同来源不转移发布者；只接受已有版本的同哈希包作为镜像"})
                        continue
                    if prior and prior["sha256"] != digest:
                        errors.append({"id": plugin_id, "repository": address, "error": f"版本 {version} 内容已变化，拒绝覆盖"})
                        continue
                    if prior:
                        urls = list(dict.fromkeys([prior["url"], *prior.get("mirrors", []), release["url"]]))
                        prior["mirrors"] = [url for url in urls if url != prior["url"]][:4]
                    else:
                        known[version] = {"version": version, "url": release["url"], "sha256": digest, "mirrors": [],
                            "size": len(package), "minHostVersion": candidate["minHostVersion"],
                            "maxHostVersion": candidate.get("maxHostVersion", ""), "reviewed": False,
                            "notes": release["notes"], "publishedAt": release["publishedAt"]}
                    if manifest is None or tuple(map(int, version.split("."))) > tuple(map(int, manifest["version"].split("."))):
                        manifest = candidate
            except (OSError, ValueError, KeyError, TypeError, zipfile.BadZipFile) as error:
                errors.append({"id": plugin_id, "repository": address, "error": str(error)})
        if not known:
            continue
        row = dict(old.get(plugin_id, {}))
        newest = max(known, key=lambda number: tuple(map(int, number.split("."))))
        if manifest and manifest["version"] == newest:
            row.update({key: manifest.get(key) for key in ("id", "name", "author", "description", "permissions")})
            row["hasDriver"] = bool(manifest.get("drivers"))
            row["hasPage"] = bool(manifest.get("pages"))
            row["hasBackground"] = bool(manifest.get("background"))
            row["hasWindow"] = bool(manifest.get("windows"))
        for version in known.values():
            version["reviewed"] = any(item.get("id") == plugin_id and item.get("version") == version["version"]
                and item.get("sha256", "").lower() == version["sha256"] and item.get("status") == "approved" for item in reviews)
        row.update({"id": plugin_id, "publisher": source["publisher"], "repository": source["repository"],
            "official": source["publisher"] in registry.get("officialPublishers", []),
            "distribution": source, "repositories": [source["repository"], *source.get("mirrors", [])],
            "listed": plugin_id in active and plugin_id not in policy["delisted"],
            "versions": sorted(known.values(), key=lambda item: tuple(map(int, item["version"].split("."))), reverse=True)[:128]})
        result.append(row)
    catalog = {"format": 1, "revision": max(int(time.time()), int(previous.get("revision", 0)) + 1),
        "plugins": result, "blocked": policy["blocked"]}
    write_json(ROOT / ".cache" / "catalog.json", catalog)
    write_json(ROOT / "dist" / "collection-errors.json", errors)
    return catalog


def sign(catalog: dict):
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature

    secret = os.environ.get("XIAOJI_REGISTRY_SIGNING_KEY_PEM", "")
    if not secret:
        raise ValueError("尚未配置目录签名密钥；不发布未签名目录")
    private = serialization.load_pem_private_key(secret.encode(), password=None)
    if not isinstance(private, ec.EllipticCurvePrivateKey) or not isinstance(private.curve, ec.SECP256R1):
        raise ValueError("目录密钥必须是 ECDSA P-256")
    payload = json.dumps(catalog, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    if len(payload) > MAX_DOCUMENT_BYTES:
        raise ValueError("目录内容超过客户端容量，不发布本次修订")
    r, s = decode_dss_signature(private.sign(payload, ec.ECDSA(hashes.SHA256())))
    signature = r.to_bytes(32, "big") + s.to_bytes(32, "big")
    public = private.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
    envelope = {"format": 1, "algorithm": "ecdsa-p256-sha256", "keyId": hashlib.sha256(public).hexdigest()[:16],
        "payload": base64.b64encode(payload).decode(), "signature": base64.b64encode(signature).decode()}
    write_json(ROOT / "dist" / "catalog.json", envelope)
    (ROOT / "dist" / "public-key.txt").write_text(base64.b64encode(public).decode() + "\n", encoding="ascii")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--register-event", type=Path)
    parser.add_argument("--collect", action="store_true")
    parser.add_argument("--bootstrap-catalog", action="store_true", help="仅首次发布：所有历史目录均为 404 且没有本地缓存时允许初始化")
    args = parser.parse_args()
    if args.bootstrap_catalog and not args.collect:
        parser.error("初始化目录必须同时指定 --collect")
    if args.register_event:
        register_issue(args.register_event)
    elif args.collect:
        sign(collect(args.bootstrap_catalog))
    else:
        parser.error("需要登记事件或收录操作")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as exception:
        print(str(exception), file=sys.stderr)
        raise SystemExit(1)
