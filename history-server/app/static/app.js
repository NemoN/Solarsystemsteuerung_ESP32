"use strict";

const $ = (id) => document.getElementById(id);
const navToggle = $("nav-toggle"), navLinks = $("nav-links");
const compactNav = window.matchMedia("(max-width: 760px)");
function syncNavigation() {
  navToggle.hidden = !compactNav.matches;
  navLinks.hidden = compactNav.matches;
  navToggle.setAttribute("aria-expanded", "false");
}
navToggle.addEventListener("click", () => {
  const expanded = navToggle.getAttribute("aria-expanded") === "true";
  navLinks.hidden = expanded;
  navToggle.setAttribute("aria-expanded", String(!expanded));
});
navLinks.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && compactNav.matches && !navLinks.hidden) {
    navLinks.hidden = true; navToggle.setAttribute("aria-expanded", "false"); navToggle.focus();
  }
});
navToggle.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && !navLinks.hidden) {
    navLinks.hidden = true; navToggle.setAttribute("aria-expanded", "false"); navToggle.focus();
  }
});
navLinks.addEventListener("click", (event) => {
  if (compactNav.matches && event.target.closest("a")) {
    navLinks.hidden = true; navToggle.setAttribute("aria-expanded", "false");
  }
});
if (compactNav.addEventListener) compactNav.addEventListener("change", syncNavigation);
else compactNav.addListener(syncNavigation);
syncNavigation();

const zone = "Europe/Berlin";
const fullDate = new Intl.DateTimeFormat("de-DE", {timeZone: zone, dateStyle: "short", timeStyle: "short"});
const tooltipDate = new Intl.DateTimeFormat("de-DE", {timeZone: zone, day: "2-digit", month: "2-digit", year: "numeric", hour: "2-digit", minute: "2-digit", timeZoneName: "short"});
const dateOnly = new Intl.DateTimeFormat("de-DE", {timeZone: zone, day: "2-digit", month: "2-digit", year: "numeric"});
const number = new Intl.NumberFormat("de-DE", {maximumFractionDigits: 1});
let selectedDays = 7;
let query = new URLSearchParams();
let revision = 0;
let request;
let chart;
let lastData;
let runtimeKind = "day", runtimeRevision = 0;
const sensorVisibility = new Map();

function localDay(value) {
  const parts = new Intl.DateTimeFormat("en-CA", {timeZone: zone, year: "numeric", month: "2-digit", day: "2-digit"}).formatToParts(value);
  const part = (type) => parts.find((p) => p.type === type).value;
  return `${part("year")}-${part("month")}-${part("day")}`;
}

function message(text, retry = false) {
  $("chart-message-text").textContent = text;
  $("chart-message").hidden = false;
  $("retry").hidden = !retry;
}

async function json(url, signal) {
  const response = await fetch(url, {signal, cache: "no-store"});
  if (!response.ok) {
    const body = await response.json().catch(() => ({}));
    throw new Error(typeof body.detail === "string" ? body.detail : "Der Archivserver antwortet nicht. Bitte erneut laden.");
  }
  return response.json();
}

function selected(id) {
  return sensorVisibility.get(id) !== false;
}

function renderLegend(series) {
  const legend = $("sensor-legend");
  const labels = series.map((sensor, index) => {
    const label = document.createElement("label");
    const input = document.createElement("input");
    input.type = "checkbox"; input.dataset.sensor = sensor.id; input.checked = selected(sensor.id);
    const line = document.createElement("span"); line.className = "line";
    line.style.borderColor = sensor.color;
    line.style.borderTopStyle = ["solid", "dashed", "dotted"][index % 3];
    label.append(input, line, document.createTextNode(sensor.label));
    input.addEventListener("change", () => {
      sensorVisibility.set(sensor.id, input.checked);
      if (chart) { chart.setDatasetVisibility(index, input.checked); chart.update("none"); }
      showVisibilityMessage();
    });
    return label;
  });
  const heading = document.createElement("legend");
  heading.className = "sr-only"; heading.textContent = "Temperaturkurven einblenden";
  legend.replaceChildren(heading, ...labels);
}

