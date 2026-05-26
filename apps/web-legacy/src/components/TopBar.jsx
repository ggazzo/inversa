// TopBar.jsx — single header replacing the old ConnectionBar + Navbar.
//
// Holds: brand, connection chip, current brewing mode pill, and an
// overflow menu (⋮) that opens the contextual wizards/sheets. There's no
// bottom nav anymore — wizards are reached from here, and the rest of
// the UI changes contextually based on state.

import { isConnected, deviceName, mode, modeLabel } from '../stores/state';
import { isStale } from '../stores/telemetryFreshness';
import { theme, toggleTheme } from '../stores/theme';
import { ConnectionManager } from '../services/ConnectionManager';
import { BLEService } from '../services/BLEService';

const MENU_ITEMS = [
    { id: 'recipes',     label: 'Receitas salvas' },
    { id: 'brewlog',     label: 'Brew log'       },
    { id: 'equipment',   label: 'Equipamento'    },
    { id: 'connectivity',label: 'WiFi & OTA'     },
    { id: 'tuning',      label: 'PID & AutoTune' },
    { id: 'notifications', label: 'Notificações' },
    { id: 'about',       label: 'Sobre'          },
];

export function TopBar({ onMenuSelect }) {
    const connected = isConnected.value;
    const stale     = isStale.value;
    const supported = BLEService.isSupported();

    return (
        <header class="sticky top-0 z-30 bg-base-100/95 backdrop-blur border-b border-base-300">
            <div class="flex items-center gap-3 px-3 sm:px-6 h-12">
                {/* Brand */}
                <div class="flex items-center gap-2 min-w-0">
                    <div class="w-7 h-7 rounded bg-primary text-primary-content
                                grid place-items-center font-bold">I</div>
                    <span class="font-semibold tracking-tight hidden xs:inline">BrewPilot</span>
                </div>

                {/* Connection chip */}
                <button
                    type="button"
                    class={`btn btn-xs ${connected ? (stale ? 'btn-warning' : 'btn-success') : 'btn-ghost'}`}
                    onClick={() => connected ? ConnectionManager.disconnect()
                                             : ConnectionManager.connect().catch(() => {})}
                    disabled={!supported}
                    aria-label={connected ? 'Desconectar' : 'Conectar'}
                >
                    <span class={`w-2 h-2 rounded-full mr-1 ${
                        !supported ? 'bg-base-content/30' :
                        !connected ? 'bg-base-content/40' :
                        stale      ? 'bg-warning animate-pulse' : 'bg-success'
                    }`}/>
                    {!supported ? 'Web BT?' :
                     !connected ? 'Conectar' :
                     stale      ? 'Sem dados' :
                                  (deviceName.value || 'Conectado')}
                </button>

                {/* Mode pill — visible only when meaningful */}
                {connected && mode.value !== 'idle' && (
                    <span class="badge badge-outline badge-sm capitalize hidden sm:inline-flex">
                        {modeLabel.value}
                    </span>
                )}

                <span class="flex-1" />

                {/* Theme toggle */}
                <button
                    type="button"
                    class="btn btn-ghost btn-sm btn-square"
                    onClick={toggleTheme}
                    aria-label={theme.value === 'dark' ? 'Mudar para tema claro' : 'Mudar para tema escuro'}
                    title={theme.value === 'dark' ? 'Tema claro' : 'Tema escuro'}
                >
                    {theme.value === 'dark' ? '☀️' : '🌙'}
                </button>

                {/* Overflow menu */}
                <div class="dropdown dropdown-end">
                    <button tabIndex={0} class="btn btn-ghost btn-sm btn-square" aria-label="Menu">
                        <svg xmlns="http://www.w3.org/2000/svg" class="w-5 h-5"
                             fill="none" viewBox="0 0 24 24" stroke="currentColor">
                            <circle cx="12" cy="5"  r="1.5"/>
                            <circle cx="12" cy="12" r="1.5"/>
                            <circle cx="12" cy="19" r="1.5"/>
                        </svg>
                    </button>
                    <ul tabIndex={0}
                        class="dropdown-content z-40 menu p-2 shadow bg-base-200
                               rounded-box w-56 mt-1">
                        {MENU_ITEMS.map(m => (
                            <li key={m.id}>
                                <button onClick={() => onMenuSelect?.(m.id)}>{m.label}</button>
                            </li>
                        ))}
                    </ul>
                </div>
            </div>
        </header>
    );
}
