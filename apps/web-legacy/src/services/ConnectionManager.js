// ConnectionManager.js — Manages BLE connection lifecycle and message routing
import { BLEService } from './BLEService';
import { lastTelemetryMs } from '../stores/telemetryFreshness';
import {
  isConnected, deviceName, updateFromTelemetry, showToast,
  hasRecovery, recoveryRecipeName,
  wifiConnected, wifiSSID, wifiIP, wifiConfiguredSSID,
  otaStatus, otaLatestVersion, otaProgress, otaError, firmwareVersion,
  rampActive, rampRate, rampTarget, rampCurrent,
  brewLogActive, brewLogEntries, brewLogData,
  notificationsEnabled, notifyOnTempReached, notifyOnStepComplete,
  targetTemp, currentTemp,
  boilActive, boilTotal, boilRemaining, boilAlerts,
  mashOutEnabled, mashOutTemp,
  timerActive, timerRemaining,
  schedulerActive, schedulerStatus,
  autoTuneActive, autoTuneProgress,
  waitingForConfirm, confirmMessage
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
      deviceName.value = BLEService.device?.name || 'BrewPilot';
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

  // Thermal parameters (P13). Persisted in NVS; loaded on boot by
  // main.cpp before plugins start.
  async getThermalParams() {
    return BLEService.request('req:settings:thermal:get');
  }
  async setThermalParams(volumeL, powerW, ambientC, diameterM, lossCoeff) {
    return BLEService.request('req:settings:thermal:set',
      { volumeL, powerW, ambientC, diameterM, lossCoeff });
  }

  // Factory reset (P7). Requires `confirm: "ERASE_ALL"` literal; the
  // firmware refuses anything else.
  async factoryReset(confirm) {
    return BLEService.request('req:factory:reset', { confirm });
  }

  // Timezone offset in minutes (P14).
  async getTimezone() { return BLEService.request('req:rtc:tz:get'); }
  async setTimezone(min) { return BLEService.request('req:rtc:tz:set', { min }); }

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

  // Boil Timer
  async startBoil(minutes, additions = []) {
    return BLEService.request('req:boil:start', { min: minutes, additions });
  }

  async stopBoil() {
    return BLEService.request('req:boil:stop');
  }

  async pauseBoil() {
    return BLEService.request('req:boil:pause');
  }

  async resumeBoil() {
    return BLEService.request('req:boil:resume');
  }

  async addBoilAddition(minutes, name) {
    return BLEService.request('req:boil:add', { min: minutes, name });
  }

  // Mash-Out
  async setMashOut(enabled, temp = 76.0) {
    return BLEService.request('req:mashout:set', { enabled, temp });
  }

  // RTC
  async getRtcTime() {
    return BLEService.request('req:rtc:get');
  }

  async setRtcTime(timestamp) {
    return BLEService.request('req:rtc:set', { ts: timestamp });
  }

  async syncRtcNtp() {
    return BLEService.request('req:rtc:sync');
  }

  // Timer (relative countdown)
  async startTimer(seconds) {
    return BLEService.request('req:timer:start', { sec: seconds });
  }

  async startTimerMinutes(minutes) {
    return BLEService.request('req:timer:start', { min: minutes });
  }

  // Timer (absolute alarm at HH:MM)
  async setTimerAlarm(hour, minute) {
    return BLEService.request('req:timer:alarm', { hour, min: minute });
  }

  async stopTimer() {
    return BLEService.request('req:timer:stop');
  }

  async pauseTimer() {
    return BLEService.request('req:timer:pause');
  }

  async resumeTimer() {
    return BLEService.request('req:timer:resume');
  }

  async addTimerTime(seconds) {
    return BLEService.request('req:timer:add', { sec: seconds });
  }

  // Scheduler ("be ready at HH:MM")
  async setScheduler(hour, minute, temp, volume) {
    return BLEService.request('req:sched:set', { hour, min: minute, temp, vol: volume });
  }

  async stopScheduler() {
    return BLEService.request('req:sched:stop');
  }

  // Auto-Tune
  async startAutoTune(targetTemp = 65) {
    return BLEService.request('req:autotune:start', { temp: targetTemp });
  }

  async stopAutoTune() {
    return BLEService.request('req:autotune:stop');
  }

  _handleMessage(data) {
    switch (data.tp) {
      case 'evt:status':
        lastTelemetryMs.value = Date.now();
        updateFromTelemetry(data);
        break;

      case 'evt:recipe:confirm':
        waitingForConfirm.value = true;
        confirmMessage.value = data.msg || '';
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

      case 'evt:boil:status':
        if (data.st === 'started') {
          boilActive.value = true;
          showToast('Fervura iniciada', 'success');
        } else if (data.st === 'stopped' || data.st === 'complete') {
          boilActive.value = false;
          if (data.st === 'complete') {
            showToast('Fervura completa!', 'success');
            this._sendNotification('Fervura Completa', 'O tempo de fervura acabou');
          }
        } else if (data.st === 'paused') {
          showToast('Fervura pausada', 'info');
        } else if (data.st === 'resumed') {
          showToast('Fervura retomada', 'info');
        }
        if (data.rem !== undefined) boilRemaining.value = data.rem;
        if (data.total !== undefined) boilTotal.value = data.total;
        break;

      case 'evt:boil:addition':
        boilAlerts.value = [...boilAlerts.value, { name: data.name, min: data.min, time: new Date() }];
        showToast(`Adicionar: ${data.name} (${data.min} min)`, 'warning', 10000);
        this._sendNotification('Adição de Lúpulo', `Adicionar ${data.name} agora! (${data.min} min)`);
        break;

      case 'evt:timer:status':
        if (data.rem !== undefined) timerRemaining.value = data.rem;
        break;

      case 'evt:timer:complete':
        timerActive.value = false;
        showToast('Timer concluído!', 'success');
        this._sendNotification('Timer Concluído', 'O timer terminou');
        break;

      case 'evt:sched:status':
        if (data.status !== undefined) schedulerStatus.value = data.status;
        break;

      case 'evt:sched:starting':
        showToast('Agendamento: Aquecimento iniciado', 'info');
        this._sendNotification('Agendamento', 'Aquecimento iniciado automaticamente');
        break;

      case 'evt:sched:ready':
        schedulerActive.value = false;
        showToast('Agendamento: Pronto!', 'success');
        this._sendNotification('Pronto!', 'A temperatura foi atingida no horário agendado');
        break;

      case 'evt:autotune:status':
        if (data.pct !== undefined) autoTuneProgress.value = data.pct;
        if (data.status !== undefined) {
          showToast(`Auto-Tune: ${data.status}`, 'info');
        }
        break;

      case 'evt:autotune:result':
        autoTuneActive.value = false;
        showToast(`Auto-Tune concluído! Kp=${data.kp?.toFixed(2)}, Ki=${data.ki?.toFixed(4)}, Kd=${data.kd?.toFixed(0)}`, 'success', 10000);
        this._sendNotification('Auto-Tune Concluído', `Novos valores PID calculados`);
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
        tag: 'brewpilot-notification',
        renotify: true
      });
    } catch (e) {
      console.warn('Notification failed:', e);
    }
  }
}

export const ConnectionManager = new ConnectionManagerClass();
