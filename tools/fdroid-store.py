#!/usr/bin/env python3
"""Opt-in F-Droid browsing. Verify the pinned signed index before exposing APKs.

Uses Python's standard library and the appliance's OpenSSL executable. Never
starts Android, installs packages, follows repository-supplied mirrors, or
fetches icons. The caller owns consent, cancellation, and the profile cache.
"""
import argparse
import base64
import gzip
import hashlib
import html
import io
import json
import os
from pathlib import Path
import re
import shutil
import ssl
import subprocess
import tempfile
import time
import urllib.request
import zipfile
import zlib
import xml.etree.ElementTree as ET

REPOSITORY = "https://f-droid.org/repo/"
# https://f-droid.org/docs/Release_Channels_and_Signing_Keys/
SIGNER = "43238d512c1e5eb2d6569f4a3afbf5523418b82e0a3ed1552770abb9a9c9ccab"
MAX_ARCHIVE = 1024**2
MAX_INDEX = 128 * 1024**2
PAGE_SIZE = 50
PACKAGE = re.compile(r"[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)+")


def sections(raw):
    """Parse JAR manifest attributes including continuation lines, rejecting duplicates."""
    if len(raw) > 1024**2:
        raise ValueError("Oversized JAR signature metadata")
    result = []
    for block in raw.replace(b"\r\n", b"\n").split(b"\n\n"):
        if not block.strip():
            continue
        lines = []
        for line in block.split(b"\n"):
            if line.startswith(b" "):
                if not lines:
                    raise ValueError("Invalid JAR continuation")
                lines[-1] += line[1:]
            elif line:
                lines.append(line)
        attrs = {}
        for line in lines:
            key, separator, value = line.partition(b": ")
            key = key.decode("ascii").lower()
            if not separator or key in attrs:
                raise ValueError("Invalid or duplicate JAR attribute")
            attrs[key] = value.decode("utf-8")
        result.append(attrs)
    return result


def check_digest(value, data):
    if base64.b64decode(value, validate=True) != hashlib.sha256(data).digest():
        raise ValueError("F-Droid signed index digest mismatch")


