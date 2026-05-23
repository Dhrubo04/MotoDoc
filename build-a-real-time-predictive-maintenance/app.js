const maxPoints = 60;
const state = {
  mode: "simulate",
  paused: false,
  samples: [],
  timer: null,
  socket: null,
  port: null,
  reader: null,
  keepReading: false,
  lastData: null,
  activeTrendKey: null,
};

const sensors = {
  vibration: { label: "Vibration", unit: "g", decimals: 2, min: 0, max: 2.5, color: "#12848a", value: document.querySelector("#vibrationValue"), chart: document.querySelector("#vibrationChart") },
  sound: { label: "Sound", unit: "dB", decimals: 0, min: 35, max: 110, color: "#3454d1", value: document.querySelector("#soundValue"), chart: document.querySelector("#soundChart") },
  current: { label: "Current", unit: "A", decimals: 2, min: 0, max: 3.2, color: "#c97808", value: document.querySelector("#currentValue"), chart: document.querySelector("#currentChart") },
  motorTemperature: { label: "Motor Temperature", unit: "C", decimals: 1, min: 20, max: 105, color: "#c93a31", value: document.querySelector("#motorTemperatureValue"), chart: document.querySelector("#motorTemperatureChart") },
  ambientTemperature: { label: "Ambient Temperature", unit: "C", decimals: 1, min: 15, max: 70, color: "#ff776d", value: document.querySelector("#ambientTemperatureValue"), chart: document.querySelector("#ambientTemperatureChart") },
  humidity: { label: "Humidity", unit: "%", decimals: 0, min: 20, max: 90, color: "#1f9d66", value: document.querySelector("#humidityValue"), chart: document.querySelector("#humidityChart") },
};

const elements = {
  clock: document.querySelector("#clock"),
  themeToggle: document.querySelector("#themeToggle"),
  connectionStatus: document.querySelector("#connectionStatus"),
  alertBand: document.querySelector("#alertBand"),
  predictionTitle: document.querySelector("#predictionTitle"),
  predictionDetail: document.querySelector("#predictionDetail"),
  faultType: document.querySelector("#faultType"),
  faultDescription: document.querySelector("#faultDescription"),
  faultConfidence: document.querySelector("#faultConfidence"),
  futureRisk: document.querySelector("#futureRisk"),
  faultIndicators: document.querySelector("#faultIndicators"),
  recommendedAction: document.querySelector("#recommendedAction"),
  riskScore: document.querySelector("#riskScore"),
  riskRing: document.querySelector("#riskRing"),
  sampleCount: document.querySelector("#sampleCount"),
  latestTimestamp: document.querySelector("#latestTimestamp"),
  timeline: document.querySelector("#timeline"),
  baudRateInput: document.querySelector("#baudRateInput"),
  endpointInput: document.querySelector("#endpointInput"),
  connectBtn: document.querySelector("#connectBtn"),
  pauseBtn: document.querySelector("#pauseBtn"),
  clearBtn: document.querySelector("#clearBtn"),
  ingestBtn: document.querySelector("#ingestBtn"),
  manualJson: document.querySelector("#manualJson"),
  modeButtons: document.querySelectorAll("[data-mode]"),
  metricCards: document.querySelectorAll(".metric-card"),
  trendModal: document.querySelector("#trendModal"),
  closeTrendModal: document.querySelector("#closeTrendModal"),
  trendTitle: document.querySelector("#trendTitle"),
  trendChart: document.querySelector("#trendChart"),
  trendLatest: document.querySelector("#trendLatest"),
  trendMin: document.querySelector("#trendMin"),
  trendMax: document.querySelector("#trendMax"),
  trendAvg: document.querySelector("#trendAvg"),
};

function applyTheme(theme) {
  const isDark = theme === "dark";
  document.body.classList.toggle("dark-theme", isDark);
  elements.themeToggle.textContent = isDark ? "Light Theme" : "Dark Theme";
  localStorage.setItem("maintenance-theme", theme);
  renderCharts();
}

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function randomBetween(min, max) {
  return min + Math.random() * (max - min);
}

