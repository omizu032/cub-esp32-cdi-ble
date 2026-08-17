<template>
  <div class="cdi-dashboard">
    <header class="header">
      <div class="title-area">
        <h1>CDI Tuning Dashboard</h1>
        <p class="subtitle">ESP32 Wireless Engine Management</p>
      </div>
      <button @click="!isConnected && !isOtaReconnecting ? connectBLE() : null" 
              :class="['connect-btn', { connected: isConnected, reconnecting: isOtaReconnecting }]">
        {{ isOtaReconnecting ? 'Reconnecting...' : (isConnected ? 'BLE Connected' : 'Connect to ESP32') }}
      </button>
    </header>

    <div class="view-selector">
      <select v-model="currentView" class="view-select">
        <option value="telemetry">📊 Live Telemetry</option>
        <option value="mapEditor">📈 Ignition Map Editor</option>
        <option value="profile">📁 Profile Management</option>
        <option value="safety">⚙️ Configuration</option>
      </select>
    </div>

    <div class="dashboard-grid">
      <!-- テレメトリパネル -->
      <section v-if="currentView === 'telemetry'" class="panel telemetry-panel dark-mode">
        <h2>Live Telemetry</h2>
        
        <div class="combined-gauge-wrapper">
          <svg viewBox="-15 -15 130 110" class="analog-gauge">
            <circle cx="50" cy="50" r="40" class="gauge-bg" />
            <circle cx="50" cy="50" r="40" class="gauge-val" 
                    stroke="#28a745" 
                    :stroke-dashoffset="188.5 * (1 - advRatio)" />

            <g transform="translate(50, 50)">
              <g class="adv-ticks">
                <template v-for="tick in advTicks" :key="'advtick'+tick.val">
                  <line 
                    x1="0" :y1="tick.isMajor ? -44 : -45" 
                    x2="0" :y2="tick.isMajor ? -49 : -48"
                    :stroke="tick.isCenter ? '#28a745' : '#888'" 
                    :stroke-width="tick.isMajor ? 1.5 : 0.5"
                    :transform="`rotate(${tick.angle})`" 
                  />
                  <text v-if="tick.isLabel"
                        :x="56 * Math.sin(tick.angle * Math.PI / 180)"
                        :y="-56 * Math.cos(tick.angle * Math.PI / 180) + 2.5"
                        class="tick-label-adv"
                        :fill="tick.isCenter ? '#28a745' : '#999'"
                        :font-weight="tick.isCenter ? 'bold' : 'normal'">
                    {{ tick.val }}
                  </text>
                </template>
              </g>

              <g class="rpm-ticks">
                <template v-for="tick in rpmTicks" :key="'rpmtick'+tick.val">
                  <line 
                    x1="0" :y1="tick.isMajor ? -31 : -33" 
                    x2="0" y2="-36"
                    stroke="#666" :stroke-width="tick.isMajor ? 1.5 : 0.5"
                    :transform="`rotate(${tick.angle})`" 
                  />
                  <text v-if="tick.isLabel"
                        :x="24 * Math.sin(tick.angle * Math.PI / 180)"
                        :y="-24 * Math.cos(tick.angle * Math.PI / 180) + 2"
                        class="tick-label-rpm">
                    {{ tick.val / 1000 }}
                  </text>
                </template>
              </g>

              <polygon points="-2,0 2,0 0,-38" class="gauge-needle" 
                       :style="{ transform: `rotate(${225 + (270 * rpmRatio)}deg)` }"
                       :fill="rpmColor" />
              <circle cx="0" cy="0" r="4" fill="#111" :stroke="rpmColor" stroke-width="1.5" />
            </g>
          </svg>
        </div>

        <div class="digital-readouts">
          <div class="readout-item">
            <div class="readout-val" :style="{ color: isConnected ? rpmColor : '#555' }">
              {{ isConnected ? currentRpm : '----' }}
            </div>
            <div class="readout-unit">RPM</div>
          </div>
          <div class="readout-item">
            <div class="readout-val" :style="{ color: isConnected ? '#28a745' : '#555' }">
              {{ isConnected ? (currentAdvance / 10).toFixed(1) : '--.-' }}
            </div>
            <div class="readout-unit">DEG (ADV)</div>
          </div>
        </div>

        <div class="engine-status" :class="{ unknown: !isConnected, kill: isConnected && !isRunState, revlimit: isRevLimitActive }">
          <span class="status-label">STATUS:</span>
          <span class="status-text" v-if="!isConnected">OFFLINE</span>
          <span class="status-text revlimit-text" v-else-if="isRevLimitActive">REV LIMITER ACTIVE!</span>
          <span class="status-text" v-else-if="isRunState">RUN (ACTIVE)</span>
          <span class="status-text" v-else>KILL (IGNITION CUT)</span>
        </div>
      </section>

      <!-- マップエディタパネル -->
      <section v-if="currentView === 'mapEditor'" class="panel map-panel">
        <div class="panel-header">
          <h2>Ignition Map Editor</h2>
          <button @click="writeMapToESP32" :disabled="!isConnected" class="action-btn">
            Save to CDI Flash
          </button>
        </div>

        <div v-if="!isConnected" class="offline-overlay">
          <p>Please connect to ESP32 to load the current map.</p>
        </div>

        <div class="safety-notice">
          ※ Advance Limit: Max {{ GRAPH.maxDeg }} degrees. (Hardware Clamped)
        </div>

        <div class="graph-container" ref="graphContainer" :class="{ 'disabled-graph': !isConnected }">
          <svg class="interactive-graph" viewBox="0 0 600 590" 
            @mousemove="onDrag" @mouseup="endDrag" @mouseleave="endDrag"
            @touchmove.prevent="onDrag" @touchend="endDrag">
            <g class="grid-lines">
              <line v-for="y in gridY" :key="'gy'+y.val" x1="50" :y1="y.pos" x2="580" :y2="y.pos" />
              <text v-for="y in gridY" :key="'ty'+y.val" x="40" :y="y.pos + 4" class="axis-label y-axis">{{ y.val }}°</text>
              <line v-for="x in points" :key="'gx'+x.rpm" :x1="x.cx" y1="20" :x2="x.cx" y2="550" />
              <text v-for="x in points" :key="'tx'+x.rpm" :x="x.cx" y="570" class="axis-label x-axis">{{ x.rpm }}</text>
            </g>
            <polyline :points="polylinePoints" class="graph-line" />
            <g class="data-points">
              <circle v-for="(pt, index) in points" :key="'pt'+index" :cx="pt.cx" :cy="pt.cy" r="6" 
                class="draggable-point" :class="{ active: draggingIndex === index }"
                @mousedown="startDrag(index)" @touchstart.prevent="startDrag(index)" />
            </g>
          </svg>
        </div>

        <div class="map-list">
          <div v-for="(deg, index) in ignitionMap" :key="'input'+index" class="map-list-item">
            <div class="rpm-label">{{ (index + 1) * 1000 }} <span class="unit">rpm</span></div>
            <div class="spin-control">
              <button @click="decrementPoint(index)" :disabled="!isConnected" class="spin-btn minus">▼</button>
              <input type="text" inputmode="decimal"
                :value="(deg / 10).toFixed(1)" @change="updateMapPoint(index, $event.target.value)"
                :disabled="!isConnected" class="map-input-value" />
              <button @click="incrementPoint(index)" :disabled="!isConnected" class="spin-btn plus">▲</button>
            </div>
          </div>
        </div>
      </section>

      <!-- プロファイル管理パネル -->
      <section v-if="currentView === 'profile'" class="panel profile-panel">
        <div class="panel-header"><h2>Profile Management</h2></div>
        
        <div class="profile-actions-row">
          <input type="text" v-model="newProfileName" placeholder="Name (e.g., Street Eco)" class="profile-input" />
          <button @click="saveProfile" class="action-btn">Save Current Map</button>
        </div>

        <div class="profile-list" v-if="profiles.length > 0">
          <div class="profile-item" v-for="p in profiles" :key="p.id">
            <span class="profile-name">{{ p.name }}</span>
            <div class="profile-buttons">
              <button @click="loadProfile(p)" class="action-btn small">Load</button>
              <button @click="deleteProfile(p.id)" class="action-btn small danger">Delete</button>
            </div>
          </div>
        </div>
        <div v-else class="empty-list">No profiles saved in browser.</div>

        <div class="import-export-row">
          <button @click="exportProfile" class="action-btn outline">Export JSON</button>
          <input type="file" ref="fileInput" @change="importProfile" accept=".json" style="display:none" />
          <button @click="$refs.fileInput.click()" class="action-btn outline">Import JSON</button>
        </div>
      </section>

      <!-- Configuration パネル -->
      <section v-if="currentView === 'safety'" class="panel safety-panel">
        
        <!-- デバイス情報 -->
        <div class="panel-header"><h2>Device Information</h2></div>
        <div class="device-info-container">
          <div class="info-row">
            <span class="info-label">Firmware Version:</span>
            <span class="info-value">v{{ fwVersion }}</span>
          </div>
          <div class="info-row">
            <span class="info-label">Build Timestamp:</span>
            <span class="info-value">{{ formattedBuildTimestamp }}</span>
          </div>
        </div>

        <hr class="divider" />

        <!-- レブリミッター設定 -->
        <div class="panel-header"><h2>Engine Rev Limiter</h2></div>
        <div class="rev-limit-container">
          <p class="desc">Set the maximum engine speed. The ignition will be cut off if this RPM is exceeded.</p>
          <div class="slider-group">
            <input type="range" min="4000" max="12000" step="100" v-model.number="revLimitRpm" :disabled="!isConnected" class="rev-slider" />
            <div class="rev-val">{{ revLimitRpm }} <span class="unit">RPM</span></div>
          </div>
          <button @click="writeConfigToESP32" :disabled="!isConnected" class="action-btn danger outline full-width">
            Apply Rev Limit to CDI
          </button>
        </div>

        <hr class="divider" />

        <!-- OTA ファームウェアアップデート -->
        <div class="panel-header"><h2>Over-The-Air (OTA) Update</h2></div>
        <div class="ota-container">
          <p class="desc">Flash a new <code>.bin</code> firmware wirelessly to the ESP32.</p>
          
          <div class="file-upload-box">
            <input type="file" @change="onFirmwareSelected" accept=".bin" :disabled="!isConnected || otaInProgress || isOtaReconnecting" />
          </div>

          <div v-if="otaInProgress || isOtaReconnecting" class="progress-box">
            <div class="progress-bar">
              <div class="progress-fill" :style="{ width: otaProgress + '%' }"></div>
            </div>
            <p v-if="isOtaReconnecting" style="color: #ff9800;">CDI is Rebooting... Please wait.</p>
            <p v-else>{{ otaProgress }}% Flashed...</p>
          </div>

          <button @click="startOTAUpdate" :disabled="!isConnected || !selectedFirmware || otaInProgress || isOtaReconnecting" class="action-btn full-width">
            {{ isOtaReconnecting ? 'Reconnecting...' : (otaInProgress ? 'Flashing Firmware...' : 'Start OTA Update') }}
          </button>
        </div>
      </section>

    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue';

