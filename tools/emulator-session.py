#!/usr/bin/env python3
"""Trusted native-namespace emulator adapter. No downloads or shell execution.

Only packaged runtime identities are accepted. HOME is the native sandbox's
profile/app-private home; firmware, settings and saves never use the host home.
"""
import argparse
import ctypes
import ctypes.util
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile

MAX_SETTINGS = 16384
OPTIONS = {"video": {"automatic", "software"}, "controller": {"xbox", "xbox-wii-classic"}}


def read_json(path, limit=MAX_SETTINGS):
    path = Path(path)
    if path.is_symlink() or path.stat().st_size > limit:
        raise ValueError("Invalid or redirected emulator configuration")
    return json.loads(path.read_text())


def definitions(path):
    document = read_json(path, 65536)
    if document.get("schemaVersion") != 1 or not isinstance(document.get("runtimes"), list):
        raise ValueError("Unsupported emulator manifest")
    return {item["id"]: item for item in document["runtimes"]}


def protected(path):
    """Refuse redirects at every ancestor, including dangling links."""
    path = Path(path).absolute()
    for ancestor in [path, *path.parents]:
        if ancestor.is_symlink():
            raise ValueError("Emulator storage is redirected; repair it before playing")
    return path


def atomic(path, data):
    path = protected(path)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, temporary = tempfile.mkstemp(prefix=".emulator-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(data); output.flush(); os.fsync(output.fileno())
        os.replace(temporary, path)
        directory = os.open(path.parent, os.O_DIRECTORY)
        try: os.fsync(directory)
        finally: os.close(directory)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)


def settings(home):
    path = protected(home / ".config/isolde-emulator/settings.json")
    if not path.exists():
        return {"schemaVersion": 1, "video": "automatic", "controller": "xbox", "firmware": ""}
    result = read_json(path)
    if (set(result) != {"schemaVersion", "video", "controller", "firmware"}
            or type(result["schemaVersion"]) is not int or result["schemaVersion"] != 1
            or any(not isinstance(result[key],str) or result[key] not in values for key, values in OPTIONS.items())
            or not isinstance(result["firmware"], str)
            or (result["firmware"] and not re.fullmatch(r"[a-f0-9]{64}\.bin", result["firmware"]))):
        raise ValueError("Invalid emulator settings. Review Emulator setup in Isolde")
    return result


def ini(path, sections):
    # These files are generated exclusively by Isolde, never imported user INI.
    result = "".join("[" + section + "]\n" + "".join(k+" = "+str(v)+"\n" for k,v in values.items())
                     + "\n" for section,values in sections.items())
    atomic(path, result.encode())


def ps2_firmware(home, options):
    name = options["firmware"]
    if not name:
        raise ValueError("PS2 firmware is required. Import your firmware in Emulator setup")
    path = protected(home / "firmware" / name)
    if not path.is_file() or path.stat().st_size not in (4*1024**2, 8*1024**2):
        raise ValueError("PS2 firmware is missing or has an invalid size. Import it again")
    content = path.read_bytes()
    if hashlib.sha256(content).hexdigest()+".bin" != name or b"ROMVER" not in content or b"ROMDIR" not in content:
        raise ValueError("PS2 firmware changed or is invalid. Import it again")
    return path


def dolphin_controller():
    """Use the same SDL3 gamepad names as the pinned Dolphin build."""
    library=ctypes.util.find_library("SDL3")
    if not library:return None
    sdl=ctypes.CDLL(library)
    sdl.SDL_Init.argtypes=[ctypes.c_uint32];sdl.SDL_Init.restype=ctypes.c_bool
    sdl.SDL_GetGamepads.argtypes=[ctypes.POINTER(ctypes.c_int)];sdl.SDL_GetGamepads.restype=ctypes.POINTER(ctypes.c_uint32)
    sdl.SDL_GetGamepadNameForID.argtypes=[ctypes.c_uint32];sdl.SDL_GetGamepadNameForID.restype=ctypes.c_char_p
    sdl.SDL_free.argtypes=[ctypes.c_void_p]
    if not sdl.SDL_Init(0x2000):return None
    try:
        count=ctypes.c_int();devices=sdl.SDL_GetGamepads(ctypes.byref(count))
        try:
            if not devices or count.value<1:return None
            name=sdl.SDL_GetGamepadNameForID(devices[0])
            if not name:return None
            value=name.decode("utf-8",errors="strict")
            if len(value)>160 or any(c in value for c in "\r\n`[]/") or any(ord(c)<32 for c in value):
                raise ValueError("The controller name cannot be used safely")
            return value
        finally:
            if devices:sdl.SDL_free(devices)
    finally:sdl.SDL_Quit()


