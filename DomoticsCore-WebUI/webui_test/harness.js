// Loads index.html and app.js into jsdom with a scripted fetch. The app's own
// init() is replaced and its DOMContentLoaded bootstrap removed, so nothing
// polls: a test builds what it needs and drives it.
//
// Limits: scripts are run from outside (runScripts 'outside-only'), so the
// custom JavaScript a component injects as a <script> element is not executed.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const { JSDOM, VirtualConsole } = require('jsdom');

const SRC = path.join(__dirname, '..', 'webui_src');
const BOOTSTRAP = /document\.addEventListener\('DOMContentLoaded',\s*\(\)\s*=>\s*\{\s*new DomoticsApp\(\);\s*\}\);\s*$/;
const open = [];

// jsdom windows keep the process alive; a suite closes them after each case.
function closeAll() {
    while (open.length) open.pop().close();
}

function appSource() {
    const src = fs.readFileSync(path.join(SRC, 'app.js'), 'utf8');
    if (!BOOTSTRAP.test(src)) throw new Error('app.js bootstrap not found: update the harness');
    return src.replace(BOOTSTRAP, '');
}

function pageHtml() {
    const html = fs.readFileSync(path.join(SRC, 'index.html'), 'utf8');
    const scripts = html.match(/<script\b[\s\S]*?<\/script>/gi) || [];
    if (scripts.length !== 1) throw new Error(`index.html has ${scripts.length} scripts: update the harness`);
    return html.replace(scripts[0], '');
}

// routes: { prefix: reply | [reply, ...] | (url, opts) => reply }, longest prefix
// wins; an array is consumed one reply per call. reply: { status, body }.
function makeApp({ routes = {} } = {}) {
    const errors = [];
    const vc = new VirtualConsole();
    vc.on('jsdomError', e => errors.push(e));
    const dom = new JSDOM(pageHtml(), { runScripts: 'outside-only', url: 'http://device.local/', virtualConsole: vc });
    const { window } = dom;
    open.push(window);

    const calls = [];
    window.fetch = async (url, opts = {}) => {
        url = String(url);
        calls.push({ url, method: opts.method || 'GET', headers: opts.headers || {} });
        const key = Object.keys(routes).filter(p => url.startsWith(p)).sort((a, b) => b.length - a.length)[0];
        let reply = key ? routes[key] : { status: 404, body: {} };
        if (typeof reply === 'function') reply = reply(url, opts);
        else if (Array.isArray(reply)) reply = reply.length > 1 ? reply.shift() : reply[0];
        const status = reply.status || 200;
        const body = reply.body === undefined ? {} : reply.body;
        return {
            ok: status >= 200 && status < 300, status,
            headers: new window.Headers(reply.headers || {}),
            json: async () => body,
            text: async () => JSON.stringify(body),
        };
    };
    window.console.log = () => {};
    window.console.warn = () => {};

    window.eval(appSource() + '\nwindow.TestApp = class extends DomoticsApp { init() {} };');
    const app = new window.TestApp();
    return { app, window, document: window.document, calls, errors };
}

// A settings context the way the device serializes one.
function settingsContext(app, contextId, fields) {
    return {
        contextId, title: contextId, location: app.WebUILocation.Settings,
        alwaysInteractive: false, fields,
    };
}

module.exports = { makeApp, settingsContext, closeAll };