async function stopStreams() {
  if (state.timer) {
    clearInterval(state.timer);
    state.timer = null;
  }
  if (state.socket) {
    state.socket.close();
    state.socket = null;
  }
  await stopSerial();
}

function setStatus(text, level = "normal") {
  elements.connectionStatus.className = `status-pill ${level === "normal" ? "" : level}`;
  elements.connectionStatus.querySelector("span:last-child").textContent = text;
}

function setConnectButton(text) {
  elements.connectBtn.textContent = text;
}

function normalizePayload(raw) {
  const payload = typeof raw === "string" ? JSON.parse(raw) : raw;
  const vibration = Number(payload.vibration ?? payload.vibration_g ?? payload.accel ?? 0);
  const sound = Number(payload.sound ?? payload.sound_db ?? payload.noise ?? 0);
  const current = Number(payload.current ?? payload.current_a ?? payload.amps ?? 0);
  const ambientTemperature = Number(payload.ambient_temperature ?? payload.ambientTemperature ?? payload.ambient_temp ?? payload.temperature ?? payload.temp ?? payload.temperature_c ?? 0);
  const motorTemperature = Number(payload.motor_temperature ?? payload.motorTemperature ?? payload.motor_temp ?? payload.motorTemp ?? ambientTemperature);
  const humidity = Number(payload.humidity ?? payload.rh ?? 0);
  const computedRisk = calculateRisk({ vibration, sound, current, motorTemperature, ambientTemperature, humidity });
  const risk = clamp(Number(payload.risk ?? payload.failureRisk ?? computedRisk), 0, 100);
  const prediction = normalizePrediction(payload.prediction ?? payload.ml_prediction ?? payload.alert, risk);
  const diagnosis = classifyFault({
    vibration,
    sound,
    current,
    motorTemperature,
    ambientTemperature,
    humidity,
    faultType: payload.fault_type ?? payload.fault ?? payload.faultType,
  });

  return {
    vibration,
    sound,
    current,
    motorTemperature,
    ambientTemperature,
    humidity,
    risk,
    prediction,
    diagnosis,
    timestamp: payload.timestamp ? new Date(payload.timestamp) : new Date(),
  };
}

function normalizePrediction(prediction, risk) {
  const text = String(prediction || "").toLowerCase();
  if (text.includes("critical") || text.includes("failure") || risk >= 75) return "critical";
  if (text.includes("warning") || text.includes("attention") || text.includes("anomaly") || risk >= 45) return "warning";
  return "normal";
}

function calculateRisk(data) {
  const vibrationRisk = clamp((data.vibration - 0.55) / 1.35, 0, 1) * 34;
  const soundRisk = clamp((data.sound - 68) / 30, 0, 1) * 24;
  const currentRisk = clamp((data.current - 1.25) / 1.25, 0, 1) * 18;
  const motorTempRisk = clamp((data.motorTemperature - 48) / 38, 0, 1) * 28;
  const ambientTempRisk = clamp((data.ambientTemperature - 42) / 24, 0, 1) * 10;
  const humidityRisk = clamp(Math.abs(data.humidity - 48) / 34, 0, 1) * 14;
  return Math.round(Math.min(100, vibrationRisk + soundRisk + currentRisk + motorTempRisk + ambientTempRisk + humidityRisk));
}

