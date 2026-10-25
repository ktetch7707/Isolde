#!/usr/bin/env python3
"""Explicit Isolde-Test VPN setup. Never collect account credentials or keys."""
import argparse
import configparser
import importlib.util
import json
import ipaddress
import os
from pathlib import Path
import re
import socket
import getpass
import subprocess
import tempfile
import signal
import time
import uuid as uuid_module
import hashlib
import stat

PROFILE = "default"
ROOT_COPY = False
OWNER_USER = None
OWNER_PROCESS = None
ROOT_ABORTED = False
COPY_HELPER = Path("/usr/libexec/isolde/vpn-copy")
COPY_POLICY = Path("/usr/share/polkit-1/actions/org.isolde.vpn.policy")
COPY_RULE = Path("/usr/share/polkit-1/rules.d/50-isolde-vpn.rules")


class SetupError(ValueError):
    """A user-facing validation error containing no input values or secrets."""

PACKAGES = {"surfshark": "surfshark", "protonvpn": "proton-vpn-gnome-desktop"}
ACCOUNTS = {"surfshark": "https://my.surfshark.com/vpn/manual-setup/main/wireguard",
            "protonvpn": "https://account.protonvpn.com/downloads"}
PROTON_URL = "https://repo.protonvpn.com/debian/dists/stable/main/binary-all/protonvpn-stable-release_1.0.10_all.deb"
PROTON_SHA = "c8a9fffb4ebdd63db08ce7f11c9b0ac394064ae6a210feb9ab63a0f7d8d060a7"


def command(*args, timeout=30):
    check_copy_owner()
    result = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    if len(result.stdout) > 65536 or result.returncode:
        raise RuntimeError("Operation failed. Check administrator approval and your connection, then Refresh before retrying. Installation may have partially completed.")
    return result.stdout.strip()


def check_copy_owner():
    global ROOT_ABORTED
    if not ROOT_COPY or OWNER_PROCESS is None or ROOT_ABORTED:return
    pid, started, uid = OWNER_PROCESS
    try:
        process = Path("/proc")/str(pid)
        valid = process.stat().st_uid == uid and process.joinpath("stat").read_text().rsplit(")",1)[1].split()[19] == started
    except OSError:valid = False
    if not valid:
        # Further commands are only the enclosing transaction's rollback.
        ROOT_ABORTED = True
        raise RuntimeError("VPN setup cancelled. The previous connection is being restored.")


def admin_command(program, *args, timeout=180):
    if os.geteuid() == 0:
        return command(program, *args, timeout=timeout)
    # Reuse the VM's existing narrowly delegated package-management grants.
    # This is a read-only authorization query, not a failed installation retry.
    if program in ("/usr/bin/apt-get", "/usr/bin/dpkg"):
        permission = subprocess.run(["sudo", "-n", "-l", program, *args], capture_output=True, timeout=5)
        if permission.returncode == 0:
            return command("sudo", "-n", program, *args, timeout=timeout)
    return command("pkexec", program, *args, timeout=timeout)


def guest_only():
    if os.name != "posix" or socket.gethostname() != "isolde-test":
        raise RuntimeError("VPN setup is available only inside Isolde-Test.")


def provider_package(provider):
    if provider not in PACKAGES:
        raise SetupError("Choose a supported VPN provider.")
    return PACKAGES[provider]


def system_user():
    if OWNER_USER:return OWNER_USER
    if os.name == "posix":
        import pwd
        return pwd.getpwuid(os.getuid()).pw_name
    return getpass.getuser()


def installed(provider):
    try:
        return command("dpkg-query", "-W", "-f=${Status}", provider_package(provider)) == "install ok installed"
    except RuntimeError:
        return False


def status():
    providers = [{"id": p, "name": "Surfshark" if p == "surfshark" else "Proton VPN", "installed": installed(p)} for p in PACKAGES]
    # No --show-secrets; only connection identity and activation state.
    rows = command("nmcli", "-t", "--escape", "no", "-f", "UUID,TYPE,NAME", "connection", "show")
    active = command("nmcli", "-t", "-f", "UUID", "connection", "show", "--active").splitlines()
    connections = []
    for line in rows.splitlines():
        cells = line.split(":", 2)
        if len(cells) == 3 and cells[1] in ("vpn", "wireguard") and re.fullmatch(r"[a-fA-F0-9-]{36}", cells[0]):
            connections.append({"uuid": cells[0], "name": cells[2][:160], "active": cells[0] in active})
    if len(connections) > 128:
        raise RuntimeError("Too many VPN connections (maximum 128).")
    owned = managed_connections()
    for entry in owned:
        entry["active"] = entry["uuid"] in active
    return {"providers": providers, "connections": owned, "activeVpnCount": sum(c["active"] for c in connections),
            "downloads": [{"name": p.name} for p in configuration_files()], "countries": countries()}


def nm_bindings():
    try:
        import gi
        gi.require_version("NM", "1.0")
        from gi.repository import NM, GLib, Gio
        return NM, GLib, Gio
    except (ImportError, ValueError):
        raise RuntimeError("Install a VPN provider to enable saved locations, then Refresh.") from None


