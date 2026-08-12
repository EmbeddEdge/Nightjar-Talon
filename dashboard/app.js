/* ==========================================================================
   Nightjar Talon Web Bluetooth & Survey Engine
   ========================================================================== */

// BLE Configuration Constants
const SERVICE_UUID = '19b10000-e8f2-537e-4f6c-d104768a1214';
const LUX_CHAR_UUID = '19b10001-e8f2-537e-4f6c-d104768a1214';
const SURVEY_CHAR_UUID = '19b10002-e8f2-537e-4f6c-d104768a1214';
const CONTROL_CHAR_UUID = '19b10003-e8f2-537e-4f6c-d104768a1214';

// Room Standards Registry (Duplicates firmware config for offline lookup)
const ROOM_STANDARDS = {
  office: { name: "Office / Work Desk", targetLux: 500 },
  kitchen: { name: "Kitchen Counter", targetLux: 350 },
  living: { name: "Living / Family Room", targetLux: 150 },
  bedroom: { name: "Bedroom", targetLux: 120 },
  dining: { name: "Dining Room", targetLux: 150 },
  bathroom: { name: "Bathroom", targetLux: 200 },
  hallway: { name: "Hallway / Corridor", targetLux: 80 },
  workshop: { name: "Workshop / Garage", targetLux: 300 }
};

// Application State Variables
let bleDevice = null;
let bleServer = null;
let bleService = null;
let luxChar = null;
let surveyChar = null;
let controlChar = null;

let currentLux = 0.0;
let currentCct = 0;
let rawR = 0, rawG = 0, rawB = 0, rawC = 0;
let activeGain = 4;
let activeIt = 11.12;

let activeSurvey = false;
let surveyPoints = [];
let chartInstance = null;

// DOM Elements cache
const btnConnect = document.getElementById('btn-connect');
const btnDisconnect = document.getElementById('btn-disconnect');
const connBadge = document.getElementById('conn-badge');
const valRssi = document.getElementById('val-rssi');
const valGain = document.getElementById('val-gain');
const valCct = document.getElementById('val-cct');
const txtLux = document.getElementById('txt-lux');
const gaugeFill = document.getElementById('gauge-fill');

const roomSelect = document.getElementById('room-select');
const roomArea = document.getElementById('room-area');
const btnStartSurvey = document.getElementById('btn-start-survey');
const btnResetSurvey = document.getElementById('btn-reset-survey');
const loggerPanel = document.getElementById('logger-panel');
const pointLabel = document.getElementById('point-label');
const btnAddPoint = document.getElementById('btn-add-point');
const pointsList = document.getElementById('points-list');
const emptyTableRow = document.getElementById('empty-table-row');

const reportPanel = document.getElementById('report-panel');
const statAvg = document.getElementById('stat-avg');
const statUnif = document.getElementById('stat-unif');
const statCompliance = document.getElementById('stat-compliance');
const unifFeedback = document.getElementById('unif-feedback');
const lumenFeedback = document.getElementById('lumen-feedback');
const darkSpotTitle = document.getElementById('dark-spot-title');
const darkSpotsList = document.getElementById('dark-spots-list');
const btnExportCsv = document.getElementById('btn-export-csv');
const btnFinalizeSurvey = document.getElementById('btn-finalize-survey');

// Initialize on page load
window.addEventListener('DOMContentLoaded', () => {
  initChart();
  restoreSurveyFromCache();
  setupEventListeners();
});

// Setup Listeners
function setupEventListeners() {
  btnConnect.addEventListener('click', connectBLE);
  btnDisconnect.addEventListener('click', disconnectBLE);
  
  btnStartSurvey.addEventListener('click', startSurveySession);
  btnResetSurvey.addEventListener('click', resetSurveySession);
  btnAddPoint.addEventListener('click', recordMeasurementPoint);
  btnExportCsv.addEventListener('click', exportSurveyToCsv);
  btnFinalizeSurvey.addEventListener('click', finalizeSurveySession);

  // Auto-save changes in inputs if survey is running
  roomArea.addEventListener('input', () => {
    if (activeSurvey) {
      saveSurveyToCache();
      updateAnalysisReport();
    }
  });

  roomSelect.addEventListener('change', () => {
    if (activeSurvey) {
      // Sync room target change
      sendControlCommand(`START:${roomSelect.value}`);
      saveSurveyToCache();
      updateAnalysisReport();
    }
  });

  // Enter key support for logger input
  pointLabel.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') {
      recordMeasurementPoint();
    }
  });
}