function draw(data) {
  renderLegend(data.series);
  if (chart) chart.destroy();
  const duration = new Date(data.to) - new Date(data.from);
  const ticks = new Intl.DateTimeFormat("de-DE", duration <= 2 * 86400000
    ? {timeZone: zone, hour: "2-digit", minute: "2-digit"}
    : {timeZone: zone, day: "2-digit", month: "2-digit"});
  chart = new Chart($("history-chart"), {
    type: "line",
    data: {datasets: data.series.map((s, i) => ({
      label: s.label, data: s.points, borderColor: s.color, backgroundColor: s.color,
      borderWidth: 2, borderDash: [[], [7, 4], [2, 4]][i % 3],
      pointRadius: (context) => {
        const points = context.dataset.data;
        const i = context.dataIndex;
        return points[i]?.y != null && points[i - 1]?.y == null && points[i + 1]?.y == null ? 3 : 0;
      },
      pointHitRadius: 14, pointHoverRadius: 4, spanGaps: false, hidden: !selected(s.id),
    }))},
    options: {
      responsive: true, maintainAspectRatio: false, animation: false, parsing: false,
      interaction: {mode: "nearest", axis: "x", intersect: false},
      plugins: {
        legend: {display: false},
        tooltip: {callbacks: {
          title: (items) => tooltipDate.format(items[0].parsed.x),
          label: (item) => `${item.dataset.label}: ${number.format(item.parsed.y)} °C`,
        }},
      },
      scales: {
        x: {type: "linear", min: Date.parse(data.from), max: Date.parse(data.to),
          grid: {display: false}, border: {color: "#49606a"},
          ticks: {color: "#c0cfd6", maxTicksLimit: window.innerWidth < 650 ? 4 : 8, maxRotation: 0, callback: (value) => ticks.format(value)}},
        y: {title: {display: true, text: "Temperatur (°C)", color: "#c0cfd6"},
          grid: {color: "#2d414b"}, border: {display: false}, ticks: {color: "#c0cfd6", callback: (value) => number.format(value)}},
      },
    },
  });
}

function showVisibilityMessage() {
  if (lastData && lastData.valid_value_count > 0) {
    if (!lastData.series.some((sensor) => selected(sensor.id))) message("Alle Kurven sind ausgeblendet. Oben einen Sensor auswählen.");
    else $("chart-message").hidden = true;
  }
}

async function loadHistory() {
  const current = ++revision;
  request?.abort();
  const controller = new AbortController();
  request = controller;
  const timeout = setTimeout(() => controller.abort(), 20000);
  $("chart-area").setAttribute("aria-busy", "true");
  $("refresh").disabled = true;
  lastData = null;
  message("Messungen werden geladen …");
  if (selectedDays !== null) {
    const end = new Date();
    query = new URLSearchParams({from: new Date(end - selectedDays * 86400000).toISOString(), to: end.toISOString()});
  }
  $("csv-link").href = `/api/export.csv?${query}`;
  try {
    const data = await json(`/api/history?${query}`, controller.signal);
    if (current !== revision) return;
    lastData = data;
    draw(data);
    const stop = new Date(Date.parse(data.to) - 1);
    $("range-caption").textContent = `${dateOnly.format(new Date(data.from))} – ${dateOnly.format(stop)} · Europe/Berlin (MEZ/MESZ)`;
    $("sample-summary").textContent = `${new Intl.NumberFormat("de-DE").format(data.sample_count)} gespeicherte Messzeitpunkte${data.display_reduced ? " · Diagramm für die Übersicht ausgedünnt" : ""}`;
    if (data.sample_count === 0) message("Für diesen Zeitraum sind keine Messungen gespeichert. Bitte einen anderen Zeitraum wählen.");
    else if (data.valid_value_count === 0) message("In diesem Zeitraum wurden nur Ausfälle oder Sensorfehler erfasst. Es liegen keine gültigen Temperaturen vor.");
    else showVisibilityMessage();
  } catch (error) {
    if (current !== revision) return;
    lastData = null;
    $("sample-summary").textContent = "Verlauf konnte nicht geladen werden";
    message(error.name === "AbortError" ? "Das Laden dauert zu lange. Bitte erneut versuchen." : error.message, true);
  } finally {
    clearTimeout(timeout);
    if (current === revision) {
      $("chart-area").setAttribute("aria-busy", "false");
      $("refresh").disabled = false;
    }
  }
}

