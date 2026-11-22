# Isolde release signing

Isolde releases and updates are signed with an Ed25519 key pair. The **private key
is never stored in the repository**. It is held offline by the release maintainer
and provided only to the release signing environment (for example, a GitHub
Actions secret or an HSM).

## Key pair generation

Generate the pair on an offline machine. Do not copy the private key into any
repository, package, issue, log, or chat message.

```sh
# Private key (keep offline, never commit)
openssl genpkey -algorithm ED25519 -out isolde-signing-key.pem

# Public key (commit to the repo; used for verification)
openssl pkey -in isolde-signing-key.pem -pubout -out tools/isolde-signing-key.pub
```

The public key at `tools/isolde-signing-key.pub` is the trust anchor that every
Isolde installation ships with. It verifies payload signatures and update
manifests.

## Key custody

- Designate at least two release maintainers who can access the private key.
- Store the private key in a protected environment (encrypted offline storage
  or an HSM). Back it up to a separate offline medium.
- Define a revocation/rotation procedure before the first release.
- Rotate the key on a fixed schedule or immediately upon suspected compromise.
- The previous public key must remain verifiable for a transition period so
  already-installed devices can validate the last signed release.

## Signing what

1. **Appliance payload** (`isolde-<version>-linux-<arch>.tar.gz`):
   the `payload_digest()` SHA-256 of the staged release tree is signed. The
   64-byte raw signature ships as a standalone asset alongside the tarball.

2. **Update manifest** (`update-manifest.json`):
   binds version, OS, architecture, filename, size, SHA-256 and channel. The
   canonical JSON bytes are signed; the signature ships as `update-manifest.sig`.

3. **Desktop package** (`isolde_<version>_<arch>.deb`):
   signed separately by the package maintainer following Debian package signing
   conventions. Isolde's appliance updater does not touch `/opt/isolde` package
   installs; desktop updates use the package manager.

## Verification

`tools/install-appliance.py` already verifies a payload signature when
`--signature` and `--public-key` are supplied, using the same `openssl pkeyutl`
primitive as `sign-release.py`.

## Rotation

To rotate, publish a release signed with the new key and ship the new public
key with the next application update. Until devices have picked up the new
public key, keep the old public key available for verifying the previous
release.
