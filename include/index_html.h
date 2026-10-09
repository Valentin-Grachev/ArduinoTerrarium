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
    margin: 0 0 4px;
    font-size: 1.5rem;
    font-weight: 600;
  }
  .subtitle {
    margin: 0 0 16px;
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
</style>
</head>
<body>
<main>
  <h1>Террариум</h1>
  <p class="subtitle">Последняя минута, шаг 1 секунда</p>
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
  <section class="chart">
    <h2>Температура, °C</h2>
    <canvas id="tempChart"></canvas>
  </section>
  <section class="chart">
    <h2>Влажность, %</h2>
    <canvas id="humChart"></canvas>
  </section>
  <p class="status" id="status">Ожидание данных…</p>
</main>
<script>
const tempCanvas = document.getElementById("tempChart");
const humCanvas = document.getElementById("humChart");
let lastData = null;
let busy = false;

function latest(values) {
  for (let i = values.length - 1; i >= 0; i--) {
    if (values[i] !== null && Number.isFinite(values[i])) return values[i];
  }
  return null;
}

function drawChart(canvas, values, color) {
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  if (width < 10 || height < 10) return;

  const dpr = window.devicePixelRatio || 1;
  canvas.width = Math.round(width * dpr);
  canvas.height = Math.round(height * dpr);
  const ctx = canvas.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, width, height);

  const pad = { left: 48, right: 16, top: 12, bottom: 28 };
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
  const span = values.length;
  for (let step = 0; step <= span; step += 10) {
    const ago = span - step;
    const x = pad.left + (step / span) * plotW;
    ctx.textAlign = step === 0 ? "left" : (step === span ? "right" : "center");
    ctx.fillText(ago === 0 ? "0" : "\u2212" + ago, x, pad.top + plotH + 8);
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

function render() {
  if (!lastData) return;
  drawChart(tempCanvas, lastData.temperature, "#d35400");
  drawChart(humCanvas, lastData.humidity, "#1d6fbf");
  const temp = latest(lastData.temperature);
  const hum = latest(lastData.humidity);
  document.getElementById("temp").textContent = temp === null ? "—" : temp.toFixed(2) + " °C";
  document.getElementById("hum").textContent = hum === null ? "—" : hum.toFixed(1) + " %";
}

async function tick() {
  if (busy) return;
  busy = true;
  try {
    const res = await fetch("/api/history", { cache: "no-store" });
    if (!res.ok) throw new Error("http");
    const data = await res.json();
    if (!data || !Array.isArray(data.temperature) || !Array.isArray(data.humidity)) {
      throw new Error("payload");
    }
    lastData = data;
    render();
    document.getElementById("status").textContent = "Данные обновляются раз в секунду";
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
