// ConnectionManager.js — Manages BLE connection lifecycle and message routing
import { BLEService } from './BLEService';
import { 
  isConnected, deviceName, updateFromTelemetry, showToast, 
  hasRecovery, recoveryRecipeName,
  wifiConnected, wifiSSID, wifiIP, wifiConfiguredSSID,
  otaStatus, otaLatestVersion, otaProgress, otaError, firmwareVersion,
  rampActive, rampRate, rampTarget, rampCurrent,
  brewLogActive, brewLogEntries, brewLogData,
  notificationsEnabled, notifyOnTempReached, notifyOnStepComplete,
  targetTemp, currentTemp
} from '../stores/state';

class ConnectionManagerClass {
  constructor() {
    this._initialized = false;
    this._tempReachedNotified = false;
  }

  async requestNotificationPermission() {
    if (!('Notification' in window)) {
      showToast('Notificações não suportadas', 'error');
      return false;
    }

    const permission = await Notification.requestPermission();
    if (permission === 'granted') {
      notificationsEnabled.value = true;
      showToast('Notificações ativadas', 'success');
      return true;
    } else {
      notificationsEnabled.value = false;
      showToast('Permissão negada', 'error');
      return false;
    }
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

  // Ramp Mode
  async setRampRate(rate) {
    return BLEService.request('req:ramp:set', { rate });
  }

  async stopRamp() {
    return BLEService.request('req:ramp:stop');
  }

  // Brew Log
  async startBrewLog() {
    return BLEService.request('req:log:start');
  }

  async stopBrewLog() {
    return BLEService.request('req:log:stop');
  }

  async exportBrewLog(format = 'csv') {
    brewLogData.value = [];
    const result = await BLEService.request('req:log:export', { fmt: format, chunk: 0 });
    return result;
  }

  async fetchLogChunk(format, chunkIndex) {
    return BLEService.request('req:log:export', { fmt: format, chunk: chunkIndex });
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

      case 'evt:ramp:status':
        rampActive.value = data.active || false;
        rampRate.value = data.rate || 0;
        rampTarget.value = data.target || 0;
        rampCurrent.value = data.current || 0;
        break;

      case 'evt:ramp:complete':
        rampActive.value = false;
        showToast(`Rampa concluída: ${data.temp?.toFixed(1)}°C`, 'success');
        this._sendNotification('Rampa Concluída', `Temperatura atingiu ${data.temp?.toFixed(1)}°C`);
        break;

      case 'evt:log:status':
        brewLogActive.value = data.active || false;
        brewLogEntries.value = data.entries || 0;
        break;

      case 'evt:log:data':
        // Append chunk data
        if (data.data) {
          brewLogData.value = [...brewLogData.value, data.data];
        }
        break;
    }

    // Check for notification triggers
    this._checkNotificationTriggers(data);
  }

  // Notification helpers
  _checkNotificationTriggers(data) {
    if (!notificationsEnabled.value) return;

    // Notify when temperature is reached (within 0.5°C)
    if (notifyOnTempReached.value && data.ct !== undefined && data.tt !== undefined) {
      const current = data.ct;
      const target = data.tt;
      if (target > 0 && Math.abs(current - target) < 0.5 && !this._tempReachedNotified) {
        this._tempReachedNotified = true;
        this._sendNotification('Temperatura Atingida', `${current.toFixed(1)}°C de ${target.toFixed(1)}°C`);
      } else if (Math.abs(current - target) >= 2) {
        this._tempReachedNotified = false;
      }
    }

    // Notify on recipe step completion
    if (notifyOnStepComplete.value && data.rst === 'waiting_confirm') {
      this._sendNotification('Etapa Concluída', 'Aguardando confirmação para continuar');
    }
  }

  _sendNotification(title, body) {
    if (!('Notification' in window)) return;
    if (Notification.permission !== 'granted') return;
    
    try {
      new Notification(title, {
        body,
        icon: '/icon-192.png',
        badge: '/icon-192.png',
        tag: 'inversa-notification',
        renotify: true
      });
    } catch (e) {
      console.warn('Notification failed:', e);
    }
  }
}

export const ConnectionManager = new ConnectionManagerClass();