/* ==========================================================================
   Web Bluetooth Implementation
   ========================================================================== */

async function connectBLE() {
  updateStatusBadge("Connecting...", "connecting");
  btnConnect.disabled = true;

  try {
    #if DEBUG
    console.log("Requesting Bluetooth device...");
    #endif
    
    bleDevice = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }],
      optionalServices: [SERVICE_UUID]
    });

    bleDevice.addEventListener('gattserverdisconnected', onDisconnected);

    #if DEBUG
    console.log("Connecting to GATT server...");
    #endif
    bleServer = await bleDevice.gatt.connect();

    #if DEBUG
    console.log("Getting primary service...");
    #endif
    bleService = await bleServer.getPrimaryService(SERVICE_UUID);

    #if DEBUG
    console.log("Getting characteristics...");
    #endif
    luxChar = await bleService.getCharacteristic(LUX_CHAR_UUID);
    surveyChar = await bleService.getCharacteristic(SURVEY_CHAR_UUID);
    controlChar = await bleService.getCharacteristic(CONTROL_CHAR_UUID);

    // Subscribe to notifications
    await luxChar.startNotifications();
    luxChar.addEventListener('characteristicvaluechanged', handleLuxNotification);

    await surveyChar.startNotifications();
    surveyChar.addEventListener('characteristicvaluechanged', handleSurveyNotification);

    updateStatusBadge("Connected", "connected");
    btnConnect.classList.add('hidden');
    btnDisconnect.classList.remove('hidden');
    document.getElementById('connection-meta').classList.remove('hidden');
    
    #if DEBUG
    console.log("BLE Connection Successful!");
    #endif

    // Sync current survey state if already running
    if (activeSurvey) {
      sendControlCommand(`START:${roomSelect.value}`);
    }

  } catch (error) {
    console.error("Bluetooth connection failed:", error);
    updateStatusBadge("Disconnected", "disconnected");
    btnConnect.disabled = false;
  }
}

function onDisconnected() {
  updateStatusBadge("Disconnected", "disconnected");
  btnConnect.classList.remove('hidden');
  btnConnect.disabled = false;
  btnDisconnect.classList.add('hidden');
  document.getElementById('connection-meta').classList.add('hidden');
  
  luxChar = null;
  surveyChar = null;
  controlChar = null;
  bleServer = null;
  bleDevice = null;
}

function disconnectBLE() {
  if (bleDevice && bleDevice.gatt.connected) {
    bleDevice.gatt.disconnect();
  }
}

function updateStatusBadge(text, type) {
  connBadge.textContent = text;
  connBadge.className = `status-badge ${type}`;
}

// Handle Real-time lux updates
function handleLuxNotification(event) {
  const value = event.target.value;
  // Read Float32 (little endian)
  currentLux = value.getFloat32(0, true);
  
  if (currentLux < 0) currentLux = 0.0;
  
  updateRealtimeLuxDisplay(currentLux);
}

// Handle Survey stats updates (sync from Arduino)
function handleSurveyNotification(event) {
  const value = event.target.value;
  const decoder = new TextDecoder('utf-8');
  const jsonStr = decoder.decode(value);

  try {
    const data = JSON.parse(jsonStr);
    
    // Sync settings if they are reported
    if (data.gain) {
      activeGain = data.gain;
      valGain.textContent = `${activeGain}x`;
    }
    // RSSI can be estimated dynamically, mock here since Web BLE doesn't yield RSSI directly
    valRssi.textContent = "-55 dBm";
  } catch (e) {
    // String might contain other raw format
  }
}

// Send control commands back to Arduino
async function sendControlCommand(cmdString) {
  if (!controlChar) return;
  try {
    const encoder = new TextEncoder();
    await controlChar.writeValue(encoder.encode(cmdString));
    #if DEBUG
    console.log("Sent command:", cmdString);
    #endif
  } catch (e) {
    console.error("Failed to send command:", e);
  }
}

/* ==========================================================================
   Real-Time Visualizations (Gauge & Chart)
   ========================================================================== */

