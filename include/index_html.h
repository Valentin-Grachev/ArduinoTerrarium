#pragma once

#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"INDEXHTML(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Террариум</title>
<style>
  body {
    margin: 0;
    background: #f4f6f8;
    color: #1c2430;
    font-family: system-ui, sans-serif;
  }
  main {
    max-width: 960px;
    margin: 0 auto;
    padding: 24px 16px 48px;
  }
  h1 {
    margin: 0 0 16px;
    font-size: 1.5rem;
    font-weight: 600;
  }
  .subtitle {
    margin: 0 0 12px;
    color: #5c6b7a;
  }
  .readings {
    display: flex;
    gap: 12px;
    margin-bottom: 16px;
  }
  .card, .chart {
    background: #fff;
    border: 1px solid #e1e6ec;
    border-radius: 12px;
  }
  .card {
    flex: 1;
    padding: 14px 16px;
  }
  .card .label, .status {
    color: #5c6b7a;
    font-size: 0.9rem;
  }
  .card .value {
    margin-top: 4px;
    font-size: 2rem;
    font-variant-numeric: tabular-nums;
  }
  .tabs {
    display: flex;
    gap: 8px;
    margin-bottom: 12px;
  }
  .tab {
    flex: 1;
    border: 1px solid #e1e6ec;
    background: #fff;
    color: #1c2430;
    border-radius: 999px;
    padding: 10px 12px;
    font: inherit;
    font-size: 0.95rem;
    cursor: pointer;
  }
  .tab.active {
    background: #1c2430;
    color: #fff;
    border-color: #1c2430;
  }
  .chart {
    padding: 14px 12px 8px;
    margin-bottom: 12px;
  }
  .chart h2 {
    margin: 0 8px 8px;
    font-size: 1rem;
  }
  canvas {
    width: 100%;
    height: 220px;
    display: block;
  }
  .status { margin: 4px 8px 0; }
  .panel[hidden] { display: none; }
</style>
</head>
<body>
<main>
  <h1>Террариум</h1>
  <div class="readings">
    <div class="card">
      <div class="label">Температура</div>
      <div class="value" id="temp">—</div>
    </div>
    <div class="card">
      <div class="label">Влажность</div>
      <div class="value" id="hum">—</div>
    </div>
  </div>
  <div class="tabs" role="tablist">
    <button type="button" class="tab active" id="tabNow" role="tab" aria-selected="true" aria-controls="panelNow">Сейчас</button>
    <button type="button" class="tab" id="tabHistory" role="tab" aria-selected="false" aria-controls="panelHistory">История</button>
  </div>
  <p class="subtitle" id="subtitle">Последняя минута, шаг 1 секунда</p>
  <div id="panelNow">
    <section class="chart">
      <h2>Температура, °C</h2>
      <canvas id="tempChart"></canvas>
    </section>
    <section class="chart">
      <h2>Влажность, %</h2>
      <canvas id="humChart"></canvas>
    </section>
  </div>
  <div id="panelHistory" hidden>
    <section class="chart">
      <h2>Температура, °C</h2>
      <canvas id="histTempChart"></canvas>
    </section>
    <section class="chart">
      <h2>Влажность, %</h2>
      <canvas id="histHumChart"></canvas>
    </section>
  </div>
  <p class="status" id="status">Ожидание данных…</p>
</main>
<script>
const tempCanvas = document.getElementById("tempChart");
const humCanvas = document.getElementById("humChart");
const histTempCanvas = document.getElementById("histTempChart");
const histHumCanvas = document.getElementById("histHumChart");
let lastData = null;
let busy = false;
let view = "now";

function readingText(value, digits, unit) {
  if (typeof value !== "number" || !Number.isFinite(value)) return "—";
  return value.toFixed(digits) + " " + unit;
}

