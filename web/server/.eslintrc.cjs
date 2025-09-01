module.exports = {
  root: true,
  env: {
    node: true,
    es2021: true,
  },
  extends: ["eslint:recommended", "plugin:node/recommended", "prettier"],
  parserOptions: {
    ecmaVersion: 2021,
    sourceType: "module",
  },
  rules: {
    // prefer error for obvious issues
    "no-unused-vars": ["warn", { "argsIgnorePattern": "^_" }],
    "no-console": "off",
    "node/no-unsupported-features/es-syntax": ["error", { "ignores": ["modules"] }]
  }
};
