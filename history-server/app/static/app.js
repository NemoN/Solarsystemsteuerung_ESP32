"use strict";

const $ = (id) => document.getElementById(id);
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
  return document.querySelector(`[data-sensor="${id}"]`).checked;
}

function draw(data) {
  if (chart) chart.destroy();
  const duration = new Date(data.to) - new Date(data.from);
  const ticks = new Intl.DateTimeFormat("de-DE", duration <= 2 * 86400000
    ? {timeZone: zone, hour: "2-digit", minute: "2-digit"}
    : {timeZone: zone, day: "2-digit", month: "2-digit"});
  chart = new Chart($("history-chart"), {
    type: "line",
    data: {datasets: data.series.map((s, i) => ({
      label: s.label, data: s.points, borderColor: s.color, backgroundColor: s.color,
      borderWidth: 2, borderDash: [[], [7, 4], [2, 4]][i],
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
          grid: {display: false}, border: {color: "#b9c8c9"},
          ticks: {color: "#4e6268", maxTicksLimit: window.innerWidth < 650 ? 4 : 8, maxRotation: 0, callback: (value) => ticks.format(value)}},
        y: {title: {display: true, text: "Temperatur (°C)", color: "#4e6268"},
          grid: {color: "#e4eae9"}, border: {display: false}, ticks: {color: "#4e6268", callback: (value) => number.format(value)}},
      },
    },
  });
}

function showVisibilityMessage() {
  if (lastData && lastData.valid_value_count > 0) {
    if (![0, 1, 2].some(selected)) message("Alle Kurven sind ausgeblendet. Oben einen Sensor auswählen.");
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

document.querySelectorAll("[data-days]").forEach((button) => {
  button.addEventListener("click", () => {
    selectedDays = Number(button.dataset.days);
    document.querySelectorAll(".periods button").forEach((b) => b.setAttribute("aria-pressed", String(b === button)));
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
  document.querySelectorAll(".periods button").forEach((b) => b.setAttribute("aria-pressed", String(b.id === "custom-toggle")));
  query = new URLSearchParams({from: $("date-from").value, to: $("date-to").value});
  loadHistory();
});
$("date-to").addEventListener("input", () => $("date-to").setCustomValidity(""));
$("date-from").addEventListener("input", () => $("date-to").setCustomValidity(""));
document.querySelectorAll("[data-sensor]").forEach((input) => input.addEventListener("change", () => {
  if (chart) {
    chart.setDatasetVisibility(Number(input.dataset.sensor), input.checked);
    chart.update("none");
  }
  showVisibilityMessage();
}));
$("refresh").addEventListener("click", () => {loadHistory(); loadStatus();});
$("retry").addEventListener("click", loadHistory);
$("date-from").value = localDay(new Date(Date.now() - 7 * 86400000));
$("date-to").value = localDay(new Date());
loadHistory();
loadStatus();
setInterval(() => {loadStatus(); loadHistory();}, 60000);