def verified_entry(archive, signer=SIGNER, openssl=None):
    if not 0 < len(archive) <= MAX_ARCHIVE:
        raise ValueError("F-Droid index download exceeds its limit")
    executable = openssl or shutil.which("openssl")
    if not executable:
        raise ValueError("F-Droid signature verification needs OpenSSL from appliance setup")
    with zipfile.ZipFile(io.BytesIO(archive)) as jar:
        names = jar.namelist()
        if len(names) != len(set(names)) or len(names) > 32:
            raise ValueError("Ambiguous signed F-Droid index")
        def read(name, limit):
            info = jar.getinfo(name)
            if info.file_size > limit or info.flag_bits & 1:
                raise ValueError("Oversized or encrypted F-Droid index entry")
            return jar.read(name)
        signatures = [n for n in names if re.fullmatch(r"META-INF/[A-Za-z0-9_-]+\.RSA", n)]
        if len(signatures) != 1:
            raise ValueError("F-Droid index must have exactly one RSA signature")
        signature = signatures[0]
        sf = read(signature[:-4] + ".SF", 1024**2)
        manifest = read("META-INF/MANIFEST.MF", 1024**2)
        index = read("entry.json", 1024**2)
        with tempfile.TemporaryDirectory(prefix="isolde-fdroid-verify-") as scratch:
            root = Path(scratch)
            (root/"signature.der").write_bytes(read(signature, 65536))
            (root/"signature.sf").write_bytes(sf)
            # -noverify disables CA trust only. Actual signers emitted by OpenSSL
            # must match the pinned certificate, not an unrelated cert in the bag.
            completed = subprocess.run([executable, "smime", "-verify", "-binary", "-inform", "DER",
                "-in", str(root/"signature.der"), "-content", str(root/"signature.sf"),
                "-noverify", "-signer", str(root/"signer.pem"), "-out", str(root/"verified.sf")],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
            if completed.returncode or not (root/"signer.pem").exists():
                raise ValueError("F-Droid repository signature is invalid")
            certs = re.findall(r"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----",
                               (root/"signer.pem").read_text(), re.S)
            if len(certs) != 1 or hashlib.sha256(ssl.PEM_cert_to_DER_cert(certs[0])).hexdigest() != signer:
                raise ValueError("F-Droid repository signer does not match the pinned certificate")
        attributes = sections(sf)
        if not attributes:
            raise ValueError("Missing signed manifest digest")
        check_digest(attributes[0].get("sha-256-digest-manifest", ""), manifest)
        entries = [s for s in sections(manifest) if s.get("name") == "entry.json"]
        if len(entries) != 1:
            raise ValueError("Missing signed index digest")
        check_digest(entries[0].get("sha-256-digest", ""), index)
        document = json.loads(index)
        if document.get("version") != 30000:
            raise ValueError("Unsupported F-Droid repository protocol version")
        now = time.time()*1000
        stamp = document.get("timestamp", 0)
        if not isinstance(stamp, (int, float)) or not now-14*86400000 <= stamp <= now+86400000:
            raise ValueError("F-Droid index is expired or dated in the future; retry after checking the clock")
        descriptor = document.get("index", {})
        if (descriptor.get("name") != "/index-v2.json"
                or not re.fullmatch(r"[a-f0-9]{64}", descriptor.get("sha256", ""))
                or not isinstance(descriptor.get("size"), int) or not 0 < descriptor["size"] <= MAX_INDEX):
            raise ValueError("Unsupported signed F-Droid index descriptor")
        return document


def verified_index(entry, data):
    descriptor = entry["index"]
    if len(data) != descriptor["size"] or hashlib.sha256(data).hexdigest() != descriptor["sha256"]:
        raise ValueError("F-Droid index SHA-256 or size does not match the signed entry")
    document = json.loads(data)
    if document.get("repo", {}).get("address", "").rstrip("/") != REPOSITORY.rstrip("/"):
        raise ValueError("Unexpected signed repository address")
    packages = document.get("packages")
    if not isinstance(packages, dict) or len(packages) > 20000:
        raise ValueError("Unsupported F-Droid catalog size or format")
    # Normalize the signed v2 records for the bounded search/version selector.
    apps, versions = [], {}
    for identity, record in packages.items():
        metadata = record.get("metadata", {})
        def english(field):
            value = metadata.get(field, {})
            return value.get("en-US", next(iter(value.values()), "")) if isinstance(value, dict) else value
        apps.append({"packageName": identity, "name": english("name"), "summary": english("summary"),
                     "categories": metadata.get("categories", []), "authorName": metadata.get("authorName", ""),
                     "suggestedVersionCode": ""})
        values = record.get("versions", {})
        if not isinstance(values, dict) or len(values) > 256:
            continue
        versions[identity] = []
        preferred = metadata.get("preferredSigner")
        for version in values.values():
            manifest, file = version.get("manifest", {}), version.get("file", {})
            # Do not offer known vulnerable/disabled builds or alpha/beta tracks.
            if (version.get("releaseChannels") or version.get("obbMainFile") or version.get("obbPatchFile")
                    or any(key in version.get("antiFeatures", {})
                    for key in ("KnownVuln", "DisabledAlgorithm", "NoSourceSince"))
                    or (preferred and preferred not in manifest.get("signer", {}).get("sha256", []))):
                continue
            permissions = manifest.get("usesPermission", []) + manifest.get("usesPermissionSdk23", [])
            versions[identity].append({"packageName": manifest.get("packageName", identity),
                "versionCode": manifest.get("versionCode"), "versionName": manifest.get("versionName", ""),
                "apkName": file.get("name", "").removeprefix("/"), "hash": file.get("sha256", ""),
                "hashType": "sha256", "size": file.get("size", 0),
                "nativecode": manifest.get("nativecode", []),
                "minSdkVersion": manifest.get("usesSdk", {}).get("minSdkVersion", 1),
                "maxSdkVersion": manifest.get("maxSdkVersion", 10000),
                "features": [f["name"] for f in manifest.get("features", []) if f.get("required", True)],
                "uses-permission": [p.get("name", "") for p in permissions]})
    return {"apps": apps, "packages": versions}


class RepositoryRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        if not newurl.startswith(REPOSITORY):
            raise ValueError("F-Droid redirected outside the trusted repository")
        return super().redirect_request(request, fp, code, msg, headers, newurl)


def download(name, maximum):
    opener = urllib.request.build_opener(RepositoryRedirect())
    request = urllib.request.Request(REPOSITORY+name, headers={"User-Agent": "Isolde Store", "Accept-Encoding": "gzip"})
    with opener.open(request, timeout=20) as response:
        length=int(response.headers.get("Content-Length", "0"))
        if response.status != 200 or not 0 <= length <= maximum:
            raise ValueError("F-Droid returned an invalid index download")
        encoding=response.headers.get("Content-Encoding", "identity").strip().lower()
        if encoding not in {"identity", "gzip"}:
            raise ValueError("F-Droid returned an unsupported transfer encoding")
        chunks, total, deadline = [], 0, time.monotonic()+120
        class BoundedResponse:
            transferred=0
            def read(self, size=-1):
                block=response.read(min(65536, size if size>=0 else 65536, maximum-self.transferred+1))
                self.transferred+=len(block)
                if self.transferred>maximum:
                    raise ValueError("F-Droid transfer exceeded its size limit")
                if time.monotonic()>deadline:
                    raise ValueError("F-Droid download exceeded 120 seconds after %d bytes; search again to retry"%self.transferred)
                return block
        wire=BoundedResponse()
        stream=gzip.GzipFile(fileobj=wire,mode="rb") if encoding=="gzip" else wire
        try:
            while True:
                block=stream.read(min(65536,maximum-total+1))
                if not block:break
                chunks.append(block);total+=len(block)
                if total>maximum:raise ValueError("F-Droid expanded index exceeded its size limit")
                if time.monotonic()>deadline:raise ValueError("F-Droid metadata expansion exceeded its time limit")
        except (gzip.BadGzipFile,EOFError,zlib.error) as error:
            raise ValueError("F-Droid returned invalid compressed metadata") from error
        finally:
            if encoding=="gzip":stream.close()
        return b"".join(chunks)


def plain(value, limit):
    return re.sub(r"[\x00-\x1f<>]", " ", html.unescape(re.sub(r"<[^>]*>", " ", str(value))))[:limit].strip()


def read_runtime_target(root=Path('/var/lib/waydroid/rootfs'), vendor=None):
    """Read image metadata without starting an Android session."""
    vendor=vendor if vendor is not None else root/'vendor'
    properties = {}
    for path in (root/'build.prop',root/'system/build.prop',root/'system/system/build.prop',vendor/'build.prop'):
        try:
            if not path.resolve().is_relative_to(root.resolve()) and not path.resolve().is_relative_to(vendor.resolve()):continue
            if path.stat().st_size > 1024**2:
                continue
            for line in path.read_text().splitlines():
                key, separator, value = line.partition("=")
                if separator:
                    properties[key.strip()] = value.strip()
        except OSError:
            continue
    sdk = properties.get("ro.build.version.sdk") or properties.get('ro.system.build.version.sdk','')
    abis = (properties.get("ro.product.cpu.abilist") or properties.get('ro.system.product.cpu.abilist') or properties.get('ro.product.system.cpu.abilist','')).split(",")
    if not sdk.isdigit() or not any(abis):
        return None
    features, unavailable = set(), set()
    for directory in (root/'etc/permissions',root/'system/etc/permissions',vendor/'etc/permissions',root/'etc/sysconfig',root/'system/etc/sysconfig',vendor/'etc/sysconfig'):
        files = sorted(directory.glob("*.xml"))
        if len(files) > 256:
            return None
        for path in files:
            try:
                if not path.resolve().is_relative_to(root.resolve()) and not path.resolve().is_relative_to(vendor.resolve()):continue
                if path.stat().st_size > 256*1024:
                    continue
                tree = ET.fromstring(path.read_bytes())
                features.update(item.attrib["name"] for item in tree.iter("feature")
                                if "name" in item.attrib)
                unavailable.update(item.attrib["name"] for item in tree.iter("unavailable-feature")
                                   if "name" in item.attrib)
            except (OSError, ET.ParseError):
                continue
    return {"sdk": int(sdk), "abis": abis, "features": sorted(features-unavailable)}


def image_identity(root=Path('/var/lib/waydroid/images')):
    result={}
    for name in ('system.img','vendor.img'):
        path=root/name;info=path.stat()
        if path.resolve()!=path or not path.is_file() or info.st_uid!=0 or info.st_mode & 0o022:raise ValueError('Untrusted Android image')
        result[name]=[info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns]
    return result


def runtime_target():
    target=read_runtime_target()
    if target:return target
    # Idle Waydroid unmounts rootfs. Setup retains only verified image metadata,
    # bound to the exact installed root-owned images, never guessed host ABIs.
    path=Path('/var/lib/isolde/android-store-capabilities.json')
    try:
        info=path.stat()
        if path.resolve()!=path or info.st_uid!=0 or info.st_mode & 0o022 or info.st_size>65536:return None
        value=json.loads(path.read_text())
        if value.get('schemaVersion')!=1 or value.get('images')!=image_identity():return None
        target=value['target']
        if (set(target)!={'sdk','abis','features'} or type(target['sdk']) is not int or not 16<=target['sdk']<=100
            or not isinstance(target['abis'],list) or not 1<=len(target['abis'])<=16
            or any(abi not in {'x86','x86_64','arm64-v8a','armeabi','armeabi-v7a','riscv64'} for abi in target['abis'])
            or not isinstance(target['features'],list) or len(target['features'])>2048
            or any(not isinstance(f,str) or not re.fullmatch(r'[A-Za-z0-9_.-]{1,200}',f) for f in target['features'])):return None
        return target
    except (OSError,ValueError,KeyError,TypeError):return None


def compatible(package, target):
    if not target:
        return False
    native = package.get("nativecode", [])
    minimum = package.get("minSdkVersion", 1)
    maximum = package.get("maxSdkVersion", 10000)
    return (isinstance(native, list) and (not native or bool(set(native) & set(target["abis"])))
            and isinstance(minimum, int) and isinstance(maximum, int)
            and minimum <= target["sdk"] <= maximum
            and isinstance(package.get("features", []), list)
            and set(package.get("features", [])) <= set(target["features"]))


def browse(document, query, page, target, packages=None):
    if len(query) > 128 or page < 0 or page > 400:
        raise ValueError("Invalid F-Droid search or page")
    results = []
    if packages is not None and (len(packages)>256 or any(not PACKAGE.fullmatch(p) for p in packages)):
        raise ValueError("Invalid update subscription list")
    for app in document["apps"]:
        identity = app.get("packageName", "")
        if packages is not None and identity not in packages:
            continue
        if not PACKAGE.fullmatch(identity) or len(identity) > 200 or app.get("disabled"):
            continue
        localized = app.get("localized", {}).get("en-US", {})
        name = plain(localized.get("name") or app.get("name") or identity, 80) or identity[:80]
        summary = plain(localized.get("summary") or app.get("summary", ""), 1000)
        categories = app.get("categories", [])
        if not isinstance(categories, list):
            continue
        category = plain(", ".join(str(c) for c in categories), 160)
        if query.casefold() not in (name+" "+identity+" "+summary+" "+category).casefold():
            continue
        candidates = []
        versions = document["packages"].get(identity, [])
        if not isinstance(versions, list) or len(versions) > 256:
            continue
        suggested = str(app.get("suggestedVersionCode", ""))
        for package in versions:
            filename = package.get("apkName", "")
            code = str(package.get("versionCode", ""))
            size = package.get("size", 0)
            if (package.get("packageName") != identity or package.get("hashType") != "sha256"
                    or not re.fullmatch(r"[a-f0-9]{64}", package.get("hash", ""))
                    or not re.fullmatch(r"[A-Za-z0-9_.+-]+\.apk", filename)
                    or not re.fullmatch(r"[0-9]{1,18}", code)
                    or not isinstance(size, int) or not 0 < size <= 128*1024**2
                    or (suggested.isdigit() and int(code) > int(suggested))):
                continue
            candidates.append(package)
        if not candidates:
            continue
        supported = [p for p in candidates if compatible(p, target)]
        package = max(supported or candidates, key=lambda p: int(p["versionCode"]))
        usable = bool(supported)
        reason = "" if usable else ("Android runtime details unavailable. Complete Android setup and retry."
                    if not target else "This version requires an unsupported Android ABI, SDK or device feature.")
        results.append({"id": identity, "storeId": identity, "packageName": identity, "type": "android",
            "storeType": "android", "store": "fdroid", "name": name, "description": summary,
            "category": category, "publisher": plain(app.get("authorName") or identity, 160),
            "version": plain(package.get("versionName", ""), 80), "versionCode": str(package["versionCode"]),
            "sizeBytes": package["size"], "downloadUrl": REPOSITORY+package["apkName"], "sha256": package["hash"],
            "provenance": "F-Droid official repository · pinned signed index", "metadataVerified": True,
            "compatible": usable, "compatibilityReason": reason,
            "consentText": plain("; ".join(str(p) for p in package.get("uses-permission", [])), 2000)})
    results.sort(key=lambda r: (r["name"].casefold(), r["id"]))
    size = 256 if packages is not None else PAGE_SIZE
    start = page*size
    return {"results": results[start:start+size], "page": page,
            "hasPrevious": page > 0, "hasNext": start+size < len(results), "total": len(results)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--query", default="")
    parser.add_argument("--page", type=int, default=0)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--packages", default=None)
    args = parser.parse_args()
    packages = args.packages.split(',') if args.packages is not None else None
    if len(args.query)>128 or not 0<=args.page<=400:
        raise ValueError("Invalid F-Droid search or page")
    if packages is not None and (len(packages)>256 or any(len(p)>200 or not PACKAGE.fullmatch(p) for p in packages)):
        raise ValueError("Invalid update subscription list")
    cache = args.cache.absolute()
    if cache.resolve() != cache:
        raise ValueError("F-Droid profile cache is redirected")
    cache.mkdir(parents=True, exist_ok=True, mode=0o700)
    archive, index = cache/"entry.jar", cache/"index-v2.json"
    if archive.is_symlink() or index.is_symlink():
        raise ValueError("F-Droid cache is redirected")
    entry_data = None
    if archive.exists() and time.time()-archive.stat().st_mtime < 3600 and archive.stat().st_size <= MAX_ARCHIVE:
        entry_data = archive.read_bytes()
        try:
            entry = verified_entry(entry_data)
        except (ValueError, zipfile.BadZipFile, KeyError):
            entry_data = None  # A corrupt/expired cache never prevents an explicit retry.
    refreshed = entry_data is None
    if entry_data is None:
        entry_data = download("entry.jar", 1024**2)
        entry = verified_entry(entry_data)
    data = index.read_bytes() if index.exists() and index.stat().st_size <= MAX_INDEX else b""
    if len(data) != entry["index"]["size"] or hashlib.sha256(data).hexdigest() != entry["index"]["sha256"]:
        data = download("index-v2.json", MAX_INDEX)
    document = verified_index(entry, data)
    # Verification precedes replacement; every cache read is verified again.
    for destination, payload in ((index, data), (archive, entry_data)):
        if destination.exists() and destination.read_bytes() == payload:
            if destination == archive and refreshed:
                os.utime(destination, None)
            continue
        fd, temporary = tempfile.mkstemp(prefix=".index-", dir=cache)
        try:
            with os.fdopen(fd, "wb") as stream:
                stream.write(payload); stream.flush(); os.fsync(stream.fileno())
            os.replace(temporary, destination)
        finally:
            Path(temporary).unlink(missing_ok=True)
    output=json.dumps(browse(document, args.query, args.page, runtime_target(), packages), ensure_ascii=True)
    if len(output)>4*1024**2: raise ValueError("F-Droid results exceed their output limit")
    print(output)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(json.dumps({"error": str(error)[:512]}))
        raise SystemExit(1)