function drawChart(canvas, values, color, labels) {
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  if (width < 10 || height < 10 || !values.length) return;

  const dpr = window.devicePixelRatio || 1;
  canvas.width = Math.round(width * dpr);
  canvas.height = Math.round(height * dpr);
  const ctx = canvas.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, width, height);

  const pad = { left: 48, right: 16, top: 12, bottom: labels ? 32 : 28 };
  const plotW = width - pad.left - pad.right;
  const plotH = height - pad.top - pad.bottom;
  const nums = values.filter(function (v) { return v !== null && Number.isFinite(v); });

  let min = 0;
  let max = 1;
  if (nums.length) {
    min = Math.min.apply(null, nums);
    max = Math.max.apply(null, nums);
  }
  if (max - min < 1) {
    const mid = (min + max) / 2;
    min = mid - 0.5;
    max = mid + 0.5;
  }
  const margin = (max - min) * 0.1;
  min -= margin;
  max += margin;

  function yAt(v) {
    return pad.top + (max - v) / (max - min) * plotH;
  }
  function xAt(i) {
    return pad.left + (i + 0.5) / values.length * plotW;
  }

  ctx.strokeStyle = "#e1e6ec";
  ctx.lineWidth = 1;
  ctx.fillStyle = "#5c6b7a";
  ctx.font = "12px system-ui, sans-serif";
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  for (let g = 0; g <= 4; g++) {
    const v = min + (max - min) * (g / 4);
    const y = yAt(v);
    ctx.beginPath();
    ctx.moveTo(pad.left, y);
    ctx.lineTo(width - pad.right, y);
    ctx.stroke();
    ctx.fillText(v.toFixed(1), pad.left - 8, y);
  }

  ctx.textBaseline = "top";
  ctx.fillStyle = "#5c6b7a";
  if (labels && labels.length) {
    ctx.font = "11px system-ui, sans-serif";
    const step = width < 520 ? 12 : 6;
    let lastDrawnX = -1000;
    const lastIndex = values.length - 1;
    for (let i = 0; i < values.length; i++) {
      const last = i === lastIndex;
      if (i % step !== 0 && !last) continue;
      const text = labels[i] || "";
      if (!text) continue;
      const x = xAt(i);
      if (last && x - lastDrawnX < 72) continue;
      ctx.textAlign = i === 0 ? "left" : (last ? "right" : "center");
      ctx.fillText(text, x, pad.top + plotH + 8);
      lastDrawnX = x;
    }
  } else {
    ctx.font = "12px system-ui, sans-serif";
    const span = values.length;
    for (let step = 0; step <= span; step += 10) {
      const ago = span - step;
      const x = pad.left + (step / span) * plotW;
      ctx.textAlign = step === 0 ? "left" : (step === span ? "right" : "center");
      ctx.fillText(ago === 0 ? "0" : "\u2212" + ago, x, pad.top + plotH + 8);
    }
  }

  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.lineJoin = "round";
  ctx.lineCap = "round";
  ctx.beginPath();
  let drawing = false;
  for (let i = 0; i < values.length; i++) {
    const v = values[i];
    if (v === null || !Number.isFinite(v)) {
      drawing = false;
      continue;
    }
    if (!drawing) {
      ctx.moveTo(xAt(i), yAt(v));
      drawing = true;
    } else {
      ctx.lineTo(xAt(i), yAt(v));
    }
  }
  ctx.stroke();

  ctx.fillStyle = color;
  let lastIndex = -1;
  for (let i = 0; i < values.length; i++) {
    const v = values[i];
    if (v === null || !Number.isFinite(v)) continue;
    lastIndex = i;
    ctx.beginPath();
    ctx.arc(xAt(i), yAt(v), 2.2, 0, Math.PI * 2);
    ctx.fill();
  }
  if (lastIndex >= 0) {
    ctx.beginPath();
    ctx.arc(xAt(lastIndex), yAt(values[lastIndex]), 4, 0, Math.PI * 2);
    ctx.fill();
  }
}

function statusText() {
  if (view === "now") return "Данные обновляются раз в секунду";
  if (!lastData || !lastData.synced) {
    return "Нет сетевого времени. История появится после подключения к Wi-Fi";
  }
  return "Час считается по московскому времени";
}

function render() {
  if (!lastData) return;
  if (view === "now") {
    drawChart(tempCanvas, lastData.temperature, "#d35400");
    drawChart(humCanvas, lastData.humidity, "#1d6fbf");
  } else {
    drawChart(histTempCanvas, lastData.history_temperature, "#d35400", lastData.history_labels);
    drawChart(histHumCanvas, lastData.history_humidity, "#1d6fbf", lastData.history_labels);
  }
  document.getElementById("temp").textContent = readingText(lastData.live_temperature, 2, "°C");
  document.getElementById("hum").textContent = readingText(lastData.live_humidity, 1, "%");
  document.getElementById("status").textContent = statusText();
}

function setView(next) {
  view = next;
  const now = view === "now";
  document.getElementById("panelNow").hidden = !now;
  document.getElementById("panelHistory").hidden = now;
  document.getElementById("tabNow").classList.toggle("active", now);
  document.getElementById("tabHistory").classList.toggle("active", !now);
  document.getElementById("tabNow").setAttribute("aria-selected", now ? "true" : "false");
  document.getElementById("tabHistory").setAttribute("aria-selected", now ? "false" : "true");
  document.getElementById("subtitle").textContent = now
    ? "Последняя минута, шаг 1 секунда"
    : "Последние 48 часов, среднее за час";
  render();
}

document.getElementById("tabNow").addEventListener("click", function () { setView("now"); });
document.getElementById("tabHistory").addEventListener("click", function () { setView("history"); });

async function tick() {
  if (busy) return;
  busy = true;
  try {
    const res = await fetch("/api/history", { cache: "no-store" });
    if (!res.ok) throw new Error("http");
    const data = await res.json();
    if (!data || !Array.isArray(data.temperature) || !Array.isArray(data.humidity) ||
        !Array.isArray(data.history_temperature) || !Array.isArray(data.history_humidity) ||
        !Array.isArray(data.history_labels)) {
      throw new Error("payload");
    }
    lastData = data;
    render();
  } catch (err) {
    document.getElementById("status").textContent = "Нет связи с ESP32";
  } finally {
    busy = false;
  }
}

tick();
setInterval(tick, 1000);
window.addEventListener("resize", render);
</script>
</body>
</html>
)INDEXHTML";
