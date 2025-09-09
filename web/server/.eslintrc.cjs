module.exports = {
  root: true,
  env: {
    node: true,
    es2021: true,
  },
  extends: ["eslint:recommended", "prettier"],
  parserOptions: {
    ecmaVersion: 2021,
    sourceType: "module",
  },
  rules: {
    // prefer error for obvious issues
    "no-unused-vars": ["warn", { "argsIgnorePattern": "^_" }],
    "no-console": "off",
  // plugin:node was removed from devDependencies; keep ES module support permissive
  // If you want node-specific checks, re-add eslint-plugin-node and the extend above.
  }
};
