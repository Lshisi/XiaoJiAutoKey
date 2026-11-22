"""Edit review and distribution policy; publishing remains a separate action."""

import argparse
from catalog import ROOT, VERSION, read_json, validate_identity, write_json


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("operation", choices=["review", "revoke-review", "delist", "relist", "block", "unblock"])
    parser.add_argument("id")
    parser.add_argument("--version", default="*")
    parser.add_argument("--sha256", default="")
    parser.add_argument("--reason", default="")
    args = parser.parse_args()
    validate_identity(args.id)
    if args.version != "*" and not VERSION.fullmatch(args.version):
        parser.error("版本格式无效")
    digest = args.sha256.lower()
    if digest and (len(digest) != 64 or any(char not in "0123456789abcdef" for char in digest)):
        parser.error("需要 64 位 SHA-256")
    if args.operation in ("review", "revoke-review"):
        if args.version == "*" or not digest:
            parser.error("审核操作必须指定确切版本与包 SHA-256")
        document = read_json(ROOT / "reviews.json")
        document["reviews"] = [row for row in document["reviews"] if not (row["id"] == args.id and row["version"] == args.version and row["sha256"] == digest)]
        if args.operation == "review":
            document["reviews"].append({"id": args.id, "version": args.version, "sha256": digest, "status": "approved"})
        write_json(ROOT / "reviews.json", document)
    else:
        document = read_json(ROOT / "policy.json")
        if args.operation in ("delist", "relist"):
            document["delisted"] = sorted((set(document["delisted"]) | {args.id}) if args.operation == "delist" else (set(document["delisted"]) - {args.id}))
        else:
            document["blocked"] = [row for row in document["blocked"] if not (row["id"] == args.id and row["version"] == args.version and row.get("sha256", "") == digest)]
            if args.operation == "block":
                if not args.reason.strip():
                    parser.error("禁用策略需要原因")
                document["blocked"].append({"id": args.id, "version": args.version, "sha256": digest, "reason": args.reason.strip()})
        write_json(ROOT / "policy.json", document)
    print("已更新本地管理记录；提交后由目录任务签名发布。")


if __name__ == "__main__":
    main()