function classifyFault(data) {
  const indicators = getFaultIndicators(data);
  const explicitFault = normalizeFaultType(data.faultType);
  if (explicitFault) return buildFaultDiagnosis(explicitFault, indicators, 96, true);

  const currentHigh = data.current >= 1.4;
  const highVibration = data.vibration >= 1.05;
  const mediumVibration = data.vibration >= 0.65;
  const highSound = data.sound >= 80;
  const mediumSound = data.sound >= 68;
  const highMotorTemperature = data.motorTemperature >= 65;
  const mediumMotorTemperature = data.motorTemperature >= 48;
  const highAmbientTemperature = data.ambientTemperature >= 45;
  const highHumidity = data.humidity >= 70;

  const candidates = [
    {
      type: "load_jam",
      score: Number(highVibration) + Number(highSound) + Number(highMotorTemperature) + Number(currentHigh),
      possible: highVibration && highSound && highMotorTemperature,
    },
    {
      type: "bearing",
      score: Number(highVibration) + Number(highSound) + Number(mediumMotorTemperature),
      possible: highVibration && highSound,
    },
    {
      type: "overheating",
      score: Number(highMotorTemperature) + Number(highAmbientTemperature || highHumidity) + Number(!highVibration),
      possible: highMotorTemperature && (!highVibration || highAmbientTemperature || highHumidity),
    },
    {
      type: "moisture",
      score: Number(highHumidity) + Number(highAmbientTemperature || mediumMotorTemperature) + Number(!mediumVibration) + Number(!mediumSound),
      possible: highHumidity && !mediumVibration,
    },
    {
      type: "imbalance",
      score: Number(mediumVibration) + Number(mediumSound) + Number(!highMotorTemperature),
      possible: mediumVibration && mediumSound && !highMotorTemperature,
    },
    {
      type: "electrical",
      score: Number(currentHigh) + Number(highMotorTemperature) + Number(!mediumVibration),
      possible: currentHigh && highMotorTemperature,
    },
  ].filter((candidate) => candidate.possible);

  if (!candidates.length) return buildFaultDiagnosis("normal", indicators, 0, false);

  candidates.sort((a, b) => b.score - a.score);
  const confidence = clamp(Math.round((candidates[0].score / 4) * 100), 45, 92);
  return buildFaultDiagnosis(candidates[0].type, indicators, confidence, false);
}

function normalizeFaultType(value) {
  const text = String(value || "").toLowerCase().replace(/[\s-]+/g, "_");
  if (!text || text === "normal" || text === "none") return "";
  if (text.includes("bearing")) return "bearing";
  if (text.includes("cooling") || text.includes("overheat")) return "overheating";
  if (text.includes("moisture") || text.includes("insulation")) return "moisture";
  if (text.includes("imbalance") || text.includes("misalignment")) return "imbalance";
  if (text.includes("electrical") || text.includes("winding")) return "electrical";
  if (text.includes("load") || text.includes("jam")) return "load_jam";
  return "";
}

function getFaultIndicators(data) {
  return [
    { label: `Vibration ${data.vibration.toFixed(2)}g`, level: data.vibration >= 1.05 ? "critical" : data.vibration >= 0.65 ? "warning" : "normal" },
    { label: `Sound ${Math.round(data.sound)}dB`, level: data.sound >= 80 ? "critical" : data.sound >= 68 ? "warning" : "normal" },
    { label: `Current ${data.current.toFixed(2)}A`, level: data.current >= 1.4 ? "critical" : data.current >= 1.1 ? "warning" : "normal" },
    { label: `Motor temp ${data.motorTemperature.toFixed(1)}C`, level: data.motorTemperature >= 65 ? "critical" : data.motorTemperature >= 48 ? "warning" : "normal" },
    { label: `Ambient temp ${data.ambientTemperature.toFixed(1)}C`, level: data.ambientTemperature >= 45 ? "critical" : data.ambientTemperature >= 38 ? "warning" : "normal" },
    { label: `Humidity ${Math.round(data.humidity)}%`, level: data.humidity >= 70 ? "critical" : data.humidity >= 60 ? "warning" : "normal" },
  ];
}