const SERVICE_UUID         = "12345678-1234-5678-1234-56789abcdef0";
const TELEMETRY_UUID       = "12345678-1234-5678-1234-56789abcdef1";
const MAP_UUID             = "12345678-1234-5678-1234-56789abcdef2";
const CONFIG_UUID          = "12345678-1234-5678-1234-56789abcdef3";
const OTA_CTRL_UUID        = "12345678-1234-5678-1234-56789abcdef4";
const OTA_DATA_UUID        = "12345678-1234-5678-1234-56789abcdef5";
const INFO_UUID            = "12345678-1234-5678-1234-56789abcdef6"; 

const isConnected = ref(false);
const currentView = ref('telemetry');
const currentRpm = ref(0);
const currentAdvance = ref(0);
const isRunState = ref(false);
const isRevLimitActive = ref(false);

const ignitionMap = ref([0, 0, 0, 0, 0, 0, 0, 0, 0]);
const revLimitRpm = ref(10000);

const fwVersion = ref('---');
const buildTimestamp = ref('---');

let bleDevice = null;
let mapCharacteristic = null;
let telemetryCharacteristic = null;
let configCharacteristic = null;
let otaCtrlCharacteristic = null;
let otaDataCharacteristic = null;
let infoCharacteristic = null; 

const graphContainer = ref(null);
const draggingIndex = ref(-1);

