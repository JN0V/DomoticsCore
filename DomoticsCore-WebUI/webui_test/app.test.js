'use strict';
const test = require('node:test');
const assert = require('node:assert');
const { makeApp, settingsContext, closeAll } = require('./harness');

const live = [];
function boot(options) {
    const e = makeApp(options);
    live.push(e);
    return e;
}

// An exception inside a handler goes to jsdom's console, not to the test.
test.afterEach(() => {
    const errors = live.flatMap(e => e.errors);
    live.length = 0;
    closeAll();
    assert.deepStrictEqual(errors.map(String), []);
});

function mountSettings(env, ctx) {
    const card = env.app.createContextCard(ctx);
    env.document.getElementById('settingsGrid').appendChild(card);
    return card;
}

test('a field renders under the id contextId_name', () => {
    const env = boot();
    const T = env.app.WebUIFieldType;
    const card = mountSettings(env, settingsContext(env.app, 'mqtt_settings', [
        { name: 'broker', label: 'Broker', type: T.Text, value: 'broker.local' },
    ]));
    const input = env.document.getElementById('mqtt_settings_broker');
    assert.ok(input, 'no element under mqtt_settings_broker');
    assert.strictEqual(input.value, 'broker.local');
    assert.ok(card.querySelector('.btn-edit'), 'a settings card has an Edit button');
});

// Cancel used to look fields up by their bare name, which matches nothing.
test('Cancel puts back what the fields held when Edit was pressed', () => {
    const env = boot();
    const T = env.app.WebUIFieldType;
    const card = mountSettings(env, settingsContext(env.app, 'mqtt_settings', [
        { name: 'broker', label: 'Broker', type: T.Text, value: 'broker.local' },
        { name: 'enabled', label: 'Enabled', type: T.Boolean, value: 'true' },
    ]));
    card.querySelector('.btn-edit').click();
    const broker = env.document.getElementById('mqtt_settings_broker');
    const enabled = env.document.getElementById('mqtt_settings_enabled');
    broker.value = 'typo.example';
    enabled.checked = false;

    card.querySelector('.btn-cancel').click();

    assert.strictEqual(broker.value, 'broker.local');
    assert.strictEqual(enabled.checked, true);
    assert.strictEqual(card.dataset.editing, 'false');
});

// A refusal the device names is shown under the field it refused.
test('a refused setting shows the device\'s reason under its field', async () => {
    const env = boot({ routes: {
        '/api/ui/token': { body: { token: 'token-1' } },
        '/api/ui/action': { body: { success: false, error: 'Port must be 1-65535' } },
    } });
    const T = env.app.WebUIFieldType;
    mountSettings(env, settingsContext(env.app, 'mqtt_settings', [
        { name: 'port', label: 'Port', type: T.Number, value: '1883' },
    ]));

    const result = await env.app.sendUICommand('mqtt_settings', 'port', '99999');

    assert.strictEqual(result.success, false);
    const row = env.document.getElementById('mqtt_settings_port').closest('.field-row');
    const msg = row.querySelector('.field-error');
    assert.ok(msg, 'no message under the field');
    assert.strictEqual(msg.textContent, 'Port must be 1-65535');
    const action = env.calls.find(c => c.url.startsWith('/api/ui/action'));
    assert.strictEqual(action.method, 'POST');
    assert.strictEqual(action.headers['X-DC-Token'], 'token-1');
});

test('a refusal with no body names the transport status', async () => {
    const env = boot({ routes: {
        '/api/ui/token': { body: { token: 'token-1' } },
        '/api/ui/action': { status: 401, body: {} },
    } });
    const T = env.app.WebUIFieldType;
    mountSettings(env, settingsContext(env.app, 'mqtt_settings', [
        { name: 'port', label: 'Port', type: T.Number, value: '1883' },
    ]));

    assert.strictEqual(await env.app.sendUICommand('mqtt_settings', 'port', '1884'), null);
    const row = env.document.getElementById('mqtt_settings_port').closest('.field-row');
    assert.strictEqual(row.querySelector('.field-error').textContent, 'Authentication required');
});

test('entering edit mode clears the last refusal', async () => {
    const env = boot({ routes: {
        '/api/ui/token': { body: { token: 'token-1' } },
        '/api/ui/action': { body: { success: false } },
    } });
    const T = env.app.WebUIFieldType;
    const card = mountSettings(env, settingsContext(env.app, 'mqtt_settings', [
        { name: 'port', label: 'Port', type: T.Number, value: '1883' },
    ]));
    await env.app.sendUICommand('mqtt_settings', 'port', 'x');
    assert.strictEqual(card.querySelector('.field-error').textContent, 'Refused');

    card.querySelector('.btn-edit').click();
    assert.strictEqual(card.querySelector('.field-error'), null);
});

// The shipped page boots itself on DOMContentLoaded; under test only the cases do.
test('loading the page starts nothing on its own', async () => {
    const env = boot();
    await new Promise(r => setTimeout(r, 50));
    assert.deepStrictEqual(env.calls, []);
});

// A token gone stale with a reboot: one refetch and one retry, then success.
test('a 403 refetches the token once and retries with the new one', async () => {
    const tokens = [{ body: { token: 'token-1' } }, { body: { token: 'token-2' } }];
    const env = boot({ routes: {
        '/api/ui/token': tokens,
        '/api/ui/action': (url, opts) => opts.headers['X-DC-Token'] === 'token-2'
            ? { body: { success: true } } : { status: 403, body: {} },
    } });
    const T = env.app.WebUIFieldType;
    const card = env.app.createContextCard(settingsContext(env.app, 'mqtt_settings', [
        { name: 'port', label: 'Port', type: T.Number, value: '1883' },
    ]));
    env.document.getElementById('settingsGrid').appendChild(card);

    const result = await env.app.sendUICommand('mqtt_settings', 'port', '1884');

    assert.deepStrictEqual(result, { success: true });
    const actions = env.calls.filter(c => c.url.startsWith('/api/ui/action'));
    assert.deepStrictEqual(actions.map(a => a.headers['X-DC-Token']), ['token-1', 'token-2']);
    assert.strictEqual(card.querySelector('.field-error'), null);
});
