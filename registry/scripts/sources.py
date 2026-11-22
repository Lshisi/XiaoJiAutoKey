"""Find public plugin repositories on GitHub and Gitee without executing them."""

from __future__ import annotations

import base64
import ipaddress
import json
import os
import re
import socket
import urllib.error
import urllib.parse
import urllib.request

IDENTITY = re.compile(r"^[a-z0-9][a-z0-9.-]{1,94}[a-z0-9]$")
VERSION = re.compile(r"^(0|[1-9][0-9]{0,8})\.(0|[1-9][0-9]{0,8})\.(0|[1-9][0-9]{0,8})$")
DOCUMENT_LIMIT = 4 * 1024 * 1024


class SourceHTTPError(ValueError):
    def __init__(self, status: int, host: str):
        self.status = status
        super().__init__(f"{host} 返回 HTTP {status}")


def safe_url(address: str) -> str:
    parts = urllib.parse.urlsplit(address)
    if parts.scheme != "https" or not parts.hostname or parts.username or parts.password or parts.fragment:
        raise ValueError("下载地址必须是无认证信息的 HTTPS URL")
    if parts.port not in (None, 443):
        raise ValueError("仅支持标准 HTTPS 端口")
    addresses = socket.getaddrinfo(parts.hostname, 443, type=socket.SOCK_STREAM)
    if not addresses or any(not ipaddress.ip_address(item[4][0]).is_global for item in addresses):
        raise ValueError("地址必须指向公共网络")
    return address


class PublicRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, address):
        source = urllib.parse.urlsplit(request.full_url)
        destination = urllib.parse.urlsplit(address)
        if source.netloc != destination.netloc:
            query = [(key, value) for key, value in urllib.parse.parse_qsl(destination.query) if key != "access_token"]
            address = urllib.parse.urlunsplit(destination._replace(query=urllib.parse.urlencode(query)))
        result = super().redirect_request(request, fp, code, message, headers, safe_url(address))
        if result and source.netloc != destination.netloc:
            result.remove_header("Authorization")
            result.remove_header("Cookie")
        return result


def download(address: str, limit: int) -> bytes:
    safe_url(address)
    parts = urllib.parse.urlsplit(address)
    headers = {"User-Agent": "XiaoJiPluginRegistry/1", "Accept": "application/json, application/octet-stream"}
    if parts.netloc == "api.github.com" and os.environ.get("XIAOJI_GITHUB_TOKEN"):
        headers["Authorization"] = "Bearer " + os.environ["XIAOJI_GITHUB_TOKEN"]
        headers["X-GitHub-Api-Version"] = "2022-11-28"
    if parts.netloc == "gitee.com" and parts.path.startswith("/api/v5/") and os.environ.get("XIAOJI_GITEE_TOKEN"):
        query = urllib.parse.parse_qsl(parts.query)
        query = [(key, value) for key, value in query if key != "access_token"]
        query.append(("access_token", os.environ["XIAOJI_GITEE_TOKEN"]))
        address = urllib.parse.urlunsplit(parts._replace(query=urllib.parse.urlencode(query)))
    request = urllib.request.Request(address, headers=headers)
    try:
        with urllib.request.build_opener(PublicRedirects()).open(request, timeout=30) as response:
            safe_url(response.url)
            length = response.headers.get("Content-Length")
            if length and int(length) > limit:
                raise ValueError("发布文件超过大小限制")
            body = response.read(limit + 1)
            if len(body) > limit:
                raise ValueError("发布文件超过大小限制")
            return body
    except urllib.error.HTTPError as error:
        raise SourceHTTPError(error.code, parts.hostname) from None
    except urllib.error.URLError as error:
        raise ValueError(f"{parts.hostname} 网络读取失败：{type(error.reason).__name__}") from None


def repository(address: str) -> tuple[str, str, str]:
    if not isinstance(address, str):
        raise ValueError("仓库地址格式无效")
    parts = urllib.parse.urlsplit(address)
    path = parts.path.removesuffix(".git").strip("/").split("/")
    if parts.scheme != "https" or parts.netloc.lower() not in ("github.com", "gitee.com") or len(path) != 2:
        raise ValueError("请提供 GitHub 或 Gitee 的 HTTPS 仓库地址")
    if parts.query or parts.fragment or any(not re.fullmatch(r"[A-Za-z0-9_.-]+", piece) or piece in (".", "..") for piece in path):
        raise ValueError("仓库地址格式无效")
    return ("github" if parts.netloc.lower() == "github.com" else "gitee", path[0], path[1])