def managed_connections():
    try:
        NM, _, _ = nm_bindings()
    except RuntimeError:
        return []
    result = []
    client = NM.Client.new(None)
    for connection in client.get_connections():
        identity = location_identity(connection)
        if not identity or identity["profile"] != PROFILE or connection.get_connection_type() != "wireguard":
            continue
        result.append({"uuid": connection.get_uuid(), "name": identity["name"], "country": identity["country"], "provider": identity["provider"]})
    if len(result) > 128:
        raise RuntimeError("Saved VPN locations are limited to 128 per profile.")
    return result


def location_identity(connection):
    name = connection.get_id()
    if not isinstance(name,str):return None
    match = re.fullmatch(r"IsoldeVPN/1/(default|[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12})/(surfshark|protonvpn)/([A-Z]{2}) — ([^\r\n]{1,160})",name)
    if not match:return None
    identity = dict(zip(("profile","provider","country","name"),match.groups()))
    metadata = connection.get_setting_by_name("user")
    # Optional user data must agree when a storage plugin preserves it.
    if metadata and any(metadata.get_data("org.isolde."+key)!=identity[key] for key in ("profile","country","provider")):return None
    return identity


def countries():
    result = {"CA": "Canada", "US": "United States"}
    path = Path("/usr/share/iso-codes/json/iso_3166-1.json")
    if path.is_file() and path.stat().st_size < 128 * 1024:
        for item in json.loads(path.read_text()).get("3166-1", []):
            if re.fullmatch(r"[A-Z]{2}", item.get("alpha_2", "")):
                result[item["alpha_2"]] = item.get("common_name", item["name"])
    return [{"code": code, "name": result[code]} for code in ["CA", "US"] + sorted(set(result) - {"CA", "US"}, key=lambda c: result[c])]


def configuration_files():
    directory = Path.home() / "Downloads"
    if directory.is_symlink() or not directory.is_dir() or directory.resolve() != directory.absolute():
        return []
    if any(str(directory.resolve()).startswith(prefix) for prefix in ("/media/", "/mnt/", "/run/media/")):
        return []
    files = sorted(p for p in directory.glob("*.conf") if not p.is_symlink() and p.is_file() and p.stat().st_size <= 16384 and len(p.name) <= 160)
    return files[:64]


def validate_configuration(path):
    parser = configparser.ConfigParser(interpolation=None, strict=True)
    parser.read_string(path.read_text(encoding="utf-8-sig"))
    fields = {"Interface": {"privatekey", "address", "dns", "mtu"},
              "Peer": {"publickey", "presharedkey", "allowedips", "endpoint", "persistentkeepalive"}}
    if parser.defaults() or set(parser.sections()) != set(fields):
        raise SetupError("Choose a standard WireGuard configuration with one peer.")
    for section, allowed in fields.items():
        if set(parser[section]) - allowed:
            raise SetupError("Configuration contains unsupported options. Scripts and custom routing are not accepted.")
    for section, key in [("Interface", "privatekey"), ("Peer", "publickey")]:
        if not re.fullmatch(r"[A-Za-z0-9+/]{43}=", parser[section].get(key, "")):
            raise SetupError("Configuration is missing a valid WireGuard key. Generate your key pair before downloading.")
    for address in parser["Interface"].get("address", "").split(","):
        ipaddress.ip_interface(address.strip())
    routes = [ipaddress.ip_network(p.strip()) for p in parser["Peer"].get("allowedips", "").split(",")]
    if ipaddress.ip_network("0.0.0.0/0") not in routes:
        raise SetupError("Choose a configuration routing all IPv4 traffic through the VPN.")
    endpoint = parser["Peer"].get("endpoint", "")
    if not re.fullmatch(r"(?:[A-Za-z0-9.-]+|\[[A-Fa-f0-9:]+\]):[0-9]{1,5}", endpoint) or not 1 <= int(endpoint.rsplit(":", 1)[1]) <= 65535:
        raise SetupError("Invalid VPN server address.")
    for dns in parser["Interface"].get("dns", "").split(","):
        ipaddress.ip_address(dns.strip())
    return parser