const GRAPH = { width: 600, height: 590, padX: 50, padRight: 20, padYTop: 20, padYBottom: 40, minDeg: 4, maxDeg: 45 };

const formattedBuildTimestamp = computed(() => {
  const ts = buildTimestamp.value;
  if (ts === '---' || ts.length !== 14) return ts;
  return `${ts.slice(0,4)}/${ts.slice(4,6)}/${ts.slice(6,8)} ${ts.slice(8,10)}:${ts.slice(10,12)}:${ts.slice(12,14)}`;
});

const MAX_RPM_GAUGE = 10000; 

const rpmRatio = computed(() => {
  if (!isConnected.value) return 0;
  return Math.max(0, Math.min(currentRpm.value / MAX_RPM_GAUGE, 1));
});

const advRatio = computed(() => {
  if (!isConnected.value) return 0;
  return Math.max(0, Math.min(( (currentAdvance.value / 10) - GRAPH.minDeg ) / (GRAPH.maxDeg - GRAPH.minDeg), 1));
});

const rpmColor = computed(() => {
  return currentRpm.value >= (revLimitRpm.value - 500) ? '#ff4757' : '#00d2ff';
});

const rpmTicks = computed(() => {
  const ticks = [];
  for (let i = 0; i <= MAX_RPM_GAUGE; i += 500) {
    ticks.push({
      val: i,
      angle: 225 + (270 * (i / MAX_RPM_GAUGE)),
      isMajor: i % 1000 === 0,
      isLabel: i % 3000 === 0
    });
  }
  return ticks;
});

const advTicks = computed(() => {
  const ticks = [];
  const min = GRAPH.minDeg; 
  const max = GRAPH.maxDeg; 
  const range = max - min; 

  for (let i = 5; i <= max; i += 5) {
    ticks.push({
      val: i,
      angle: 225 + (270 * ((i - min) / range)),
      isMajor: i % 10 === 0,
      isLabel: i % 10 === 0,
      isCenter: false
    });
  }
  ticks.push({ val: 27, angle: 225 + (270 * ((27 - min) / range)), isMajor: true, isLabel: true, isCenter: true });
  return ticks;
});

const incrementPoint = (index) => {
  if (!isConnected.value) return;
  let newVal = ignitionMap.value[index] + 1;
  if (newVal > GRAPH.maxDeg * 10) newVal = GRAPH.maxDeg * 10;
  ignitionMap.value[index] = newVal;
};
const decrementPoint = (index) => {
  if (!isConnected.value) return;
  let newVal = ignitionMap.value[index] - 1;
  if (newVal < GRAPH.minDeg * 10) newVal = GRAPH.minDeg * 10;
  ignitionMap.value[index] = newVal;
};
const updateMapPoint = (index, valueStr) => {
  let valueDeg = parseFloat(valueStr);
  if (!isNaN(valueDeg)) {
    if (valueDeg > GRAPH.maxDeg) valueDeg = GRAPH.maxDeg;
    if (valueDeg < GRAPH.minDeg) valueDeg = GRAPH.minDeg;
    ignitionMap.value[index] = Math.round(valueDeg * 10);
  }
};

