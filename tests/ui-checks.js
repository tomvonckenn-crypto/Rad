// Tests the actual page script with a DOM/API fixture; no device is contacted.
async function runUiChecks(source, ids) {
  const elements = Object.fromEntries(ids.map(id => [id, {
    disabled: false, value: '0', textContent: '', listeners: {},
    classList: { add() {}, remove() {} },
    addEventListener(name, handler) { this.listeners[name] = handler; }
  }]));
  const document = { getElementById(id) {
    if (!elements[id]) throw new Error('Missing HTML element: ' + id);
    return elements[id];
  }};
  let server = { state: 'locked', stop_reason: 'boot', safety_epoch: 100,
    control_armed: false, target_pwm: 0, actual_pwm: 0, direction: 'none',
    max_pwm: 300, station_connected: true, station_ip: '127.0.0.1',
    ap_ip: '192.168.4.1', version: 'test', build: 'test' };
  const calls = [];
  let holdArm = false, releaseArm = null, failStop = false, failControl = false;
  const clone = value => JSON.parse(JSON.stringify(value));
  function response(body, ok = true) { return { ok, json: async () => clone(body) }; }
  async function fetch(path, options = {}) {
    const data = options.body ? options.body.data : {};
    calls.push({ path, data: clone(data) });
    if (path === '/api/control' && failControl) throw new Error('Disconnected');
    if (path === '/api/status' || path === '/api/heartbeat') return response(server);
    if (path === '/api/stop') {
      if (failStop) throw new Error('Disconnected');
      server = { ...server, state: 'locked', stop_reason: 'emergency',
        control_armed: false, direction: 'none', target_pwm: 0, actual_pwm: 0,
        safety_epoch: server.safety_epoch + 1 };
      return response(server);
    }
    if (path === '/api/control' && data.action === 'arm') {
      server = { ...server, state: 'ready', stop_reason: 'none', control_armed: true };
      const snapshot = { ...clone(server), session_token: 42 };
      if (holdArm) await new Promise(resolve => { releaseArm = resolve; });
      return response(snapshot);
    }
    if (path === '/api/control' && data.token === '42') {
      if (data.action === 'start') server = { ...server, state: 'running', direction: data.direction };
      if (data.action === 'step') server.target_pwm += Number(data.delta);
      return response(server);
    }
    return response({ error: 'Command denied' }, false);
  }
  class Params { constructor(data) { this.data = data; } }
  class Abort { constructor() { this.signal = {}; } abort() {} }
  const execute = new Function('document', 'fetch', 'URLSearchParams', 'AbortController',
    'setTimeout', 'clearTimeout', 'setInterval', 'confirm', source +
    '\nreturn { command, emergency, poll, render, snapshot:()=>({token,epoch,state,online,busy,generation}) };');
  const ui = execute(document, fetch, Params, Abort, () => 1, () => {}, () => {}, () => true);
  const settle = async () => { for (let i = 0; i < 12; i++) await Promise.resolve(); };
  const assert = (condition, message) => { if (!condition) throw new Error(message); };
  await settle();
  assert(!elements.arm.disabled && elements.left.disabled && elements.plus.disabled, 'Boot controls must be locked');
  assert(!calls.some(call => call.path === '/api/control'), 'Page load must not arm or start');
  assert(typeof elements.emergency.listeners.click === 'function', 'Stop button listener missing');
  await ui.command('arm', { confirm: 'stil', epoch: '100' });
  assert(ui.snapshot().token === '42' && !elements.left.disabled && elements.plus.disabled, 'Arming must only enable direction selection');
  await ui.command('start', { direction: 'right' });
  assert(!elements.plus.disabled && elements.left.disabled && elements.right.disabled, 'Direction buttons must lock during running');
  assert(Number(elements.current.textContent) === 0 && Number(elements.target.textContent) === 0, 'Direction start must stay at zero');
  await ui.command('step', { delta: '1' });
  assert(Number(elements.target.textContent) === 1, 'PWM step not displayed');
  await ui.emergency();
  assert(ui.snapshot().token === '' && elements.plus.disabled && elements.left.disabled, 'Stop must revoke control token');
  assert(Number(elements.target.textContent) === 0 && elements.message.textContent.includes('NOODSTOP actief'), 'Stop must be confirmed on page');
  // Simulate a successful arm response held in transit until after the stop.
  holdArm = true;
  const pending = ui.command('arm', { confirm: 'stil', epoch: String(server.safety_epoch) });
  await settle();
  assert(typeof releaseArm === 'function', 'Delayed response fixture not started');
  await ui.emergency();
  releaseArm(); await pending; await settle();
  assert(ui.snapshot().token === '' && elements.left.disabled && elements.plus.disabled, 'Late arm response must not undo stop');
  assert(elements.message.textContent.includes('NOODSTOP actief'), 'Late response must not overwrite stop message');
  failStop = true;
  await ui.emergency();
  assert(elements.message.textContent.includes('STOP NIET BEVESTIGD') && elements.arm.disabled, 'Unconfirmed stop must be clearly shown');
  assert(!elements.emergency.disabled, 'Stop button must remain available while offline');
  failStop = false; holdArm = false;
  await ui.poll();
  assert(!elements.arm.disabled, 'Control must recover after status reconnects');
  failControl = true;
  await ui.command('arm', { confirm: 'stil', epoch: String(server.safety_epoch) });
  assert(!ui.snapshot().busy && !ui.snapshot().online, 'Failed command must clear pending flag');
  await ui.poll();
  assert(!elements.arm.disabled && ui.snapshot().token === '', 'Reconnect must require fresh arming');
  return { passed: true, checks: ['boot locked', 'no automatic arming', 'direction starts at zero',
    'direction buttons lock', 'PWM step display', 'stop revokes session', 'late response rejected', 'failed stop reported', 'reconnect requires rearming'] };
}

if (typeof module !== 'undefined') module.exports = runUiChecks;
if (typeof require !== 'undefined' && require.main === module) {
  const source = require('fs').readFileSync(require('path').join(__dirname, '../include/control_page.h'), 'utf8');
  runUiChecks(source.split('<script>')[1].split('</script>')[0], [...source.matchAll(/id="([^"]+)"/g)].map(match => match[1]))
    .then(result => console.log(JSON.stringify(result))).catch(error => { console.error(error); process.exit(1); });
}
