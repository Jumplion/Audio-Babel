// src/utils/base64Url.js

function encodeUrlSafeBase64(input) {
    const base64 = Buffer.from(input).toString('base64');
    return base64.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

function decodeUrlSafeBase64(input) {
    const base64 = input.replace(/-/g, '+').replace(/_/g, '/');
    const buffer = Buffer.from(base64, 'base64');
    return buffer.toString('utf-8');
}

module.exports = {
    encodeUrlSafeBase64,
    decodeUrlSafeBase64
};