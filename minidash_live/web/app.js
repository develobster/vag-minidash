import { createSettingsUI } from "./settings.js";
import { preferred, unitFor, dateTime } from "./units.js";
import {
  TYPES,
  escape as e,
  uid,
  freePosition,
  makeWidget,
  relayout,
  moveWidget,
  reading,
  displayReading,
  draftIsCompatible,
} from "./model.js";
const $ = (s) => document.querySelector(s),
  main = $("#main"),
  sheet = $("#sheet");
let catalogKey = "",
  config,
  base,
  revision = 0,
  dirty = false,
  connected = false,
  telemetry = null,
  page = "dashboard",
  screenId = null,
  selected = null,
  source,
  lastMessage = 0,
  logs = [],
  history = {},
  search = "",
  filter = "all",
  saving = false;
const clone = (v) => JSON.parse(JSON.stringify(v));
const deviceSettings = createSettingsUI({
  config: () => config,
  base: () => base,
  page: () => page,
  revision: () => revision,
  dirty: () => dirty,
  telemetry: () => telemetry,
  signals: () => signals(),
  preview: () => preview(),
  api,
  toast,
  render,
  changed,
  openSheet,
  closeSheet: () => sheet.close(),
  ask,
  download,
  accept(result, pending) {
    if (telemetry && result.operation) telemetry.operation = result.operation;
    revision = result.revision;
    base = clone(result.configuration);
    if (!pending || JSON.stringify(config) === JSON.stringify(pending)) {
      config = result.configuration;
      dirty = false;
      $("#savebar").hidden = true;
      try {
        localStorage.removeItem("minidash-draft");
      } catch {}
    } else persist();
    deviceSettings.bar();
  },
});

const signals = () =>
  telemetry?.signals || [
    {
      signal: "engine.rpm",
      label: "Engine speed",
      unit: "rpm",
      min: 0,
      max: 8000,
    },
  ];
const currentScreen = () =>
  config.screens.find((s) => s.id === screenId) ||
  config.screens.find((s) => s.id === config.active_screen);
const theme = () => config.themes.find((t) => t.id === config.active_theme);
const button = (label, action, cls = "") =>
  `<button class="${cls}" data-action="${action}">${label}</button>`;
