# `docs/js/ui/` — Generic UI helpers

Small, page-agnostic UI utilities. See [`../README.md`](../README.md) for the
overall JS layout.

## Files

| File | Purpose |
| ---- | ------- |
| `loadFragment.js` | Fetches an HTML fragment and injects it into a container, executing any inline/external `<script>` tags it contains. Returns a helper with `get()`/`getAll()` scoped to the fragment. |
| `nav.js` | Highlights the active navbar link for the current page (`active` class + `aria-current="page"`). |