function updateRealtimeLuxDisplay(lux) {
  // Update gauge text
  txtLux.textContent = lux.toFixed(1);

  // Update CCT approximation on screen
  // (We mock CCT values lightly in JS based on color parameters or sensor updates,
  // since Arduino does McCamy math we can display it)
  // Let's estimate CCT based on a typical indoor range if BLE hasn't sent color temp yet.
  if (currentCct === 0) {
    // Default mock CCT range
    let estCct = 2700 + Math.min(lux * 3, 3800);
    valCct.textContent = `${Math.round(estCct)} K`;
  }

  // Update gauge circular fill stroke dashoffset
  // Arc length is 251.2 (for half circle). Lux range capped at 1000 for gauge visualization.
  const maxScale = 1000.0;
  const percentage = Math.min(lux / maxScale, 1.0);
  const offset = 251.2 * (1.0 - percentage);
  gaugeFill.style.strokeDashoffset = offset;

  // Change gauge color dynamically based on illumination level
  let strokeColor = '#f59e0b'; // Dim: Amber
  if (lux >= 500) {
    strokeColor = '#f1f5f9'; // Very Bright: Soft white
  } else if (lux >= 300) {
    strokeColor = '#06b6d4'; // Bright/Optimal: Cyan
  } else if (lux >= 100) {
    strokeColor = '#10b981'; // Moderate: Emerald
  }
  gaugeFill.style.stroke = strokeColor;

  // Add lux to scrolling chart data
  if (chartInstance) {
    const labels = chartInstance.data.labels;
    const dataset = chartInstance.data.datasets[0].data;

    const timeLabel = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    labels.push(timeLabel);
    dataset.push(lux);

    // Cap chart dataset at 20 elements
    if (labels.length > 20) {
      labels.shift();
      dataset.shift();
    }

    chartInstance.update('none'); // Update without full animations for smooth performance
  }
}

function initChart() {
  const ctx = document.getElementById('luxChart').getContext('2d');
  
  chartInstance = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        label: 'Light Level (Lux)',
        data: [],
        borderColor: '#06b6d4',
        borderWidth: 2,
        backgroundColor: 'rgba(6, 182, 212, 0.05)',
        fill: true,
        tension: 0.3,
        pointRadius: 0
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { display: false }
      },
      scales: {
        x: {
          grid: { display: false },
          ticks: { display: false }
        },
        y: {
          min: 0,
          grid: { color: 'rgba(255, 255, 255, 0.04)' },
          ticks: {
            color: '#64748b',
            font: { size: 9 }
          }
        }
      }
    }
  });
}

/* ==========================================================================
   Survey Logger & Recommendations Engine
   ========================================================================== */

function startSurveySession() {
  activeSurvey = true;
  surveyPoints = [];
  
  // Set UI views
  btnStartSurvey.classList.add('hidden');
  btnResetSurvey.classList.remove('hidden');
  loggerPanel.classList.remove('hidden');
  
  // Clear table rows
  renderPointsList();

  // Sync to Arduino
  sendControlCommand(`START:${roomSelect.value}`);

  saveSurveyToCache();
}

function recordMeasurementPoint() {
  if (!activeSurvey) return;

  let label = pointLabel.value.trim();
  if (label === "") {
    // Generate placeholder label
    label = `Point ${surveyPoints.length + 1}`;
  }

  const newPoint = {
    label: label,
    lux: parseFloat(currentLux.toFixed(1)),
    timestamp: new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
  };

  surveyPoints.push(newPoint);
  pointLabel.value = ""; // Reset input

  // Sync to Arduino
  sendControlCommand(`ADD:${label}`);

  renderPointsList();
  updateAnalysisReport();
  saveSurveyToCache();

  // Scroll table down
  const tableContainer = document.querySelector('.table-container');
  tableContainer.scrollTop = tableContainer.scrollHeight;
}

function deleteMeasurementPoint(index) {
  surveyPoints.splice(index, 1);
  renderPointsList();
  updateAnalysisReport();
  saveSurveyToCache();
}

