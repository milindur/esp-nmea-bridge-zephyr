const $ = (id) => document.getElementById(id);
const fmt = new Intl.NumberFormat('en-US');
const format = (v) => fmt.format(v ?? 0);

function setState(id, state, okState) {
  const el = $(id);
  if (!el) return;
  const isOk = state === okState;
  el.classList.remove('state-ok', 'state-warn', 'state-bad');
  el.classList.add(isOk ? 'state-ok' : 'state-warn');
  el.querySelector('b').textContent = state.toUpperCase();
}

function setOffline() {
  for (const id of ['connection-state', 'input-state', 'wifi-state']) {
    const el = $(id);
    el.classList.remove('state-ok', 'state-warn');
    el.classList.add('state-bad');
    el.querySelector('b').textContent = 'OFFLINE';
  }
}

function rowItem(label, value, severity) {
  const cls = severity ? ` class="row is-${severity}"` : ' class="row"';
  return `<div${cls}><dt>${label}</dt><dd>${value}</dd></div>`;
}

function render(status) {
  setState('connection-state', status.connection_state, 'connected');
  setState('input-state', status.input_state, 'active');
  setState('wifi-state', status.wifi.sta_ready ? 'linked' : 'idle', 'linked');

  $('frames').textContent = format(status.bridge.frames_in);
  $('bytes').textContent = format(status.uart.bytes_rx);
  $('clients').textContent = `${format(status.tcp_server.active_clients)} / ${format(status.tcp_server.max_clients)}`;
  $('drops').textContent = format(status.bridge.sink_dropped_oldest);

  const warnCount = (status.warnings.data_quality ? 1 : 0) + (status.warnings.frame_loss ? 1 : 0);
  $('warn-count').textContent = String(warnCount);

  $('details').innerHTML =
    rowItem('Active TCP NMEA sessions', format(status.tcp.active_sessions), status.tcp.active_sessions > 0 ? 'ok' : null) +
    rowItem('Inbound TCP clients', `${format(status.tcp_server.active_clients)} / ${format(status.tcp_server.max_clients)}`) +
    rowItem('UART lines received', format(status.uart.lines_rx)) +
    rowItem('Overlong UART lines', format(status.uart.overlong_lines), status.uart.overlong_lines > 0 ? 'warn' : null) +
    rowItem('Ingest dropped (oldest)', format(status.bridge.ingest_dropped_oldest), status.bridge.ingest_dropped_oldest > 0 ? 'warn' : null) +
    rowItem('No-sink publishes', format(status.bridge.publish_no_sinks)) +
    rowItem('Invalid / oversize', `${format(status.bridge.publish_invalid)} · ${format(status.bridge.publish_oversize)}`) +
    rowItem('STA IPv4 ready', status.wifi.sta_ready ? 'yes' : 'no', status.wifi.sta_ready ? 'ok' : null) +
    rowItem('Data quality warning', status.warnings.data_quality ? 'yes' : 'no', status.warnings.data_quality ? 'bad' : 'ok') +
    rowItem('Frame loss warning', status.warnings.frame_loss ? 'yes' : 'no', status.warnings.frame_loss ? 'bad' : 'ok');

  $('updated').textContent = `Updated ${new Date().toLocaleTimeString([], {hour:'2-digit', minute:'2-digit', second:'2-digit'})}`;
}

// Synthetic data so the preview is live when /api/status is unreachable.
const mock = {
  framesBase: 1284, bytesBase: 198410, linesBase: 1389, drops: 0,
  startedAt: Date.now(),
};

function nextMock() {
  const e = (Date.now() - mock.startedAt) / 1000;
  mock.framesBase += Math.floor(2 + Math.random() * 4);
  mock.bytesBase += Math.floor(180 + Math.random() * 220);
  mock.linesBase += Math.floor(2 + Math.random() * 4);
  if (Math.random() < 0.04) mock.drops += 1;

  return {
    connection_state: e < 5 ? 'idle' : 'connected',
    input_state: e < 2 ? 'idle' : 'active',
    bridge: {
      frames_in: mock.framesBase,
      sink_dropped_oldest: mock.drops,
      ingest_dropped_oldest: 0,
      publish_no_sinks: 12,
      publish_invalid: 2,
      publish_oversize: 0,
    },
    uart: { bytes_rx: mock.bytesBase, lines_rx: mock.linesBase, overlong_lines: 0 },
    tcp: { active_sessions: e < 5 ? 0 : 2 },
    tcp_server: { active_clients: e < 10 ? 0 : 2, max_clients: 4 },
    wifi: { sta_ready: e > 3 },
    warnings: { data_quality: false, frame_loss: false },
  };
}

async function pollStatus() {
  try {
    const r = await fetch('/api/status', { cache: 'no-store' });
    if (!r.ok) throw new Error(r.status);
    render(await r.json());
  } catch {
    render(nextMock());
  }
}

pollStatus();
setInterval(pollStatus, 2000);