// ==========================================
// OTA (Over-The-Air) アップデートロジック (自動再接続改修)
// ==========================================
const selectedFirmware = ref(null);
const otaInProgress = ref(false);
const otaProgress = ref(0);
const isOtaReconnecting = ref(false); // 再接続待機フラグ

const onFirmwareSelected = (event) => {
  selectedFirmware.value = event.target.files[0];
};

const startOTAUpdate = async () => {
  if (!selectedFirmware.value || !otaCtrlCharacteristic || !otaDataCharacteristic) return;
  
  const confirmOTA = confirm('Are you sure you want to update the firmware? DO NOT turn off the engine/power during this process.');
  if (!confirmOTA) return;

  otaInProgress.value = true;
  otaProgress.value = 0;

  try {
    const buffer = await selectedFirmware.value.arrayBuffer();
    const totalSize = buffer.byteLength;
    
    // 1. Start Command
    const startCmd = new Uint8Array(5);
    startCmd[0] = 0x01;
    startCmd[1] = (totalSize >> 24) & 0xFF;
    startCmd[2] = (totalSize >> 16) & 0xFF;
    startCmd[3] = (totalSize >> 8) & 0xFF;
    startCmd[4] = totalSize & 0xFF;
    await otaCtrlCharacteristic.writeValueWithResponse(startCmd);

    // 2. Transmit Data Chunks
    const CHUNK_SIZE = 200; 
    let offset = 0;

    while (offset < totalSize) {
      const chunk = buffer.slice(offset, offset + CHUNK_SIZE);
      await otaDataCharacteristic.writeValueWithoutResponse(chunk);
      offset += chunk.byteLength;
      await new Promise(r => setTimeout(r, 15)); 
      otaProgress.value = Math.floor((offset / totalSize) * 100);
    }

    // 3. End & Restart Command (エラーを無視して自動再接続フローへ)
    isOtaReconnecting.value = true; 
    try {
      const endCmd = new Uint8Array([0x02]);
      await otaCtrlCharacteristic.writeValueWithResponse(endCmd);
    } catch (e) {
      // 成功してもESP32が即座に再起動し通信が切断されるため、ここでエラーが出るのは「正常」です。
      console.log('Expected disconnect during OTA restart:', e);
    }

    // UIを再接続待機モードに切り替え
    otaInProgress.value = false;
    otaProgress.value = 100;

    // 5秒待機（ESP32の再起動とBLEアドバタイズ開始を待つ）
    setTimeout(async () => {
      try {
        if (bleDevice) {
          const server = await bleDevice.gatt.connect();
          await setupBleDevice(server);
          alert(`OTA Update Successful!\nReconnected to Firmware v${fwVersion.value}`);
        }
      } catch (err) {
        console.error("Auto-reconnect failed:", err);
        alert('Firmware flashed successfully, but auto-reconnect failed. Please click "Connect to ESP32" manually.');
      } finally {
        isOtaReconnecting.value = false;
      }
    }, 5000);

  } catch (error) {
    console.error('OTA Update Failed:', error);
    alert('OTA Update Failed during transfer. Please reconnect and try again.');
    otaInProgress.value = false;
    isOtaReconnecting.value = false;
  }
};

const profiles = ref([]);
const newProfileName = ref('');
const fileInput = ref(null);
const STORAGE_KEY = 'cdi_profiles';

onMounted(() => {
  const data = localStorage.getItem(STORAGE_KEY);
  if (data) { try { profiles.value = JSON.parse(data); } catch(e) {} }
});

const saveProfilesToStorage = () => { localStorage.setItem(STORAGE_KEY, JSON.stringify(profiles.value)); };

const saveProfile = () => {
  if (!newProfileName.value.trim()) return alert('Please enter a profile name.');
  const newProfile = { id: Date.now(), name: newProfileName.value.trim(), map: [...ignitionMap.value] };
  profiles.value.push(newProfile); saveProfilesToStorage(); newProfileName.value = ''; 
};
const loadProfile = (profile) => {
  if (confirm(`Load profile "${profile.name}" to the editor?`)) {
    ignitionMap.value = [...profile.map]; newProfileName.value = profile.name;
    currentView.value = 'mapEditor';
  }
};
const deleteProfile = (id) => {
  if (confirm('Are you sure you want to delete this profile?')) {
    profiles.value = profiles.value.filter(p => p.id !== id); saveProfilesToStorage();
  }
};
const exportProfile = () => {
  const safeName = (newProfileName.value || 'CDI_Map').replace(/\s+/g, '_');
  const exportData = { name: newProfileName.value || 'CDI_Map', map: ignitionMap.value, exportedAt: new Date().toISOString() };
  const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob); const a = document.createElement('a'); a.href = url; a.download = `cdi_${safeName}_${Date.now()}.json`; a.click(); URL.revokeObjectURL(url);
};
const importProfile = (event) => {
  const file = event.target.files[0]; if (!file) return;
  const reader = new FileReader();
  reader.onload = (e) => {
    try {
      const data = JSON.parse(e.target.result);
      if (data.map && Array.isArray(data.map) && data.map.length === 9) {
        ignitionMap.value = data.map; if (data.name) newProfileName.value = data.name; alert('Map imported successfully!');
      } else { alert('Invalid map format.'); }
    } catch(err) { alert('Failed to parse JSON file.'); }
  };
  reader.readAsText(file); event.target.value = ''; 
};

