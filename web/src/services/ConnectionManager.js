// ConnectionManager.js — Manages BLE connection lifecycle and message routing
import { BLEService } from './BLEService';
import { 
  isConnected, deviceName, updateFromTelemetry, showToast, 
  hasRecovery, recoveryRecipeName,
  wifiConnected, wifiSSID, wifiIP, wifiConfiguredSSID,
  otaStatus, otaLatestVersion, otaProgress, otaError, firmwareVersion
} from '../stores/state';

class ConnectionManagerClass {
  constructor() {
    this._initialized = false;
  }

  init() {
    if (this._initialized) return;
    this._initialized = true;

    BLEService.onConnect(() => {
      isConnected.value = true;
      deviceName.value = BLEService.device?.name || 'Inversa';
      showToast('Conectado!', 'success');
    });

    BLEService.onDisconnect(() => {
      isConnected.value = false;
      deviceName.value = '';
      showToast('Desconectado', 'error');
    });

    BLEService.onMessage((data) => {
      this._handleMessage(data);
    });
  }

  async connect() {
    this.init();
    await BLEService.connect();
  }

  disconnect() {
    BLEService.disconnect();
  }

  // Shorthand for requests
  async setTemp(value) {
    return BLEService.request('req:set-temp', { v: value });
  }

  async heaterOn() {
    return BLEService.request('req:heater:on');
  }

  async heaterOff() {
    return BLEService.request('req:heater:off');
  }

  async pumpOn() {
    return BLEService.request('req:pump:on');
  }

  async pumpOff() {
    return BLEService.request('req:pump:off');
  }

  async setPID(kp, ki, kd) {
    return BLEService.request('req:set-pid', { kp, ki, kd });
  }

  // Save PID settings with NVS persistence
  async saveSettings(kp, ki, kd) {
    return BLEService.request('req:settings:set', { kp, ki, kd });
  }

  async listRecipes() {
    const res = await BLEService.request('req:recipe:list');
    return res.recipes || [];
  }

  async loadRecipe(filename) {
    return BLEService.request('req:recipe:load', { file: filename });
  }

  async startRecipe(filename) {
    return BLEService.request('req:recipe:start', { file: filename });
  }

  async stopRecipe() {
    return BLEService.request('req:recipe:stop');
  }

  async pauseRecipe() {
    return BLEService.request('req:recipe:pause');
  }

  async resumeRecipe() {
    return BLEService.request('req:recipe:resume');
  }

  async confirmRecipe() {
    return BLEService.request('req:recipe:confirm');
  }

  async saveRecipe(filename, content) {
    return BLEService.request('req:recipe:save', { file: filename, content });
  }

  async deleteRecipe(filename) {
    return BLEService.request('req:recipe:delete', { file: filename });
  }

  async getSettings() {
    return BLEService.request('req:settings:get');
  }

  async getInfo() {
    return BLEService.request('req:info');
  }

  // Recovery
  async resumeRecovery() {
    return BLEService.request('req:recovery:resume');
  }

  async discardRecovery() {
    return BLEService.request('req:recovery:discard');
  }

  // WiFi
  async configureWiFi(ssid, password) {
    return BLEService.request('req:wifi:config', { ssid, pwd: password });
  }

  async connectWiFi() {
    return BLEService.request('req:wifi:connect');
  }

  async disconnectWiFi() {
    return BLEService.request('req:wifi:disconnect');
  }

  async getWiFiStatus() {
    return BLEService.request('req:wifi:status');
  }

  // OTA
  async checkForUpdates() {
    return BLEService.request('req:ota:check');
  }

  async installUpdate() {
    return BLEService.request('req:ota:install');
  }

  _handleMessage(data) {
    switch (data.tp) {
      case 'evt:status':
        updateFromTelemetry(data);
        break;

      case 'evt:recipe:confirm':
        showToast(data.msg || 'Confirmacao necessaria', 'info', 10000);
        break;

      case 'evt:recipe:state':
        // Recipe state change events handled via telemetry
        break;

      case 'evt:error':
        showToast(data.err || 'Erro', 'error');
        break;

      case 'evt:log':
        console.log('[Device]', data.msg);
        break;

      case 'evt:recipe:recovery':
        hasRecovery.value = true;
        recoveryRecipeName.value = data.recipe || '';
        showToast(`Receita "${data.recipe}" pode ser recuperada`, 'info', 10000);
        break;

      case 'evt:wifi:status':
        wifiConnected.value = data.conn || false;
        wifiSSID.value = data.ssid || '';
        wifiIP.value = data.ip || '';
        if (data.cfg) wifiConfiguredSSID.value = data.cfg;
        if (data.err) showToast(data.err, 'error');
        break;

      case 'evt:ota:status':
        otaStatus.value = data.st || 'idle';
        otaLatestVersion.value = data.ver || '';
        otaProgress.value = data.pct || 0;
        otaError.value = data.err || '';
        if (data.cur) firmwareVersion.value = data.cur;
        
        // Show toasts for important OTA events
        if (data.st === 'available') {
          showToast(`Update available: ${data.ver}`, 'info');
        } else if (data.st === 'error') {
          showToast(data.err || 'OTA error', 'error');
        } else if (data.st === 'up-to-date') {
          showToast('Firmware is up to date', 'success');
        } else if (data.st === 'installing') {
          showToast('Installing update... Device will restart', 'info', 10000);
        }
        break;
    }
  }
}

export const ConnectionManager = new ConnectionManagerClass();
