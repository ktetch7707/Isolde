# App-supplied artwork

`roles.json` and `foreground/` define explicit full-colour foreground assets,
ink-mask exclusions, optical scale and contrast backings. Originals below are
unchanged. Run `tools/prepare-app-icon-roles.ps1` to reproduce these SVG/text
derivatives; hashes use UTF-8/LF normalization. Runtime rasterization is bounded
to512px and caches foreground and ink roles separately in the active profile.
Only declared knockout colours become negative space; white is never globally
erased from colour artwork. Selection glow is a renderer effect, not an asset.
See `docs/app-icons-implementation-2026-11-10.md` for verification and limits.

`youtube.svg` is the unmodified `video-youtube-red` symbol from [YouTube's official brand site](https://brand.youtube/), retrieved 2026-10-23 and wrapped in an SVG document. YouTube retains its trademark. This local asset requires no runtime request. It is registered as declarative app metadata, not special-cased in the renderer.

Desktop applications resolve their own `.desktop` Icon metadata or installed icon theme. Resolved artwork is rasterized into a bounded, content-addressed, profile-scoped cache; generated cache files are never committed. User-authored launcher artwork takes precedence.

The bundled provider icons listed in `provenance.json` are normalized copies from pinned Simple Icons revisions or the provider's public site. Their original sources, hashes and changes are recorded there. See `SIMPLE-ICONS-LICENSE` and `SIMPLE-ICONS-DISCLAIMER.md`; project licensing does not grant trademark rights. These assets require no runtime network requests.

Movies, Shows, Settings, Store, Desktop and capture-controller symbols are original Isolde semantic artwork. Registered app metadata selects packaged artwork generically. User-authored launcher artwork retains precedence.