const gridY = computed(() => {
  const lines = [];
  for (let d = GRAPH.minDeg; d <= GRAPH.maxDeg; d += (GRAPH.maxDeg - GRAPH.minDeg)/4 ) {
    const ratio = (GRAPH.maxDeg - d) / (GRAPH.maxDeg - GRAPH.minDeg);
    const pos = GRAPH.padYTop + ratio * (GRAPH.height - GRAPH.padYTop - GRAPH.padYBottom); 
    lines.push({ val: Math.round(d), pos });
  }
  return lines;
});
const points = computed(() => {
  const spanX = (GRAPH.width - GRAPH.padX - GRAPH.padRight) / 8;
  const drawHeight = GRAPH.height - GRAPH.padYTop - GRAPH.padYBottom;
  return ignitionMap.value.map((val, index) => {
    const deg = val / 10; let ratioY = (GRAPH.maxDeg - deg) / (GRAPH.maxDeg - GRAPH.minDeg); ratioY = Math.max(0, Math.min(1, ratioY)); 
    return { index, rpm: (index + 1) * 1000, cx: GRAPH.padX + (index * spanX), cy: GRAPH.padYTop + (ratioY * drawHeight), deg: deg };
  });
});
const polylinePoints = computed(() => { return points.value.map(p => `${p.cx},${p.cy}`).join(' '); });
const startDrag = (index) => { if (!isConnected.value) return; draggingIndex.value = index; };
const endDrag = () => { draggingIndex.value = -1; };
const onDrag = (event) => {
  if (draggingIndex.value === -1 || !graphContainer.value) return;
  const rect = graphContainer.value.querySelector('svg').getBoundingClientRect();
  const clientY = event.touches ? event.touches[0].clientY : event.clientY;
  const relativeY = (clientY - rect.top) / rect.height; const svgY = relativeY * GRAPH.height;
  const drawHeight = GRAPH.height - GRAPH.padYTop - GRAPH.padYBottom;
  let ratio = (svgY - GRAPH.padYTop) / drawHeight; ratio = Math.max(0, Math.min(1, ratio)); 
  let newDeg = GRAPH.maxDeg - (ratio * (GRAPH.maxDeg - GRAPH.minDeg)); newDeg = Math.round(newDeg * 2) / 2;
  ignitionMap.value[draggingIndex.value] = Math.round(newDeg * 10);
};

// ==========================================
// BLE 通信ロジック (共通初期化関数の分離)
// ==========================================
const handleDisconnect = () => {
  isConnected.value = false;
  // OTAの自動再接続待機中ではない純粋な切断の場合はUIを初期化
  if (!isOtaReconnecting.value) {
    ignitionMap.value = [0, 0, 0, 0, 0, 0, 0, 0, 0]; 
    fwVersion.value = '---'; 
    buildTimestamp.value = '---'; 
  }
};

const setupBleDevice = async (server) => {
  await new Promise(resolve => setTimeout(resolve, 500)); 
  const service = await server.getPrimaryService(SERVICE_UUID);
  
  telemetryCharacteristic = await service.getCharacteristic(TELEMETRY_UUID);
  mapCharacteristic = await service.getCharacteristic(MAP_UUID);
  configCharacteristic = await service.getCharacteristic(CONFIG_UUID);
  otaCtrlCharacteristic = await service.getCharacteristic(OTA_CTRL_UUID);
  otaDataCharacteristic = await service.getCharacteristic(OTA_DATA_UUID);
  infoCharacteristic = await service.getCharacteristic(INFO_UUID); 

  isConnected.value = true;
  await readMapFromESP32();
  await readConfigFromESP32();
  await readInfoFromESP32(); 
  
  telemetryCharacteristic.removeEventListener('characteristicvaluechanged', handleTelemetryUpdate);
  telemetryCharacteristic.addEventListener('characteristicvaluechanged', handleTelemetryUpdate);
  await telemetryCharacteristic.startNotifications();
};

const connectBLE = async () => {
  try {
    bleDevice = await navigator.bluetooth.requestDevice({ filters: [{ namePrefix: 'CDI-' }], optionalServices: [SERVICE_UUID] });
    bleDevice.addEventListener('gattserverdisconnected', handleDisconnect);
    const server = await bleDevice.gatt.connect();
    await setupBleDevice(server);
  } catch (error) { console.error('[BLE] Error:', error); alert('Connection Failed.'); }
};

const handleTelemetryUpdate = (event) => {
  const dataView = event.target.value; if (!dataView || dataView.byteLength < 5) return;
  currentRpm.value = dataView.getUint16(0, true);         
  currentAdvance.value = dataView.getInt16(2, true);      
  const flags = dataView.getUint8(4);
  isRunState.value = (flags & 0x01) === 1;
  isRevLimitActive.value = (flags & 0x02) === 2;
};