function buildFaultDiagnosis(type, indicators, confidence, fromModel) {
  const guide = {
    normal: {
      title: "No fault detected",
      description: "Sensor values currently match the normal operating profile.",
      futureRisk: "None",
      action: "Continue monitoring.",
    },
    bearing: {
      title: "Bearing fault",
      description: "High vibration with elevated sound suggests bearing wear, damage, or grinding noise.",
      futureRisk: "Bearing seizure",
      action: "Inspect bearings, lubrication, shaft play, and mounting alignment.",
    },
    overheating: {
      title: "Cooling failure / overheating",
      description: "High motor temperature, especially with high humidity or weak vibration change, suggests poor ventilation or heat dissipation.",
      futureRisk: "Overheating shutdown",
      action: "Check fan, airflow path, dust blockage, load level, and heat dissipation.",
    },
    moisture: {
      title: "Moisture / insulation risk",
      description: "High humidity with low mechanical symptoms suggests moisture ingress or insulation deterioration risk.",
      futureRisk: "Insulation breakdown",
      action: "Inspect enclosure sealing, dry the motor area, and test winding insulation.",
    },
    imbalance: {
      title: "Mechanical imbalance / misalignment",
      description: "Moderate to high vibration with sound increase suggests imbalance, misalignment, or loose coupling.",
      futureRisk: "Shaft bending",
      action: "Check coupling, shaft alignment, mounting bolts, and rotor balance.",
    },
    electrical: {
      title: "Electrical fault",
      description: "High current with rising temperature suggests winding, insulation, or electrical imbalance.",
      futureRisk: "Coil burning",
      action: "Check current draw, winding resistance, supply imbalance, and insulation condition.",
    },
    load_jam: {
      title: "Load overload / mechanical jam",
      description: "High vibration, sound, and temperature together suggest excessive load or mechanical jamming.",
      futureRisk: "Motor burnout",
      action: "Reduce load, inspect driven mechanism, coupling, bearing drag, and shaft obstruction.",
    },
  };

  return {
    type,
    fromModel,
    confidence,
    indicators,
    ...guide[type],
  };
}

function makeSimulatedSample() {
  const previous = state.lastData || {
    vibration: 0.38,
    sound: 61,
    current: 0.82,
    motorTemperature: 39,
    ambientTemperature: 31,
    humidity: 49,
  };
  const drift = Math.sin(Date.now() / 9000) * 0.6 + Math.random() * 0.4;
  const spike = Math.random() > 0.91 ? randomBetween(0.35, 1.1) : 0;
  const data = {
    vibration: clamp(previous.vibration + randomBetween(-0.08, 0.08) + spike, 0.08, 2.35),
    sound: clamp(previous.sound + randomBetween(-3, 3) + spike * 18, 42, 106),
    current: clamp(previous.current + randomBetween(-0.06, 0.07) + spike * 0.28, 0.35, 3.0),
    motorTemperature: clamp(previous.motorTemperature + randomBetween(-0.45, 0.65) + drift * 0.42 + spike * 8, 28, 96),
    ambientTemperature: clamp(previous.ambientTemperature + randomBetween(-0.25, 0.35) + drift * 0.2, 24, 62),
    humidity: clamp(previous.humidity + randomBetween(-1.3, 1.3) - drift * 0.25, 28, 82),
  };
  if (Math.random() > 0.94) {
    const pattern = ["bearing", "overheating", "moisture", "imbalance", "load_jam"][Math.floor(randomBetween(0, 5))];
    if (pattern === "bearing") Object.assign(data, { vibration: randomBetween(1.15, 1.9), sound: randomBetween(82, 101), current: randomBetween(0.75, 1.1), motorTemperature: randomBetween(48, 60), ambientTemperature: randomBetween(30, 38), humidity: randomBetween(38, 56) });
    if (pattern === "overheating") Object.assign(data, { vibration: randomBetween(0.25, 0.58), sound: randomBetween(52, 66), current: randomBetween(0.8, 1.2), motorTemperature: randomBetween(66, 88), ambientTemperature: randomBetween(42, 56), humidity: randomBetween(58, 76) });
    if (pattern === "moisture") Object.assign(data, { vibration: randomBetween(0.22, 0.55), sound: randomBetween(48, 64), current: randomBetween(0.65, 1.0), motorTemperature: randomBetween(38, 52), ambientTemperature: randomBetween(36, 48), humidity: randomBetween(72, 88) });
    if (pattern === "imbalance") Object.assign(data, { vibration: randomBetween(0.76, 1.28), sound: randomBetween(69, 79), current: randomBetween(1.0, 1.38), motorTemperature: randomBetween(38, 52), ambientTemperature: randomBetween(30, 39), humidity: randomBetween(38, 58) });
    if (pattern === "load_jam") Object.assign(data, { vibration: randomBetween(1.22, 2.2), sound: randomBetween(84, 106), current: randomBetween(1.45, 2.7), motorTemperature: randomBetween(68, 94), ambientTemperature: randomBetween(32, 42), humidity: randomBetween(36, 58) });
  }
  const risk = calculateRisk(data);
  return normalizePayload({ ...data, risk, prediction: normalizePrediction("", risk) });
}

