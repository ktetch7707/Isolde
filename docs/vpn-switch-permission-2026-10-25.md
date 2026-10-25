# Proposed automatic VPN switching permission

The existing approval covers the `org.isolde.vpn.capture` action for `isolde-dev`, running the root-owned `/usr/libexec/isolde/vpn-copy` inside Isolde-Test. It does not explicitly cover a persistent automatic switching action. The guest reports `NetworkManager.network-control:auth` and `settings.modify.own:auth`, so an unattended user process cannot complete the switch.

Proposed installer extension, pending explicit user approval:

- Add action `org.isolde.vpn.switch` to `/usr/share/polkit-1/actions/org.isolde.vpn.policy`.
- Bind it to executable `/usr/libexec/isolde/vpn-copy` and first argument `switch`. Bind the existing capture action to first argument `capture`.
- Extend `/usr/share/polkit-1/rules.d/50-isolde-vpn.rules` so only `isolde-dev` receives passwordless access to those two actions.
- The root-owned tool accepts a validated target UUID and fixed Isolde profile identity. Switching reads no WireGuard keys or provider account files.
- Before any change, require the target and any existing VPN to belong to that Isolde profile and installing Linux user. Refuse an unrelated/external VPN. Only the explicit capture transaction can hand off from its selected native-provider source.
- Verify IPv4/IPv6 routing and observed country before opening the app. Failure/cancellation rolls back the previous owned location. Never modify physical network profiles or grant general NetworkManager administration.
- Package and verify the exact helper/action/rule together through the existing repeatable installer. No permission is added at startup or by status checks.

This proposal changes only the guest. No host policy or network setting is involved. Installation of the new permission remains pending user approval.
