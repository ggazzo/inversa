import { isConnected, deviceName } from '../stores/state';
import { ConnectionManager } from '../services/ConnectionManager';
import { BLEService } from '../services/BLEService';

export function ConnectionBar() {
  const connected = isConnected.value;
  const supported = BLEService.isSupported();

  if (!supported) {
    return (
      <div class="alert alert-warning mx-4 mt-2 text-sm">
        <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor">
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-2.5L13.732 4c-.77-.833-1.964-.833-2.732 0L4.082 16.5c-.77.833.192 2.5 1.732 2.5z" />
        </svg>
        <span>Web Bluetooth nao suportado neste navegador.</span>
      </div>
    );
  }

  return (
    <div class="flex items-center justify-between px-4 py-2 bg-base-100 border-b border-base-300">
      <div class="flex items-center gap-2">
        <div class={`badge badge-sm ${connected ? 'badge-success' : 'badge-ghost'}`}>
          {connected ? 'Conectado' : 'Desconectado'}
        </div>
        {connected && deviceName.value && (
          <span class="text-xs text-base-content/60">{deviceName.value}</span>
        )}
      </div>
      {connected ? (
        <button
          class="btn btn-ghost btn-xs"
          onClick={() => ConnectionManager.disconnect()}
        >
          Desconectar
        </button>
      ) : (
        <button
          class="btn btn-primary btn-xs"
          onClick={() => ConnectionManager.connect().catch(() => {})}
        >
          Conectar
        </button>
      )}
    </div>
  );
}