function ingest(raw) {
  if (state.paused) return;
  try {
    const sample = normalizePayload(raw);
    state.lastData = sample;
    state.samples.push(sample);
    if (state.samples.length > maxPoints) state.samples.shift();
    updateDashboard(sample);
  } catch (error) {
    setStatus(`JSON error: ${error.message}`, "critical");
  }
}

function ingestSerialLine(line) {
  const message = line.trim();
  if (!message.startsWith("{") || !message.endsWith("}")) {
    setStatus("Ignoring ESP32 boot/debug line", "warning");
    return;
  }
  ingest(message);
}

function updateDashboard(sample) {
  Object.entries(sensors).forEach(([key, sensor]) => {
    sensor.value.textContent = formatSensorValue(sample[key], sensor, false);
  });

  const level = sample.prediction;
  const title = {
    normal: "Healthy operation",
    warning: "Maintenance attention needed",
    critical: "High failure probability",
  }[level];
  const detail = {
    normal: "The model currently sees normal vibration, sound, current, motor temperature, ambient temperature, and humidity patterns.",
    warning: "The model detected drift from the normal operating profile. Schedule inspection soon.",
    critical: "The model detected a high-risk pattern. Reduce load and inspect bearings, mounting, and cooling.",
  }[level];

  elements.alertBand.className = `alert-band ${level === "normal" ? "" : level}`;
  elements.predictionTitle.textContent = title;
  elements.predictionDetail.textContent = detail;
  elements.riskScore.textContent = `${Math.round(sample.risk)}%`;
  elements.riskRing.style.strokeDashoffset = 302 - (302 * sample.risk) / 100;
  elements.riskRing.style.stroke = level === "critical" ? "#ffb1aa" : level === "warning" ? "#ffd27d" : "#69e0a8";
  elements.sampleCount.textContent = `${state.samples.length} samples`;
  elements.latestTimestamp.textContent = sample.timestamp.toLocaleString();
  updateFaultDiagnosis(sample.diagnosis);

  renderCharts();
  renderTimeline();
}

function updateFaultDiagnosis(diagnosis) {
  elements.faultType.textContent = diagnosis.title;
  elements.faultDescription.textContent = diagnosis.fromModel
    ? `${diagnosis.description} This fault type was supplied by the ESP32/backend model.`
    : `${diagnosis.description} This is rule-based until a trained model is connected.`;
  elements.faultConfidence.textContent = `${diagnosis.confidence}%`;
  elements.futureRisk.textContent = diagnosis.futureRisk;
  elements.recommendedAction.textContent = diagnosis.action;
  elements.faultIndicators.innerHTML = diagnosis.indicators
    .map((indicator) => `<span class="indicator ${indicator.level}">${indicator.label}</span>`)
    .join("");
}