function toast(text) {
  $("#toast").textContent = text;
  $("#toast").style.display = "block";
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => ($("#toast").style.display = "none"), 4500);
}
async function api(path, method = "GET", body) {
  const r = await fetch("/api/" + path, {
    method,
    headers: body ? { "Content-Type": "application/json" } : {},
    body: body ? JSON.stringify(body) : undefined,
    cache: "no-store",
  });
  const data = await r.json();
  if (!r.ok) throw Error(data.error || `Request failed (${r.status})`);
  return data;
}
function persist() {
  try {
    localStorage.setItem(
      "minidash-draft",
      JSON.stringify({ configuration: config, revision, base }),
    );
  } catch {
    toast("Phone storage unavailable. Keep this tab open until saved.");
  }
}
function changed() {
  dirty = true;
  persist();
  $("#savebar").hidden = false;
  appearance();
  updateDisplayPreview();
  deviceSettings.bar();
}
function updateDisplayPreview() {
  if (!config) return;
  const settings = config.device_settings.display;
  let mode = settings.mode;
  if (mode === "auto") {
    const hour = Number(
      new Intl.DateTimeFormat("en", {
        timeZone: config.device_settings.general.timezone,
        hour: "numeric",
        hour12: false,
      }).format(new Date()),
    );
    mode = hour >= 7 && hour < 20 ? "day" : "night";
  }
  const level =
    mode === "day"
      ? settings.day_brightness
      : mode === "night"
        ? settings.night_brightness
        : settings.brightness;
  document.querySelectorAll(".preview").forEach((el) => {
    el.style.filter = `brightness(${level / 100})`;
    const rotated = [90, 270].includes(settings.rotation);
    el.style.setProperty(
      "--ratio",
      rotated
        ? `${config.settings.display_height}/${config.settings.display_width}`
        : `${config.settings.display_width}/${config.settings.display_height}`,
    );
  });
}
function appearance() {
  const setting = config.settings.appearance;
  document.body.classList.toggle(
    "light",
    setting === "light" ||
      (setting === "auto" &&
        matchMedia("(prefers-color-scheme:light)").matches),
  );
}
function input(label, name, value, type = "text", extra = "") {
  return `<label>${e(label)}<input name="${name}" type="${type}" value="${e(value)}" ${extra}></label>`;
}
function select(label, name, options, value) {
  return `<label>${e(label)}<select name="${name}">${options
    .map((o) => {
      const [v, l] = Array.isArray(o) ? o : [o, o];
      return `<option value="${e(v)}" ${v === value ? "selected" : ""}>${e(l)}</option>`;
    })
    .join("")}</select></label>`;
}
function preview(s = currentScreen(), editable = false) {
  const t = theme();
  return `<div class="preview" style="--display-bg:${t.background};--display-fg:${t.foreground};--display-accent:${t.accent};--warn:${t.warning};--critical:${t.critical};--spacing:${t.spacing}px;--display-font:${t.typography === "mono" ? "monospace" : "system-ui"};--gauge-radius:${t.gauge === "square" ? 0 : 10}px;--ratio:${config.settings.display_width}/${config.settings.display_height};grid-template-columns:repeat(${s.columns},minmax(0,1fr));${t.grid ? "background-image:linear-gradient(#8881 1px,transparent 1px),linear-gradient(90deg,#8881 1px,transparent 1px);background-size:16px 16px;" : ""}">${s.widgets.map((w) => `<button class="${w.type}" ${editable ? `data-widget="${e(w.id)}"` : ""} style="grid-column:${w.x + 1}/span ${w.width};grid-row:${w.y + 1}/span ${w.height}" aria-label="${e(w.label)}${editable ? " settings" : ""}"><span class="label">${e(w.label)}</span><span class="value" data-value="${e(w.signal)}" data-precision="${w.precision}" data-warning="${w.warning}" data-critical="${w.critical}">—</span><span class="unit">${e(unitFor(w.units, config.device_settings.general))}</span>${["gauge", "compact"].includes(w.type) ? `<span class="visual dial"><svg viewBox="0 0 100 52"><path d="M5 47 A45 45 0 0 1 95 47" fill="none" stroke="${t.foreground}33" stroke-width="7"/><path d="M5 47 A45 45 0 0 1 95 47" fill="none" stroke="${t.accent}" stroke-width="7" pathLength="100" data-gauge="${e(w.signal)}" data-min="${w.min}" data-max="${w.max}" stroke-dasharray="0 100"/></svg></span>` : w.type === "bar" ? `<span class="track"><i data-bar="${e(w.signal)}" data-min="${w.min}" data-max="${w.max}"></i></span>` : w.type === "graph" ? `<span class="visual"><svg viewBox="0 0 100 22" preserveAspectRatio="none"><polyline data-graph="${e(w.signal)}" data-min="${w.min}" data-max="${w.max}" fill="none" stroke="${t.accent}" stroke-width="2"/></svg></span>` : ""}</button>`).join("")}</div>`;
}
function dashboard() {
  const active = dirty
    ? config.active_screen
    : telemetry?.active_screen || config.active_screen;
  const s =
    config.screens.find((s) => s.id === active) ||
    config.screens.find((s) => s.id === config.active_screen);
  return `<div class="eyebrow">YOUR DRIVE · YOUR DISPLAY</div><h1>Ready at a glance</h1><p>Live signals and the controls you use most.</p><div class="columns"><section><div class="row"><h2>${e(s.name)}</h2><small>${s.widgets.length} widgets · ${config.settings.display_width} × ${config.settings.display_height}</small></div>${preview(s)}<small>Host preview · physical display sync unavailable</small><div class="actions" style="margin-top:16px">${button("Edit screens", "screens", "primary")}${button("Add a signal", "signals")}</div></section><section><div class="grid"><div class="stat"><small>CAN</small><strong data-status="can">—</strong></div><div class="stat"><small>Vehicle</small><strong data-status="vehicle">—</strong></div><div class="stat"><small>Uptime</small><strong data-status="uptime">—</strong></div><div class="stat"><small>ECUs</small><strong data-status="ecus">—</strong></div></div><div class="card"><h2>Trip recording</h2><p data-log-status>No recording</p>${button("Start recording", "record", "primary")}<p><small>Records configured signals on this host. No ECU writes.</small></p></div>${button("Device status", "device")}</section></div>`;
}
function screens() {
  const s = currentScreen();
  return `<h1>Your screens</h1><p>Tap a widget to edit. Every move snaps to the grid.</p><div class="row"><select id="screen-select" aria-label="Screen">${config.screens.map((s) => `<option value="${e(s.id)}" ${s.id === currentScreen().id ? "selected" : ""}>${e(s.name)}</option>`).join("")}</select></div><div class="actions" style="margin-top:10px">${button("New screen", "new-screen")}${button(s.id === config.active_screen ? "Active screen" : "Set active", "activate", s.id === config.active_screen ? "selected" : "primary")}</div><div class="columns"><section><div class="card">${input("Screen name", "screen-name", s.name)}<label>Layout</label><div class="actions">${[1, 2, 3, 4, 6, 8].map((n) => button(String(n), `layout-${n}`)).join("")}</div>${preview(s, true)}<small>320 × 240 reference; custom positions and size available in widget settings.</small><div class="actions">${button("Add widget", "add-widget")}${button("Duplicate screen", "duplicate-screen")}${button("Delete screen", "delete-screen", "danger")}</div></div></section><section><div class="card"><h2>Display theme</h2>${select(
    "Theme",
    "theme-select",
    config.themes.map((t) => [t.id, t.name]),
    config.active_theme,
  )}<div class="actions" style="margin-top:12px">${button("Edit theme", "theme")}${button("Duplicate", "duplicate-theme")}</div><p><small>Themes, thresholds and layouts save together.</small></p></div></section></div>`;
}
function signalCards() {
  const rows = signals().filter(
    (s) =>
      (s.label + " " + s.signal).toLowerCase().includes(search.toLowerCase()) &&
      (filter === "all" ||
        s.source === filter ||
        (filter === "unavailable" && s.quality !== "valid")),
  );
  return rows.length
    ? rows
        .map(
          (s) =>
            `<article class="card signal"><div class="name"><h3>${e(s.label || s.signal)}</h3><small>${e(s.signal)}</small></div><div><strong data-value="${e(s.signal)}" data-precision="1">—</strong> <small>${e(unitFor(s.unit, config.device_settings.general))}</small></div><div class="meta" data-meta="${e(s.signal)}">${e(s.source)} · ${e(s.quality)}</div><button data-add-signal="${e(s.signal)}">＋ Add to screen</button></article>`,
        )
        .join("")
    : '<div class="card">No signals match. Try a different search or source.</div>';
}
function signalPage() {
  return `<h1>Signal library</h1><p>Canonical values. Source and freshness stay visible.</p><input id="search" type="search" aria-label="Search signals" placeholder="Search signals…" value="${e(search)}">${select("Source", "source-filter", ["all", "can", "uds", "simulated", "unavailable"], filter)}<div id="signal-cards" class="cards">${signalCards()}</div>`;
}
function logPage() {
  return `<h1>Trip recordings</h1><p>Save a session, then download or inspect its samples.</p><div class="card"><p data-log-status>Connecting…</p>${button("Start recording", "record", "primary")}<small style="display:block;margin-top:12px">Bounded to 1 hour / 20 MB per recording. Files are stored on the host.</small></div><div class="row"><h2>Saved recordings</h2>${button("Refresh", "reload-logs")}</div><div id="log-list">Loading recordings…</div>`;
}
function renderLogs() {
  if (page !== "logs") return;
  $("#log-list").innerHTML = logs.length
    ? logs
        .map(
          (l) =>
            `<article class="card"><div class="row"><h3>${e(dateTime(l.date, config.device_settings.general))}</h3><small>${e(l.mode)}</small></div><p>${Math.round(l.duration)} s · ${l.samples} samples · ${(l.bytes / 1024).toFixed(1)} KB</p><small>${l.signals.map(e).join(" · ")}</small><div class="actions" style="margin-top:12px"><button data-log-graph="${l.id}">Graph</button><button data-log-download="${l.id}">Download</button><button class="danger" data-log-delete="${l.id}">Delete</button></div></article>`,
        )
        .join("")
    : '<div class="card"><h3>No recordings yet</h3><p>Start a recording to capture the signals configured on your screens.</p></div>';
}
const pair = (label, value) =>
  `<div class="pair"><span>${e(label)}</span><span>${e(value ?? "Unavailable")}</span></div>`;
