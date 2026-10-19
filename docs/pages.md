# Pages and configurable libraries

Open **Settings → Home → Pages**. Pages are reusable destinations: the same page can open from a Home tab, a button, another page, or the Movies/Shows application action. Its layout belongs to the page, not to the route used to reach it.

## Design workflow

1. Choose **New blank page**, **New Movies page**, **New Shows page**, **New standalone Movies**, or **Copy Home to page**. Name it with the on-screen keyboard. Templates are editable starting points.
2. Select the page and choose **Edit components**. This opens the same editor used by Home, bound to that page's own configuration. Add/remove/reorder cards and sections, add clocks/text/buttons/library widgets, choose artwork, and edit appearance.
3. **Design page** inside that editor controls the selected page's dashboard, sections, cards and navigation tabs. It exposes the same margins, columns, spans, section heights, fitting/scrolling, palette, typography, wallpaper, glass, crops and card styles as Home. The editor identifies the selected target as a page.
4. Choose **Page navigation** under Dashboard: **left**, **top**, or **none**. Sidebar width is adjustable. The page list links Home and the profile's pages. Menu focuses the list; Right enters content from the left sidebar, Down enters it from the top bar, and Back returns. A standalone page remains reachable through Home/Back and its Settings/Edit controls.
5. In Pages, choose **Add Home tab**, **Add Home button**, **Use for Movies**, or **Use for Shows**. Adding a Home tab enables the composed Home and switches its header to navigation so the new tab is visible. **Classic Movies/Shows** restores the original library destination. From a page's editor, **Add button/tab to edited page** creates a link on that page instead of Home.

**Add button** creates a shell-owned button. In Design page → Card, edit its name and action/destination. `open-page` uses a page ID shown in the page manager; other supported actions keep the central typed validation and launch guards. **Add text widget** adds a plain-text block; edit its subtitle/message, font size and surface through the card controls. Existing image/banner/summary/source/orb/recent cards and clocks remain available. Sections provide ordered grid/lane placement and scrolling; this is not a freehand drag-and-drop canvas.

## Kodi library widgets

**Add Movies widget** and **Add Shows widget** add independent library sections. In Design page → Section, choose:

- Library kind: Movies or Shows.
- View: all, recent, continue watching, search, genre, tag or saved movie collection. Search/genre/tag/collection uses the accompanying value/ID. Genre/tag and saved movie collections apply to Movies; Shows recent/continue reads episodes from Kodi.
- Arrangement: grid, list or horizontal row, with configurable columns and artwork/card height.
- Section size/span/centered width, title/subtitle, padding, colors, opacity, blur, corners and heading typography.

Each widget has its own Kodi query, paging, selection and TV hierarchy. Appearance edits reuse that client and do not restart its read or erase selection. OK enters a widget; OK on a title browses or plays/resumes. Menu shows details, Right from details restarts, and Back returns through the TV hierarchy before leaving the widget. The toolbar provides refresh, previous/next result pages and All/Recent/Continue views. Empty/offline/invalid-source messages keep the page usable, and Refresh retries. Page navigation retains the enclosing page and originating Home card/tab. Home during playback returns to Home.

Kodi remains the source of library metadata, artwork, progress, hierarchy and playback. Widgets browse using the existing local bridge; activation passes a validated result identity to the shared Kodi player and existing native-window presentation/remote controls. No second media database or playback engine is introduced. Web/provider application contents and device setup dialogs retain their own interfaces.

## Storage and skins

Pages are stored in the profile's versioned `home-layout.json` as `pages: [{id, name, layout}]`. Each nested layout uses the same versioned appearance/composition/category/component contracts and cannot nest another page collection. An edit to a page atomically saves the containing document, with rollback of both models on failure. Existing layouts without pages still load.

Home skins now snapshot the complete page collection and managed artwork as well as Home. Applying built-in Home presets or restoring default Home preserves authored pages. Applying a saved whole-interface skin restores its own page collection. A page's own Home skins control saves a page-only snapshot under `page-skins/<page-id>`; these are separate from whole-interface skins. Skins do not copy Kodi libraries/history or install/sign in to providers. Artwork imports use content-addressed paths so the same card IDs on different pages cannot overwrite each other's images; clearing an image preserves a file still referenced elsewhere.

Deleting a page leaves its existing links visible, with an explanatory error when opened. Remove/rebind the card/tab or create a replacement link in Pages. Missing Movies/Shows destinations fall back to the classic library. Profile changes rebuild the runtime and keep page definitions and clients separate.

Bounds: 64 pages per profile, 4 MiB for the complete active layout, 128 cached library-widget clients and 512 cached artwork clients per runtime. Skin and image bounds remain as documented in [skins.md](skins.md). Library result pages retain the existing 50-item backend limit. A very large layout can reach these bounds; widgets with unavailable/invalid sources show a usable message rather than executing arbitrary configuration.

## Verification

Component tests cover page creation/reload, independent child edits, typed page actions and Movies routing, whole-interface skin restoration, stable model pointers, text/buttons, Home-preset preservation and failed-write rollback. QML tests create a page through Settings, open its component editor, render two independent Kodi widgets, verify appearance edits do not re-query or erase selection, play the selected result through the shared player and test page/history/Home recovery. Existing reference layouts remain regression cases.

`tools/tv-library-acceptance.py <installed-executable> --pages --inspect` creates a disposable real Kodi TV fixture, creates a Shows page entirely through Settings, captures its left/top/standalone modes, opens it through a Home tab and direct application action, plays/pauses/stops an episode, and saves a skin containing the page. It checks source media hashes and restores the normal service in `finally`. Synthetic media is identified as acceptance data; it is not a claim about personal libraries or streaming access.

Adding a library widget also enables composed layout when the target still uses the original grid, so its results are visible immediately. Plain-text widgets work in both the original grid and composed pages.

Library card style is independent of arrangement: media, recent, orb, source, summary or banner. This reuses the same native card components and their sizing/colors/artwork options rather than fixing library results to one poster design.