function renderPointsList() {
  pointsList.innerHTML = "";

  if (surveyPoints.length === 0) {
    pointsList.appendChild(emptyTableRow);
    reportPanel.classList.add('hidden');
    return;
  }

  const targetLux = ROOM_STANDARDS[roomSelect.value].targetLux;

  surveyPoints.forEach((pt, index) => {
    const row = document.createElement('tr');
    
    // Status Badge calculation
    let badgeClass = "badge-green";
    let badgeText = "Pass";
    
    if (pt.lux < targetLux * 0.7) {
      badgeClass = "badge-red";
      badgeText = "Low";
    } else if (pt.lux < targetLux) {
      badgeClass = "badge-amber";
      badgeText = "Marginal";
    }

    row.innerHTML = `
      <td><strong>${pt.label}</strong> <br><small class="text-muted">${pt.timestamp}</small></td>
      <td>${pt.lux.toFixed(1)} Lux</td>
      <td><span class="badge ${badgeClass}">${badgeText}</span></td>
      <td><button class="btn-delete" data-index="${index}">Delete</button></td>
    `;
    
    // Row delete click binding
    row.querySelector('.btn-delete').addEventListener('click', () => {
      deleteMeasurementPoint(index);
    });

    pointsList.appendChild(row);
  });
}

function updateAnalysisReport() {
  if (surveyPoints.length < 2) {
    reportPanel.classList.add('hidden');
    return;
  }
  
  reportPanel.classList.remove('hidden');

  const standard = ROOM_STANDARDS[roomSelect.value];
  const target = standard.targetLux;
  const area = parseFloat(roomArea.value) || 1.0;

  // Calculat statistics
  let sum = 0;
  let min = surveyPoints[0].lux;
  let max = surveyPoints[0].lux;

  surveyPoints.forEach(p => {
    sum += p.lux;
    if (p.lux < min) min = p.lux;
    if (p.lux > max) max = p.lux;
  });

  const avg = sum / surveyPoints.length;
  const uniformity = min / avg;

  // Render stats boxes
  statAvg.textContent = `${avg.toFixed(1)} Lux`;
  statUnif.textContent = uniformity.toFixed(2);

  // Overall compliance check
  const meetsAvg = avg >= target;
  const meetsUnif = uniformity >= 0.40;
  const passed = meetsAvg && meetsUnif;

  if (passed) {
    statCompliance.textContent = "PASSED";
    statCompliance.className = "stat-badge badge-green";
  } else {
    statCompliance.textContent = "FAILED";
    statCompliance.className = "stat-badge badge-red";
  }

  // Uniformity analysis feedback
  if (meetsUnif) {
    unifFeedback.innerHTML = `Lighting uniformity is <strong>compliant (U = ${uniformity.toFixed(2)} &ge; 0.40)</strong>. This means illumination is spread evenly across the room layout without stark shadowed areas.`;
  } else {
    unifFeedback.innerHTML = `Lighting uniformity is <strong>poor (U = ${uniformity.toFixed(2)} &lt; 0.40)</strong>. This indicates uneven distribution, causing high-contrast shadows. Consider spacing ceiling luminaires more evenly or choosing wider-angle diffusers.`;
  }

  // Lumen deficit recommendations
  const targetLumens = target * area;
  const currentLumens = avg * area;
  const deficit = targetLumens - currentLumens;

  if (deficit <= 0) {
    lumenFeedback.innerHTML = `The room average illuminance of <strong>${avg.toFixed(1)} Lux</strong> meets the <strong>${target} Lux</strong> target. The current lighting layout is sufficient.`;
  } else {
    // Bulb calculation helpers
    const ledBulbs = Math.ceil(deficit / 800);
    const spots = Math.ceil(deficit / 400);

    lumenFeedback.innerHTML = `
      This space has a <strong>lumen deficit of ${Math.round(deficit)} lumens</strong> to meet the CIBSE room target of ${target} Lux.<br>
      To satisfy this requirement, consider installing either:<br>
      - <strong>${ledBulbs}x</strong> standard 800-lumen LED light bulbs (e.g. 9.5W A19), or<br>
      - <strong>${spots}x</strong> 400-lumen spotlight fixtures or track nodes.
    `;
  }

  // Dark spots remedies checklist
  darkSpotsList.innerHTML = "";
  let darkSpotsCount = 0;

  surveyPoints.forEach(pt => {
    if (pt.lux < target * 0.70) {
      darkSpotsCount++;
      const li = document.createElement('li');
      li.innerHTML = `
        <strong>${pt.label}</strong> is under-illuminated (only <strong>${pt.lux.toFixed(1)} Lux</strong>, which is below 70% of target).<br>
        <span class="text-muted">Recommendation: Place a task lamp (e.g. table lamp, under-cabinet light strip, or localized ceiling spotlight) here.</span>
      `;
      darkSpotsList.appendChild(li);
    }
  });

  if (darkSpotsCount > 0) {
    darkSpotTitle.classList.remove('hidden');
  } else {
    darkSpotTitle.classList.add('hidden');
  }
}