def validate_identity(plugin_id: str):
    if not isinstance(plugin_id, str) or not IDENTITY.fullmatch(plugin_id) or ".." in plugin_id:
        raise ValueError("插件 ID 格式无效")


def descriptor(address: str, declaration: dict, official_publishers: list[str]) -> dict:
    if not isinstance(declaration, dict):
        raise ValueError("插件来源声明必须是对象")
    plugin_id = declaration.get("id")
    validate_identity(plugin_id)
    host, owner, name = repository(address)
    publisher = f"{host}:{owner.lower()}"
    if plugin_id.startswith("xiaoji."):
        if publisher not in official_publishers:
            raise ValueError("xiaoji 命名空间保留给官方发布者")
    elif not plugin_id.startswith(owner.lower() + "."):
        raise ValueError("第三方插件 ID 必须以仓库所有者的小写名称加点开头")
    source = {"id": plugin_id, "repository": f"https://{host}.com/{owner}/{name}", "publisher": publisher}
    for field in ("tagPrefix", "assetPrefix", "asset"):
        value = declaration.get(field, "")
        if not isinstance(value, str) or len(value) > 120 or any(ord(char) < 32 for char in value):
            raise ValueError(f"{field} 格式无效")
        if value:
            source[field] = value
    return source


def document(address: str) -> dict:
    value = json.loads(download(address, DOCUMENT_LIMIT))
    if not isinstance(value, dict):
        raise ValueError("来源信息必须是 JSON 对象")
    return value


