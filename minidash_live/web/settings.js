import { escape as e } from "./model.js";
import { setUnitsPreset } from "./units.js";
const TITLES = {
  general: "General",
  display: "Display",
  wifi: "Wi-Fi",
  vehicle: "Vehicle / CAN",
  acquisition: "Data Acquisition",
  logging: "Logging",
  power: "Power",
  system: "System",
  advanced: "Advanced",
};
const NOTES = {
  general: "Names, units, clock and startup",
  display: "Brightness, rotation and sleep",
  wifi: "Access point and station",
  vehicle: "SAFE / READ ONLY",
  acquisition: "Safe rates, freshness and source priorities",
  logging: "Recording, rotation and retention",
  power: "KCD11 master switch and future power control",
  system: "Identity, backup and reset",
  advanced: "Diagnostics and developer tools",
};
export function createSettingsUI(ctx) {
  let schema = null,
    caps = null,
    showAdvanced = false,
    query = "",
    secrets = {},
    status = {},
    busy = false;
  const $ = (s) => document.querySelector(s);
  const supported = (key) => caps?.features[key]?.supported === true;
  const feature = (key) =>
    caps?.features[key] || {
      supported: false,
      scope: "unavailable",
      reason: "Capabilities unavailable",
    };
  const scope = (key) =>
    feature(key).supported
      ? feature(key).scope === "simulation"
        ? "Simulation"
        : feature(key).scope === "host"
          ? "Host"
          : "Device"
      : feature(key).reason;
  const btn = (label, action, disabled = false) =>
    `<button data-setting-action="${action}" ${disabled ? "disabled" : ""}>${label}</button>`;
  async function load() {
    try {
      schema = await ctx.api("settings/schema");
      caps = schema.capabilities;
      status = await ctx.api("settings/status");
      if (ctx.page() === "device") ctx.render();
    } catch (err) {
      ctx.toast(err.message);
    }
  }
  function visibleSections() {
    return Object.keys(TITLES).filter(
      (s) =>
        (showAdvanced || !["advanced", "acquisition"].includes(s)) &&
        (!query ||
          (
            `${TITLES[s]} ${NOTES[s]} ` +
            Object.values(schema?.sections[s] || {})
              .map((f) => f.label)
              .join(" ")
          )
            .toLowerCase()
            .includes(query.toLowerCase())),
    );
  }
  function cards() {
    const sections = visibleSections();
    return (
      sections
        .map(
          (s) =>
            `<button class="setting-card card" data-setting-section="${s}"><span><strong>${TITLES[s]}</strong><small>${NOTES[s]}</small>${
              query
                ? `<small class="matches">${Object.values(
                    schema?.sections[s] || {},
                  )
                    .filter((f) =>
                      f.label.toLowerCase().includes(query.toLowerCase()),
                    )
                    .map((f) => e(TITLES[s] + " → " + f.label))
                    .join("<br>")}</small>`
                : ""
            }</span><span aria-hidden="true">›</span></button>`,
        )
        .join("") ||
      '<div class="card">No settings match. Advanced settings are hidden unless enabled.</div>'
    );
  }
  function page() {
    return `<h1>Device settings</h1><p>${caps?.mode === "simulation" ? "Simulation · hardware settings affect the simulator only." : "Capabilities reported by the connected backend."}</p><div class="row"><span class="safe-badge">SAFE / READ ONLY</span><span class="muted">${e(ctx.config().device_settings.general.device_name)}</span></div><div id="settings-operation" role="status" class="card" hidden></div><label>Find a setting<input id="settings-search" type="search" placeholder="Brightness, Wi-Fi, logging…" value="${e(query)}"></label><label class="toggle"><input id="show-advanced" type="checkbox" ${showAdvanced ? "checked" : ""}><span>Show advanced settings</span></label><div id="settings-cards" class="settings-cards">${cards()}</div><div class="card"><h2>Firmware update</h2><p>Not supported by current firmware</p><button disabled>Select firmware · unavailable</button></div><p id="pwa-note"></p><input id="config-file" type="file" accept="application/json,.json" hidden>`;
  }
  function control(section, key, spec) {
    const value = ctx.config().device_settings[section][key],
      info = feature(spec.capability),
      disabled = !info.supported || busy;
    let options = spec.choices || [];
    if (spec.kind === "screen") options = ctx.config().screens.map((s) => s.id);
    if (section === "wifi" && key === "mode")
      options = options.filter((v) => caps?.wifi_modes.includes(v));
    if (section === "power" && key === "mode")
      options = options.filter(
        (v) => v !== "hardware_off" || supported("hardware_power_off"),
      );
    if (key === "wake_behaviour")
      options = options.filter(
        (v) => v !== "ignition" || supported("ignition"),
      );
    const common = `data-setting="${section}.${key}" name="${key}" ${disabled ? "disabled" : ""}`;
    let input;
    if (spec.kind === "toggle")
      input = `<label class="toggle"><input type="checkbox" ${common} ${value ? "checked" : ""}><span>${e(spec.label)}</span></label>`;
    else if (["select", "screen"].includes(spec.kind))
      input = `<label>${e(spec.label)}<select ${common}>${(options.length ? options : [value]).map((v) => `<option value="${e(v)}" ${v === value ? "selected" : ""}>${e(spec.kind === "screen" ? ctx.config().screens.find((s) => s.id === v)?.name : v)}</option>`).join("")}</select></label>`;
    else if (spec.kind === "signals")
      input = `<label>${e(spec.label)}<select multiple ${common}>${ctx
        .signals()
        .map(
          (s) =>
            `<option value="${e(s.signal)}" ${value.includes(s.signal) ? "selected" : ""}>${e(s.label || s.signal)}</option>`,
        )
        .join("")}</select></label>`;
    else
      input = `<label>${e(spec.label)}${spec.kind === "range" ? ` <output data-output="${section}.${key}">${value}%</output>` : ""}<input ${common} type="${spec.kind === "range" ? "range" : spec.kind === "number" ? "number" : "text"}" ${spec.kind === "ip" ? 'inputmode="decimal" autocomplete="off"' : ""} value="${e(value)}" ${spec.min !== undefined ? `min="${spec.min}" max="${spec.max}" step="${spec.step || 1}"` : ""} ${spec.max_length ? `maxlength="${spec.max_length}"` : ""}></label>`;
    return `<div class="setting-field">${input}<small>${e(scope(spec.capability))}${info.supported ? " · " + { live: "Safe preview; Apply to save", apply: "Apply required", reboot: "Device reboot required", wifi_reconnect: "Wi-Fi reconnect required", runtime_restart: "Acquisition restart required" }[spec.impact] : ""}${spec.note ? " · " + e(spec.note) : ""}</small></div>`;
  }
  function password(key, label, cap) {
    return `<div class="setting-field"><label>${label}<input id="${key}" data-secret="${key}" type="password" autocomplete="new-password" maxlength="64" placeholder="${status.secret_status?.[key] ? "Saved password · unchanged" : "Set a new password"}" value="${e(secrets[key] || "")}" ${supported(cap) ? "" : "disabled"}></label><label class="toggle"><input data-reveal="${key}" type="checkbox"><span>Show password</span></label><small>Write-only · never stored in phone drafts or exported backups. ${e(scope(cap))}</small></div>`;
  }
  function section(name) {
    if (!schema)
      return ctx.toast("Waiting for capabilities. Reconnect and try again.");
    let html = `<h2>${TITLES[name]}</h2><p>${NOTES[name]}</p>`;
    if (name === "system") {
      const d = status.device || {};
      html += Object.entries({
        "Device ID": d.id,
        "Firmware version": d.firmware_version,
        "Hardware revision": d.hardware_revision,
        Build: d.build,
        "Reset reason": d.reset_reason,
        Uptime: `${Math.floor(ctx.telemetry()?.uptime || 0)} s`,
        Filesystem: d.filesystem,
        "Host recording storage": `${((d.storage_used_bytes || 0) / 1e6).toFixed(2)} MB`,
        Flash: d.flash_bytes,
        PSRAM: d.psram_bytes,
        Heap: d.heap_bytes,
      })
        .map(
          ([k, v]) =>
            `<div class="pair"><span>${e(k)}</span><span>${e(v ?? "Not available from firmware")}</span></div>`,
        )
        .join("");
      html += `<div class="actions">${btn("Restart device", "restart_device", !supported("restart_device"))}${btn("Restart Wi-Fi", "restart_wifi", !supported("restart_wifi"))}${btn("Reset settings", "reset_settings")}${btn("Factory reset", "factory_reset")}</div><p>Reset actions affect host configuration. Factory reset removes credentials and resets screens/themes; saved recordings remain separately deletable.</p><div class="actions"><button data-action="export">Export configuration</button><button data-action="import">Import configuration</button><button data-action="reload">Reload saved</button><button data-action="reset">Reset configuration draft</button></div>`;
    } else {
      if (name === "wifi")
        html += `<div class="card">${e(scope("wifi_config"))} · AP address ${e(status.device?.wifi?.ap_address || "Not available")}<br>Connection may be lost after changing SSID, password or IP.</div>${btn("Scan networks", "scan", !supported("wifi_scan"))}<div id="wifi-networks"></div>`;
      if (name === "vehicle")
        html += `<div class="safe-badge">SAFE / READ ONLY</div><p id="vehicle-settings-state">Detected vehicle and ECUs: see live status below.</p>`;
      if (name === "power")
        html += `<div class="card">KCD11 physically switches power. Display off keeps the device running; sleep/deep sleep still consume power. Hardware off requires the master switch or a future MOSFET cutoff. Ignition hardware is not implemented.</div>`;
      html += Object.entries(schema.sections[name])
        .map(([key, spec]) => control(name, key, spec))
        .join("");
      if (name === "general")
        html += `<label>App appearance<select name="appearance"><option ${ctx.config().settings.appearance === "auto" ? "selected" : ""}>auto</option><option ${ctx.config().settings.appearance === "light" ? "selected" : ""}>light</option><option ${ctx.config().settings.appearance === "dark" ? "selected" : ""}>dark</option></select></label>`;
      if (name === "display")
        html += `<div id="brightness-preview">${ctx.preview()}</div><label>Active theme<select data-active-theme>${ctx
          .config()
          .themes.map(
            (t) =>
              `<option value="${e(t.id)}" ${t.id === ctx.config().active_theme ? "selected" : ""}>${e(t.name)}</option>`,
          )
          .join(
            "",
          )}</select></label><label>Default display screen<select data-default-screen>${ctx
          .config()
          .screens.map(
            (s) =>
              `<option value="${e(s.id)}" ${s.id === ctx.config().active_screen ? "selected" : ""}>${e(s.name)}</option>`,
          )
          .join(
            "",
          )}</select></label><p>Brightness previews affect the host display preview only. ESP32 sync is unavailable.</p>`;
      if (name === "wifi")
        html +=
          password("ap_password", "AP password", "wifi_config") +
          password("station_password", "Station password", "wifi_station");
      if (name === "acquisition")
        html +=
          `<p>Simulation has one canonical source. CAN/UDS priority, fallback and bus rates are stored policy preferences; no vehicle traffic is emulated.</p>` +
          btn("Reset to recommended defaults", "recommended");
      if (name === "logging")
        html += `<p>Estimated snapshot storage: <span id="storage-estimate">depends on selected signal count and rate</span>. Retention and storage limits delete oldest completed recordings when starting a new recording. Auto start runs on host startup.</p>`;
      if (name === "advanced")
        html += `<details><summary>API / CAN / UDS / task statistics</summary><pre id="settings-diagnostics"></pre></details><div class="actions">${btn("Download diagnostic bundle", "bundle")}<button data-action="pause" ${caps.mode === "simulation" ? "" : "disabled"}>Pause simulated signals</button></div>`;
    }
    html += `<div class="actions">${btn("Done", "done")}</div>`;
    ctx.openSheet(html);
    live(ctx.telemetry());
  }
  function bar() {
    if (!$("#savebar")) return;
    const settingsChanged =
      ctx.dirty() &&
      (JSON.stringify(ctx.config().device_settings) !==
        JSON.stringify(ctx.base()?.device_settings) ||
        Object.keys(secrets).length > 0);
    $("#savebar [data-action=save]").textContent =
      ctx.page() === "device" || settingsChanged ? "Apply" : "Save";
    const operationBusy = busy || status.operation?.status === "running";
    $("#savebar [data-action=save]").disabled = operationBusy;
    $("#savebar [data-action=discard]").disabled = operationBusy;
    $("#savebar span").textContent = busy
      ? "Applying settings…"
      : "Unsaved changes · passwords remain in memory only";
  }
  function live(telemetry) {
    if (!telemetry) return;
    const op = telemetry.operation;
    status.device = telemetry.device;
    if (telemetry.device_capabilities) caps = telemetry.device_capabilities;
    status.operation = op;
    document.querySelectorAll("[data-setting-action]").forEach((button) => {
      const action = button.dataset.settingAction;
      if (
        [
          "restart_device",
          "restart_wifi",
          "reset_settings",
          "factory_reset",
        ].includes(action)
      )
        button.disabled =
          !supported(action === "reset_settings" ? "settings_reset" : action) ||
          op?.status === "running";
    });
    bar();
    if ($("#settings-operation")) {
      const el = $("#settings-operation");
      el.hidden = !op;
      el.textContent = op
        ? `${op.simulation ? "Simulation · " : ""}${op.phase}`
        : "";
    }
    if ($("#vehicle-settings-state"))
      $("#vehicle-settings-state").textContent =
        `CAN: ${telemetry.device.can} · vehicle: ${telemetry.device.vehicle} · interface: ${telemetry.device.interface} · ECUs: ${telemetry.device.ecus.map((v) => v.role || v).join(", ") || "none detected"} · SAFE / READ ONLY`;
    if ($("#settings-diagnostics"))
      $("#settings-diagnostics").textContent = JSON.stringify(
        { device: telemetry.device, statistics: telemetry.device.diagnostics },
        null,
        2,
      );
    if ($("#storage-estimate")) {
      const s = ctx.config().device_settings,
        count =
          s.logging.signal_scope === "selected"
            ? s.logging.selected_signals.length
            : new Set(
                ctx
                  .config()
                  .screens.flatMap((s) => s.widgets.map((w) => w.signal)),
              ).size;
      $("#storage-estimate").textContent =
        `~${((Math.max(1, count) * 400 * 2 * 3600) / 1e6).toFixed(1)} MB / hour at 2 snapshots/s (estimate; actual bytes shown in Logs)`;
    }
  }
  function setValue(el) {
    const [section, key] = el.dataset.setting.split("."),
      spec = schema.sections[section][key];
    let value = el.value;
    if (spec.kind === "toggle") value = el.checked;
    else if (spec.kind === "signals")
      value = [...el.selectedOptions].map((o) => o.value);
    else if (["number", "range"].includes(spec.kind)) value = Number(value);
    else if (spec.kind === "select")
      value = spec.choices.find((v) => String(v) === value);
    ctx.config().device_settings[section][key] = value;
    if (section === "display" && key === "brightness") {
      ctx.config().device_settings.display.mode = "manual";
      const mode = document.querySelector('[data-setting="display.mode"]');
      if (mode) mode.value = "manual";
    }
    if (section === "general" && key === "units")
      setUnitsPreset(ctx.config().device_settings.general, value);
    if (
      section === "general" &&
      ["speed", "temperature", "pressure"].includes(key)
    )
      ctx.config().device_settings.general.units = "custom";
    if (section === "wifi" && key === "mode") {
      const mode = value;
      ctx.config().device_settings.wifi.ap_enabled = mode !== "station";
      ctx.config().device_settings.wifi.station_enabled = mode !== "ap";
    }
    if (section === "wifi" && ["ap_enabled", "station_enabled"].includes(key)) {
      const w = ctx.config().device_settings.wifi;
      w.mode = w.ap_enabled
        ? w.station_enabled
          ? "ap_station"
          : "ap"
        : "station";
      if (!w.ap_enabled && !w.station_enabled) {
        w.ap_enabled = true;
        w.mode = "ap";
        ctx.toast("Keep one Wi-Fi interface enabled");
      }
    }
    if ($("#sheet").querySelector(`[data-output="${section}.${key}"]`))
      $("#sheet").querySelector(
        `[data-output="${section}.${key}"]`,
      ).textContent = value + "%";
    ctx.changed();
    bar();
    live(ctx.telemetry());
    if (
      (section === "wifi" &&
        ["mode", "ap_enabled", "station_enabled"].includes(key)) ||
      (section === "general" && key === "units")
    )
      sectionRefresh(section);
  }
  function sectionRefresh(name) {
    section(name);
  }
  function hasSettingsChanges() {
    return (
      JSON.stringify(ctx.config().device_settings) !==
        JSON.stringify(ctx.base()?.device_settings) ||
      Object.keys(secrets).length > 0
    );
  }
  async function apply() {
    if (busy) return;
    const snapshot = JSON.parse(JSON.stringify(ctx.config())),
      credentials = { ...secrets };
    const body = {
      configuration: snapshot,
      revision: ctx.revision(),
      secrets: credentials,
    };
    let plan;
    try {
      plan = await ctx.api("settings/plan", "POST", body);
    } catch (err) {
      ctx.toast(err.message);
      return;
    }
    const execute = async () => {
      busy = true;
      bar();
      try {
        const result = await ctx.api("settings/apply", "POST", {
          ...body,
          confirmed: true,
        });
        secrets = {};
        ctx.accept(result, snapshot);
        status = await ctx.api("settings/status");
        ctx.render();
        ctx.toast(
          plan.simulation
            ? "Settings applied to simulation"
            : "Settings applied on host",
        );
        if (result.operation?.status === "running") poll(result.operation.id);
      } finally {
        busy = false;
        bar();
      }
    };
    if (
      plan.reconnect_required ||
      plan.reboot_required ||
      plan.runtime_restart_required
    ) {
      ctx.ask(
        "Apply device settings?",
        `${plan.simulation ? "Simulation only. " : ""}${plan.reconnect_required ? "Wi-Fi reconnect required. New SSID: " + plan.destination.ssid + "; address: " + plan.destination.address + ". Your phone may need to join the new network. " : ""}${plan.reboot_required ? "Device reboot required. " : ""}${plan.runtime_restart_required ? "Acquisition restart required. " : ""}`,
        execute,
      );
    } else await execute();
  }
  async function poll(id) {
    for (let attempts = 0; attempts < 30; attempts++) {
      await new Promise((resolve) => setTimeout(resolve, 500));
      try {
        status = await ctx.api("settings/status");
        const op = status.operation;
        if (op?.id === id && op.status !== "running") {
          ctx.toast(op.phase);
          live({ ...ctx.telemetry(), operation: op });
          return;
        }
      } catch {
        ctx.toast("Waiting for MiniDash… reconnect to the new SSID/address");
      }
    }
    ctx.toast("MiniDash has not reconnected. Check the SSID and address.");
  }
  async function action(name) {
    if (name === "done") {
      ctx.closeSheet();
      return;
    }
    if (name === "recommended") {
      for (const [key, spec] of Object.entries(schema.sections.acquisition))
        if (supported(spec.capability))
          ctx.config().device_settings.acquisition[key] = spec.default;
      ctx.changed();
      section("acquisition");
      return;
    }
    if (name === "scan") {
      const result = await ctx.api("wifi/scan", "POST", {});
      $("#wifi-networks").innerHTML =
        `<p>${e(result.scope)} networks</p>` +
        result.networks
          .map(
            (n) =>
              `<button data-network="${e(n.ssid)}" data-security="${n.security}">${e(n.ssid)} · ${n.security} · ${n.rssi} dBm</button>`,
          )
          .join("");
      return;
    }
    if (name === "bundle") {
      ctx.download(
        await ctx.api("diagnostics/bundle"),
        "minidash-diagnostics.json",
      );
      return;
    }
    const execute = async (extra = {}) => {
      const result = await ctx.api("system/action", "POST", {
        action: name,
        confirmed: true,
        revision: ctx.revision(),
        ...extra,
      });
      secrets = {};
      ctx.accept(result);
      status = await ctx.api("settings/status");
      ctx.closeSheet();
      ctx.render();
      if (result.operation?.status === "running") poll(result.operation.id);
      ctx.toast(
        "System action accepted · " +
          (caps.mode === "simulation" ? "simulation" : "host"),
      );
    };
    if (name === "factory_reset") {
      const id = status.device.id;
      ctx.openSheet(
        `<h2>Factory reset</h2><p>Erase settings, screens, themes and stored Wi-Fi credentials. Recordings remain. Prepared Wi-Fi returns to MiniDash-${e(id.slice(-4).toUpperCase())}; reconnect/reboot may be required in simulation. Enter the full Device ID to confirm:</p><code>${e(id)}</code><label>Device ID confirmation<input id="factory-id" autocomplete="off" spellcheck="false"></label><div class="actions">${btn("Cancel", "done")}<button id="factory-confirm" class="danger" disabled>Factory reset now</button></div>`,
      );
      $("#factory-id").oninput = () =>
        ($("#factory-confirm").disabled = $("#factory-id").value !== id);
      $("#factory-confirm").onclick = async () => {
        try {
          await execute({ device_id_confirmation: $("#factory-id").value });
        } catch (err) {
          ctx.toast(err.message);
        }
      };
      return;
    }
    ctx.ask(
      name.replaceAll("_", " ") + "?",
      `${scope(name === "reset_settings" ? "settings_reset" : name)}. ${name === "restart_wifi" ? "Wi-Fi reconnect required. " : name === "restart_device" ? "Device reboot required. " : ""}Confirm before proceeding.`,
      execute,
    );
  }
  document.addEventListener("input", (ev) => {
    const el = ev.target;
    if (el.id === "settings-search") {
      query = el.value;
      $("#settings-cards").innerHTML = cards();
    }
    if (el.dataset.secret) {
      secrets[el.dataset.secret] = el.value;
      ctx.changed();
      bar();
    }
    if (
      el.dataset.setting &&
      ["text", "number", "range", "ip"].includes(
        schema.sections[el.dataset.setting.split(".")[0]][
          el.dataset.setting.split(".")[1]
        ].kind,
      )
    )
      setValue(el);
  });
  document.addEventListener("change", (ev) => {
    const el = ev.target;
    if (el.id === "show-advanced") {
      showAdvanced = el.checked;
      $("#settings-cards").innerHTML = cards();
    }
    if (el.dataset.reveal)
      $("#" + el.dataset.reveal).type = el.checked ? "text" : "password";
    if (
      el.dataset.setting &&
      !["text", "number", "range", "ip"].includes(
        schema.sections[el.dataset.setting.split(".")[0]][
          el.dataset.setting.split(".")[1]
        ].kind,
      )
    )
      setValue(el);
    if (el.hasAttribute("data-active-theme")) {
      ctx.config().active_theme = el.value;
      ctx.changed();
    }
    if (el.hasAttribute("data-default-screen")) {
      ctx.config().active_screen = el.value;
      ctx.config().device_settings.general.startup_screen = el.value;
      ctx.changed();
    }
  });
  document.addEventListener("click", async (ev) => {
    const el = ev.target.closest("button");
    if (!el) return;
    try {
      if (el.dataset.settingSection) section(el.dataset.settingSection);
      if (el.dataset.settingAction) await action(el.dataset.settingAction);
      if (el.dataset.network) {
        const w = ctx.config().device_settings.wifi;
        w.station_ssid = el.dataset.network;
        w.station_security = el.dataset.security;
        w.station_enabled = true;
        w.mode = w.ap_enabled ? "ap_station" : "station";
        ctx.changed();
        section("wifi");
      }
    } catch (err) {
      ctx.toast(err.message);
    }
  });
  return {
    page,
    load,
    bar,
    live,
    apply,
    hasSettingsChanges,
    discard() {
      secrets = {};
    },
    caps: () => caps,
  };
}
