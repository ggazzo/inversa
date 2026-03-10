import { useState, useEffect } from 'preact/hooks';
import { ConnectionManager } from '../services/ConnectionManager';
import {
  isConnected, pidKp, pidKi, pidKd, showToast,
} from '../stores/state';

export function Settings() {
  const connected = isConnected.value;
  const [kp, setKp] = useState(pidKp.value);
  const [ki, setKi] = useState(pidKi.value);
  const [kd, setKd] = useState(pidKd.value);
  const [saving, setSaving] = useState(false);
  const [info, setInfo] = useState(null);
  const [loadingInfo, setLoadingInfo] = useState(false);

  // Sync local state with signal values when they change
  useEffect(() => {
    setKp(pidKp.value);
    setKi(pidKi.value);
    setKd(pidKd.value);
  }, [pidKp.value, pidKi.value, pidKd.value]);

  async function fetchSettings() {
    try {
      const res = await ConnectionManager.getSettings();
      if (res.kp !== undefined) { pidKp.value = res.kp; setKp(res.kp); }
      if (res.ki !== undefined) { pidKi.value = res.ki; setKi(res.ki); }
      if (res.kd !== undefined) { pidKd.value = res.kd; setKd(res.kd); }
      showToast('Configuracoes carregadas', 'success');
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  async function savePID() {
    const kpVal = parseFloat(kp);
    const kiVal = parseFloat(ki);
    const kdVal = parseFloat(kd);

    if (isNaN(kpVal) || isNaN(kiVal) || isNaN(kdVal)) {
      showToast('Valores PID invalidos', 'error');
      return;
    }

    setSaving(true);
    try {
      // Use saveSettings for NVS persistence
      await ConnectionManager.saveSettings(kpVal, kiVal, kdVal);
      pidKp.value = kpVal;
      pidKi.value = kiVal;
      pidKd.value = kdVal;
      showToast('PID salvo na memoria!', 'success');
    } catch (e) {
      showToast(e.message, 'error');
    } finally {
      setSaving(false);
    }
  }

  async function fetchInfo() {
    setLoadingInfo(true);
    try {
      const res = await ConnectionManager.getInfo();
      setInfo(res);
    } catch (e) {
      showToast(e.message, 'error');
    } finally {
      setLoadingInfo(false);
    }
  }

  if (!connected) {
    return (
      <div class="flex flex-col items-center justify-center py-16 text-base-content/50">
        <svg xmlns="http://www.w3.org/2000/svg" class="h-12 w-12 mb-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5" d="M18.364 5.636a9 9 0 11-12.728 0M12 3v9" />
        </svg>
        <p>Conecte ao dispositivo para configurar.</p>
      </div>
    );
  }

  return (
    <div class="flex flex-col gap-4">
      {/* PID Tuning */}
      <div class="card bg-base-100 shadow-md">
        <div class="card-body p-4">
          <div class="flex items-center justify-between mb-3">
            <h3 class="text-sm font-semibold uppercase text-base-content/60">Sintonia PID</h3>
            <button class="btn btn-ghost btn-xs" onClick={fetchSettings}>
              Carregar
            </button>
          </div>

          <div class="space-y-3">
            <PIDInput label="Kp (Proporcional)" value={kp} onChange={setKp} step="0.1" />
            <PIDInput label="Ki (Integral)" value={ki} onChange={setKi} step="0.001" />
            <PIDInput label="Kd (Derivativo)" value={kd} onChange={setKd} step="1" />
          </div>

          <button
            class="btn btn-primary btn-sm mt-4 w-full"
            onClick={savePID}
            disabled={saving}
          >
            {saving ? <span class="loading loading-spinner loading-xs" /> : 'Salvar PID'}
          </button>
        </div>
      </div>

      {/* Device Info */}
      <div class="card bg-base-100 shadow-md">
        <div class="card-body p-4">
          <div class="flex items-center justify-between mb-3">
            <h3 class="text-sm font-semibold uppercase text-base-content/60">Informacoes</h3>
            <button class="btn btn-ghost btn-xs" onClick={fetchInfo} disabled={loadingInfo}>
              {loadingInfo ? <span class="loading loading-spinner loading-xs" /> : 'Atualizar'}
            </button>
          </div>

          {info ? (
            <div class="space-y-1 text-sm">
              <InfoRow label="Firmware" value={info.fw || '--'} />
              <InfoRow label="Board" value={info.board || '--'} />
              <InfoRow label="Chip" value={info.chip || '--'} />
              <InfoRow label="Free Heap" value={info.heap ? `${(info.heap / 1024).toFixed(1)} KB` : '--'} />
              <InfoRow label="SD Card" value={info.sd ? 'OK' : 'N/A'} />
              <InfoRow label="Uptime" value={info.up ? formatUptime(info.up) : '--'} />
            </div>
          ) : (
            <div class="text-center text-base-content/40 py-4 text-sm">
              Clique "Atualizar" para ver informacoes.
            </div>
          )}
        </div>
      </div>

      {/* About */}
      <div class="card bg-base-100 shadow-md">
        <div class="card-body p-4">
          <h3 class="text-sm font-semibold uppercase text-base-content/60 mb-2">Sobre</h3>
          <div class="text-xs text-base-content/50 space-y-1">
            <p>Inversa v2 — Controlador de temperatura para brassagem</p>
            <p>Comunicacao via Web Bluetooth (NUS)</p>
            <p class="text-base-content/30">Feito com Preact + DaisyUI</p>
          </div>
        </div>
      </div>
    </div>
  );
}

function PIDInput({ label, value, onChange, step }) {
  return (
    <div>
      <label class="text-xs text-base-content/50 mb-1 block">{label}</label>
      <input
        type="number"
        class="input input-bordered input-sm w-full font-mono"
        value={value}
        onInput={(e) => onChange(e.target.value)}
        step={step}
      />
    </div>
  );
}

function InfoRow({ label, value }) {
  return (
    <div class="flex justify-between">
      <span class="text-base-content/50">{label}</span>
      <span class="font-mono">{value}</span>
    </div>
  );
}

function formatUptime(seconds) {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;
  if (h > 0) return `${h}h ${m}m`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}