function finalizeSurveySession() {
  activeSurvey = false;
  btnStartSurvey.classList.remove('hidden');
  btnResetSurvey.classList.add('hidden');
  loggerPanel.classList.add('hidden');

  // Sync to Arduino
  sendControlCommand("END");

  // Keep report panel open to display finalized analysis
  saveSurveyToCache();
}

function resetSurveySession() {
  if (confirm("Are you sure you want to discard the current survey data?")) {
    activeSurvey = false;
    surveyPoints = [];
    
    btnStartSurvey.classList.remove('hidden');
    btnResetSurvey.classList.add('hidden');
    loggerPanel.classList.add('hidden');
    reportPanel.classList.add('hidden');
    
    // Clear list
    renderPointsList();

    // Sync to Arduino
    sendControlCommand("RESET");

    localStorage.removeItem('nightjar_active_survey');
  }
}

/* ==========================================================================
   Persistence & CSV Exports
   ========================================================================== */

function saveSurveyToCache() {
  const dataToSave = {
    activeSurvey: activeSurvey,
    roomType: roomSelect.value,
    roomArea: roomArea.value,
    points: surveyPoints
  };
  localStorage.setItem('nightjar_active_survey', JSON.stringify(dataToSave));
}

function restoreSurveyFromCache() {
  const saved = localStorage.getItem('nightjar_active_survey');
  if (saved) {
    try {
      const parsed = JSON.parse(saved);
      activeSurvey = parsed.activeSurvey;
      roomSelect.value = parsed.roomType;
      roomArea.value = parsed.roomArea;
      surveyPoints = parsed.points || [];

      if (activeSurvey) {
        btnStartSurvey.classList.add('hidden');
        btnResetSurvey.classList.remove('hidden');
        loggerPanel.classList.remove('hidden');
      }

      renderPointsList();
      updateAnalysisReport();

    } catch (e) {
      console.error("Failed to restore survey cache:", e);
    }
  }
}

function exportSurveyToCsv() {
  if (surveyPoints.length === 0) return;

  const roomName = ROOM_STANDARDS[roomSelect.value].name;
  const targetLux = ROOM_STANDARDS[roomSelect.value].targetLux;
  
  // Calculate final stats
  let sum = 0;
  let min = surveyPoints[0].lux;
  surveyPoints.forEach(p => {
    sum += p.lux;
    if (p.lux < min) min = p.lux;
  });
  const avg = sum / surveyPoints.length;
  const uniformity = min / avg;
  const compliance = (avg >= targetLux && uniformity >= 0.4) ? "PASSED" : "FAILED";

  // Build CSV content
  let csvRows = [];
  csvRows.push("NIGHTJAR TALON LIGHTING SURVEY REPORT");
  csvRows.push(`Timestamp,${new Date().toLocaleString()}`);
  csvRows.push("");
  csvRows.push("ROOM OVERVIEW");
  csvRows.push(`Room Type,${roomName}`);
  csvRows.push(`Room Area (sqm),${roomArea.value}`);
  csvRows.push(`Target Illuminance,${targetLux} Lux`);
  csvRows.push(`Average Illuminance,${avg.toFixed(1)} Lux`);
  csvRows.push(`Uniformity Ratio (U),${uniformity.toFixed(2)}`);
  csvRows.push(`Compliance Status,${compliance}`);
  csvRows.push("");
  csvRows.push("DETAILED MEASUREMENTS");
  csvRows.push("Location,Illuminance (Lux),Compliance");

  surveyPoints.forEach(p => {
    const pointStatus = p.lux >= targetLux ? "PASS" : (p.lux < targetLux * 0.7 ? "LOW" : "MARGINAL");
    csvRows.push(`"${p.label}",${p.lux.toFixed(1)},${pointStatus}`);
  });

  const csvString = csvRows.join("\n");
  
  // Triger download
  const blob = new Blob([csvString], { type: 'text/csv;charset=utf-8;' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.setAttribute("href", url);
  link.setAttribute("download", `Lux_Survey_${roomSelect.value}_${new Date().toISOString().slice(0,10)}.csv`);
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
}
