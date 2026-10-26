#!/usr/bin/env python3
"""Explicit appliance Android setup. Never run on the Windows host.

Stages the verified SmartTube APK; installing into a user's Android data is a
separate profile-scoped operation. A receipt never claims an app was launched.
"""
import argparse
import configparser
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
import io
import stat

spec = importlib.util.spec_from_file_location("default_provisioner", Path(__file__).with_name("provision-default-apps.py"))
shared = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shared)


def load(source):
    path = source / "config/android-runtime.json"
    if path.stat().st_size > 65536: raise ValueError("Oversized Android runtime configuration")
    data = json.loads(path.read_text())
    if set(data) != {"schemaVersion", "runtime", "runtimeVersion", "repository", "systemType", "apps"} or data["schemaVersion"] != 1:
        raise ValueError("Unsupported Android runtime configuration")
    if data["runtime"] != "waydroid" or data["systemType"] != "VANILLA" or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", data["runtimeVersion"]):
        raise ValueError("Unsupported Android runtime")
    repo = data["repository"]
    if set(repo) != {"url", "suite", "keyUrl", "keySha256"} or repo["url"] != "https://repo.waydro.id/" or repo["suite"] != "resolute" or repo["keyUrl"] != repo["url"]+"waydroid.gpg":
        raise ValueError("Unsupported Android repository")
    if not re.fullmatch(r"[a-f0-9]{64}", repo["keySha256"]): raise ValueError("Missing repository key checksum")
    if not isinstance(data["apps"], dict) or set(data["apps"]) != {"smarttube"}: raise ValueError("Unsupported bundled Android apps")
    app = data["apps"]["smarttube"]
    if set(app) != {"packageName", "version", "abi", "url", "sha256"} or app["packageName"] != "org.smarttube.stable" or app["abi"] != "x86" or not re.fullmatch(r"[0-9]+\.[0-9]+", app["version"]):
        raise ValueError("Invalid SmartTube identity")
    version = app["version"]
    if app["url"] != f"https://github.com/yuliskov/SmartTube/releases/download/{version}s/SmartTube_stable_{version}_x86.apk" or not re.fullmatch(r"[a-f0-9]{64}", app["sha256"]):
        raise ValueError("Invalid SmartTube source")
    return data


def verified_download(url, target, expected, limit):
    if target.exists() or target.is_symlink(): raise ValueError("Download target must be new")
    actual = shared.download(url, target, limit)
    if actual != expected:
        target.unlink(missing_ok=True)
        raise ValueError("Android download checksum mismatch")


def configure_software_rendering(path):
    if path.is_symlink() or path.stat().st_size > 65536: raise RuntimeError("Unsafe Android runtime configuration")
    before = path.read_bytes()
    mode = stat.S_IMODE(path.stat().st_mode)
    cfg = configparser.ConfigParser(interpolation=None)
    cfg.read_string(before.decode())
    if not cfg.has_section("properties"): cfg.add_section("properties")
    changes = {"ro.hardware.gralloc": "default", "ro.hardware.egl": "swiftshader"}
    if all(cfg.get("properties", k, fallback=None) == v for k, v in changes.items()): return False
    for k, v in changes.items(): cfg.set("properties", k, v)
    output = io.StringIO(); cfg.write(output)
    shared.atomic(path.with_suffix(".cfg.isolde-backup"), before, 0o600)
    shared.atomic(path, output.getvalue().encode(), mode)
    try:
        shared.run("waydroid", "--details-to-stdout", "upgrade", "-o", timeout=120)
    except Exception:
        shared.atomic(path, before, mode)
        raise
    return True


def require_idle_runtime(initialized):
    # A newly installed daemon raises "not initialized" for GetSession. There
    # cannot be an Android session before initialization has completed.
    if not initialized: return
    import dbus
    bus = dbus.SystemBus()
    if bus.name_has_owner("id.waydro.Container"):
        manager = dbus.Interface(bus.get_object("id.waydro.Container", "/ContainerManager"), "id.waydro.ContainerManager")
        if manager.GetSession(timeout=5): raise RuntimeError("Close the active Android session before runtime setup; existing sessions are never taken over")


