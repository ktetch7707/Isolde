"""Typed private-storage adapters for the six pinned expanded runtime routes.

Called only by emulator-session inside the native namespace. No downloads,
shells, firmware distribution, account access or external configuration.
"""
import ctypes
import ctypes.util
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import signal
import stat
import struct
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET
import zipfile

MAX_FIRMWARE_BYTES=2*1024**3
MAX_GAME_BYTES=8*1024**3
MAX_FILES=32768
MCPX_MD5="d49c52a4102f6df7bcf8d0617ac475ed"


def sha256(path,limit):
    total=0; result=hashlib.sha256()
    with Path(path).open("rb") as source:
        while block:=source.read(1024*1024):
            total+=len(block)
            if total>limit:raise ValueError("Emulator input exceeds its byte limit")
            result.update(block)
    return result.hexdigest()


def archive_entries(archive,limit=MAX_GAME_BYTES,allowed=None):
    rows=archive.infolist();names=set();files=set();total=0
    if not rows or len(rows)>MAX_FILES:raise ValueError("Game or firmware archive has too many entries")
    for entry in rows:
        name=entry.filename.rstrip("/");path=PurePosixPath(name)
        if (not name or path.is_absolute() or str(path)!=name or ".." in path.parts
                or any(c in name for c in "\\\0\r\n:") or name.casefold() in names
                or stat.S_ISLNK(entry.external_attr>>16) or entry.flag_bits&1
                or stat.S_IFMT(entry.external_attr>>16) not in (0,stat.S_IFREG,stat.S_IFDIR)
                or entry.compress_type not in (zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED)
                or (allowed is not None and name not in allowed)):
            raise ValueError("Game or firmware archive contains unsafe, duplicate or unsupported paths")
        names.add(name.casefold());total+=entry.file_size
        if not entry.is_dir():files.add(name.casefold())
        if total>limit:raise ValueError("Expanded game or firmware exceeds its byte limit")
    for entry in rows:
        parts=PurePosixPath(entry.filename.rstrip("/")).parts
        if any("/".join(parts[:i]).casefold() in files for i in range(1,len(parts))):
            raise ValueError("Archive file and directory paths collide")
    return rows


def unpack(archive,destination,limit,allowed=None):
    with zipfile.ZipFile(archive) as z:
        for entry in archive_entries(z,limit,allowed):
            path=destination/entry.filename
            if entry.is_dir():path.mkdir(parents=True,exist_ok=True);continue
            path.parent.mkdir(parents=True,exist_ok=True)
            with z.open(entry) as source,path.open("xb") as output:
                total=0
                while block:=source.read(1024*1024):
                    total+=len(block)
                    if total>entry.file_size:raise ValueError("Archive entry exceeds its declared size")
                    output.write(block)
                if total!=entry.file_size:raise ValueError("Archive entry is truncated")
                output.flush();os.fsync(output.fileno())
            path.chmod(0o600)


def tree_manifest(root,limit):
    result=[];total=0;count=0
    for parent,directories,files in os.walk(root,followlinks=False):
        for name in directories+files:
            count+=1
            path=Path(parent)/name;mode=path.lstat().st_mode
            if stat.S_ISLNK(mode) or not (stat.S_ISDIR(mode) or stat.S_ISREG(mode)):
                raise ValueError("Prepared runtime data contains redirected or special files")
            if count>MAX_FILES:raise ValueError("Prepared runtime data has too many files")
            if stat.S_ISREG(mode):
                size=path.stat().st_size;total+=size
                if total>limit:raise ValueError("Prepared runtime data exceeds its byte limit")
                result.append([path.relative_to(root).as_posix(),size,sha256(path,limit)])
    return sorted(result)