function formatSensorValue(value, sensor, includeUnit = true) {
  const numeric = Number(value || 0);
  const formatted = sensor.decimals === 0 ? Math.round(numeric).toString() : numeric.toFixed(sensor.decimals);
  return includeUnit ? `${formatted} ${sensor.unit}` : formatted;
}

function renderCharts() {
  Object.entries(sensors).forEach(([key, sensor]) => {
    const canvas = sensor.chart;
    const context = canvas.getContext("2d");
    const width = canvas.width;
    const height = canvas.height;
    const values = state.samples.map((sample) => sample[key]);
    context.clearRect(0, 0, width, height);
    context.fillStyle = getComputedStyle(document.body).getPropertyValue("--chart-bg").trim() || "#f7faf8";
    context.fillRect(0, 0, width, height);

    context.strokeStyle = "#d9e1dc";
    context.lineWidth = 1;
    for (let i = 1; i < 4; i += 1) {
      const y = (height / 4) * i;
      context.beginPath();
      context.moveTo(0, y);
      context.lineTo(width, y);
      context.stroke();
    }

    if (values.length < 2) return;
    context.beginPath();
    values.forEach((value, index) => {
      const x = (index / (maxPoints - 1)) * width;
      const ratio = (clamp(value, sensor.min, sensor.max) - sensor.min) / (sensor.max - sensor.min);
      const y = height - ratio * (height - 18) - 9;
      if (index === 0) context.moveTo(x, y);
      else context.lineTo(x, y);
    });
    context.strokeStyle = sensor.color;
    context.lineWidth = 4;
    context.lineCap = "round";
    context.lineJoin = "round";
    context.stroke();
  });
  renderTrendModal();
}

function openTrendModal(sensorKey) {
  state.activeTrendKey = sensorKey;
  elements.trendModal.hidden = false;
  renderTrendModal();
}

function closeTrendModal() {
  elements.trendModal.hidden = true;
  state.activeTrendKey = null;
}

function renderTrendModal() {
  if (!state.activeTrendKey || elements.trendModal.hidden) return;
  const sensor = sensors[state.activeTrendKey];
  if (!sensor) return;

  const values = state.samples.map((sample) => Number(sample[state.activeTrendKey] || 0));
  elements.trendTitle.textContent = `${sensor.label} Trend`;
  const latest = values.at(-1) ?? 0;
  const min = values.length ? Math.min(...values) : 0;
  const max = values.length ? Math.max(...values) : 0;
  const avg = values.length ? values.reduce((sum, value) => sum + value, 0) / values.length : 0;
  elements.trendLatest.textContent = formatSensorValue(latest, sensor);
  elements.trendMin.textContent = formatSensorValue(min, sensor);
  elements.trendMax.textContent = formatSensorValue(max, sensor);
  elements.trendAvg.textContent = formatSensorValue(avg, sensor);

  const canvas = elements.trendChart;
  const context = canvas.getContext("2d");
  const width = canvas.width;
  const height = canvas.height;
  const styles = getComputedStyle(document.body);
  const chartBg = styles.getPropertyValue("--chart-bg").trim() || "#f7faf8";
  const gridColor = styles.getPropertyValue("--line").trim() || "#d9e1dc";
  const textColor = styles.getPropertyValue("--muted").trim() || "#65726c";

  context.clearRect(0, 0, width, height);
  context.fillStyle = chartBg;
  context.fillRect(0, 0, width, height);
  context.strokeStyle = gridColor;
  context.lineWidth = 1;
  context.fillStyle = textColor;
  context.font = "18px system-ui";

  const pad = { left: 62, right: 28, top: 28, bottom: 48 };
  for (let i = 0; i <= 4; i += 1) {
    const y = pad.top + ((height - pad.top - pad.bottom) / 4) * i;
    const value = sensor.max - ((sensor.max - sensor.min) / 4) * i;
    context.beginPath();
    context.moveTo(pad.left, y);
    context.lineTo(width - pad.right, y);
    context.stroke();
    context.fillText(formatSensorValue(value, sensor), 8, y + 6);
  }

  if (values.length < 2) return;

  context.beginPath();
  values.forEach((value, index) => {
    const x = pad.left + (index / Math.max(1, values.length - 1)) * (width - pad.left - pad.right);
    const ratio = (clamp(value, sensor.min, sensor.max) - sensor.min) / (sensor.max - sensor.min);
    const y = height - pad.bottom - ratio * (height - pad.top - pad.bottom);
    if (index === 0) context.moveTo(x, y);
    else context.lineTo(x, y);
  });
  context.strokeStyle = sensor.color;
  context.lineWidth = 5;
  context.lineCap = "round";
  context.lineJoin = "round";
  context.stroke();
}