function device() {
  return deviceSettings.page();
}

function render() {
  if (!config) return;
  main.innerHTML = {
    dashboard,
    screens,
    signals: signalPage,
    logs: logPage,
    device,
  }[page]();
  document
    .querySelectorAll("[data-page]")
    .forEach((b) =>
      b.setAttribute(
        "aria-current",
        b.dataset.page === page ? "page" : "false",
      ),
    );
  appearance();
  updateDisplayPreview();
  deviceSettings.bar();
  updateLive();
  if (page === "logs") loadLogs();
  if (page === "device")
    $("#pwa-note").textContent = window.isSecureContext
      ? "Offline shell available on this secure origin. Install using your browser menu."
      : "PWA installation/offline requires HTTPS or localhost. Plain HTTP over car Wi-Fi remains a browser UI.";
}
function navigate(next) {
  if (!["dashboard", "screens", "signals", "logs", "device"].includes(next))
    next = "dashboard";
  page = next;
  location.hash = next;
  render();
  window.scrollTo(0, 0);
}
function updateLive() {
  const map = new Map(signals().map((s) => [s.signal, s]));
  document.querySelectorAll("[data-value]").forEach((el) => {
    const row = map.get(el.dataset.value);
    const v = displayReading(row, connected);
    const shown = preferred(v, row?.unit, config.device_settings.general).value;
    el.textContent =
      v === null
        ? "—"
        : typeof v === "number"
          ? shown.toFixed(Number(el.dataset.precision || 0))
          : v;
    el.classList.toggle("warn", v !== null && v >= Number(el.dataset.warning));
    el.classList.toggle(
      "critical",
      v !== null && v >= Number(el.dataset.critical),
    );
  });
  document.querySelectorAll("[data-bar]").forEach((el) => {
    const v = reading(map.get(el.dataset.bar), connected);
    const fraction =
      v === null
        ? 0
        : Math.max(
            0,
            Math.min(
              1,
              (v - Number(el.dataset.min)) /
                (Number(el.dataset.max) - Number(el.dataset.min)),
            ),
          );
    el.style.width = `${fraction * 100}%`;
  });
  document.querySelectorAll("[data-gauge]").forEach((el) => {
    const v = reading(map.get(el.dataset.gauge), connected);
    const percent =
      v === null
        ? 0
        : Math.max(
            0,
            Math.min(
              100,
              ((v - Number(el.dataset.min)) /
                (Number(el.dataset.max) - Number(el.dataset.min))) *
                100,
            ),
          );
    el.setAttribute("stroke-dasharray", percent + " 100");
  });
  document.querySelectorAll("[data-graph]").forEach((el) => {
    const points = connected ? history[el.dataset.graph] || [] : [];
    el.setAttribute(
      "points",
      points
        .map(
          (v, i) =>
            `${(i / Math.max(1, points.length - 1)) * 100},${22 - Math.max(0, Math.min(1, (v - Number(el.dataset.min)) / (Number(el.dataset.max) - Number(el.dataset.min)))) * 22}`,
        )
        .join(" "),
    );
  });
  document.querySelectorAll("[data-meta]").forEach((el) => {
    const s = map.get(el.dataset.meta);
    el.textContent = `${s?.source || "unavailable"} · ${connected ? s?.quality || "unavailable" : "stale · disconnected"} · age ${s?.age == null ? "unknown" : s.age.toFixed(1) + " s"} · ${s?.expected_period ? `${(1 / s.expected_period).toFixed(1)} Hz expected` : "rate unknown"} · range ${s?.min ?? "?"}–${s?.max ?? "?"}`;
  });
  updateDisplayPreview();
  deviceSettings.live(telemetry);
  if (!telemetry) return;
  document.querySelectorAll("[data-status]").forEach((el) => {
    const key = el.dataset.status;
    el.textContent =
      key === "uptime"
        ? `${Math.floor(telemetry.uptime / 60)}m ${Math.floor(telemetry.uptime % 60)}s`
        : key === "ecus"
          ? telemetry.device.ecus.length || "Unknown"
          : telemetry.device[key] || "Unknown";
  });
  const log = telemetry.logging;
  document
    .querySelectorAll("[data-log-status]")
    .forEach(
      (el) =>
        (el.textContent = log.active
          ? `${Math.floor(log.duration)} s · ${log.samples} samples · ${(log.bytes / 1024).toFixed(1)} KB · ${log.signals.length} signals`
          : log.error || "No active recording"),
    );
  document.querySelectorAll("[data-action=record]").forEach((b) => {
    b.textContent = log.active ? "Stop recording" : "Start recording";
    b.disabled = !connected;
  });
  if ($("#device-status"))
    $("#device-status").innerHTML =
      pair("Platform", telemetry.device.platform) +
      pair("Build", telemetry.device.build) +
      pair("Firmware", telemetry.device.firmware) +
      pair("CAN", telemetry.device.can) +
      pair("Vehicle", telemetry.device.vehicle) +
      pair(
        "ECUs",
        telemetry.device.ecus.length ||
          (telemetry.mode === "simulation" ? "None · simulation" : "Unknown"),
      ) +
      telemetry.device.ecus
        .map((ecu) =>
          pair(
            ecu.role || ecu,
            typeof ecu === "object"
              ? `${ecu.state} · ${ecu.identity_verified ? "identity verified" : "identity unverified"}`
              : "Discovered",
          ),
        )
        .join("");
  document.querySelectorAll("[data-action=pause]").forEach((b) => {
    b.disabled = telemetry.mode !== "simulation";
    b.textContent =
      telemetry.mode !== "simulation"
        ? "Simulation unavailable in live runtime"
        : telemetry.status === "simulation_paused"
          ? "Resume simulated signals"
          : "Pause simulated signals";
  });
  if ($("#diagnostics"))
    $("#diagnostics").innerHTML =
      Object.entries(telemetry.device.diagnostics)
        .map(([k, v]) => pair(k, typeof v === "object" ? JSON.stringify(v) : v))
        .join("") || "<p>No runtime metrics available.</p>";
}
function connection(value) {
  connected = value;
  $("#connection").textContent = value
    ? "● Connected"
    : "○ Reconnecting · stale";
  updateLive();
}
function connect() {
  source?.close();
  source = new EventSource("/api/events");
  source.onmessage = (event) => {
    try {
      const previousActive = telemetry?.active_screen;
      telemetry = JSON.parse(event.data);
      if (
        config &&
        page === "dashboard" &&
        !dirty &&
        previousActive !== telemetry.active_screen
      )
        render();
      const key = signals()
        .map((s) => s.signal)
        .join("|");
      if (key !== catalogKey) {
        catalogKey = key;
        if ($("#signal-cards")) $("#signal-cards").innerHTML = signalCards();
      }
      lastMessage = Date.now();
      connection(true);
      $("#mode").textContent =
        telemetry.mode === "simulation"
          ? "SIMULATION · synthetic canonical signals · no vehicle connected"
          : "HOST RUNTIME · values retain source/quality · firmware control unavailable";
      for (const s of signals()) {
        const v = reading(s);
        if (v !== null) {
          history[s.signal] ||= [];
          history[s.signal].push(v);
          if (history[s.signal].length > 60) history[s.signal].shift();
        }
      }
      updateLive();
    } catch {
      connection(false);
    }
  };
  source.onerror = () => connection(false);
}
setInterval(() => {
  if (Date.now() - lastMessage > 2500) connection(false);
}, 1000);
function ask(title, text, action) {
  const d = $("#confirm");
  d.innerHTML = `<h2>${e(title)}</h2><p>${e(text)}</p><div class="actions"><button data-cancel>Cancel</button><button class="danger" id="confirm-ok">Confirm</button></div>`;
  d.showModal();
  $("#confirm-ok").onclick = async () => {
    d.close();
    try {
      await action();
    } catch (err) {
      toast(err.message);
    }
  };
  d.querySelector("[data-cancel]").onclick = () => d.close();
}
function openSheet(html) {
  sheet.innerHTML = `<button class="close" data-action="close-sheet" aria-label="Close settings">✕</button>${html}`;
  sheet.showModal();
  sheet.setAttribute(
    "aria-label",
    sheet.querySelector("h2")?.textContent || "Settings",
  );
}
function editWidget(id) {
  const s = currentScreen(),
    w = s.widgets.find((w) => w.id === id);
  selected = id;
  const options = signals().map((r) => [r.signal, r.label || r.signal]);
  if (!options.some((o) => o[0] === w.signal))
    options.push([w.signal, w.signal]);
  openSheet(
    `<h2>Widget settings</h2><form id="widget-form"><div class="fields"><div class="wide">${select("Signal", "signal", options, w.signal)}</div>${input("Label", "label", w.label)}${input("Units", "units", w.units)}<div class="wide">${select("Visualization", "type", TYPES, w.type)}</div>${input("Minimum", "min", w.min, "number", 'step="any"')}${input("Maximum", "max", w.max, "number", 'step="any"')}${input("Warning ≥", "warning", w.warning, "number", 'step="any"')}${input("Critical ≥", "critical", w.critical, "number", 'step="any"')}${input("Decimals", "precision", w.precision, "number", 'min="0" max="3"')}${input("Width (cells)", "width", w.width, "number", 'min="1" max="4"')}${input("Height (cells)", "height", w.height, "number", 'min="1" max="4"')}</div><label>Move to an empty grid cell</label><div class="actions">${button("←", "move-left")}${button("↑", "move-up")}${button("↓", "move-down")}${button("→", "move-right")}</div><p id="position">Position: ${w.x + 1}, ${w.y + 1}</p><div class="actions"><button type="submit" class="primary">Apply widget</button>${button("Duplicate", "duplicate-widget")}${button("Delete", "delete-widget", "danger")}</div></form>`,
  );
  sheet.querySelectorAll("[data-action]").forEach((b) => (b.type = "button"));
}
function editTheme() {
  const t = theme();
  openSheet(
    `<h2>Theme editor</h2><form id="theme-form">${input("Name", "name", t.name)}<div class="fields">${["background", "foreground", "accent", "warning", "critical"].map((k) => input(k, k, t[k], "color")).join("")}${input("Spacing", "spacing", t.spacing, "number", 'min="0" max="24"')}${select("Gauge shape", "gauge", ["round", "square"], t.gauge)}${select("Typography", "typography", ["system", "mono"], t.typography)}</div><label><input name="grid" type="checkbox" ${t.grid ? "checked" : ""}> Background grid</label><div class="actions"><button type="submit" class="primary">Apply theme</button>${button("Export", "export-theme")}${button("Import", "import-theme")}${button("Reset style", "reset-theme")}</div><input id="theme-file" type="file" accept=".json" hidden></form>`,
  );
  sheet.querySelectorAll("[data-action]").forEach((b) => (b.type = "button"));
}
function download(data, name, type = "application/json") {
  const url = URL.createObjectURL(
    new Blob(
      [typeof data === "string" ? data : JSON.stringify(data, null, 2)],
      { type },
    ),
  );
  const a = document.createElement("a");
  a.href = url;
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
async function save() {
  if (deviceSettings.hasSettingsChanges()) return deviceSettings.apply();
  if (saving) return;
  saving = true;
  const pending = clone(config);
  try {
    const result = await api("configuration", "PUT", {
      configuration: pending,
      revision,
    });
    revision = result.revision;
    base = clone(result.configuration);
    if (JSON.stringify(config) === JSON.stringify(pending)) {
      config = result.configuration;
      dirty = false;
      $("#savebar").hidden = true;
      try {
        localStorage.removeItem("minidash-draft");
      } catch {}
    } else {
      persist();
    }
    toast("Configuration saved on host");
  } catch (err) {
    toast(err.message);
  } finally {
    saving = false;
  }
}
async function loadLogs() {
  try {
    logs = await api("logs");
    renderLogs();
  } catch (err) {
    if ($("#log-list"))
      $("#log-list").textContent =
        "Recordings unavailable. Reconnect and refresh.";
    toast(err.message);
  }
}
async function logGraph(id) {
  const r = await fetch("/api/logs/" + id);
  if (!r.ok) throw Error("Recording unavailable");
  const rows = (await r.text())
    .trim()
    .split("\n")
    .filter(Boolean)
    .map((s) => JSON.parse(s));
  const names = [
    ...new Set(rows.flatMap((r) => r.signals.map((s) => s.signal))),
  ];
  openSheet(
    `<h2>Recording graph</h2><p>Recorded samples · gaps remain gaps</p>${select("Signal", "graph-signal", names, names[0])}<div id="recorded-graph"></div>`,
  );
  const draw = () => {
    const name = sheet.querySelector("[name=graph-signal]").value;
    const values = rows.map((r) =>
      reading(r.signals.find((s) => s.signal === name)),
    );
    const valid = values.filter((v) => v !== null);
    if (!valid.length) {
      $("#recorded-graph").textContent = "No valid samples for this signal";
      return;
    }
    const low = Math.min(...valid),
      high = Math.max(...valid);
    let segments = [],
      segment = [];
    values.forEach((v, i) => {
      if (v === null) {
        if (segment.length) segments.push(segment);
        segment = [];
      } else
        segment.push(
          `${(i / Math.max(1, values.length - 1)) * 300},${110 - ((v - low) / (high - low || 1)) * 100}`,
        );
    });
    if (segment.length) segments.push(segment);
    $("#recorded-graph").innerHTML =
      `<svg class="spark" viewBox="0 0 300 120" aria-label="Recorded signal plot">${segments.map((p) => `<polyline points="${p.join(" ")}"/>`).join("")}</svg><p>Min ${low.toFixed(2)} · max ${high.toFixed(2)} · ${valid.length} valid samples</p>`;
  };
  sheet.querySelector("[name=graph-signal]").onchange = draw;
  draw();
}
function addSignal(name) {
  const signal = signals().find((s) => s.signal === name);
  openSheet(
    `<h2>Add ${e(signal.label || name)}</h2>${select(
      "Screen",
      "add-screen",
      config.screens.map((s) => [s.id, s.name]),
      currentScreen().id,
    )}<p>Added at the next free grid cell. Position can be changed in widget settings.</p>${button("Add widget", "confirm-add", "primary")}`,
  );
  sheet.dataset.addSignal = name;
}
document.addEventListener("click", async (event) => {
  const b = event.target.closest("button");
  if (!b) return;
  try {
    if (b.dataset.page) return navigate(b.dataset.page);
    if (b.dataset.widget) return editWidget(b.dataset.widget);
    if (b.dataset.addSignal) return addSignal(b.dataset.addSignal);
    if (b.dataset.logDownload) {
      const r = await fetch("/api/logs/" + b.dataset.logDownload);
      if (!r.ok) throw Error("Download unavailable");
      return download(
        await r.text(),
        `minidash-${b.dataset.logDownload}.jsonl`,
        "application/x-ndjson",
      );
    }
    if (b.dataset.logGraph) return await logGraph(b.dataset.logGraph);
    if (b.dataset.logDelete)
      return ask(
        "Delete recording?",
        "This removes the saved file from the host.",
        async () => {
          await api("logs/" + b.dataset.logDelete, "DELETE", {});
          await loadLogs();
          toast("Recording deleted");
        },
      );
    const a = b.dataset.action;
    if (!a) return;
    const s = currentScreen();
    if (["screens", "signals", "device"].includes(a)) return navigate(a);
    if (a === "close-sheet") return sheet.close();
    if (a === "save") return await save();
    if (a === "discard")
      return ask(
        "Discard unsaved changes?",
        "Replace this draft with the last saved configuration. Unsaved passwords are forgotten.",
        () => {
          config = clone(base);
          dirty = false;
          deviceSettings.discard();
          $("#savebar").hidden = true;
          sheet.close();
          try {
            localStorage.removeItem("minidash-draft");
          } catch {}
          render();
        },
      );
    if (a === "record") {
      b.disabled = true;
      await api(
        "logs/" + (telemetry.logging.active ? "stop" : "start"),
        "POST",
        {},
      );
      telemetry = await api("telemetry");
      updateLive();
      if (page === "logs") await loadLogs();
      return toast(
        telemetry.logging.active ? "Recording started" : "Recording saved",
      );
    }
    if (a === "reload-logs") return await loadLogs();
    if (a === "pause") {
      const result = await api("simulation", "POST", {
        paused: telemetry?.status !== "simulation_paused",
      });
      b.textContent = result.paused
        ? "Resume simulated signals"
        : "Pause simulated signals";
      return;
    }
    if (a === "export")
      return download(
        config,
        `minidash-configuration-v${config.schema_version}.json`,
      );
    if (a === "import") return $("#config-file").click();
    if (a === "reload")
      return ask(
        "Reload saved configuration?",
        "Your current draft will be replaced.",
        async () => {
          const result = await api("configuration");
          config = result.configuration;
          revision = result.revision;
          base = clone(config);
          dirty = false;
          localStorage.removeItem("minidash-draft");
          $("#savebar").hidden = true;
          render();
        },
      );
    if (a === "reset")
      return ask(
        "Reset configuration?",
        "Replaces the current draft. Save to persist the reset.",
        async () => {
          const result = await fetch("/api/defaults");
          if (!result.ok) throw Error("Defaults unavailable");
          config = await result.json();
          changed();
          render();
        },
      );
    if (a === "new-screen") {
      if (config.screens.length >= 16) throw Error("Maximum 16 screens");
      const ns = {
        id: uid(),
        name: `Screen ${config.screens.length + 1}`,
        columns: 2,
        widgets: [],
      };
      ns.widgets.push(makeWidget(signals()[0], ns));
      config.screens.push(ns);
      screenId = ns.id;
      changed();
      return render();
    }
    if (a === "activate") {
      config.active_screen = s.id;
      changed();
      return render();
    }
    if (a === "duplicate-screen") {
      if (config.screens.length >= 16) throw Error("Maximum 16 screens");
      const ns = clone(s);
      ns.id = uid();
      ns.name = (s.name + " copy").slice(0, 80);
      ns.widgets.forEach((w) => (w.id = uid()));
      config.screens.push(ns);
      screenId = ns.id;
      changed();
      return render();
    }
    if (a === "delete-screen") {
      if (config.screens.length === 1) throw Error("Keep at least one screen");
      return ask("Delete screen?", s.name, () => {
        config.screens = config.screens.filter((x) => x.id !== s.id);
        if (config.device_settings.general.startup_screen === s.id)
          config.device_settings.general.startup_screen = config.screens[0].id;
        if (config.active_screen === s.id)
          config.active_screen = config.screens[0].id;
        screenId = config.screens[0].id;
        changed();
        render();
      });
    }
    if (a.startsWith("layout-"))
      return ask(
        "Change layout?",
        "Positions and sizes will be reset; reducing the count removes extra widgets.",
        () => {
          relayout(s, Number(a.split("-")[1]), signals());
          changed();
          render();
        },
      );
    if (a === "add-widget") return addSignal(signals()[0].signal);
    if (a === "confirm-add") {
      const target = config.screens.find(
        (s) => s.id === sheet.querySelector("[name=add-screen]").value,
      );
      if (target.widgets.length >= 24) throw Error("Maximum 24 widgets");
      target.widgets.push(
        makeWidget(
          signals().find((s) => s.signal === sheet.dataset.addSignal),
          target,
        ),
      );
      screenId = target.id;
      changed();
      sheet.close();
      navigate("screens");
      return toast("Widget added to draft");
    }
    if (a === "theme") return editTheme();
    if (a === "duplicate-theme") {
      if (config.themes.length >= 32) throw Error("Maximum 32 themes");
      const t = clone(theme());
      t.id = uid();
      t.name = (t.name + " copy").slice(0, 80);
      config.themes.push(t);
      config.active_theme = t.id;
      changed();
      return render();
    }
    if (a === "export-theme")
      return download(theme(), "minidash-theme-v1.json");
    if (a === "import-theme") return $("#theme-file").click();
    if (a === "reset-theme")
      return ask(
        "Reset theme style?",
        "Restores OEM Dark colors and spacing in this draft.",
        () => {
          Object.assign(theme(), {
            background: "#0b1016",
            foreground: "#eff5fa",
            accent: "#59dfc0",
            warning: "#ffbc55",
            critical: "#ff596a",
            spacing: 8,
            gauge: "round",
            typography: "system",
            grid: false,
          });
          changed();
          render();
          editTheme();
        },
      );
    const w = s.widgets.find((w) => w.id === selected);
    if (a.startsWith("move-")) {
      const direction = {
        left: [-1, 0],
        right: [1, 0],
        up: [0, -1],
        down: [0, 1],
      }[a.slice(5)];
      if (!moveWidget(s, w, ...direction))
        return toast("That cell is occupied or outside the grid");
      changed();
      $("#position").textContent = `Position: ${w.x + 1}, ${w.y + 1}`;
      return render();
    }
    if (a === "duplicate-widget") {
      if (s.widgets.length >= 24) throw Error("Maximum 24 widgets");
      const copy = { ...w, id: uid(), ...freePosition(s, w.width, w.height) };
      s.widgets.push(copy);
      changed();
      render();
      sheet.close();
      return editWidget(copy.id);
    }
    if (a === "delete-widget") {
      if (s.widgets.length === 1) throw Error("Keep at least one widget");
      return ask("Delete widget?", w.label, () => {
        s.widgets = s.widgets.filter((x) => x.id !== w.id);
        sheet.close();
        changed();
        render();
      });
    }
  } catch (err) {
    toast(err.message);
    updateLive();
  }
});
document.addEventListener("submit", (event) => {
  event.preventDefault();
  try {
    const form = event.target,
      values = Object.fromEntries(new FormData(form));
    if (form.id === "widget-form") {
      const s = currentScreen(),
        w = s.widgets.find((w) => w.id === selected);
      for (const k of [
        "min",
        "max",
        "warning",
        "critical",
        "precision",
        "width",
        "height",
      ])
        values[k] = Number(values[k]);
      if (
        !values.label.trim() ||
        values.label.length > 80 ||
        values.units.length > 20 ||
        !["min", "max", "warning", "critical"].every((k) =>
          Number.isFinite(values[k]),
        ) ||
        !(
          values.min < values.max &&
          values.min <= values.warning &&
          values.warning <= values.critical &&
          values.critical <= values.max
        ) ||
        !Number.isInteger(values.precision) ||
        values.precision < 0 ||
        values.precision > 3
      )
        throw Error("Check label, range, thresholds and decimals");
      if (
        !Number.isInteger(values.width) ||
        !Number.isInteger(values.height) ||
        values.width < 1 ||
        values.width > s.columns ||
        values.height < 1 ||
        values.height > 4
      )
        throw Error("Size must fit the grid");
      const position = freePosition(
        { ...s, widgets: s.widgets.filter((x) => x.id !== w.id) },
        values.width,
        values.height,
      );
      const candidate = { ...w, ...values };
      const others = s.widgets.filter((x) => x.id !== w.id);
      if (
        candidate.x + candidate.width > s.columns ||
        candidate.y + candidate.height > 24 ||
        others.some(
          (o) =>
            candidate.x < o.x + o.width &&
            candidate.x + candidate.width > o.x &&
            candidate.y < o.y + o.height &&
            candidate.y + candidate.height > o.y,
        )
      )
        Object.assign(candidate, position);
      Object.assign(w, candidate);
    }
    if (form.id === "theme-form") {
      if (!values.name.trim() || values.name.length > 80)
        throw Error("Theme name required, up to 80 characters");
      values.spacing = Number(values.spacing);
      if (
        !Number.isFinite(values.spacing) ||
        values.spacing < 0 ||
        values.spacing > 24
      )
        throw Error("Spacing must be 0–24");
      values.grid = form.elements.grid.checked;
      Object.assign(theme(), values);
    }
    changed();
    sheet.close();
    render();
    toast("Applied to draft · Save to persist");
  } catch (err) {
    toast(err.message);
  }
});
document.addEventListener("input", (event) => {
  const el = event.target;
  if (el.id === "search") {
    search = el.value;
    $("#signal-cards").innerHTML = signalCards();
    updateLive();
  }
  if (el.name === "screen-name") {
    currentScreen().name = el.value;
    changed();
  }
  if (el.name === "vehicle-profile") {
    config.settings.vehicle_profile = el.value;
    changed();
  }
});
document.addEventListener("change", async (event) => {
  const el = event.target;
  try {
    if (el.id === "screen-select") {
      screenId = el.value;
      return render();
    }
    if (el.name === "theme-select") {
      config.active_theme = el.value;
      changed();
      return render();
    }
    if (el.name === "source-filter") {
      filter = el.value;
      $("#signal-cards").innerHTML = signalCards();
      return updateLive();
    }
    if (el.name === "appearance") {
      config.settings.appearance = el.value;
      return changed();
    }
    if (el.name === "display-width" || el.name === "display-height") {
      const v = Number(el.value);
      if (!Number.isFinite(v) || v < 128 || v > 1024)
        throw Error("Display size must be 128–1024");
      config.settings[el.name.replace("display-", "display_")] = v;
      changed();
    }
    if (el.id === "config-file" || el.id === "theme-file") {
      const file = el.files[0];
      if (!file) return;
      if (file.size > 262144) throw Error("Import file too large");
      let value = JSON.parse(await file.text());
      if (el.id === "theme-file") {
        const imported = { ...value, id: uid() };
        const trial = clone(config);
        trial.themes.push(imported);
        trial.active_theme = imported.id;
        await api("configuration/validate", "POST", { configuration: trial });
        config = trial;
        changed();
        sheet.close();
        render();
        toast("Theme imported into draft");
      } else {
        const validated = await api("configuration/validate", "POST", {
          configuration: value,
        });
        value = validated.configuration;
        ask(
          "Import configuration?",
          "Replace this draft with the validated backup. Save to persist.",
          () => {
            config = value;
            screenId = null;
            changed();
            render();
          },
        );
      }
      el.value = "";
    }
  } catch (err) {
    toast(err.message);
  }
});
window.addEventListener("hashchange", () => {
  const next = location.hash.slice(1);
  if (next !== page) navigate(next);
});
window.addEventListener("online", () => {
  connect();
  if (!config) bootstrap();
});
window.addEventListener("offline", () => {
  source?.close();
  connection(false);
});
window.addEventListener("beforeunload", () => {
  if (dirty) persist();
});
matchMedia("(prefers-color-scheme:light)").addEventListener("change", () => {
  if (config) appearance();
});
async function bootstrap() {
  try {
    const server = await api("configuration");
    config = server.configuration;
    base = clone(config);
    revision = server.revision;
    let draft = null;
    try {
      draft = JSON.parse(localStorage.getItem("minidash-draft"));
    } catch {}
    if (draftIsCompatible(draft, server)) {
      config = draft.configuration;
      dirty = true;
      $("#savebar").hidden = false;
      toast("Unsaved draft restored");
    } else if (draft) {
      toast("Saved configuration changed. Previous draft retained for export.");
      openSheet(
        `<h2>Previous draft</h2><p>The host configuration changed since this draft. Export the draft before replacing it.</p>${button("Export previous draft", "export-old-draft")}`,
      );
      sheet.querySelector("[data-action=export-old-draft]").onclick = () =>
        download(draft.configuration, "minidash-previous-draft.json");
    }
    navigate(location.hash.slice(1) || "dashboard");
    connect();
    deviceSettings.load();
  } catch (err) {
    connection(false);
    let cached = null;
    try {
      cached = JSON.parse(localStorage.getItem("minidash-draft"));
    } catch {}
    if (cached?.configuration?.schema_version === 2) {
      config = cached.configuration;
      base = cached.base;
      revision = cached.revision;
      dirty = true;
      $("#savebar").hidden = false;
      navigate(location.hash.slice(1) || "dashboard");
    } else
      main.innerHTML = `<h1>MiniDash unavailable</h1><div class="card"><p>Connect to the device Wi-Fi or restart the host. Your saved configuration stays on the host.</p><button id="retry">Reconnect</button></div>`;
    if ($("#retry")) $("#retry").onclick = bootstrap;
    setTimeout(bootstrap, 5000);
  }
}
if ("serviceWorker" in navigator && window.isSecureContext)
  navigator.serviceWorker.register("/sw.js").catch(() => {});
bootstrap();
