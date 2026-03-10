// BLEService.js — Web Bluetooth connection to Inversa controller
// Uses Nordic UART Service (NUS) for bidirectional JSON communication

const NUS_SERVICE_UUID    = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_TX_CHAR_UUID    = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // Notify (device → app)
const NUS_RX_CHAR_UUID    = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // Write  (app → device)

class BLEServiceClass {
  constructor() {
    this.device = null;
    this.server = null;
    this.txChar = null;
    this.rxChar = null;
    this.connected = false;
    this._onMessage = null;
    this._onConnect = null;
    this._onDisconnect = null;
    this._rxBuffer = '';
    this._requestMap = new Map();
    this._requestId = 0;
  }

  // Check if Web Bluetooth is available
  isSupported() {
    return !!navigator.bluetooth;
  }

  // Connect to the Inversa device
  async connect() {
    if (!this.isSupported()) {
      throw new Error('Web Bluetooth is not supported in this browser');
    }

    try {
      // Request device with Nordic UART Service filter
      this.device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [NUS_SERVICE_UUID] }],
        optionalServices: [NUS_SERVICE_UUID],
      });

      this.device.addEventListener('gattserverdisconnected', () => {
        this.connected = false;
        this._onDisconnect?.();
      });

      // Connect to GATT server
      this.server = await this.device.gatt.connect();
      const service = await this.server.getPrimaryService(NUS_SERVICE_UUID);

      // Get characteristics
      this.txChar = await service.getCharacteristic(NUS_TX_CHAR_UUID);
      this.rxChar = await service.getCharacteristic(NUS_RX_CHAR_UUID);

      // Subscribe to notifications (device → app)
      await this.txChar.startNotifications();
      this.txChar.addEventListener('characteristicvaluechanged', (e) => {
        this._handleNotification(e.target.value);
      });

      this.connected = true;
      this._onConnect?.();
      return true;
    } catch (err) {
      console.error('[BLE] Connection failed:', err);
      throw err;
    }
  }

  // Disconnect
  disconnect() {
    if (this.device?.gatt?.connected) {
      this.device.gatt.disconnect();
    }
    this.connected = false;
    this._onDisconnect?.();
  }

  // Send a JSON command to the device
  async send(data) {
    if (!this.connected || !this.rxChar) {
      throw new Error('Not connected');
    }

    const json = JSON.stringify(data);
    const encoder = new TextEncoder();
    const encoded = encoder.encode(json);

    // Send in chunks (BLE MTU limitation, typically ~500 bytes max)
    const chunkSize = 500;
    for (let i = 0; i < encoded.length; i += chunkSize) {
      const chunk = encoded.slice(i, i + chunkSize);
      await this.rxChar.writeValueWithoutResponse(chunk);
    }
  }

  // Send a request and wait for response (with timeout)
  async request(type, data = {}, timeoutMs = 5000) {
    const rid = `r${++this._requestId}`;
    const message = { tp: type, rid, ...data };

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this._requestMap.delete(rid);
        reject(new Error(`Request timeout: ${type}`));
      }, timeoutMs);

      this._requestMap.set(rid, (response) => {
        clearTimeout(timer);
        this._requestMap.delete(rid);
        if (response.tp === 'res:error') {
          reject(new Error(response.err || 'Unknown error'));
        } else {
          resolve(response);
        }
      });

      this.send(message).catch((err) => {
        clearTimeout(timer);
        this._requestMap.delete(rid);
        reject(err);
      });
    });
  }

  // Event handlers
  onMessage(callback)    { this._onMessage = callback; }
  onConnect(callback)    { this._onConnect = callback; }
  onDisconnect(callback) { this._onDisconnect = callback; }

  // Handle incoming BLE notifications
  _handleNotification(dataView) {
    const decoder = new TextDecoder();
    const chunk = decoder.decode(dataView);
    this._rxBuffer += chunk;

    // Try to parse accumulated JSON
    try {
      const data = JSON.parse(this._rxBuffer);
      this._rxBuffer = '';

      // Check if this is a response to a pending request
      if (data.rid && this._requestMap.has(data.rid)) {
        this._requestMap.get(data.rid)(data);
      }

      // Always notify message handler
      this._onMessage?.(data);
    } catch {
      // Incomplete JSON, keep accumulating
      // But if buffer gets too large, reset (likely corrupt)
      if (this._rxBuffer.length > 10000) {
        console.warn('[BLE] Buffer overflow, resetting');
        this._rxBuffer = '';
      }
    }
  }
}

export const BLEService = new BLEServiceClass();