function renderTimeline() {
  const events = state.samples
    .filter((sample) => sample.prediction !== "normal" || sample.risk >= 35)
    .slice(-10)
    .reverse();

  elements.timeline.innerHTML = events.length
    ? events.map((sample) => {
        const level = sample.prediction;
        return `<article class="event ${level}">
          <time>${sample.timestamp.toLocaleTimeString()}</time>
          <div>
            <strong>${sample.diagnosis.title}</strong>
            <span>V ${sample.vibration.toFixed(2)}g | S ${Math.round(sample.sound)}dB | A ${sample.current.toFixed(2)}A | MT ${sample.motorTemperature.toFixed(1)}C | AT ${sample.ambientTemperature.toFixed(1)}C | H ${Math.round(sample.humidity)}%</span>
          </div>
          <b class="badge ${level}">${Math.round(sample.risk)}%</b>
        </article>`;
      }).join("")
    : `<article class="event">
        <time>Live</time>
        <div>
          <strong>No alerts yet</strong>
          <span>Incoming samples are inside the normal operating envelope.</span>
        </div>
        <b class="badge">OK</b>
      </article>`;
}

async function startSimulation() {
  await stopStreams();
  setStatus("Simulation running");
  setConnectButton("Restart Simulation");
  state.timer = setInterval(() => ingest(makeSimulatedSample()), 900);
  ingest(makeSimulatedSample());
}

async function startWebSocket() {
  await stopStreams();
  const url = elements.endpointInput.value.trim();
  if (!url) return setStatus("Missing WebSocket URL", "critical");
  state.socket = new WebSocket(url);
  state.socket.addEventListener("open", () => setStatus("WebSocket connected"));
  state.socket.addEventListener("message", (event) => ingest(event.data));
  state.socket.addEventListener("close", () => setStatus("WebSocket closed", "warning"));
  state.socket.addEventListener("error", () => setStatus("WebSocket error", "critical"));
  setConnectButton("Reconnect WebSocket");
}

async function startPolling() {
  await stopStreams();
  const url = elements.endpointInput.value.trim();
  if (!url) return setStatus("Missing HTTP endpoint", "critical");
  async function poll() {
    try {
      const response = await fetch(url, { cache: "no-store" });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      ingest(await response.json());
      setStatus("HTTP polling active");
    } catch (error) {
      setStatus(`Polling error: ${error.message}`, "critical");
    }
  }
  poll();
  state.timer = setInterval(poll, 1500);
  setConnectButton("Restart Polling");
}

async function stopSerial() {
  state.keepReading = false;
  if (state.reader) {
    try {
      await state.reader.cancel();
      state.reader.releaseLock();
    } catch (error) {
      console.warn(error);
    }
    state.reader = null;
  }
  if (state.port) {
    try {
      await state.port.close();
    } catch (error) {
      console.warn(error);
    }
    state.port = null;
  }
}

