/**
 * indexViewer.js
 *
 * Renders a full audio index as a standalone HTML page in a new browser tab.
 */

import { escapeHtml } from '../utils/dom.js';

/**
 * Generate HTML for index viewer page
 * @param {string} indexContent - Full index string to display
 * @returns {string} Complete HTML page
 */
function generateIndexViewerHTML(indexContent) {
  return `
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Audio Index</title>
      <style>
        body {
          font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
          background: #1a1f2e;
          color: #e0e0e0;
          padding: 20px;
          margin: 0;
        }
        pre {
          white-space: pre-wrap;
          word-break: break-all;
          background: #0f1419;
          padding: 20px;
          border-radius: 8px;
          border: 1px solid #2a3f5f;
          line-height: 1.5;
        }
        h1 {
          color: #64b5f6;
          margin-bottom: 20px;
        }
      </style>
    </head>
    <body>
      <h1>Audio Index</h1>
      <pre>${escapeHtml(indexContent)}</pre>
    </body>
    </html>
  `;
}

/**
 * Open index content in a new browser tab
 * @param {string} fullIndex - Complete index string
 */
export function openIndexInNewTab(fullIndex) {
  const newWindow = window.open('', '_blank');
  if (newWindow) {
    newWindow.document.write(generateIndexViewerHTML(fullIndex));
    newWindow.document.close();
  }
}