def gamepad(sdl_version):
    library=ctypes.util.find_library("SDL3" if sdl_version==3 else "SDL2")
    if not library:raise ValueError("The SDL controller runtime is missing. Repair emulators in Isolde")
    sdl=ctypes.CDLL(library)
    sdl.SDL_Init.argtypes=[ctypes.c_uint32]
    sdl.SDL_Init.restype=ctypes.c_bool if sdl_version==3 else ctypes.c_int
    initialized=sdl.SDL_Init(0x2000)
    if (sdl_version==3 and not initialized) or (sdl_version==2 and initialized!=0):
        raise ValueError("Controller discovery failed. Reconnect the controller and retry")
    try:
        if sdl_version==3:
            count=ctypes.c_int();sdl.SDL_GetGamepads.argtypes=[ctypes.POINTER(ctypes.c_int)]
            sdl.SDL_GetGamepads.restype=ctypes.POINTER(ctypes.c_uint32)
            devices=sdl.SDL_GetGamepads(ctypes.byref(count))
            try:
                if count.value<=0 or count.value>32 or not devices:raise ValueError("Connect an Xbox controller, then retry Play")
                sdl.SDL_GetGamepadNameForID.argtypes=[ctypes.c_uint32];sdl.SDL_GetGamepadNameForID.restype=ctypes.c_char_p
                value=sdl.SDL_GetGamepadNameForID(devices[0]);guid=""
            finally:
                if devices:sdl.SDL_free.argtypes=[ctypes.c_void_p];sdl.SDL_free(devices)
        else:
            class GUID(ctypes.Structure):_fields_=[("data",ctypes.c_ubyte*16)]
            sdl.SDL_NumJoysticks.restype=ctypes.c_int
            sdl.SDL_IsGameController.argtypes=[ctypes.c_int];sdl.SDL_IsGameController.restype=ctypes.c_int
            count=sdl.SDL_NumJoysticks()
            if count<0 or count>32:raise ValueError("Controller discovery exceeded its device limit")
            index=next((i for i in range(count) if sdl.SDL_IsGameController(i)),None)
            if index is None:raise ValueError("Connect an Xbox controller, then retry Play")
            sdl.SDL_GameControllerNameForIndex.argtypes=[ctypes.c_int];sdl.SDL_GameControllerNameForIndex.restype=ctypes.c_char_p
            value=sdl.SDL_GameControllerNameForIndex(index)
            sdl.SDL_JoystickGetDeviceGUID.argtypes=[ctypes.c_int];sdl.SDL_JoystickGetDeviceGUID.restype=GUID
            guid=bytes(sdl.SDL_JoystickGetDeviceGUID(index).data).hex()
        name=value.decode("utf-8",errors="strict") if value else ""
        if not name or len(name)>160 or any(ord(c)<32 for c in name):raise ValueError("The controller name is invalid")
        return {"name":name,"guid":guid}
    finally:sdl.SDL_Quit()