const readInfoFromESP32 = async () => {
  if (!infoCharacteristic) return;
  try {
    const dataView = await infoCharacteristic.readValue();
    const decoder = new TextDecoder('utf-8');
    const infoStr = decoder.decode(dataView);
    const parts = infoStr.split(',');
    if (parts.length === 2) {
      fwVersion.value = parts[0];
      buildTimestamp.value = parts[1];
    }
  } catch (e) { console.error("Info read failed:", e); }
};

const readMapFromESP32 = async () => {
  if (!mapCharacteristic) return;
  try {
    const dataView = await mapCharacteristic.readValue();
    const newMap = []; for (let i = 0; i < 9; i++) { newMap.push(dataView.getInt16(i * 2, true)); }
    ignitionMap.value = newMap; newProfileName.value = "CDI Current Map";
  } catch (e) { console.error("Map read failed:", e); }
};

const writeMapToESP32 = async () => {
  if (!mapCharacteristic || !isConnected.value) return;
  const buffer = new ArrayBuffer(18); const dataView = new DataView(buffer);
  ignitionMap.value.forEach((val, index) => { dataView.setInt16(index * 2, val, true); });
  try { await mapCharacteristic.writeValue(buffer); alert('Ignition Map successfully saved to ESP32 Flash Memory!'); } catch (error) { console.error('Write failed:', error); }
};

const readConfigFromESP32 = async () => {
  if (!configCharacteristic) return;
  try {
    const dataView = await configCharacteristic.readValue();
    revLimitRpm.value = dataView.getUint16(0, true);
  } catch (e) { console.error("Config read failed:", e); }
};

const writeConfigToESP32 = async () => {
  if (!configCharacteristic || !isConnected.value) return;
  const buffer = new ArrayBuffer(2); const dataView = new DataView(buffer);
  dataView.setUint16(0, revLimitRpm.value, true);
  try { await configCharacteristic.writeValue(buffer); alert('Rev Limiter saved to ESP32!'); } catch (error) { console.error('Write failed:', error); }
};

</script>