def command(runtime, target, home):
    home = protected(home)
    if not isinstance(target, str) or not target or len(target)>4096:
        raise ValueError("Choose a game in Isolde; the emulator desktop is not needed")
    game = Path(target)
    if not game.is_absolute() or not game.is_file() or game.is_symlink():
        raise ValueError("The selected game is missing or redirected")
    if game.suffix[1:].lower() not in runtime["formats"]:
        raise ValueError("This game format is not declared for the selected emulator")
    binary = Path(runtime["binary"])
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise ValueError("The emulator is not installed. Repair it in Isolde")
    options = settings(home)
    engine = runtime["engine"]
    if options["controller"]!="xbox" and engine!="dolphin":
        raise ValueError("This controller preset belongs to Dolphin only")
    base = home / ".config/isolde-emulator/generated"
    for path in (base, home/"saves", home/"states", home/"firmware"):
        protected(path).mkdir(parents=True, exist_ok=True, mode=0o700)
    if engine == "ppsspp":
        # v1.20.4 uses XDG_CONFIG_HOME/ppsspp as the memory-stick root on Linux.
        # --root is host0, and must never be used for save isolation.
        ini(home/".config/ppsspp/PSP/SYSTEM/ppsspp.ini", {
            "General": {"FirstRun": "False", "CheckForNewVersion": "False", "EnableDiscordPresence": "False"},
            "Graphics": {"GraphicsBackend": 0, "SoftwareRendering": str(options["video"]=="software"), "FullScreen": "True"},
            "Control": {"ShowTouchControls": "False"},
            "Network": {"EnableWlan": "False", "EnableAdhocServer": "False"}})
        return [str(binary), "--fullscreen", "--pause-menu-exit", "--escape-exit", str(game)]
    if engine == "dolphin":
        user = home/".local/share/dolphin-emu"
        ini(user/"Config/Dolphin.ini", {"General": {"ShowLag": "False"},
            "Interface": {"ConfirmStop": "False"}, "Display": {"Fullscreen": "True"},
            "Analytics": {"Enabled": "False", "PermissionAsked": "True"},
            "Core": {"SIDevice0": 6}, "AutoUpdate": {"UpdateTrack": ""}})
        controller=dolphin_controller()
        if controller:
            pad={"Device":"SDL/0/"+controller}
            for key,button in {"Buttons/A":"Button A","Buttons/B":"Button B","Buttons/X":"Button X","Buttons/Y":"Button Y",
                    "Buttons/Z":"Shoulder R","Buttons/Start":"Start","D-Pad/Up":"Pad N","D-Pad/Down":"Pad S",
                    "D-Pad/Left":"Pad W","D-Pad/Right":"Pad E","Main Stick/Up":"Left Y+","Main Stick/Down":"Left Y-",
                    "Main Stick/Left":"Left X-","Main Stick/Right":"Left X+","C-Stick/Up":"Right Y+","C-Stick/Down":"Right Y-",
                    "C-Stick/Left":"Right X-","C-Stick/Right":"Right X+","Triggers/L":"Trigger L","Triggers/R":"Trigger R",
                    "Triggers/L-Analog":"Trigger L","Triggers/R-Analog":"Trigger R"}.items():pad[key]="`"+button+"`"
            ini(user/"Config/GCPadNew.ini",{"GCPad1":pad})
            wii={"Device":"SDL/0/"+controller,"Source":1,"Extension":"Classic" if options["controller"]=="xbox-wii-classic" else "Nunchuk"}
            for key,button in {"Buttons/A":"Button A","Buttons/B":"Button B","Buttons/1":"Button X","Buttons/2":"Button Y",
                    "Buttons/+":"Start","Buttons/-":"Back","Buttons/Home":"Guide","D-Pad/Up":"Pad N","D-Pad/Down":"Pad S",
                    "D-Pad/Left":"Pad W","D-Pad/Right":"Pad E","IR/Up":"Right Y+","IR/Down":"Right Y-",
                    "IR/Left":"Right X-","IR/Right":"Right X+","Nunchuk/Stick/Up":"Left Y+","Nunchuk/Stick/Down":"Left Y-",
                    "Nunchuk/Stick/Left":"Left X-","Nunchuk/Stick/Right":"Left X+","Nunchuk/Buttons/C":"Shoulder L",
                    "Nunchuk/Buttons/Z":"Shoulder R","Shake/X":"Button Y","Shake/Y":"Button Y","Shake/Z":"Button Y"}.items():wii[key]="`"+button+"`"
            for key,button in {"Buttons/A":"Button A","Buttons/B":"Button B","Buttons/X":"Button X","Buttons/Y":"Button Y",
                    "Buttons/+":"Start","Buttons/-":"Back","Buttons/Home":"Guide","Buttons/ZL":"Shoulder L","Buttons/ZR":"Shoulder R",
                    "Triggers/L":"Trigger L","Triggers/R":"Trigger R","D-Pad/Up":"Pad N","D-Pad/Down":"Pad S",
                    "D-Pad/Left":"Pad W","D-Pad/Right":"Pad E","Left Stick/Up":"Left Y+","Left Stick/Down":"Left Y-",
                    "Left Stick/Left":"Left X-","Left Stick/Right":"Left X+","Right Stick/Up":"Right Y+","Right Stick/Down":"Right Y-",
                    "Right Stick/Left":"Right X-","Right Stick/Right":"Right X+"}.items():wii["Classic/"+key]="`"+button+"`"
            ini(user/"Config/WiimoteNew.ini",{"Wiimote1":wii})
        arguments = [str(binary), "--user", str(user), "--batch", "--exec", str(game)]
        if options["video"] == "software": arguments += ["--video_backend", "Software Renderer"]
        return arguments
    if engine == "pcsx2":
        firmware = ps2_firmware(home, options)
        config = home/".config/PCSX2/inis"
        pad = {"Type":"DualShock2"}
        for key, binding in {"Up":"DPadUp", "Down":"DPadDown", "Left":"DPadLeft", "Right":"DPadRight",
                "Cross":"A", "Circle":"B", "Square":"X", "Triangle":"Y", "Start":"Start", "Select":"Back",
                "L1":"LeftShoulder", "R1":"RightShoulder", "L2":"+LeftTrigger", "R2":"+RightTrigger",
                "L3":"LeftStick", "R3":"RightStick", "LLeft":"-LeftX", "LRight":"+LeftX",
                "LUp":"-LeftY", "LDown":"+LeftY", "RLeft":"-RightX", "RRight":"+RightX",
                "RUp":"-RightY", "RDown":"+RightY"}.items():
            pad[key] = "SDL-0/"+binding
        ini(config/"PCSX2.ini", {"UI": {"SettingsVersion": 1, "SetupWizardIncomplete": "false"},
            "Folders": {"Bios":str(firmware.parent),"MemoryCards":str(home/"saves"),"Savestates":str(home/"states")},
            "Filenames": {"BIOS":firmware.name}, "InputSources": {"SDL":"true"}, "Pad1":pad,
            "EmuCore/GS": {"Renderer":13 if options["video"]=="software" else -1}})
        # 2.6.3 has no -settings option. HOME/XDG isolation selects this profile's
        # normal configuration directory; do not invent a launch flag.
        return [str(binary), "-batch", "-nogui", "-fullscreen", "--", str(game)]
    if engine == "retroarch":
        core = Path(runtime["core"])
        if not core.is_file() or core.is_symlink():
            raise ValueError("This console core is missing. Repair the emulator in Isolde")
        config = base/"retroarch.cfg"
        values = {"video_fullscreen":"true", "menu_driver":"ozone", "input_joypad_driver":"sdl2",
            "input_autodetect_enable":"true", "config_save_on_exit":"false", "savefile_directory":str(home/"saves"),
            "savestate_directory":str(home/"states"), "system_directory":str(home/"firmware"),
            "joypad_autoconfig_dir":"/usr/share/libretro/autoconfig", "menu_show_online_updater":"false",
            "network_cmd_enable":"false", "stdin_cmd_enable":"false", "menu_mouse_enable":"false",
            "input_exit_emulator":"escape", "savestate_auto_save":"false", "savestate_auto_load":"false"}
        if options["video"]=="software": values["video_driver"]="sdl2"
        atomic(config, "".join(k+' = '+json.dumps(v)+'\n' for k,v in values.items()).encode())
        return [str(binary), "--fullscreen", "--config", str(config), "--libretro", str(core), str(game)]
    raise ValueError("Unsupported emulator engine")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", required=True)
    parser.add_argument("target")
    args = parser.parse_args()
    if os.environ.get("ISOLDE_NATIVE_NAMESPACE") != "1":
        raise ValueError("Emulators must be opened inside an Isolde profile session")
    runtime = definitions(Path(__file__).with_name("game-runtimes.json")).get(args.runtime)
    if not runtime: raise ValueError("Unknown emulator runtime")
    arguments = command(runtime, args.target, Path.home())
    os.execv(arguments[0], arguments)


if __name__ == "__main__":
    try: main()
    except (OSError, ValueError, KeyError) as error:
        print("Emulator setup: "+str(error), file=sys.stderr)
        sys.exit(2)