async function loadStatus() {
  try {
    const data = await json("/api/collector-status", AbortSignal.timeout(10000));
    if (data.esp_url) {
      $("esp-link").href = data.esp_url;
      $("esp-link").hidden = false;
    }
    $("last-capture").textContent = data.last_capture ? fullDate.format(new Date(data.last_capture)) : "Noch keine Messung";
    $("last-result").textContent = data.last_result ? data.last_result.message : "Erfassung startet zur nächsten Viertelstunde";
    $("last-result").title = data.last_result?.detail || "";
    $("last-result").classList.toggle("result-warning", !!data.last_result && data.last_result.code !== "ok");
    $("next-capture").textContent = fullDate.format(new Date(data.next_capture));
    $("status-error").hidden = true;
  } catch {
    $("status-error").textContent = "Erfassungsstatus nicht erreichbar. Automatischer Neuversuch in einer Minute.";
    $("status-error").hidden = false;
  }
}

function runtimeDuration(seconds) {
  return `${Math.floor(seconds / 3600)} h ${String(Math.floor(seconds % 3600 / 60)).padStart(2, "0")} min`;
}

function runtimePeriodLabel(kind, value) {
  const text = String(value);
  if (kind === "day") return `${text.slice(6, 8)}.${text.slice(4, 6)}.${text.slice(0, 4)}`;
  if (kind === "month") return `${text.slice(4, 6)}.${text.slice(0, 4)}`;
  return text;
}

async function loadRuntime() {
  const current = ++runtimeRevision, kind = runtimeKind;
  try {
    const data = await json(`/api/pump-runtime?kind=${kind}`, AbortSignal.timeout(10000));
    if (current !== runtimeRevision) return;
    const rows = data.periods.map((period) => {
      const tr = document.createElement("tr");
      const heading = document.createElement("th"); heading.scope = "row";
      heading.textContent = runtimePeriodLabel(kind, period.period); tr.appendChild(heading);
      for (const value of [runtimeDuration(period.solar), runtimeDuration(period.combi_to_buffer), runtimeDuration(period.buffer_to_combi), fullDate.format(new Date(period.observed_at_utc * 1000))]) {
        const td = document.createElement("td"); td.textContent = value; tr.appendChild(td);
      }
      return tr;
    });
    $("runtime-rows").replaceChildren(...rows);
    $("runtime-status").textContent = rows.length ? `${rows.length} Zeiträume · angezeigte Laufzeiten auf Minuten abgerundet` : "Noch keine Pumpenlaufzeiten archiviert. Die Erfassung beginnt mit der nächsten Abfrage des aktualisierten ESP32.";
  } catch {
    if (current !== runtimeRevision) return;
    $("runtime-status").textContent = "Pumpenlaufzeiten konnten nicht geladen werden. Automatischer Neuversuch in einer Minute.";
  }
}

document.querySelectorAll("[data-runtime]").forEach((button) => button.addEventListener("click", () => {
  runtimeKind = button.dataset.runtime;
  document.querySelectorAll("[data-runtime]").forEach((item) => item.setAttribute("aria-pressed", String(item === button)));
  loadRuntime();
}));

document.querySelectorAll("[data-days]").forEach((button) => {
  button.addEventListener("click", () => {
    selectedDays = Number(button.dataset.days);
    document.querySelectorAll(".history .periods button").forEach((b) => b.setAttribute("aria-pressed", String(b === button)));
    $("custom-toggle").setAttribute("aria-expanded", "false");
    $("custom-range").hidden = true;
    loadHistory();
  });
});
$("custom-toggle").addEventListener("click", () => {
  const opening = $("custom-range").hidden;
  $("custom-range").hidden = !opening;
  $("custom-toggle").setAttribute("aria-expanded", String(opening));
  if (opening) $("date-from").focus();
});
$("custom-range").addEventListener("submit", (event) => {
  event.preventDefault();
  if ($("date-from").value > $("date-to").value) {
    $("date-to").setCustomValidity("Das Enddatum muss am oder nach dem Anfangsdatum liegen.");
    $("date-to").reportValidity();
    return;
  }
  selectedDays = null;
  document.querySelectorAll(".history .periods button").forEach((b) => b.setAttribute("aria-pressed", String(b.id === "custom-toggle")));
  query = new URLSearchParams({from: $("date-from").value, to: $("date-to").value});
  loadHistory();
});
$("date-to").addEventListener("input", () => $("date-to").setCustomValidity(""));
$("date-from").addEventListener("input", () => $("date-to").setCustomValidity(""));
$("refresh").addEventListener("click", () => {loadHistory(); loadStatus(); loadRuntime();});
$("retry").addEventListener("click", loadHistory);
$("date-from").value = localDay(new Date(Date.now() - 7 * 86400000));
$("date-to").value = localDay(new Date());
loadHistory();
loadStatus();
loadRuntime();
setInterval(() => {loadStatus(); loadHistory(); loadRuntime();}, 60000);

