import importlib.util
from pathlib import Path
import unittest
import tempfile
from unittest.mock import MagicMock, patch
import json
import signal

spec = importlib.util.spec_from_file_location("vpn_setup", Path(__file__).resolve().parents[1] / "tools/vpn-setup.py")
vpn = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vpn)
UUID = "12345678-1234-1234-1234-123456789abc"
OTHER = "87654321-1234-1234-1234-123456789abc"
CONFIG = "[Interface]\nPrivateKey=" + "A" * 43 + "=\nAddress=10.2.0.2/32\nDNS=10.2.0.1\n[Peer]\nPublicKey=" + "B" * 43 + "=\nAllowedIPs=0.0.0.0/0\nEndpoint=example.com:51820\n"


class VpnSetupTests(unittest.TestCase):
    def test_required_identity_survives_absent_optional_data_and_rejects_conflicts(self):
        connection=MagicMock();connection.get_setting_by_name.return_value=None
        connection.get_id.return_value="IsoldeVPN/1/"+vpn.PROFILE+"/surfshark/CA — Canada"
        self.assertEqual(vpn.location_identity(connection)["profile"],vpn.PROFILE)
        for name in ("Canada","IsoldeVPN/2/"+vpn.PROFILE+"/surfshark/CA — Canada","IsoldeVPN/1/../surfshark/CA — Canada"):
            connection.get_id.return_value=name
            self.assertIsNone(vpn.location_identity(connection))
        connection.get_id.return_value="IsoldeVPN/1/"+vpn.PROFILE+"/surfshark/CA — Canada"
        connection.get_setting_by_name.return_value=MagicMock()
        connection.get_setting_by_name.return_value.get_data.return_value="another-profile"
        self.assertIsNone(vpn.location_identity(connection))

    def test_networkmanager_metadata_uses_supported_generic_setting_lookup(self):
        NM = MagicMock()
        connection = MagicMock(spec=["get_setting_by_name", "get_connection_type", "get_uuid", "get_id"])
        connection.get_setting_by_name.return_value.get_data.side_effect = [vpn.PROFILE, "CA", "surfshark"]
        connection.get_connection_type.return_value = "wireguard"
        connection.get_uuid.return_value = UUID
        connection.get_id.return_value = "IsoldeVPN/1/"+vpn.PROFILE+"/surfshark/CA — Canada"
        NM.Client.new.return_value.get_connections.return_value = [connection]
        with patch.object(vpn, "nm_bindings", return_value=(NM, None, None)):
            result = vpn.managed_connections()
        self.assertEqual(result[0]["uuid"], UUID)
        connection.get_setting_by_name.assert_called_once_with("user")
    def test_host_refused(self):
        with patch.object(vpn.socket, "gethostname", return_value="personal-host"):
            with self.assertRaises(RuntimeError): vpn.guest_only()

    def test_unknown_provider_refused_before_execution(self):
        with patch.object(vpn, "command") as execute:
            with self.assertRaises(ValueError): vpn.install("--unsafe")
            execute.assert_not_called()

    def test_status_only_profile_owned_locations_and_no_secrets(self):
        def execute(*args, **kwargs):
            self.assertNotIn("--show-secrets", args)
            if args[0] == "dpkg-query": return "install ok installed"
            if "--active" in args: return UUID
            return UUID + ":wireguard:Canada:Toronto\n" + OTHER + ":ethernet:LAN"
        owned = [{"uuid": UUID, "name": "Canada", "country": "CA", "provider": "surfshark"}]
        with patch.object(vpn, "command", side_effect=execute), patch.object(vpn, "managed_connections", return_value=owned), patch.object(vpn, "configuration_files", return_value=[]), patch.object(vpn, "countries", return_value=[]):
            result = vpn.status()
        self.assertEqual(len(result["providers"]), 2)
        self.assertEqual(result["connections"][0]["country"], "CA")
        self.assertTrue(result["connections"][0]["active"])

    def test_refuses_competing_tunnel(self):
        with patch.object(vpn, "status", return_value={"connections": [
            {"uuid": UUID, "active": False}], "activeVpnCount": 1}), patch.object(vpn, "command") as execute:
            with self.assertRaises(RuntimeError): vpn.connection_action("connect", UUID)
            execute.assert_not_called()

    def test_invalid_uuid_refused(self):
        with patch.object(vpn, "command") as execute:
            with self.assertRaises(ValueError): vpn.connection_action("connect", "$(touch /tmp/bad)")
            execute.assert_not_called()

    def test_activation_requires_verification(self):
        state = {"connections": [{"uuid": UUID, "active": False}], "activeVpnCount": 0}
        with patch.object(vpn, "status", return_value=state), patch.object(vpn, "command"), patch.object(vpn, "privileged_switch", side_effect=RuntimeError("unverified")):
            with self.assertRaises(RuntimeError): vpn.connection_action("connect", UUID)

    def test_proton_checksum_mismatch_never_elevates(self):
        provision = MagicMock()
        provision.download.return_value = "changed"
        with patch.object(vpn.importlib.util, "module_from_spec", return_value=provision), \
             patch.object(vpn.importlib.util, "spec_from_file_location") as spec, \
             patch.object(vpn, "admin_command") as execute:
            spec.return_value.loader.exec_module = MagicMock()
            with self.assertRaises(RuntimeError): vpn.install("protonvpn")
            execute.assert_not_called()

    def test_config_scripts_unknown_fields_and_partial_routes_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "canada.conf"
            path.write_text(CONFIG)
            vpn.validate_configuration(path)
            for invalid in [CONFIG.replace("Address=", "PostUp=unsafe\nAddress="),
                            CONFIG.replace("0.0.0.0/0", "10.0.0.0/8"),
                            CONFIG.replace("[Peer]", "[Peer]\nUnreviewed=yes")]:
                path.write_text(invalid)
                with self.assertRaises(ValueError): vpn.validate_configuration(path)

    def test_import_commits_once_with_autoconnect_disabled_and_owned_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "canada.conf"; path.write_text(CONFIG)
            NM, GLib, Gio = MagicMock(), MagicMock(), MagicMock()
            connection = NM.conn_wireguard_import.return_value
            connection.get_setting_by_name.return_value.get_peers_len.return_value = 1
            connection.get_setting_by_name.return_value.get_private_key.return_value = "synthetic-key"
            connection.get_setting_by_name.return_value.get_property.return_value = 0
            peer = connection.get_setting_by_name.return_value.get_peer.return_value.new_clone.return_value
            peer.get_allowed_ips_len.return_value = 1
            peer.get_allowed_ip.return_value = "0.0.0.0/0"
            connection.get_setting_ip4_config.return_value.get_num_addresses.return_value = 1
            connection.get_setting_ip6_config.return_value.get_num_addresses.return_value = 0
            client = NM.Client.new.return_value
            loop = GLib.MainLoop.return_value
            loop.run.side_effect = lambda: client.add_connection2.call_args.args[5](client, object())
            saved = client.add_connection2_finish.return_value
            saved.get_id.return_value = "IsoldeVPN/1/"+vpn.PROFILE+"/surfshark/CA — Canada"
            saved.get_setting_by_name.return_value.get_data.side_effect = [vpn.PROFILE,"CA","surfshark"]
            with patch.object(vpn, "configuration_files", return_value=[path]), \
                 patch.object(vpn, "managed_connections", return_value=[]), \
                 patch.object(vpn, "countries", return_value=[{"code": "CA", "name": "Canada"}]), \
                 patch.object(vpn, "status", return_value={"connections": []}), \
                 patch.object(vpn, "nm_bindings", return_value=(NM, GLib, Gio)), \
                 patch.object(vpn.os, "O_NOFOLLOW", getattr(vpn.os, "O_NOFOLLOW", 0), create=True), \
                 patch.object(vpn, "command") as execute:
                vpn.import_configuration("canada.conf", "surfshark", "CA")
            identity = connection.get_setting_connection.return_value
            self.assertIn(unittest.mock.call("autoconnect", False), identity.set_property.call_args_list)
            self.assertIn(unittest.mock.call("org.isolde.profile", vpn.PROFILE), NM.SettingUser.new.return_value.set_data.call_args_list)
            self.assertEqual(client.add_connection2.call_count, 1)
            self.assertEqual(GLib.Variant.call_args_list[-2].args,("s","keyfile"))
            execute.assert_not_called()
            self.assertEqual(path.read_text(), CONFIG)

    def test_switch_failure_restores_previous_owned_connection(self):
        owned = [{"uuid": UUID, "country": "CA"}, {"uuid": OTHER, "country": "US"}]
        with patch.object(vpn, "managed_connections", return_value=owned), \
             patch.object(vpn, "active_connections", side_effect=[[{"uuid": OTHER, "device": "old"}],[{"uuid":UUID,"device":"new"}]]), \
             patch.object(vpn, "verify_location", side_effect=RuntimeError("wrong country")), \
             patch.object(vpn.signal, "signal"), patch.object(vpn, "command", return_value="old:wireguard") as execute:
            with self.assertRaisesRegex(RuntimeError, "wrong country"): vpn.switch_location(UUID)
            self.assertEqual(execute.call_args.args, ("nmcli", "--wait", "20", "connection", "up", "uuid", OTHER))
            self.assertIn(unittest.mock.call("nmcli", "--wait", "10", "connection", "down", "uuid", UUID), execute.call_args_list)

    def test_denied_disconnect_leaves_previous_active_without_false_restore(self):
        owned=[{"uuid":UUID,"country":"CA"},{"uuid":OTHER,"country":"US"}]
        def execute(*args,**kwargs):
            if args[0]=="nmcli" and "device" in args:return "old:wireguard"
            raise RuntimeError("permission required")
        with patch.object(vpn,"managed_connections",return_value=owned), \
             patch.object(vpn,"active_connections",return_value=[{"uuid":OTHER,"device":"old"}]), \
             patch.object(vpn.signal,"signal"),patch.object(vpn,"command",side_effect=execute) as call:
            with self.assertRaisesRegex(RuntimeError,"permission required"):vpn.switch_location(UUID)
        self.assertFalse(any("up" in item.args for item in call.call_args_list))
        self.assertEqual(sum("down" in item.args for item in call.call_args_list),1)

    def test_same_tunnel_failure_never_disconnects_it(self):
        with patch.object(vpn, "managed_connections", return_value=[{"uuid": UUID, "country": "CA"}]), \
             patch.object(vpn, "active_connections", return_value=[{"uuid": UUID, "device": "same"}]), \
             patch.object(vpn, "verify_location", side_effect=RuntimeError("failed")), \
             patch.object(vpn, "command", return_value="same:wireguard") as execute:
            with self.assertRaises(RuntimeError): vpn.switch_location(UUID)
            self.assertFalse(any("down" in call.args for call in execute.call_args_list))

    def test_cross_profile_and_unmanaged_tunnel_refused(self):
        with patch.object(vpn, "managed_connections", return_value=[]), patch.object(vpn, "nm_bindings",return_value=(MagicMock(),None,None)), patch.object(vpn, "command") as execute:
            with self.assertRaises(ValueError): vpn.switch_location(UUID)
            execute.assert_not_called()
        with patch.object(vpn, "managed_connections", return_value=[{"uuid": UUID, "country": "CA"}]), \
             patch.object(vpn, "active_connections", return_value=[{"uuid": OTHER, "device": "other"}]), \
             patch.object(vpn, "command") as execute:
            with self.assertRaises(RuntimeError): vpn.switch_location(UUID)
            execute.assert_not_called()

    def test_country_check_is_bound_to_vpn_and_rejects_mismatch(self):
        def execute(*args, **kwargs):
            if args[0] == "ip": return json.dumps([{"dev": "ivpn123"}])
            self.assertIn("--interface", args); self.assertIn("ivpn123", args); self.assertIn("--noproxy", args)
            return "US"
        with patch.object(vpn, "active_connections", return_value=[{"uuid": UUID, "device": "ivpn123"}]), patch.object(vpn, "command", side_effect=execute):
            with self.assertRaisesRegex(RuntimeError, "country"): vpn.verify_location(UUID, "CA")

    def test_escaping_ipv6_route_blocks_launch_before_online_check(self):
        with patch.object(vpn, "active_connections", return_value=[{"uuid": UUID, "device": "ivpn123"}]), \
             patch.object(vpn, "command", side_effect=[json.dumps([{"dev": "ivpn123"}]), json.dumps([{"dev": "ethernet"}])]) as execute:
            with self.assertRaisesRegex(RuntimeError, "routed"): vpn.verify_location(UUID, "CA")
            self.assertFalse(any(call.args[0] == "curl" for call in execute.call_args_list))

    def test_default_providers_are_both_installed(self):
        with patch.object(vpn, "installed", return_value=False), patch.object(vpn, "install") as install, patch.object(vpn, "install_copy_helper"), patch.object(vpn, "admin_command"), patch.object(vpn, "status", return_value={}):
            vpn.install_defaults()
            self.assertEqual(install.call_args_list, [unittest.mock.call("surfshark"), unittest.mock.call("protonvpn")])

    def test_capture_uses_authorized_local_api_without_mutating_source(self):
        NM = MagicMock();source = NM.Client.new.return_value.get_connection_by_uuid.return_value
        source.get_connection_type.return_value = "wireguard"
        source.get_setting_by_name.return_value = None
        saved = UUID
        with patch.object(vpn, "nm_bindings", return_value=(NM, None, None)), \
             patch.object(vpn, "active_connections", return_value=[{"uuid": OTHER, "device": "provider"}]), \
             patch.object(vpn, "observed_country", return_value="CA"), \
             patch.object(vpn, "ROOT_COPY", True), \
             patch.object(vpn, "transport_configuration", side_effect=lambda connection,device:connection), \
             patch.object(vpn, "nm_async", return_value="opaque-local-variant"), \
             patch.object(vpn, "save_location", return_value=saved), \
             patch.object(vpn, "handoff_location", return_value={"country": "CA"}) as switch, \
             patch.object(vpn, "status", return_value={}):
            result = vpn.capture_location("surfshark")
        self.assertEqual(result["country"], "CA")
        self.assertEqual(result["savedUuid"],UUID)
        switch.assert_called_once_with(UUID,OTHER,"CA")
        source.get_secrets_async.assert_not_called()
        source.update_secrets.assert_not_called()
        NM.SimpleConnection.new_clone.return_value.update_secrets.assert_not_called()

    def test_capture_refuses_another_profiles_keys_before_secret_request(self):
        NM = MagicMock();source = NM.Client.new.return_value.get_connection_by_uuid.return_value
        source.get_connection_type.return_value = "wireguard"
        source.get_setting_by_name.return_value.get_data.return_value = "another-profile"
        with patch.object(vpn, "nm_bindings", return_value=(NM, None, None)), \
             patch.object(vpn, "active_connections", return_value=[{"uuid": OTHER, "device": "provider"}]), \
             patch.object(vpn, "nm_async") as secrets:
            with self.assertRaisesRegex(RuntimeError, "another profile"): vpn.capture_location("surfshark")
            secrets.assert_not_called()

    def test_frontend_only_delegates_to_verified_installed_copy_tool(self):
        NM = MagicMock();source = NM.Client.new.return_value.get_connection_by_uuid.return_value
        source.get_connection_type.return_value = "wireguard"
        source.get_setting_by_name.return_value = None
        response = MagicMock(returncode=0, stdout=json.dumps({"country":"CA","savedUuid":UUID,"sourceUuid":OTHER}))
        with patch.object(vpn, "nm_bindings", return_value=(NM,None,None)), \
             patch.object(vpn, "active_connections", return_value=[{"uuid":OTHER,"device":"provider"}]), \
             patch.object(vpn, "copy_helper_installed", return_value=True), \
             patch.object(vpn.subprocess, "run", return_value=response) as execute, \
             patch.object(vpn,"verify_location",return_value={"country":"CA"}) as switch, \
             patch.object(vpn,"status",return_value={}):
            self.assertEqual(vpn.capture_location("surfshark")["country"],"CA")
        switch.assert_called_once_with(UUID,"CA")
        source.get_secrets_async.assert_not_called()
        self.assertEqual(execute.call_args.args[0], ["pkexec", str(vpn.COPY_HELPER), "capture", "surfshark", "--profile", vpn.PROFILE])

    def test_privileged_capture_refuses_another_system_accounts_connection(self):
        NM = MagicMock();source = NM.Client.new.return_value.get_connection_by_uuid.return_value
        source.get_connection_type.return_value = "wireguard"
        source.get_setting_by_name.return_value = None
        source.get_setting_connection.return_value.get_property.return_value = ["user:someone-else:"]
        with patch.object(vpn, "nm_bindings", return_value=(NM,None,None)), \
             patch.object(vpn, "active_connections", return_value=[{"uuid":OTHER,"device":"provider"}]), \
             patch.object(vpn, "ROOT_COPY",True), patch.object(vpn,"OWNER_USER","isolde-dev"), patch.object(vpn,"nm_async") as secrets:
            with self.assertRaisesRegex(RuntimeError,"another system account"):vpn.capture_location("surfshark")
        secrets.assert_not_called()

    def test_support_package_contains_only_root_owned_isolated_copy_code(self):
        inspected = []
        def build(*args, **kwargs):
            staging = Path(args[-2])
            files = sorted(p.relative_to(staging).as_posix() for p in staging.rglob("*") if p.is_file())
            self.assertEqual(files, ["DEBIAN/control", "usr/libexec/isolde/vpn-copy", "usr/share/polkit-1/actions/org.isolde.vpn.policy", "usr/share/polkit-1/rules.d/50-isolde-vpn.rules"])
            self.assertTrue((staging/"usr/libexec/isolde/vpn-copy").read_bytes().startswith(b"#!/usr/bin/python3 -I\n"))
            rule = (staging/"usr/share/polkit-1/rules.d/50-isolde-vpn.rules").read_text()
            self.assertIn('action.id === "org.isolde.vpn.capture"', rule)
            self.assertIn('action.id === "org.isolde.vpn.switch"',rule)
            import xml.etree.ElementTree as ET
            policy=ET.fromstring((staging/"usr/share/polkit-1/actions/org.isolde.vpn.policy").read_bytes())
            actions={action.attrib["id"]:{item.attrib["key"]:item.text for item in action.findall("annotate")} for action in policy.findall("action")}
            self.assertEqual(set(actions),{"org.isolde.vpn.capture","org.isolde.vpn.switch"})
            for verb in ("capture","switch"):
                self.assertEqual(actions["org.isolde.vpn."+verb],{"org.freedesktop.policykit.exec.path":vpn.COPY_HELPER.as_posix(),"org.freedesktop.policykit.exec.argv1":verb})
            self.assertIn('subject.user === ', rule)
            self.assertNotIn('org.freedesktop.NetworkManager', rule)
            self.assertIn("--root-owner-group", args)
            inspected.append(True)
            return ""
        with patch.object(vpn, "copy_helper_installed", side_effect=[False, True]), \
             patch.object(vpn, "command", side_effect=build), patch.object(vpn, "admin_command") as install:
            vpn.install_copy_helper()
        self.assertTrue(inspected)
        self.assertEqual(install.call_args.args[:2], ("/usr/bin/dpkg", "-i"))

    def test_repeat_support_install_does_no_package_or_privileged_work(self):
        with patch.object(vpn, "copy_helper_installed", return_value=True), \
             patch.object(vpn, "command") as execute, patch.object(vpn, "admin_command") as install:
            vpn.install_copy_helper()
        execute.assert_not_called();install.assert_not_called()

    def test_switch_delegates_only_fixed_owned_tool_without_key_access(self):
        response=MagicMock(returncode=0,stdout='{"country":"CA","verifiedAt":123}')
        with patch.object(vpn,"copy_helper_installed",return_value=True),patch.object(vpn.subprocess,"run",return_value=response) as run,patch.object(vpn,"nm_bindings") as keys:
            self.assertEqual(vpn.privileged_switch(UUID)["country"],"CA")
            run.assert_called_once_with(["pkexec",str(vpn.COPY_HELPER),"switch",UUID,"--profile",vpn.PROFILE],capture_output=True,text=True,timeout=100)
            keys.assert_not_called()

    def test_root_listing_refuses_other_linux_users_tagged_location(self):
        NM=MagicMock();connection=NM.Client.new.return_value.get_connections.return_value=[MagicMock()]
        connection=connection[0];connection.get_id.return_value="IsoldeVPN/1/"+vpn.PROFILE+"/surfshark/CA — Canada"
        connection.get_setting_by_name.return_value=None;connection.get_connection_type.return_value="wireguard"
        connection.get_setting_connection.return_value.get_property.return_value=["user:someone-else:"]
        with patch.object(vpn,"OWNER_USER","isolde-dev"),patch.object(vpn,"nm_bindings",return_value=(NM,None,None)):
            self.assertEqual(vpn.managed_connections(),[])

    def test_handoff_failure_keeps_native_source_connected(self):
        active=[{"uuid":OTHER,"device":"native"}]
        with patch.object(vpn,"ROOT_COPY",True),patch.object(vpn,"managed_connections",return_value=[{"uuid":UUID}]), \
             patch.object(vpn,"active_connections",return_value=active),patch.object(vpn,"vpn_device",return_value="native"), \
             patch.object(vpn,"verify_location",side_effect=RuntimeError("wrong route")),patch.object(vpn.signal,"signal"),patch.object(vpn,"command") as run:
            with self.assertRaisesRegex(RuntimeError,"wrong route"):vpn.handoff_location(UUID,OTHER,"CA")
        self.assertFalse(any("down" in c.args and OTHER in c.args for c in run.call_args_list))
        self.assertEqual(run.call_args_list[-1].args,("nmcli","--wait","10","connection","down","uuid",UUID))

    def test_handoff_proves_bound_tunnel_then_global_route(self):
        with patch.object(vpn,"ROOT_COPY",True),patch.object(vpn,"managed_connections",return_value=[{"uuid":UUID}]), \
             patch.object(vpn,"active_connections",return_value=[{"uuid":OTHER,"device":"native"}]), \
             patch.object(vpn,"vpn_device",return_value="native"),patch.object(vpn,"command"), \
             patch.object(vpn,"verify_location",side_effect=[{"country":"CA"},{"country":"CA"}]) as verify:
            self.assertEqual(vpn.handoff_location(UUID,OTHER,"CA"),{"country":"CA"})
        self.assertEqual([c.args for c in verify.call_args_list],[(UUID,"CA",OTHER),(UUID,"CA")])

    def test_capture_preflight_route_is_bound_to_new_interface(self):
        with patch.object(vpn,"vpn_device",return_value="ivpn1234567890"), \
             patch.object(vpn,"observed_country",return_value="CA"), \
             patch.object(vpn,"command",return_value='[{"dev":"ivpn1234567890"}]') as run:
            vpn.verify_location(UUID,"CA",OTHER)
        self.assertTrue(all(c.args[-2:]==("oif","ivpn1234567890") for c in run.call_args_list))

    def test_native_packet_mark_requires_unambiguous_nonconflicting_rules(self):
        NM=MagicMock();connection=MagicMock();connection.get_uuid.return_value=UUID
        connection.get_setting_by_name.return_value.get_property.return_value=300000
        rules=[{"priority":0,"table":"local"},{"priority":30901,"table":"300000","fwmark":"0x493e0","not":None}]
        with patch.object(vpn,"nm_bindings",return_value=(NM,None,None)),patch.object(vpn,"command",return_value=json.dumps(rules)):
            vpn.preserve_transport_routing(connection)
        self.assertEqual(NM.IPRoutingRule.new.call_count,4)
        for setting in (connection.get_setting_ip4_config(),connection.get_setting_ip6_config()):
            self.assertEqual(setting.add_routing_rule.call_count,2)
        for invalid in ([],rules+[{'priority':30898,'table':'main'}],rules+[rules[-1]]):
            with patch.object(vpn,"nm_bindings",return_value=(NM,None,None)),patch.object(vpn,"command",return_value=json.dumps(invalid)):
                with self.assertRaises(ValueError):vpn.preserve_transport_routing(connection)

    def test_disappeared_requester_stops_privileged_work_but_allows_rollback(self):
        with patch.object(vpn, "ROOT_COPY", True), patch.object(vpn, "ROOT_ABORTED", False), \
             patch.object(vpn, "OWNER_PROCESS", (123,"456",1000)), patch.object(vpn.Path, "stat", side_effect=FileNotFoundError):
            with self.assertRaisesRegex(RuntimeError, "cancelled"):vpn.check_copy_owner()
            self.assertTrue(vpn.ROOT_ABORTED)
            vpn.check_copy_owner()

    def test_kernel_transport_snapshot_is_selected_private_and_removed(self):
        NM = MagicMock();connection = MagicMock()
        # Stored settings can look complete while daemon-adjusted kernel
        # transport differs. They must never bypass the selected snapshot.
        connection.get_setting_by_name.return_value.get_peers_len.return_value = 2
        connection.get_setting_by_name.return_value.get_private_key.return_value = "stored-key"
        connection.get_setting_ip4_config.return_value.get_num_dns.return_value = 0
        dump = "A"*43+"=\t"+"B"*43+"=\t51820\t0\n"+"B"*43+"=\t(none)\t192.0.2.10:51820\t0.0.0.0/0\t1\t2\t3\t25\n"
        dump += "C"*43+"=\t(none)\t(none)\t(none)\t0\t0\t0\toff\n"
        paths = []
        def imported(name):
            path = Path(name);paths.append(path)
            self.assertIn("DNS=10.2.0.1",path.read_text())
            self.assertIn("MTU=1280",path.read_text())
            self.assertEqual(path.read_text().count("[Peer]"),2)
            self.assertIn("PersistentKeepalive=0",path.read_text())
            if vpn.os.name=="posix":self.assertEqual(path.stat().st_mode & 0o777,0o600)
            return "imported-connection"
        NM.conn_wireguard_import.side_effect = imported
        addresses = json.dumps([{"mtu":1280,"addr_info":[{"family":"inet","scope":"global","local":"10.2.0.2","prefixlen":32}]}])
        with patch.object(vpn,"ROOT_COPY",True), patch.object(vpn,"nm_bindings",return_value=(NM,None,None)), \
             patch.object(vpn,"command",side_effect=[dump,addresses,"Link 7 (provider): 10.2.0.1"]) as execute:
            self.assertEqual(vpn.transport_configuration(connection,"provider"),"imported-connection")
        self.assertEqual(execute.call_args_list[0].args,("/usr/bin/wg","show","provider","dump"))
        self.assertTrue(paths);self.assertFalse(paths[0].exists())

    def test_kernel_transport_refuses_unprivileged_or_ambiguous_sources(self):
        connection = MagicMock();connection.get_setting_by_name.return_value = None
        with patch.object(vpn,"ROOT_COPY",False),patch.object(vpn,"command") as execute:
            with self.assertRaises(ValueError):vpn.transport_configuration(connection,"provider")
            execute.assert_not_called()
        with patch.object(vpn,"ROOT_COPY",True),patch.object(vpn,"command",return_value="one\ntwo\nthree") as execute:
            with self.assertRaisesRegex(ValueError,"one and eight"):vpn.transport_configuration(connection,"provider")
            self.assertEqual(execute.call_count,1)

    if vpn.os.name == "posix":
        def test_real_libnm_policy_is_valid_before_any_connection_write(self):
            import base64
            NM, GLib, _ = vpn.nm_bindings()
            private = base64.b64encode(bytes([1]) * 32).decode()
            public = base64.b64encode(bytes([2]) * 32).decode()
            data = CONFIG.replace("A" * 43 + "=", private).replace("B" * 43 + "=", public)
            preshared = base64.b64encode(bytes([4])*32).decode()
            data = data.replace("[Peer]\n","[Peer]\nPresharedKey="+preshared+"\n",1)
            second = base64.b64encode(bytes([3]) * 32).decode()
            data += "\n[Peer]\nPublicKey="+second+"\nAllowedIPs=10.2.0.1/32\nEndpoint=192.0.2.20:51820\n"
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "ca.conf";path.write_text(data)
                connection = NM.conn_wireguard_import(str(path))
                clone = NM.SimpleConnection.new_clone(connection);clone.clear_secrets()
                secrets = GLib.Variant("a{sa{sv}}", {"wireguard":{"private-key":GLib.Variant("s",private)}})
                self.assertTrue(clone.update_secrets("wireguard", secrets))
                self.assertEqual(clone.get_setting_by_name("wireguard").get_private_key(),private)
                with patch.object(vpn, "managed_connections", return_value=[]), \
                     patch.object(vpn, "countries", return_value=[{"code": "CA", "name": "Canada"}]):
                    prepared = vpn.prepare_location(connection, "surfshark", "CA", "")
                self.assertTrue(prepared.verify())
                self.assertFalse(prepared.get_setting_connection().get_autoconnect())
                self.assertEqual(prepared.get_setting_by_name("user").get_data("org.isolde.profile"), vpn.PROFILE)
                serialized = prepared.to_dbus(NM.ConnectionSerializationFlags.ALL).unpack()
                self.assertEqual(serialized["user"]["data"]["org.isolde.profile"],vpn.PROFILE)
                self.assertEqual(serialized["wireguard"]["private-key"],private)
                self.assertGreater(prepared.get_setting_ip6_config().get_num_addresses(), 0)
                peers = prepared.get_setting_by_name("wireguard")
                self.assertEqual(peers.get_peers_len(),2)
                self.assertEqual(peers.get_peer(0).get_preshared_key_flags(),NM.SettingSecretFlags.NONE)
                self.assertEqual(peers.get_peer(1).get_preshared_key_flags(),NM.SettingSecretFlags.NOT_REQUIRED)
                self.assertEqual(peers.get_peer(1).get_allowed_ip(0,None),"10.2.0.1/32")
                peers.set_property("fwmark",300000)
                rules=json.dumps([{"priority":30901,"table":"300000","fwmark":"0x493e0","not":None}])
                with patch.object(vpn,"command",return_value=rules):vpn.preserve_transport_routing(prepared)
                self.assertTrue(prepared.verify())
                for setting in (prepared.get_setting_ip4_config(),prepared.get_setting_ip6_config()):
                    self.assertEqual(setting.get_num_routing_rules(),2)
                    self.assertEqual(setting.get_routing_rule(1).get_fwmark(),300000)
                    self.assertTrue(setting.get_routing_rule(1).get_invert())
                    self.assertNotEqual(setting.get_route_table(),300000)


if __name__ == "__main__": unittest.main()