class Adapter:
    def __init__(self,runtime,game,home,options,atomic,protected):
        self.runtime=runtime;self.game=game;self.home=protected(home);self.options=options
        self.atomic=atomic;self.protected=protected;self.binary=runtime["binary"]
        self.generated=protected(home/".config/isolde-emulator/generated")
        self.generated.mkdir(parents=True,exist_ok=True,mode=0o700)

    def write(self,path,value):
        self.atomic(path,json.dumps(value,ensure_ascii=True,indent=2).encode())

    def asset(self):
        name=self.options["firmware"]
        if not re.fullmatch(r"[a-f0-9]{64}\.bin",name):raise ValueError("Firmware is required. Import it in Emulator setup")
        path=self.protected(self.home/"firmware"/name)
        if not path.is_file() or sha256(path,512*1024**2)+".bin"!=name:
            raise ValueError("Firmware is missing or changed. Import it again")
        return path

    def run(self,arguments,home,timeout=300):
        """Bound preparation output and elapsed time; cancel only its own child."""
        environment=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/".config"),
            XDG_DATA_HOME=str(home/".local/share"),XDG_CACHE_HOME=str(home/".cache"))
        for path in (home,home/".config",home/".local/share",home/".cache"):
            self.protected(path).mkdir(parents=True,exist_ok=True,mode=0o700)
        print("Preparing private "+self.runtime["name"]+" data. Back returns safely; retry will preserve saves.",flush=True)
        with tempfile.TemporaryFile() as output:
            child=subprocess.Popen(arguments,env=environment,cwd=home,stdout=output,stderr=output)
            deadline=time.monotonic()+timeout
            try:
                while child.poll() is None:
                    if time.monotonic()>deadline or os.fstat(output.fileno()).st_size>4*1024**2:
                        raise ValueError("Emulator preparation exceeded its time or output limit. Retry in Games")
                    time.sleep(.05)
                if child.returncode:
                    output.seek(max(0,os.fstat(output.fileno()).st_size-4096))
                    raise ValueError("Emulator preparation failed: "+output.read().decode("utf-8",errors="replace"))
            finally:
                if child.poll() is None:
                    child.terminate()
                    try:child.wait(timeout=2)
                    except subprocess.TimeoutExpired:child.kill();child.wait(timeout=2)

    def cached(self,kind,identity,prepare,limit):
        base=self.protected(self.home/".local/share/isolde-prepared"/kind)
        base.mkdir(parents=True,exist_ok=True,mode=0o700)
        final=self.protected(base/identity);receipt=self.protected(base/(identity+".json"))
        if final.exists():
            if not receipt.is_file() or receipt.stat().st_size>8*1024**2:
                raise ValueError("Prepared emulator data is incomplete. Review storage before retrying")
            record=json.loads(receipt.read_text())
            if record!={"schemaVersion":1,"source":identity,"files":tree_manifest(final,limit)}:
                raise ValueError("Prepared emulator data changed. Review storage before retrying")
            return final
        if receipt.exists():
            if receipt.stat().st_size>8*1024**2:raise ValueError("Prepared emulator receipt is invalid")
            record=json.loads(receipt.read_text())
            if (not isinstance(record,dict) or set(record)!={"schemaVersion","source","files"}
                    or record["schemaVersion"]!=1 or record["source"]!=identity or not isinstance(record["files"],list)):
                raise ValueError("Prepared emulator receipt is invalid; existing data was preserved")
            # A crash after receipt fsync but before publication left no live
            # payload. Retire only this generated receipt so Retry can prepare it.
            receipt.unlink()
        from itertools import islice
        if len(list(islice(base.iterdir(),33)))>=32:
            raise ValueError("Prepared emulator storage is incomplete or full. Existing data was preserved")
        with tempfile.TemporaryDirectory(prefix=".stage-",dir=base) as temporary:
            stage=Path(temporary);payload=stage/"payload";payload.mkdir()
            prepare(stage,payload)
            files=tree_manifest(payload,limit)
            if not files:raise ValueError("The runtime produced no usable firmware or game files")
            # Receipt first: a failed/crashed rename cannot produce a ready cache.
            self.write(receipt,{"schemaVersion":1,"source":identity,"files":files})
            try:os.replace(payload,final)
            except BaseException:receipt.unlink();raise
        return final

    def firmware(self):
        asset=self.asset();identity=asset.stem
        def prepare(stage,payload):
            private=stage/"home"
            if self.runtime["engine"]=="rpcs3":
                self.run([self.binary,"--headless","--installfw",str(asset)],private)
                source=private/".config/rpcs3/dev_flash"
                if not (source/"sys/external/liblv2.sprx").is_file():raise ValueError("PS3 firmware preparation is incomplete")
                os.replace(source,payload/"dev_flash")
            elif self.runtime["engine"]=="vita3k":
                with asset.open("rb") as source:header=source.read(8)
                if header.startswith(b"PK"):
                    unpack(asset,stage/"pups",512*1024**2,{"firmware.pup","fonts.pup"})
                    pups=[stage/"pups/firmware.pup"]
                    if (stage/"pups/fonts.pup").is_file():pups.append(stage/"pups/fonts.pup")
                else:pups=[asset]
                for pup in pups:
                    if not pup.is_file():raise ValueError("Vita firmware ZIP needs firmware.pup")
                    with pup.open("rb") as source:
                        if not source.read(8).startswith(b"SCEUF"):raise ValueError("Invalid Vita update PUP")
                    self.run([self.binary,"--firmware",str(pup)],private)
                source=private/".local/share/Vita3K/Vita3K"
                if not (source/"vs0/sys/external/libhttp.suprx").is_file():raise ValueError("Vita firmware preparation is incomplete")
                for name in ("vs0","sa0"):
                    if (source/name).is_dir():os.replace(source/name,payload/name)
            else:
                unpack(asset,payload,MAX_FIRMWARE_BYTES,{"mcpx.bin","flash.bin","hdd.qcow2","eeprom.bin"})
                self.validate_xbox(payload)
        return self.cached("firmware",identity,prepare,MAX_FIRMWARE_BYTES)

    def validate_xbox(self,root):
        mcpx=root/"mcpx.bin";flash=root/"flash.bin";disk=root/"hdd.qcow2"
        if (not mcpx.is_file() or mcpx.stat().st_size!=512
                or hashlib.md5(mcpx.read_bytes()).hexdigest()!=MCPX_MD5
                or not flash.is_file() or flash.stat().st_size not in (256*1024,512*1024,1024*1024)):
            raise ValueError("Xbox boot ZIP needs a valid 512-byte MCPX 1.0 and compatible 256/512/1024 KiB BIOS")
        self.qcow(disk)
        if (root/"eeprom.bin").exists() and (root/"eeprom.bin").stat().st_size!=256:
            raise ValueError("Xbox EEPROM must be exactly 256 bytes")

    def qcow(self,path):
        self.protected(path)
        if not path.is_file() or not 72<=path.stat().st_size<=MAX_FIRMWARE_BYTES:
            raise ValueError("Choose a bounded standalone QCOW2 Xbox hard disk")
        with path.open("rb") as source:header=source.read(104)
        version=struct.unpack_from(">I",header,4)[0]
        if (header[:4]!=b"QFI\xfb" or struct.unpack_from(">I",header,4)[0] not in (2,3)
                or struct.unpack_from(">Q",header,8)[0]!=0 or struct.unpack_from(">I",header,16)[0]!=0
                or not 1024**2<=struct.unpack_from(">Q",header,24)[0]<=16*1024**3
                or struct.unpack_from(">I",header,32)[0]!=0
                or not 9<=struct.unpack_from(">I",header,20)[0]<=21
                or (version==3 and (len(header)<104 or struct.unpack_from(">Q",header,72)[0]&~1))):
            raise ValueError("Xbox disk must be unencrypted standalone QCOW2 with no backing file")

    def private_copy_once(self,source,destination):
        self.protected(destination)
        if destination.exists():return
        destination.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
        fd,name=tempfile.mkstemp(prefix=".copy-",dir=destination.parent)
        try:
            with os.fdopen(fd,"wb") as output,source.open("rb") as input:
                shutil.copyfileobj(input,output,1024*1024);output.flush();os.fsync(output.fileno())
            # Exclusive link publishes without ever replacing an existing save.
            os.link(name,destination)
        finally:Path(name).unlink(missing_ok=True)

    def vita_links(self,firmware):
        root=self.protected(self.home/".local/share/Vita3K/Vita3K");root.mkdir(parents=True,exist_ok=True,mode=0o700)
        for name in ("vs0","sa0"):
            source=firmware/name;destination=root/name
            if not source.is_dir():continue
            if destination.is_symlink():
                old=destination.resolve()
                expected=self.home/".local/share/isolde-prepared/firmware"
                if not old.is_relative_to(expected) or old.name!=name:
                    raise ValueError("Vita firmware link is redirected; existing data was preserved")
            elif destination.exists():
                raise ValueError("Vita firmware directory is unmanaged. Review it before replacing firmware")
            link=root/(".isolde-"+name)
            if link.exists() or link.is_symlink():raise ValueError("Vita firmware link update is incomplete; review storage")
            try:link.symlink_to(source,target_is_directory=True);os.replace(link,destination)
            finally:link.unlink(missing_ok=True)
        return root

    def rpc_input(self):
        pad=gamepad(3);mapping={"Cross":"South","Circle":"East","Square":"West","Triangle":"North",
            "Up":"Up","Down":"Down","Left":"Left","Right":"Right","Start":"Start","Select":"Back",
            "PS Button":"Guide","L1":"LB","R1":"RB","L2":"LT","R2":"RT","L3":"LS","R3":"RS",
            "Left Stick Left":"LS X-","Left Stick Right":"LS X+","Left Stick Up":"LS Y-","Left Stick Down":"LS Y+",
            "Right Stick Left":"RS X-","Right Stick Right":"RS X+","Right Stick Up":"RS Y-","Right Stick Down":"RS Y+"}
        value={"Player 1 Input":{"Handler":"SDL","Device":pad["name"]+" 1","Config":mapping}}
        self.write(self.home/".config/rpcs3/input_configs/global/Isolde.yml",value)

    def cemu_input(self):
        pad=gamepad(2);root=ET.Element("emulated_controller");ET.SubElement(root,"type").text="Wii U GamePad"
        ET.SubElement(root,"toggle_display").text="1"
        controller=ET.SubElement(root,"controller")
        for key,value in {"api":"SDLController","uuid":"0_"+pad["guid"],"display_name":pad["name"]}.items():ET.SubElement(controller,key).text=value
        mappings=ET.SubElement(controller,"mappings")
        # IDs come from Cemu 2.6 VPADController::ButtonId / Buttons2.
        bindings={1:1,2:0,3:3,4:2,5:9,6:10,7:42,8:43,9:6,10:4,11:11,12:12,13:13,14:14,
            15:7,16:8,17:45,18:39,19:44,20:38,21:47,22:41,23:46,24:40,26:4,27:5}
        for mapping,button in bindings.items():
            entry=ET.SubElement(mappings,"entry");ET.SubElement(entry,"mapping").text=str(mapping);ET.SubElement(entry,"button").text=str(button)
        self.atomic(self.home/".config/Cemu/controllerProfiles/controller0.xml",ET.tostring(root,encoding="utf-8",xml_declaration=True))

    def command(self):
        engine=self.runtime["engine"]
        if self.options["video"]!="automatic":raise ValueError("This runtime has no software-renderer preset. Choose automatic in Emulator setup")
        self.protected(self.home/"saves").mkdir(parents=True,exist_ok=True,mode=0o700)
        if engine=="rpcs3":
            firmware=self.firmware();self.rpc_input()
            self.write(self.home/".config/rpcs3/vfs.yml",{"/dev_flash/":str(firmware/"dev_flash")+"/"})
            config=self.generated/"rpcs3.yml"
            self.write(config,{"Video":{"Renderer":"Vulkan"},"Miscellaneous":{"Exit RPCS3 when process finishes":True}})
            game=self.game
            if game.suffix.lower()==".pkg":
                identity=sha256(game,4*1024**3)
                def install(stage,payload):
                    private=stage/"home";self.run([self.binary,"--headless","--installpkg",str(game)],private)
                    source=private/".config/rpcs3/dev_hdd0/game"
                    if not source.is_dir():raise ValueError("The PS3 package did not install a game")
                    for path in source.iterdir():os.replace(path,payload/path.name)
                installed=self.cached("ps3-games",identity,install,MAX_GAME_BYTES)
                candidates=list(installed.glob("*/USRDIR/EBOOT.BIN"))
                if len(candidates)!=1:raise ValueError("Choose a PS3 package containing exactly one playable title")
                game=candidates[0]
            return [self.binary,"--no-gui","--fullscreen","--config",str(config),"--input-config","Isolde",str(game)]
        if engine=="shadps4":
            root=self.protected(self.home/".local/share/shadPS4");root.mkdir(parents=True,exist_ok=True,mode=0o700)
            return [self.binary,"--fullscreen","true","--override-root",str(root),"--game",str(self.game)]
        if engine=="vita3k":
            root=self.vita_links(self.firmware())
            with zipfile.ZipFile(self.game) as z:
                archive_entries(z)
                if not {"eboot.bin","sce_sys/param.sfo"}.issubset(z.namelist()):raise ValueError("Choose an unencrypted VPK containing one game")
            config=self.generated/"vita.yml"
            self.write(config,{"pref-path":str(root),"initial-setup":True,"check-for-updates":False,
                "check-for-updates-mode":0,"backend-renderer":"Vulkan","show-gui":False})
            return [self.binary,"--frontend","sdl","--fullscreen","--load-config","--config-location",str(config),str(self.game)]
        if engine=="xemu":
            firmware=self.firmware();self.validate_xbox(firmware)
            with self.game.open("rb") as disc:
                disc.seek(32*2048)
                if disc.read(20)!=b"MICROSOFT*XBOX*MEDIA":raise ValueError("Choose an Xbox XISO image, not an ordinary PC ISO")
            saves=self.protected(self.home/"saves/xemu");saves.mkdir(parents=True,exist_ok=True,mode=0o700)
            self.private_copy_once(firmware/"hdd.qcow2",saves/"hdd.qcow2");self.qcow(saves/"hdd.qcow2")
            if (firmware/"eeprom.bin").exists():self.private_copy_once(firmware/"eeprom.bin",saves/"eeprom.bin")
            if (saves/"eeprom.bin").exists() and ((saves/"eeprom.bin").is_symlink() or (saves/"eeprom.bin").stat().st_size!=256):
                raise ValueError("Private Xbox EEPROM changed. Review it before playing")
            config=self.generated/"xemu.toml"
            files={"bootrom_path":str(firmware/"mcpx.bin"),"flashrom_path":str(firmware/"flash.bin"),
                "hdd_path":str(saves/"hdd.qcow2"),"eeprom_path":str(saves/"eeprom.bin")}
            self.atomic(config,("[general]\nshow_welcome = false\n[general.updates]\ncheck = false\n[input]\nauto_bind = true\n[net]\nenable = false\n[sys.files]\n"
                +"".join(k+" = "+json.dumps(v)+"\n" for k,v in files.items())).encode())
            return [self.binary,"-full-screen","-config_path",str(config),"-dvd_path",str(self.game)]
        if engine=="xenia":
            storage=self.protected(self.home/".local/share/xenia");storage.mkdir(parents=True,exist_ok=True,mode=0o700)
            self.protected(self.home/"saves/xenia").mkdir(parents=True,exist_ok=True,mode=0o700)
            return [self.binary,"--fullscreen=true","--hid=sdl","--gpu=vulkan","--discord=false",
                "--storage_root="+str(storage),"--content_root="+str(self.home/"saves/xenia"),"--target="+str(self.game)]
        if engine=="cemu":
            self.protected(self.home/"saves/cemu-mlc").mkdir(parents=True,exist_ok=True,mode=0o700)
            self.cemu_input();root=ET.Element("content")
            for key,value in {"check_update":"false","receive_untested_updates":"false","fullscreen":"true",
                    "mlc_path":str(self.home/"saves/cemu-mlc")}.items():ET.SubElement(root,key).text=value
            self.atomic(self.home/".config/Cemu/settings.xml",ET.tostring(root,encoding="utf-8",xml_declaration=True))
            return [self.binary,"--fullscreen","--mlc",str(self.home/"saves/cemu-mlc"),"--game",str(self.game)]
        raise ValueError("Unknown expanded emulator route")


def command(runtime,game,home,options,atomic,protected):
    if os.environ.get("ISOLDE_NATIVE_NAMESPACE")!="1":raise ValueError("Open emulators through Isolde Games")
    directory=protected(home/".config/isolde-emulator");directory.mkdir(parents=True,exist_ok=True,mode=0o700)
    lock=protected(directory/"prepare.lock")
    with lock.open("a+b") as stream:
        try:fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise ValueError("Another emulator preparation is active. Wait, then retry")
        def cancelled(_signal,_frame):raise ValueError("Emulator preparation canceled. Existing saves were preserved")
        previous=signal.signal(signal.SIGTERM,cancelled)
        try:
            return Adapter(runtime,game,home,options,atomic,protected).command()
        except (zipfile.BadZipFile,struct.error,EOFError) as error:
            raise ValueError("The selected game or firmware archive is invalid. Choose it again in Games") from error
        finally:signal.signal(signal.SIGTERM,previous)
