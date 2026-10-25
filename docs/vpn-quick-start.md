# VPN setup in Isolde

**Verification status:** both providers install successfully in Isolde-Test. Saved-location identity is verified. Unattended switching is still awaiting its separate permission and live acceptance; this is the intended setup flow, not a completed acceptance claim.

1. Open **Settings → VPN**.
2. Select **Install default VPN providers (Surfshark + Proton VPN)**, then **Install**. The installer downloads both official clients and Isolde's location-saving support. Repeating installation is supported.
3. Select **Open Surfshark — sign in / choose country / settings**. Sign in inside the VM if needed, choose WireGuard in the provider's connection settings, and connect to **Canada**.
4. Press **Home** to return to Isolde. Select **Surfshark — Save connected location**, then **Save and use the connected country**. Isolde handles the connection keys locally and checks the country.
5. Repeat the provider connection and save steps for **United States**. Both saved countries then appear in Isolde.
6. Select **Choose a VPN location for each app → CBC Gem → Surfshark — Canada**. Other apps can use either saved country or **Keep current connection**.

The intended automatic behavior is: open CBC Gem, Isolde switches to its assigned Canada connection, checks routing and country, then opens Gem. If verification fails, Gem stays closed and Isolde shows the error. Connections affect the whole VM.

For Proton VPN, use the corresponding **Open Proton VPN** and **Proton VPN — Save connected location** buttons. Sign-in stays in the provider app. The same saved-location and app-assignment workflow applies.

Use **Up/Down** to choose, **Confirm/Enter** to select, **Back/Esc** to cancel, and **Home** to return from a provider app. No browser downloads, copied keys, configuration files or terminal commands are part of the normal flow. **Advanced manual setup** is optional.
