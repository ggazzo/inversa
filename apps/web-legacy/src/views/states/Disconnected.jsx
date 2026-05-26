// Disconnected.jsx — shown when no BLE/sim link is established.
// CTA-first; explains both real-device and simulator paths.

import { ConnectionManager } from '../../services/ConnectionManager';
import { BLEService } from '../../services/BLEService';

export function Disconnected() {
    const supported = BLEService.isSupported();
    const simMode   = !!new URLSearchParams(location.search).get('sim');

    return (
        <div class="card bg-base-100 shadow-sm">
            <div class="card-body items-center text-center gap-4 py-8 sm:py-12">
                <div class="w-16 h-16 rounded-full bg-base-200 grid place-items-center">
                    <svg xmlns="http://www.w3.org/2000/svg" class="w-8 h-8 text-base-content/40"
                         fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="1.5">
                        <path stroke-linecap="round" stroke-linejoin="round"
                              d="M9 19v-3a3 3 0 116 0v3M5 12V8a7 7 0 1114 0v4M5 12h14a2 2 0 012 2v6H3v-6a2 2 0 012-2z"/>
                    </svg>
                </div>
                <div>
                    <h2 class="text-xl font-semibold">
                        {simMode ? 'Conectar ao simulador' : 'Conectar BrewPilot'}
                    </h2>
                    <p class="text-sm text-base-content/60 mt-1 max-w-sm">
                        {simMode
                            ? 'O simulador está rodando em ws://localhost:8765. Clique abaixo para abrir a sessão virtual.'
                            : supported
                                ? 'Aproxime o dispositivo BrewPilot e clique abaixo para parear via Bluetooth.'
                                : 'Este navegador não suporta Web Bluetooth. Tente Chrome ou Edge em desktop, ou Bluefy no iOS.'}
                    </p>
                </div>
                <button
                    class="btn btn-primary btn-wide"
                    disabled={!supported}
                    onClick={() => ConnectionManager.connect().catch(() => {})}
                >
                    {simMode ? 'Iniciar sessão simulada' : 'Conectar'}
                </button>
            </div>
        </div>
    );
}
