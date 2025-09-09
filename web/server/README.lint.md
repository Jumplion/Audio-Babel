This folder configures linters for frontend HTML/CSS/JS and server JS.

Run from this directory:

- Install dependencies: `npm install` (we use devDependencies for eslint, stylelint, htmlhint, prettier)
- Run all linters: `npm run lint`
- Run individual linters: `npm run lint:js`, `npm run lint:css`, `npm run lint:html`

Notes:
- ESLint is configured for both server code (`src/**/*.js`) and frontend module JS (`../frontend/public/js/**`).
- Stylelint uses `stylelint-config-standard` and will report many stylistic issues in `../frontend/public/css/site.css`.
- HTMLHint rules live in `.htmlhintrc` and will report missing attributes and doctype issues in `../frontend/public`.

Next steps you can choose:
- Fix reported issues to satisfy the linters (recommended for CI).
- Relax or customize rules (I can help add rule exceptions or ignore specific files).