def import_configuration(name, provider, country):
    provider_package(provider)
    if country not in {c["code"] for c in countries()}:
        raise SetupError("Choose the country used when downloading this configuration.")
    selected = next((p for p in configuration_files() if p.name == name), None)
    if selected is None:
        raise SetupError("Choose a configuration from this guest's Downloads folder and Refresh.")
    descriptor = os.open(selected, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(descriptor, "rb") as source:
        data = source.read(16385)
    if len(data) > 16384:
        raise SetupError("Configuration exceeds 16 KiB.")
    NM, _, _ = nm_bindings()
    with tempfile.TemporaryDirectory(prefix="isolde-vpn-import-") as directory:
        snapshot = Path(directory) / "connection.conf"
        snapshot.touch(mode=0o600)
        snapshot.write_bytes(data)
        validate_configuration(snapshot)
        connection = NM.conn_wireguard_import(str(snapshot))
    saved = save_location(connection, provider, country, selected.stem[:70])
    return status() | {"message": "Location saved. Choose it for an app to connect automatically.", "savedUuid": saved}


def nm_async(start, finish, operation="save this location"):
    _, GLib, Gio = nm_bindings()
    loop = GLib.MainLoop()
    cancellation = Gio.Cancellable()
    outcome = {}
    def completed(owner, result, *_):
        try: outcome["value"] = finish(owner, result)
        except Exception as error:
            outcome["error"] = True
            # Numeric API error codes are safe; exception messages can contain
            # provider data and are deliberately not forwarded.
            if isinstance(getattr(error, "code", None), int):outcome["code"] = error.code
        loop.quit()
    def expired():
        cancellation.cancel()
        outcome["error"] = True
        loop.quit()
        return False
    timeout_id = GLib.timeout_add_seconds(25, expired)
    start(cancellation, completed)
    loop.run()
    if "value" in outcome:
        GLib.source_remove(timeout_id)
    if outcome.get("error") or "value" not in outcome:
        suffix = " (API code " + str(outcome["code"]) + ")" if "code" in outcome else ""
        raise RuntimeError("NetworkManager could not " + operation + suffix + ". Check its permission prompt, then Refresh before retrying.")
    return outcome["value"]


def prepare_location(connection, provider, country, label):
    NM, _, _ = nm_bindings()
    if len(managed_connections()) >= 128:
        raise SetupError("Saved VPN locations are limited to 128 per profile.")
    country_name = next((c["name"] for c in countries() if c["code"] == country), None)
    if country_name is None:
        raise SetupError("The connected country is not recognized.")
    identity = connection.get_setting_connection()
    display_name = ("Surfshark" if provider == "surfshark" else "Proton VPN") + " — " + country_name + (" — " + label if label else "")
    identity.set_property("id", "IsoldeVPN/1/"+PROFILE+"/"+provider+"/"+country+" — "+display_name)
    identity.set_property("uuid", str(uuid_module.uuid4()))
    identity.set_property("interface-name", "ivpn" + uuid_module.uuid4().hex[:10])
    identity.set_property("autoconnect", False)
    for index in reversed(range(identity.get_num_permissions())):
        identity.remove_permission(index)
    identity.add_permission("user", system_user(), None)
    metadata = NM.SettingUser.new()
    for key, value in {"profile": PROFILE, "country": country, "provider": provider}.items():
        metadata.set_data("org.isolde." + key, value)
    connection.add_setting(metadata)
    wireguard = connection.get_setting_by_name("wireguard")
    if wireguard is None or not 1 <= wireguard.get_peers_len() <= 8 or not wireguard.get_private_key():
        raise RuntimeError("Connect the provider app using WireGuard first, then Save connected location.")
    wireguard.set_property("private-key-flags", NM.SettingSecretFlags.NONE)
    # Preserve the provider's peer routing (including private DNS peers).
    # Add only the missing IPv6 catch-all, so unsupported IPv6 cannot escape.
    routes = []
    default_peer = None
    for index in range(wireguard.get_peers_len()):
        peer = wireguard.get_peer(index).new_clone(True)
        for route_index in range(peer.get_allowed_ips_len()):
            network = ipaddress.ip_network(peer.get_allowed_ip(route_index, None))
            routes.append(network)
            if network.version == 4 and ipaddress.ip_address("0.0.0.0") in network:default_peer = index
        peer.set_preshared_key_flags(NM.SettingSecretFlags.NONE)
        wireguard.set_peer(peer, index)
    if list(ipaddress.collapse_addresses(r for r in routes if r.version == 4)) != [ipaddress.ip_network("0.0.0.0/0")]:
        raise SetupError("The provider must route all IPv4 traffic through this interface.")
    if list(ipaddress.collapse_addresses(r for r in routes if r.version == 6)) != [ipaddress.ip_network("::/0")]:
        peer = wireguard.get_peer(default_peer).new_clone(True)
        peer.append_allowed_ip("::/0", False)
        wireguard.set_peer(peer, default_peer)
    wireguard.set_property("peer-routes", True)
    ipv4 = connection.get_setting_ip4_config()
    if ipv4 is None or ipv4.get_num_addresses() == 0:
        raise RuntimeError("The provider connection has no IPv4 address. Reconnect its app and retry.")
    ipv4.set_property("never-default", False)
    ipv4.set_property("dns-priority", -50)
    ipv4.add_dns_search("~.")
    ipv6 = connection.get_setting_ip6_config()
    if ipv6 is None:
        ipv6 = NM.SettingIP6Config.new()
        connection.add_setting(ipv6)
    ipv6.set_property("method", "manual")
    if ipv6.get_num_addresses() == 0:
        # IPv4-only providers drop unsupported IPv6 inside the tunnel.
        # It must not escape through the physical default route.
        ipv6.add_address(NM.IPAddress.new(socket.AF_INET6, "fd00:6973:6f6c:6465::2", 128))
    ipv6.set_property("never-default", False)
    connection.normalize(None)
    connection.verify()
    return connection


def save_location(connection, provider, country, label):
    check_copy_owner()
    NM, GLib, _ = nm_bindings()
    connection = prepare_location(connection, provider, country, label)
    client = NM.Client.new(None)
    settings = connection.to_dbus(NM.ConnectionSerializationFlags.ALL)
    options = GLib.Variant("a{sv}", {"plugin":GLib.Variant("s","keyfile")})
    def finished(owner, result):
        outcome = owner.add_connection2_finish(result)
        return outcome[0] if isinstance(outcome,tuple) else outcome
    saved = nm_async(lambda cancel, callback: client.add_connection2(settings, NM.SettingsAddConnection2Flags.TO_DISK, options, False, cancel, callback, None),finished)
    # Remote connection data is detached when its owning client disappears.
    # Return an immutable identity while that client is still alive.
    saved_uuid = saved.get_uuid()
    persisted = location_identity(saved)
    if not persisted or any(persisted[key]!=value for key,value in {"profile":PROFILE,"country":country,"provider":provider}.items()):
        try: command("nmcli","connection","delete","uuid",saved_uuid)
        except Exception: pass
        raise SetupError("VPN storage did not preserve this profile's ownership identity. The new location was not used.")
    return saved_uuid


def capture_location(provider):
    # Explicit user-authorized local transport-key copy into NetworkManager.
    # No provider account storage is opened; any import snapshot is root-private.
    provider_package(provider)
    active = active_connections()
    if len(active) != 1:
        raise RuntimeError("Open the provider app, sign in and connect to the country you want, then Save connected location.")
    source_uuid = active[0]["uuid"]
    NM, _, _ = nm_bindings()
    # Keep the client alive throughout the asynchronous secret request. Remote
    # connections are detached when their owning client is disposed.
    client = NM.Client.new(None)
    source = client.get_connection_by_uuid(source_uuid)
    if source is None or source.get_connection_type() != "wireguard":
        raise RuntimeError("Choose WireGuard in the provider app's connection settings, connect, then Save connected location.")
    metadata = source.get_setting_by_name("user")
    source_identity = location_identity(source)
    source_name = source.get_id()
    if (source_identity and source_identity["profile"]!=PROFILE) or (isinstance(source_name,str) and source_name.startswith("IsoldeVPN/") and not source_identity):
        raise RuntimeError("This VPN location belongs to another profile or has invalid ownership. Connect your own provider app first.")
    if metadata and metadata.get_data("org.isolde.profile") not in (None, "", PROFILE):
        raise RuntimeError("This VPN location belongs to another profile. Open your provider app to create your own connection.")
    if OWNER_USER:
        permissions = source.get_setting_connection().get_property("permissions") or []
        if permissions and ("user:" + OWNER_USER + ":") not in permissions:
            raise RuntimeError("This connection belongs to another system account. Connect your own provider app first.")
    if not ROOT_COPY:
        if not copy_helper_installed():
            raise RuntimeError("Choose Install default VPN providers once to install Isolde's secure location-saving support.")
        result = subprocess.run(["pkexec", str(COPY_HELPER), "capture", provider, "--profile", PROFILE],
                                capture_output=True, text=True, timeout=210)
        document = json.loads(result.stdout) if len(result.stdout) <= 65536 and result.stdout.startswith("{") else {}
        if result.returncode or not document:
            failure = RuntimeError(document.get("error", "Approve the guest administrator prompt to save this location, then Refresh and retry."))
            diagnostic = document.get("diagnostic", "")
            if isinstance(diagnostic,str) and re.fullmatch(r"[A-Za-z0-9_<>/:-]{1,160}",diagnostic):failure.diagnostic = diagnostic
            raise failure
        saved = document.get("savedUuid", "")
        if document.get("sourceUuid") != source_uuid or not re.fullmatch(r"[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}", saved):
            raise SetupError("The saved location could not be confirmed. Refresh VPN settings.")
        try:
            verified = switch_location(saved, handoff_from=source_uuid)
        except BaseException:
            try: command("nmcli", "connection", "delete", "uuid", saved)
            except Exception: pass
            raise
        return status() | verified | {"message": "Location saved and verified. Choose it for any app. No browser, configuration files or manual keys are needed."}
    country = observed_country(source_uuid)
    connection = NM.SimpleConnection.new_clone(source)
    secrets = nm_async(lambda cancel, callback: source.get_secrets_async("wireguard", cancel, callback, None),
                       lambda owner, result: owner.get_secrets_finish(result), "read the connected VPN key")
    connection.update_secrets("wireguard", secrets)
    connection = transport_configuration(connection, active[0]["device"])
    saved = save_location(connection, provider, country, "")
    return {"savedUuid": saved, "sourceUuid":source_uuid, "country":country}


def transport_configuration(connection, device):
    wireguard = connection.get_setting_by_name("wireguard")
    if wireguard and 1 <= wireguard.get_peers_len() <= 8 and wireguard.get_private_key():return connection
    if not ROOT_COPY or not re.fullmatch(r"[A-Za-z0-9_.-]{1,15}",device):
        raise SetupError("The selected provider interface is unavailable. Reconnect its app and retry.")
    # Read only the specifically approved active interface, never all devices
    # or provider account files. The private snapshot is removed after import.
    rows = [line.split("\t") for line in command("/usr/bin/wg","show",device,"dump").splitlines()]
    if not 2 <= len(rows) <= 9 or len(rows[0])!=4 or any(len(row)!=8 for row in rows[1:]):
        raise SetupError("The connected provider must have between one and eight WireGuard peers.")
    private = rows[0][0]
    if not re.fullmatch(r"[A-Za-z0-9+/]{43}=",private):raise SetupError("The provider's private transport key is unavailable.")
    peers = ""
    for public, preshared, endpoint, allowed, _, _, _, keepalive in rows[1:]:
        if not re.fullmatch(r"[A-Za-z0-9+/]{43}=",public) or (preshared!="(none)" and not re.fullmatch(r"[A-Za-z0-9+/]{43}=",preshared)):
            raise SetupError("The provider's transport keys are invalid.")
        if endpoint!="(none)" and (not re.fullmatch(r"(?:[A-Za-z0-9.-]+|\[[A-Fa-f0-9:]+\]):[0-9]{1,5}",endpoint) or not 1 <= int(endpoint.rsplit(":",1)[1]) <= 65535):
            raise SetupError("The provider's transport endpoint is invalid.")
        networks = [] if allowed=="(none)" else allowed.split(",")
        if len(networks)>128:raise SetupError("The provider's peer routing exceeds its limit.")
        networks = [str(ipaddress.ip_network(network)) for network in networks]
        if keepalive=="off":keepalive="0"
        if not keepalive.isdecimal() or int(keepalive)>65535:raise SetupError("The connected provider's keepalive is invalid.")
        peers += "\n[Peer]\nPublicKey="+public+"\nPersistentKeepalive="+keepalive+"\n"
        if endpoint!="(none)":peers += "Endpoint="+endpoint+"\n"
        if networks:peers += "AllowedIPs="+", ".join(networks)+"\n"
        if preshared!="(none)":peers += "PresharedKey="+preshared+"\n"
    addresses = []
    links = json.loads(command("ip","-j","address","show","dev",device))
    for link in links:
        for entry in link.get("addr_info",[]):
            if entry.get("family")=="inet" and entry.get("scope")=="global":
                addresses.append(str(ipaddress.IPv4Interface(str(entry["local"])+"/"+str(entry["prefixlen"]))))
    if not addresses or len(addresses)>8:raise SetupError("The connected provider has no usable IPv4 address.")
    ipv4 = connection.get_setting_ip4_config()
    dns = [ipv4.get_dns(i) for i in range(ipv4.get_num_dns())] if ipv4 else []
    if not dns:
        response = command("resolvectl","dns",device)
        match = re.fullmatch(r"Link [0-9]+ \("+re.escape(device)+r"\):\s*(.*)",response,re.S)
        if match:dns = match[1].split()
    if not dns or len(dns)>8:raise SetupError("Provider DNS is unavailable. Reconnect its app and retry.")
    dns = [str(ipaddress.ip_address(address)) for address in dns]
    data = ("[Interface]\nPrivateKey="+private+"\nAddress="+", ".join(addresses)+"\nDNS="+", ".join(dns)+
            "\n"+peers)
    if len(data)>16384:raise SetupError("The provider's transport configuration exceeds 16 KiB.")
    NM, _, _ = nm_bindings()
    with tempfile.TemporaryDirectory(prefix="isolde-vpn-kernel-") as directory:
        snapshot = Path(directory)/"location.conf"
        descriptor = os.open(snapshot,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
        with os.fdopen(descriptor,"w") as file:file.write(data)
        # Values above are validated before writing. Unlike Advanced import,
        # this trusted local snapshot preserves multiple standard Peer sections.
        return NM.conn_wireguard_import(str(snapshot))


def copy_helper_bytes():
    contents = Path(__file__).read_bytes().replace(b"\r\n", b"\n")
    return b"#!/usr/bin/python3 -I\n" + contents.split(b"\n", 1)[1]


def copy_policy_files():
    owner = system_user()
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_.-]{0,31}", owner):raise SetupError("This system account name is unsupported.")
    policy = ('<?xml version="1.0" encoding="UTF-8"?>\n<policyconfig><action id="org.isolde.vpn.capture">'
              '<description>Save the connected VPN location in Isolde</description>'
              '<message>Allow Isolde to save the connected VPN location</message>'
              '<defaults><allow_any>auth_admin</allow_any><allow_inactive>auth_admin</allow_inactive><allow_active>auth_admin</allow_active></defaults>'
              '<annotate key="org.freedesktop.policykit.exec.path">/usr/libexec/isolde/vpn-copy</annotate>'
              '</action></policyconfig>\n').encode()
    rule = ('polkit.addRule(function(action, subject) {\n'
            '  if (action.id === "org.isolde.vpn.capture" && subject.user === ' + json.dumps(owner) + ') return polkit.Result.YES;\n'
            '});\n').encode()
    return {COPY_POLICY:policy, COPY_RULE:rule}


def copy_helper_installed():
    for target, contents in {COPY_HELPER:copy_helper_bytes(), **copy_policy_files()}.items():
        for path in (target, *target.parents):
            try:
                info = path.lstat()
                if info.st_uid != 0 or info.st_mode & 0o022 or stat.S_ISLNK(info.st_mode):return False
            except OSError:return False
        if not target.is_file() or target.read_bytes() != contents:return False
    return True


def install_copy_helper():
    if copy_helper_installed():return
    contents = copy_helper_bytes()
    with tempfile.TemporaryDirectory(prefix="isolde-vpn-support-") as directory:
        staging = Path(directory)/"package"
        payload = {COPY_HELPER:contents, **copy_policy_files()}
        for target, data in payload.items():
            installed = staging/target.relative_to("/")
            installed.parent.mkdir(parents=True, exist_ok=True)
            installed.write_bytes(data);installed.chmod(0o755 if target == COPY_HELPER else 0o644)
        metadata = staging/"DEBIAN";metadata.mkdir()
        for folder in (staging, *(p for p in staging.rglob("*") if p.is_dir())):folder.chmod(0o755)
        version = "1.0+" + hashlib.sha256(b"".join(payload.values())).hexdigest()[:16]
        (metadata/"control").write_text("Package: isolde-vpn-support\nVersion: " + version + "\nArchitecture: all\nMaintainer: Isolde\nDescription: Isolde local VPN connection-copy support\n")
        package = Path(directory)/"isolde-vpn-support.deb"
        command("dpkg-deb", "--root-owner-group", "--build", str(staging), str(package))
        admin_command("/usr/bin/dpkg", "-i", str(package), timeout=120)
    if not copy_helper_installed():
        raise RuntimeError("VPN support installation was not confirmed. Retry Install default VPN providers.")


def install(provider):
    provider_package(provider)
    base = Path(__file__).resolve().parent
    spec = importlib.util.spec_from_file_location("provision", base / "provision-default-apps.py")
    provision = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(provision)
    if provider == "surfshark":
        with tempfile.TemporaryDirectory(prefix="isolde-surfshark-") as directory:
            installer = Path(directory) / "installer.sh"
            digest = provision.download("https://downloads.surfshark.com/linux/debian-install.sh", installer, 128 * 1024)
            if digest != "37f435b52916dadea289bf3f26aba010237f7266b55e52353e19f3daac4cda3d":
                raise RuntimeError("Surfshark installer changed. Update Isolde before installing.")
            keys = re.findall(rb"-----BEGIN PGP PUBLIC KEY BLOCK-----\n.*?-----END PGP PUBLIC KEY BLOCK-----", installer.read_bytes(), re.S)
            if len(keys) != 1:
                raise RuntimeError("Surfshark signing key is missing or ambiguous.")
            key = Path(directory) / "surfshark.asc"
            key.write_bytes(keys[0] + b"\n")
            metadata = command("gpg", "--batch", "--show-keys", "--with-colons", str(key))
            fingerprints = [line.split(":")[9] for line in metadata.splitlines() if line.startswith("fpr:")]
            if not fingerprints or fingerprints[0] != "D00FDC3221233215271F88DC1DB7590E83C8F643":
                raise RuntimeError("Unexpected Surfshark signing key.")
            repository = Path(directory) / "surfshark.list"
            repository.write_text("deb [signed-by=/usr/share/keyrings/isolde-surfshark.asc] https://ocean.surfshark.com/debian stretch main\n")
            # Only fixed system tools elevate. No Isolde/user-writable Python runs as root.
            admin_command("/usr/bin/install", "-m", "644", str(key), "/usr/share/keyrings/isolde-surfshark.asc", timeout=120)
            admin_command("/usr/bin/install", "-m", "644", str(repository), "/etc/apt/sources.list.d/isolde-surfshark.list", timeout=120)
            admin_command("/usr/bin/apt-get", "update", timeout=180)
            admin_command("/usr/bin/apt-get", "install", "-y", PACKAGES[provider], timeout=600)
    else:
        with tempfile.TemporaryDirectory(prefix="isolde-proton-") as directory:
            package = Path(directory) / "protonvpn-stable-release.deb"
            if provision.download(PROTON_URL, package, 1024 * 1024) != PROTON_SHA:
                raise RuntimeError("Proton repository package changed. Update Isolde before installing.")
            admin_command("/usr/bin/dpkg", "-i", str(package), timeout=120)
            admin_command("/usr/bin/apt-get", "update", timeout=180)
            admin_command("/usr/bin/apt-get", "install", "-y", PACKAGES[provider], timeout=600)
    if not installed(provider):
        raise RuntimeError("Installation was not confirmed. Retry setup.")
    admin_command("/usr/bin/apt-get", "install", "-y", "python3-gi", "gir1.2-nm-1.0", "wireguard-tools", "curl", "iso-codes", timeout=180)
    install_copy_helper()
    return status() | {"message": "Installed. Open the provider app to sign in and choose a country."}


def install_defaults():
    for provider in PACKAGES:
        if not installed(provider):
            install(provider)
    admin_command("/usr/bin/apt-get", "install", "-y", "python3-gi", "gir1.2-nm-1.0", "wireguard-tools", "curl", "iso-codes", timeout=180)
    install_copy_helper()
    return status() | {"message": "Surfshark, Proton VPN and location-saving support are installed. Open your provider, connect and Save connected location."}


def open_provider(provider):
    if not installed(provider):
        raise RuntimeError("Install this provider first.")
    files = command("dpkg-query", "-L", provider_package(provider)).splitlines()
    desktops = [Path(p) for p in files if p.startswith("/usr/share/applications/") and p.endswith(".desktop")]
    # Proton's metapackage can keep the launcher in its GTK dependency.
    if not desktops and provider == "protonvpn":
        files = command("dpkg-query", "-L", "proton-vpn-gtk-app").splitlines()
        desktops = [Path(p) for p in files if p.startswith("/usr/share/applications/") and p.endswith(".desktop")]
    if len(desktops) != 1:
        raise RuntimeError("Provider launcher is missing or ambiguous. Retry installation.")
    result = subprocess.run(["gtk-launch", desktops[0].stem], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
    if result.returncode:
        raise RuntimeError("Provider app could not be opened. Check its installation and retry.")
    return {"message": "Provider app opened. Sign in there, then choose Canada or United States and Connect. Return to Isolde and Refresh."}


def connection_action(action, uuid):
    if not re.fullmatch(r"[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}", uuid):
        raise SetupError("Choose a saved VPN connection.")
    connections = status()["connections"]
    selected = next((c for c in connections if c["uuid"] == uuid), None)
    if not selected:
        raise SetupError("Saved VPN connection no longer exists. Refresh.")
    if action == "connect" and status().get("activeVpnCount", 0) > int(selected["active"]):
        raise RuntimeError("Disconnect the current VPN before connecting another provider.")
    if action == "connect":
        return status() | switch_location(uuid)
    command("nmcli", "--wait", "20", "connection", "down", "uuid", uuid)
    refreshed = status()
    if not any(c["uuid"] == uuid and c["active"] == (action == "connect") for c in refreshed["connections"]):
        raise RuntimeError("VPN activation state could not be confirmed. Refresh before retrying.")
    return refreshed | {"message": "Connection state confirmed by NetworkManager. Regional access is not verified."}


def active_connections():
    text = command("nmcli", "-t", "--escape", "no", "-f", "UUID,TYPE,DEVICE", "connection", "show", "--active")
    result = []
    for line in text.splitlines():
        row = line.split(":", 2)
        if len(row) == 3 and row[1] in ("wireguard", "vpn"):
            result.append({"uuid": row[0], "device": row[2]})
    return result


def vpn_device(uuid):
    active = active_connections()
    target = next((c for c in active if c["uuid"] == uuid), None)
    if target is None or len(active) != 1:
        raise RuntimeError("The selected VPN is not the only active connection. The app was not opened.")
    device = target["device"]
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,15}", device):
        raise RuntimeError("Invalid VPN interface.")
    return device


def observed_country(uuid):
    device = vpn_device(uuid)
    observed = command("curl", "-4", "--silent", "--show-error", "--fail", "--noproxy", "*", "--interface", device,
                       "--proto", "=https", "--connect-timeout", "8", "--max-time", "12", "--max-filesize", "32",
                       "https://ipinfo.io/country", timeout=15).strip()
    if not re.fullmatch(r"[A-Z]{2}", observed):
        raise RuntimeError("The connected country could not be verified. Check the provider app and retry.")
    return observed


def verify_location(uuid, country):
    device = vpn_device(uuid)
    for family, destination in [("-4", "1.1.1.1"), ("-6", "2606:4700:4700::1111")]:
        route = json.loads(command("ip", "-j", family, "route", "get", destination))
        if not route or any(r.get("dev") != device for r in route):
            raise RuntimeError("Traffic is not routed through the selected VPN. The app was not opened.")
    # An explicit connect/launch is consent for this bounded online country check.
    # Bind the request to the verified VPN interface. No raw address is retained.
    observed = observed_country(uuid)
    if observed != country:
        raise RuntimeError("The VPN's observed country does not match the chosen location. The app was not opened.")
    return {"country": observed, "verifiedAt": int(time.time())}


def switch_location(uuid, handoff_from=None):
    owned = {c["uuid"]: c for c in managed_connections()}
    if uuid not in owned:
        raise SetupError("Choose a saved location in this profile's VPN settings.")
    previous = active_connections()
    if len(previous) > 1 or any(c["uuid"] not in owned and c["uuid"] != handoff_from for c in previous):
        raise RuntimeError("Disconnect the other VPN app first, then retry. The app was not opened.")
    devices = command("nmcli", "-t", "--escape", "no", "-f", "DEVICE,TYPE", "device", "status")
    active_devices = {c["device"] for c in previous}
    if any(len(row := line.split(":", 1)) == 2 and row[1] in ("wireguard", "tun") and row[0] not in active_devices for line in devices.splitlines()):
        raise RuntimeError("Disconnect the other VPN app first, then retry. The app was not opened.")
    changing = not previous or previous[0]["uuid"] != uuid
    old = previous[0]["uuid"] if previous and changing else None
    try:
        if old:
            command("nmcli", "--wait", "20", "connection", "down", "uuid", old)
        if changing:
            command("nmcli", "--wait", "20", "connection", "up", "uuid", uuid)
        result = verify_location(uuid, owned[uuid]["country"])
        return result | {"message": "VPN location verified. Opening your app."}
    except BaseException:
        # Cancel and failures restore only the exact previous owned connection.
        # Never disconnect a tunnel that was already active on entry.
        if changing:
            signal.signal(signal.SIGTERM, signal.SIG_IGN)
            try:
                current = active_connections()
                target_active = any(c["uuid"] == uuid for c in current)
                old_active = any(c["uuid"] == old for c in current)
            except Exception:
                target_active = True
                old_active = False
            try:
                if target_active:command("nmcli", "--wait", "10", "connection", "down", "uuid", uuid)
            except Exception: pass
            if old and not old_active:
                try: command("nmcli", "--wait", "20", "connection", "up", "uuid", old)
                except Exception:
                    raise RuntimeError("VPN switching failed and the previous VPN could not be restored. Refresh VPN settings before retrying.") from None
        raise


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["status", "install", "defaults", "open", "account", "capture", "import", "connect", "disconnect", "switch"])
    parser.add_argument("target", nargs="?", default="")
    parser.add_argument("--profile", default="default")
    parser.add_argument("--provider", choices=list(PACKAGES), default="surfshark")
    parser.add_argument("--country", default="CA")
    args = parser.parse_args()
    try:
        guest_only()
        global PROFILE
        if args.profile != "default" and not re.fullmatch(r"[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}", args.profile):
            raise SetupError("Invalid Isolde profile.")
        PROFILE = args.profile
        global ROOT_COPY, OWNER_USER, OWNER_PROCESS
        if hasattr(os, "geteuid") and os.geteuid() == 0:
            # Only the installed root-owned entrypoint may perform a privileged
            # copy, for the actual caller established by pkexec.
            caller = os.environ.get("PKEXEC_UID", "")
            if args.action != "capture" or not caller.isdecimal() or int(caller) == 0 or Path(__file__).resolve() != COPY_HELPER:
                raise SetupError("Use Isolde's installed VPN setup to request this operation.")
            import pwd
            OWNER_USER = pwd.getpwuid(int(caller)).pw_name
            if not copy_helper_installed():raise SetupError("Reinstall Isolde VPN support before saving a location.")
            parent = Path("/proc")/str(os.getppid())
            if parent.stat().st_uid != int(caller):raise SetupError("VPN setup requester is no longer available.")
            OWNER_PROCESS = (os.getppid(), parent.joinpath("stat").read_text().rsplit(")",1)[1].split()[19], int(caller))
            ROOT_COPY = True
        signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(RuntimeError("VPN switching cancelled. The app was not opened.")))
        if args.action == "status": result = status()
        elif args.action == "defaults": result = install_defaults()
        elif args.action == "install": result = install(args.target)
        elif args.action == "open": result = open_provider(args.target)
        elif args.action == "account":
            provider_package(args.target)
            command("xdg-open", ACCOUNTS[args.target])
            result = {"message": "In the guest browser: Surfshark → VPN → Manual setup → Desktop or mobile → WireGuard → Generate key pair → Locations. Proton VPN → Downloads → WireGuard configuration. Download Canada and US .conf files, then return here and Refresh to import each."}
        elif args.action == "import": result = import_configuration(args.target, args.provider, args.country)
        elif args.action == "capture": result = capture_location(args.target)
        elif args.action == "switch": result = switch_location(args.target)
        else: result = connection_action(args.action, args.target)
        print(json.dumps(result))
    except Exception as error:
        # Never forward command output, account data, or private keys.
        message = str(error) if isinstance(error, (RuntimeError, SetupError)) else "VPN setup or configuration validation failed. Check the selected configuration, Refresh and retry."
        trace = error.__traceback__
        while trace and trace.tb_next:trace = trace.tb_next
        diagnostic = getattr(error, "diagnostic", type(error).__name__ + "/" + (trace.tb_frame.f_code.co_name + "/" + str(trace.tb_lineno) if trace else "unknown"))
        print(json.dumps({"error": message, "diagnostic":diagnostic}))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