let updatePolling, checkingUpdates = false, updateBlocked = false;
async function loadUpdateStatus() {
  try {
    const data = await json("/api/update", AbortSignal.timeout(10000));
    $("update-status").textContent = data.message;
    $("update-status").classList.toggle("error", data.state === "error");
    const busy = data.state === "running" || data.state === "restarting";
    $("update-button").disabled = !data.enabled || busy || checkingUpdates || updateBlocked;
    $("check-update").disabled = !data.enabled || busy || checkingUpdates;
    if (data.state === "running" || data.state === "restarting") {
      clearTimeout(updatePolling);
      updatePolling = setTimeout(loadUpdateStatus, 3000);
    }
  } catch {
    $("update-status").textContent = "Server vorübergehend nicht erreichbar. Erneuter Verbindungsversuch …";
    clearTimeout(updatePolling); updatePolling = setTimeout(loadUpdateStatus, 3000);
  }
}
$("update-button").addEventListener("click", async () => {
  $("update-button").disabled = true;
  $("check-update").disabled = true;
  $("update-changelog").hidden = true;
  $("check-status").textContent = "";
  $("update-status").textContent = "Update wird gestartet …";
  try {
    const response = await fetch("/api/update", {method: "POST", headers: {"X-Requested-With": "HistoryUI"}});
    if (!response.ok) { const body = await response.json(); throw new Error(body.detail || "Update konnte nicht gestartet werden."); }
    updatePolling = setTimeout(loadUpdateStatus, 1500);
  } catch (error) {
    $("update-status").textContent = error.message;
    loadUpdateStatus();
  }
});
$("check-update").addEventListener("click", async () => {
  checkingUpdates = true;
  $("check-update").disabled = true;
  $("update-button").disabled = true;
  $("update-changelog").hidden = true;
  $("check-status").classList.remove("error");
  $("check-status").textContent = "Update-Quelle wird geprüft …";
  try {
    const response = await fetch("/api/update/check", {method: "POST", headers: {"X-Requested-With": "HistoryUI"}, signal: AbortSignal.timeout(45000)});
    const data = await response.json();
    if (!response.ok) throw new Error(data.detail || "Update-Prüfung fehlgeschlagen.");
    updateBlocked = Boolean(data.ahead || data.dirty);
    const warnings = [];
    if (data.ahead) warnings.push("Der lokale Branch enthält eigene Commits.");
    if (data.dirty) warnings.push("Der Checkout enthält lokale Änderungen.");
    $("check-status").textContent = (data.available ? `${data.behind} neue ${data.behind === 1 ? "Änderung" : "Änderungen"} im Git-Branch verfügbar.` : "Der Git-Branch ist aktuell.") + (warnings.length ? ` ${warnings.join(" ")} Ein Update ist so nicht möglich.` : "");
    if (data.available) {
      const items = data.changes.map((change) => {
        const li = document.createElement("li"), title = document.createElement("strong"), time = document.createElement("time");
        title.textContent = change.title;
        time.dateTime = change.date;
        const date = new Date(change.date);
        time.textContent = `${Number.isNaN(date.valueOf()) ? change.date : dateOnly.format(date)} · ${change.id}`;
        li.append(title, time);
        if (change.body) { const body = document.createElement("p"); body.textContent = change.body; li.append(body); }
        return li;
      });
      if (!items.length) { const li = document.createElement("li"); li.textContent = "Keine Änderungen am History-Server in diesen Commits."; items.push(li); }
      $("update-changes").replaceChildren(...items);
      $("update-changelog").hidden = false;
      $("update-changelog").open = true;
    }
  } catch (error) {
    $("check-status").classList.add("error");
    $("check-status").textContent = error.name === "TimeoutError" ? "Die Update-Prüfung dauert zu lange. Bitte erneut versuchen." : error.message;
  } finally {
    checkingUpdates = false;
    loadUpdateStatus();
  }
});
loadUpdateStatus();