def repository_file(address: str, branch: str, name: str) -> dict | None:
    host, owner, repo = repository(address)
    api = "https://api.github.com" if host == "github" else "https://gitee.com/api/v5"
    query = urllib.parse.urlencode({"ref": branch})
    try:
        row = document(f"{api}/repos/{owner}/{repo}/contents/{name}?{query}")
    except SourceHTTPError as error:
        if error.status == 404:
            return None
        raise
    if row.get("type") != "file" or row.get("encoding") != "base64" or row.get("size", 0) > 128 * 1024:
        raise ValueError("仓库插件声明不是受支持的小型文件")
    encoded = row.get("content", "")
    if not isinstance(encoded, str) or len(encoded) > 192 * 1024:
        raise ValueError("仓库插件声明超过大小限制")
    decoded = base64.b64decode("".join(encoded.split()), validate=True)
    if len(decoded) > 128 * 1024:
        raise ValueError("仓库插件声明超过大小限制")
    value = json.loads(decoded.decode("utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError("仓库插件声明必须是 JSON 对象")
    return value


def search_repositories(host: str, config: dict) -> tuple[list[str], bool]:
    topic = config.get("topic", "xiaoji-plugin")
    pages = min(max(int(config.get("maxPages", 5)), 1), 10)
    result = []
    for page in range(pages):
        if host == "github":
            query = urllib.parse.urlencode({"q": f"topic:{topic} archived:false fork:false", "sort": "updated", "per_page": 100, "page": page + 1})
            data = document("https://api.github.com/search/repositories?" + query)
            rows = data.get("items")
            if not isinstance(rows, list) or data.get("incomplete_results"):
                raise ValueError("GitHub 主题搜索返回了不完整结果")
            for row in rows:
                if isinstance(row, dict) and not row.get("fork") and not row.get("archived"):
                    name = row.get("full_name", "")
                    if isinstance(name, str) and len(name.split("/")) == 2:
                        result.append("https://github.com/" + name)
            total = data.get("total_count", 0)
            consumed = (page + 1) * 100
        else:
            # This is the public repository search used by search.gitee.com.
            # The old /api/v5/search/repositories can silently return [] for known repositories.
            widget = config.get("giteeWidget", "wong1slagnlmzwvsu5ya")
            if not isinstance(widget, str) or not re.fullmatch(r"[a-z0-9]{8,64}", widget):
                raise ValueError("Gitee 搜索组件配置无效")
            query = urllib.parse.urlencode({"query": 1048, "q": topic, "from": page * 50, "size": 50})
            data = document(f"https://so.gitee.com/v1/search/widget/{widget}?{query}")
            hits = data.get("hits", {})
            rows = hits.get("hits") if isinstance(hits, dict) else None
            if not isinstance(rows, list):
                raise ValueError("Gitee 仓库搜索结果格式变化")
            for row in rows:
                fields = row.get("fields", {}) if isinstance(row, dict) else {}
                urls = fields.get("url", [])
                forks = fields.get("fork", [0])
                if isinstance(urls, list) and urls and isinstance(forks, list) and not any(forks):
                    result.append(urls[0])
            total_row = hits.get("total", {})
            if not isinstance(total_row, dict) or total_row.get("relation") != "eq":
                raise ValueError("Gitee 搜索总量不完整")
            total = total_row.get("value", 0)
            consumed = (page + 1) * 50
        if not isinstance(total, int) or total < 0:
            raise ValueError("仓库搜索总量无效")
        if consumed >= total:
            return list(dict.fromkeys(result)), True
    return list(dict.fromkeys(result)), False


def discover(registry: dict, errors: list[dict]) -> tuple[list[dict], dict[str, bool]]:
    config = registry.get("discovery", {})
    topic = config.get("topic", "xiaoji-plugin")
    if not isinstance(topic, str) or not re.fullmatch(r"[a-z0-9][a-z0-9-]{2,47}", topic):
        raise ValueError("自动发现主题配置无效")
    marker = re.compile(r"(?<![a-z0-9-])" + re.escape(topic) + r"(?![a-z0-9-])", re.IGNORECASE)
    found = []
    complete = {"github": False, "gitee": False}
    for host in complete:
        try:
            addresses, complete[host] = search_repositories(host, config)
        except (OSError, ValueError, TypeError) as error:
            errors.append({"platform": host, "error": str(error)})
            continue
        if not complete[host]:
            errors.append({"platform": host, "error": "搜索达到分页上限，保留此前有效来源"})
        for address in addresses:
            try:
                actual_host, owner, repo = repository(address)
                if actual_host != host:
                    raise ValueError("搜索结果的仓库平台不一致")
                api = "https://api.github.com" if host == "github" else "https://gitee.com/api/v5"
                info = document(f"{api}/repos/{owner}/{repo}")
                if info.get("private") or info.get("fork") or info.get("archived"):
                    continue
                if host == "github":
                    if topic not in info.get("topics", []):
                        continue
                else:
                    # Gitee's public API does not expose GitHub's topic search contract.
                    # An exact marker in the public description/name is searchable on Gitee.
                    words = " ".join(str(info.get(field, "")) for field in ("description", "name", "path"))
                    if not marker.search(words):
                        continue
                branch = info.get("default_branch")
                if not isinstance(branch, str) or not branch or len(branch) > 256:
                    raise ValueError("仓库缺少默认分支")
                index = repository_file(address, branch, "xiaoji-plugins.json")
                if index is None:
                    manifest = repository_file(address, branch, "plugin.json")
                    declarations = [] if manifest is None else [{"id": manifest.get("id")}]
                else:
                    declarations = index.get("plugins")
                    if index.get("format") != 1 or not isinstance(declarations, list) or len(declarations) > 32:
                        raise ValueError("xiaoji-plugins.json 格式无效")
                declared = []
                ids = set()
                for declaration in declarations:
                    source = descriptor(address, declaration, registry.get("officialPublishers", []))
                    if source["id"] in ids:
                        raise ValueError("仓库索引中出现重复的插件 ID")
                    ids.add(source["id"])
                    source["discovered"] = True
                    declared.append(source)
                found.extend(declared)
            except SourceHTTPError as error:
                if error.status != 404:
                    complete[host] = False
                errors.append({"repository": address, "error": str(error)})
            except (OSError, ValueError, TypeError) as error:
                complete[host] = False
                errors.append({"repository": address, "error": str(error)})
    return found, complete