<style scoped>
*, *::before, *::after { box-sizing: border-box; }
.cdi-dashboard { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 15px; color: #333; width: 100%; overflow-x: hidden; }
.header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; flex-wrap: wrap; gap: 15px; }
.title-area { flex: 1; min-width: 220px; }
.title-area h1 { margin: 0; font-size: 24px; color: #1a1a1a; font-style: italic; line-height: 1.2; } 
.subtitle { margin: 4px 0 0; font-size: 14px; color: #666; }

.connect-btn { padding: 12px 20px; font-size: 15px; font-weight: bold; cursor: pointer; background-color: #007bff; color: white; border: none; border-radius: 8px; transition: all 0.2s; box-shadow: 0 4px 6px rgba(0,123,255,0.2); white-space: nowrap; }
.connect-btn:hover { background-color: #0056b3; transform: translateY(-1px); }
.connect-btn.connected { background-color: #28a745; box-shadow: 0 4px 6px rgba(40,167,69,0.2); }
/* 自動再接続中のボタンスタイル */
.connect-btn.reconnecting { background-color: #ff9800; cursor: wait; box-shadow: 0 4px 6px rgba(255,152,0,0.2); animation: pulse 1.5s infinite; }

.action-btn { padding: 10px 16px; background: #007bff; color: white; border: none; border-radius: 6px; cursor: pointer; font-weight: bold; transition: background-color 0.2s; }
.action-btn:hover:not(:disabled) { background: #0056b3; }
.action-btn:disabled { background: #6c757d; opacity: 0.5; cursor: not-allowed; }
.action-btn.small { padding: 6px 12px; font-size: 12px; }
.action-btn.danger { background: #dc3545; }
.action-btn.danger:hover { background: #c82333; }
.action-btn.outline { background: transparent; border: 2px solid #007bff; color: #007bff; }
.action-btn.outline:hover { background: #007bff; color: white; }
.action-btn.full-width { width: 100%; margin-top: 10px; }

.view-selector { margin-bottom: 24px; width: 100%; }
.view-select { width: 100%; padding: 12px 16px; font-size: 16px; font-weight: bold; border-radius: 8px; border: 1px solid #ccc; background-color: #fff; color: #333; box-shadow: 0 2px 8px rgba(0,0,0,0.05); cursor: pointer; }
.view-select:focus { border-color: #007bff; outline: none; box-shadow: 0 0 0 3px rgba(0,123,255,0.25); }

.dashboard-grid { display: flex; flex-direction: column; gap: 20px; width: 100%; }
.panel { background: #fff; padding: 20px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.08); border: 1px solid #eaeaea; position: relative; width: 100%; }
.panel h2 { margin: 0 0 20px; font-size: 18px; color: #444; border-bottom: 2px solid #f0f0f0; padding-bottom: 10px; }
.panel-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; flex-wrap: wrap; gap: 10px; }
.panel-header h2 { margin: 0; border: none; padding: 0; }

/* -------------------------------------
   ダークモード＆コンビネーションゲージ
   ------------------------------------- */
.telemetry-panel.dark-mode { background: #1e1e24; color: #fff; border: none; box-shadow: inset 0 0 20px rgba(0,0,0,0.5), 0 4px 15px rgba(0,0,0,0.2); }
.telemetry-panel.dark-mode h2 { color: #aaa; border-bottom: 1px solid #333; font-style: italic; letter-spacing: 1px; }
.combined-gauge-wrapper { display: flex; justify-content: center; margin-bottom: 5px; width: 100%; }
.analog-gauge { width: 100%; max-width: 350px; height: auto; display: block; filter: drop-shadow(0 4px 8px rgba(0,0,0,0.5)); }
.gauge-bg { fill: none; stroke: #2a2a35; stroke-width: 8; stroke-dasharray: 188.5 251.3; transform: rotate(135deg); transform-origin: 50px 50px; stroke-linecap: butt; }
.gauge-val { fill: none; stroke-width: 8; stroke-dasharray: 188.5 251.3; transform: rotate(135deg); transform-origin: 50px 50px; stroke-linecap: butt; transition: stroke-dashoffset 0.15s ease-out, stroke 0.3s; }
.gauge-needle { transition: transform 0.15s cubic-bezier(0.4, 2.5, 0.55, 0.9); transform-origin: 0px 0px; }

.tick-label-rpm { fill: #777; font-size: 7px; font-weight: bold; text-anchor: middle; font-family: sans-serif; }
.tick-label-adv { font-size: 7px; text-anchor: middle; font-family: sans-serif; }

.digital-readouts { display: flex; justify-content: space-around; margin-bottom: 20px; flex-wrap: wrap; gap: 15px; }
.readout-item { display: flex; flex-direction: column; align-items: center; }
.readout-val { font-size: 36px; font-weight: 900; font-family: "Courier New", monospace; letter-spacing: -1px; line-height: 1.1; }
.readout-unit { font-size: 12px; color: #888; font-weight: bold; letter-spacing: 1px; }

.engine-status { text-align: center; background: #111; padding: 12px; border-radius: 8px; border: 1px solid #333; display: flex; justify-content: center; align-items: center; gap: 10px; font-weight: bold; font-size: 16px; letter-spacing: 1px; flex-wrap: wrap; transition: all 0.2s; }
.engine-status .status-label { color: #666; font-size: 14px; }
.engine-status .status-text { color: #28a745; } 
.engine-status.kill .status-text { color: #ff4757; animation: blink 1s infinite alternate; }
.engine-status.revlimit { border-color: #ff4757; box-shadow: 0 0 15px rgba(255,71,87,0.5); }
.engine-status .revlimit-text { color: #ff4757; animation: blink 0.5s infinite alternate; }
.engine-status.unknown .status-text { color: #666; }
@keyframes blink { from { opacity: 1; } to { opacity: 0.5; } }
@keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.7; } 100% { opacity: 1; } }

/* -------------------------------------
   マップエディタ関連
   ------------------------------------- */
.offline-overlay { position: absolute; top: 60px; left: 0; right: 0; bottom: 0; background: rgba(255, 255, 255, 0.7); backdrop-filter: blur(2px); display: flex; justify-content: center; align-items: center; z-index: 10; border-radius: 0 0 12px 12px; }
.offline-overlay p { background: #333; color: #fff; padding: 10px 20px; border-radius: 20px; font-weight: bold; font-size: 14px; text-align: center; }
.safety-notice { background: #fff3cd; color: #856404; padding: 10px; border-radius: 6px; font-size: 13px; margin-bottom: 15px; font-weight: bold; border-left: 4px solid #ffeeba; }
.graph-container { width: 100%; background: #2a2a35; border-radius: 8px; overflow: hidden; margin-bottom: 20px; touch-action: none; }
.disabled-graph { opacity: 0.5; pointer-events: none; filter: grayscale(100%); }
.interactive-graph { width: 100%; height: auto; display: block; cursor: crosshair; }
.grid-lines line { stroke: #444; stroke-width: 1; stroke-dasharray: 4 4; }
.axis-label { fill: #888; font-size: 12px; font-family: sans-serif; user-select: none; }
.y-axis { text-anchor: end; alignment-baseline: middle; }
.x-axis { text-anchor: middle; }
.graph-line { fill: none; stroke: #00d2ff; stroke-width: 3; stroke-linejoin: round; }
.draggable-point { fill: #00d2ff; stroke: #fff; stroke-width: 2; cursor: pointer; transition: r 0.1s, fill 0.1s; }
.draggable-point:hover { r: 8; fill: #ff4757; }
.draggable-point.active { r: 9; fill: #ff4757; stroke: #ff4757; opacity: 0.8; }

.map-list { display: flex; flex-direction: column; gap: 8px; width: 100%; }
.map-list-item { display: flex; justify-content: space-between; align-items: center; background: #f8f9fa; padding: 10px; border-radius: 8px; border: 1px solid #eaeaea; }
.rpm-label { font-weight: bold; font-size: 16px; width: 90px; color: #333; }
.rpm-label .unit { font-size: 12px; color: #888; font-weight: normal; }
.spin-control { display: flex; align-items: center; gap: 8px; }
.spin-btn { width: 38px; height: 38px; border-radius: 50%; border: none; font-size: 16px; font-weight: bold; color: white; cursor: pointer; display: flex; justify-content: center; align-items: center; }
.spin-btn:active:not(:disabled) { transform: scale(0.9); }
.spin-btn:disabled { background: #ccc; cursor: not-allowed; }
.spin-btn.minus { background: #6c757d; }
.spin-btn.plus { background: #007bff; }
.map-input-value { width: 65px; height: 38px; text-align: center; font-size: 16px; font-weight: bold; color: #333; border: 1px solid #ccc; border-radius: 6px; background: white; -moz-appearance: textfield; }
.map-input-value::-webkit-inner-spin-button, .map-input-value::-webkit-outer-spin-button { -webkit-appearance: none; margin: 0; }
.map-input-value:focus { border-color: #007bff; outline: none; box-shadow: 0 0 0 2px rgba(0,123,255,0.2); }
.map-input-value:disabled { background: #f4f4f4; color: #aaa; }

/* -------------------------------------
   プロファイル＆セーフティ(OTA) 関連
   ------------------------------------- */
.profile-actions-row { display: flex; gap: 10px; margin-bottom: 20px; flex-wrap: wrap; }
.profile-input { flex: 1; min-width: 150px; padding: 10px; font-size: 16px; border: 1px solid #ccc; border-radius: 6px; width: 100%; }
.profile-actions-row .action-btn { flex: 1; min-width: 150px; }
.profile-list { border-top: 1px solid #eee; padding-top: 15px; margin-bottom: 15px; display: flex; flex-direction: column; gap: 10px; }
.profile-item { display: flex; justify-content: space-between; align-items: center; padding: 12px 15px; background: #f8f9fa; border-radius: 6px; border: 1px solid #eaeaea; flex-wrap: wrap; gap: 10px; }
.profile-name { font-weight: bold; color: #333; font-size: 15px; word-break: break-all; }
.profile-buttons { display: flex; gap: 8px; flex-wrap: wrap; }
.empty-list { text-align: center; color: #999; font-size: 14px; padding: 20px 0; font-style: italic; border-top: 1px solid #eee; }
.import-export-row { display: flex; gap: 10px; border-top: 1px solid #eee; padding-top: 20px; flex-wrap: wrap; }
.import-export-row button { flex: 1; min-width: 130px; }

/* セーフティパネル */
.device-info-container { background: #f8f9fa; padding: 15px; border-radius: 8px; border: 1px solid #eee; margin-bottom: 10px; }
.info-row { display: flex; justify-content: space-between; align-items: center; padding: 6px 0; border-bottom: 1px dashed #ddd; }
.info-row:last-child { border-bottom: none; }
.info-label { color: #666; font-size: 14px; font-weight: bold; }
.info-value { color: #333; font-size: 14px; font-weight: bold; font-family: "Courier New", monospace; }

.desc { font-size: 13px; color: #666; margin-top: 0; margin-bottom: 15px; line-height: 1.4; }
.divider { border: 0; height: 1px; background: #eee; margin: 30px 0; }
.rev-limit-container { background: #fdfdfd; padding: 15px; border-radius: 8px; border: 1px solid #eee; }
.slider-group { display: flex; align-items: center; gap: 15px; margin-bottom: 10px; }
.rev-slider { flex: 1; }
.rev-val { font-size: 20px; font-weight: bold; color: #d32f2f; min-width: 100px; text-align: right; }
.rev-val .unit { font-size: 12px; color: #888; font-weight: normal; }

.ota-container { background: #f8f9fa; padding: 15px; border-radius: 8px; border: 1px solid #eee; }
.file-upload-box { margin-bottom: 15px; }
.file-upload-box input { width: 100%; padding: 10px; border: 1px dashed #ccc; border-radius: 6px; background: #fff; }
.progress-box { margin-bottom: 15px; text-align: center; }
.progress-bar { width: 100%; height: 12px; background: #e9ecef; border-radius: 6px; overflow: hidden; margin-bottom: 5px; }
.progress-fill { height: 100%; background: #28a745; transition: width 0.1s linear; }
.progress-box p { margin: 0; font-size: 12px; color: #28a745; font-weight: bold; }

@media (max-width: 600px) {
  .cdi-dashboard { padding: 10px; }
  .panel { padding: 15px; } 
  .header { flex-direction: column; text-align: center; align-items: stretch; }
  .title-area h1 { font-size: 20px; }
  .connect-btn { width: 100%; }
  .profile-actions-row, .import-export-row { flex-direction: column; }
  .profile-input, .profile-actions-row button, .import-export-row button { width: 100%; }
  .profile-item { flex-direction: column; align-items: stretch; text-align: left; }
  .profile-buttons { justify-content: space-between; }
  .profile-buttons button { flex: 1; }
  .map-list-item { padding: 8px; }
  .rpm-label { width: 70px; font-size: 14px; }
  .spin-btn { width: 34px; height: 34px; font-size: 14px; }
  .map-input-value { width: 55px; font-size: 15px; }
  .slider-group { flex-direction: column; align-items: stretch; }
  .rev-val { text-align: center; }
}
</style>