async function startSerial() {
  if (!("serial" in navigator)) {
    setStatus("Web Serial needs Chrome or Edge", "critical");
    return;
  }

  await stopStreams();
  try {
    setStatus("Choose ESP32 COM port", "warning");
    const baudRate = Number(elements.baudRateInput.value || 115200);
    state.port = await navigator.serial.requestPort();
    await state.port.open({ baudRate });
    state.keepReading = true;
    setStatus(`Serial connected at ${baudRate}`);
    setConnectButton("Reconnect Serial");
    readSerialLoop();
  } catch (error) {
    setStatus(`Serial error: ${error.message}`, "critical");
  }
}

async function readSerialLoop() {
  const decoder = new TextDecoder();
  let buffer = "";

  while (state.port && state.keepReading) {
    try {
      state.reader = state.port.readable.getReader();
      while (state.keepReading) {
        const { value, done } = await state.reader.read();
        if (done) break;
        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split(/\r?\n/);
        buffer = lines.pop() || "";
        lines.map((line) => line.trim()).filter(Boolean).forEach(ingestSerialLine);
      }
    } catch (error) {
      if (state.keepReading) setStatus(`Serial read error: ${error.message}`, "critical");
    } finally {
      if (state.reader) {
        state.reader.releaseLock();
        state.reader = null;
      }
    }
  }

  if (state.port) {
    await stopSerial();
    setStatus("Serial disconnected", "warning");
  }
}

async function connectCurrentMode() {
  if (state.mode === "simulate") await startSimulation();
  if (state.mode === "serial") await startSerial();
  if (state.mode === "websocket") await startWebSocket();
  if (state.mode === "poll") await startPolling();
  if (state.mode === "manual") {
    await stopStreams();
    setStatus("Manual input ready", "warning");
    setConnectButton("Manual Mode");
  }
}

elements.modeButtons.forEach((button) => {
  button.addEventListener("click", async () => {
    elements.modeButtons.forEach((item) => item.classList.remove("active"));
    button.classList.add("active");
    state.mode = button.dataset.mode;
    if (state.mode === "serial") {
      await stopStreams();
      setStatus("Serial ready", "warning");
      setConnectButton("Connect Serial");
      return;
    }
    if (state.mode === "poll") {
      elements.endpointInput.value = "http://192.168.4.1/data";
      setConnectButton("Start Polling");
    }
    if (state.mode === "websocket") {
      elements.endpointInput.value = "ws://192.168.4.1/ws";
      setConnectButton("Connect WebSocket");
    }
    if (state.mode === "simulate") setConnectButton("Restart Simulation");
    await connectCurrentMode();
  });
});

elements.connectBtn.addEventListener("click", connectCurrentMode);
elements.pauseBtn.addEventListener("click", () => {
  state.paused = !state.paused;
  elements.pauseBtn.textContent = state.paused ? "Resume" : "Pause";
  setStatus(state.paused ? "Paused" : `${state.mode} active`, state.paused ? "warning" : "normal");
});
elements.clearBtn.addEventListener("click", () => {
  state.samples = [];
  state.lastData = null;
  renderCharts();
  renderTimeline();
  elements.sampleCount.textContent = "0 samples";
});
elements.ingestBtn.addEventListener("click", () => ingest(elements.manualJson.value));
elements.themeToggle.addEventListener("click", () => {
  const nextTheme = document.body.classList.contains("dark-theme") ? "light" : "dark";
  applyTheme(nextTheme);
});
elements.metricCards.forEach((card) => {
  card.addEventListener("click", () => openTrendModal(card.dataset.sensor));
  card.addEventListener("keydown", (event) => {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      openTrendModal(card.dataset.sensor);
    }
  });
});
elements.closeTrendModal.addEventListener("click", closeTrendModal);
elements.trendModal.addEventListener("click", (event) => {
  if (event.target === elements.trendModal) closeTrendModal();
});
document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && !elements.trendModal.hidden) closeTrendModal();
});

setInterval(() => {
  elements.clock.textContent = new Date().toLocaleTimeString();
}, 1000);

applyTheme(localStorage.getItem("maintenance-theme") || "light");
startSimulation();