def system(source, software_rendering=False):
    shared.platform()
    if os.geteuid() != 0: raise RuntimeError("Android system setup requires a guest administrator")
    os.umask(0o022)
    if os.uname().machine != "x86_64": raise RuntimeError("This bundled Android preset requires x86_64")
    data = load(source)
    cfg_path = Path("/var/lib/waydroid/waydroid.cfg")
    rootfs = Path("/var/lib/waydroid/rootfs")
    if cfg_path.is_symlink() or rootfs.is_symlink(): raise RuntimeError("Refusing redirected Android runtime configuration")
    initialized = cfg_path.is_file() and rootfs.is_dir()
    minimum = 512*1024**2 if initialized else 6*1024**3
    if shutil.disk_usage("/var/lib").free < minimum: raise RuntimeError("Insufficient guest space for Android setup; preserve existing data and free guest build space first")
    repo = data["repository"]
    with tempfile.TemporaryDirectory(prefix="isolde-android-") as temporary:
        scratch = Path(temporary)
        key = scratch/"waydroid.gpg"
        verified_download(repo["keyUrl"], key, repo["keySha256"], 1024*1024)
        shared.atomic(Path("/usr/share/keyrings/isolde-waydroid.gpg"), key.read_bytes(), 0o644)
        shared.atomic(Path("/etc/apt/sources.list.d/isolde-waydroid.list"),
                      b"deb [signed-by=/usr/share/keyrings/isolde-waydroid.gpg] https://repo.waydro.id/ resolute main\n", 0o644)
        shared.atomic(Path("/etc/apt/preferences.d/isolde-waydroid"),
                      b"Package: *\nPin: origin repo.waydro.id\nPin-Priority: 100\n", 0o644)
        shared.run("apt-get", "update")
        shared.run("apt-get", "install", "-y", "waydroid="+data["runtimeVersion"], "aapt", "apksigner", timeout=900)
        require_idle_runtime(initialized)
        app = data["apps"]["smarttube"]
        apk = scratch/"smarttube.apk"
        verified_download(app["url"], apk, app["sha256"], 128*1024*1024)
        shared.run("apksigner", "verify", str(apk), timeout=60)
        metadata = shared.run("aapt", "dump", "badging", str(apk), capture_output=True, text=True, timeout=30).stdout
        identity = re.search(r"^package: name='([^']+)' versionCode='[^']+' versionName='([^']+)'", metadata, re.MULTILINE)
        if not identity or identity.groups() != (app["packageName"], app["version"]): raise RuntimeError("APK package/version does not match the Android preset")
        native = re.search(r"^native-code:\s*(.*)$", metadata, re.MULTILINE)
        if not native or "'x86'" not in native.group(1): raise RuntimeError("APK lacks the selected Android ABI")
        shared.atomic(Path("/usr/share/isolde/android/smarttube.apk"), apk.read_bytes(), 0o644)
    if not initialized:
        shared.run("waydroid", "--details-to-stdout", "init", "-s", data["systemType"], timeout=1800)
    # Never force reinitialize an existing runtime or delete its userdata.
    if software_rendering:
        configure_software_rendering(cfg_path)
    shared.run("systemctl", "enable", "--now", "waydroid-container.service", timeout=60)
    shared.atomic(Path("/var/lib/isolde/android-install.json"), json.dumps({
        "schemaVersion": 1, "runtime": "waydroid", "runtimeVersion": data["runtimeVersion"],
        "softwareRenderingRequested": software_rendering, "apkStaged": True,
        "apkSha256": data["apps"]["smarttube"]["sha256"],
        "profileAppInstalled": False, "launchVerified": False,
        "configurationSha256": hashlib.sha256((source/"config/android-runtime.json").read_bytes()).hexdigest()
    }, indent=2).encode(), 0o644)


def system_receipt():
    receipt = Path("/var/lib/isolde/android-install.json")
    if not receipt.is_file(): return None
    if receipt.is_symlink() or receipt.stat().st_uid != 0 or receipt.stat().st_size > 65536:
        raise RuntimeError("Invalid Android installer receipt")
    return json.loads(receipt.read_text())


def activate(source):
    """Publish the verified Android app through the ordinary Isolde catalog."""
    if os.geteuid() == 0: raise RuntimeError("Android app activation requires the regular Isolde user")
    receipt = system_receipt()
    if receipt is None: return False
    expected = hashlib.sha256((source/"config/android-runtime.json").read_bytes()).hexdigest()
    if receipt.get("configurationSha256") != expected:
        raise RuntimeError("Android system setup does not match this release")
    load(source)
    config = Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home()/".config")))/"Isolde"
    data = Path(os.environ.get("XDG_DATA_HOME", str(Path.home()/".local/share")))/"Isolde"
    state_path = config/"installed-apps.json"
    if state_path.is_symlink() or (state_path.exists() and state_path.stat().st_size > 1024*1024):
        raise ValueError("Invalid installed app state; refusing overwrite")
    state = json.loads(state_path.read_text()) if state_path.exists() else {"installed": [], "packageStates": {}}
    if not isinstance(state.get("installed"), list) or not isinstance(state.get("packageStates", {}), dict):
        raise ValueError("Invalid installed app state; refusing overwrite")
    manifest_path = source/"config/packages/smarttube/manifest.json"
    manifest = json.loads(manifest_path.read_text())
    if manifest.get("packageId") != "smarttube" or manifest.get("app", {}).get("type") != "android":
        raise ValueError("Invalid bundled SmartTube manifest")
    shared.atomic(data/"packages/smarttube"/manifest["version"]/"manifest.json", manifest_path.read_bytes())
    if "smarttube" not in state["installed"]: state["installed"].append("smarttube")
    state.setdefault("packageStates", {}).setdefault("smarttube", "ready")
    shared.atomic(state_path, json.dumps(state, indent=2).encode())
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--software-rendering", action="store_true")
    parser.add_argument("--activate-if-configured", action="store_true", help="Register SmartTube in All Apps as the regular user, if Android setup exists")
    args = parser.parse_args()
    if args.activate_if_configured:
        print(json.dumps({"smartTubeRegistered": activate(args.source.resolve())}))
    else:
        system(args.source.resolve(), args.software_rendering)
