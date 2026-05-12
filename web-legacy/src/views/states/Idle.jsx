// Idle.jsx — context panel for the resting state.
//
// Two big primary CTAs (Iniciar Receita, Modo Manual) fill the panel,
// with a strip of secondary shortcuts below. The temperature is still
// visible on the left, so the operator can sanity-check before starting.

export function Idle({ onMenuSelect, onStartManual }) {
    return (
        <div class="flex flex-col gap-3">
            <div class="grid grid-cols-1 sm:grid-cols-2 gap-3">
                <button
                    class="btn btn-primary btn-lg h-auto py-6 flex-col gap-1"
                    onClick={() => onMenuSelect?.('recipes')}
                >
                    <span class="text-2xl">🍺</span>
                    <span class="text-lg">Iniciar Receita</span>
                    <span class="text-xs opacity-70 font-normal">
                        Escolher do SD card
                    </span>
                </button>
                <button
                    class="btn btn-outline btn-lg h-auto py-6 flex-col gap-1"
                    onClick={onStartManual}
                >
                    <span class="text-2xl">🎛️</span>
                    <span class="text-lg">Modo Manual</span>
                    <span class="text-xs opacity-70 font-normal">
                        Setpoint direto
                    </span>
                </button>
            </div>

            {/* Secondary shortcuts */}
            <div class="grid grid-cols-3 gap-2 text-sm">
                <button
                    class="btn btn-ghost btn-sm normal-case"
                    onClick={() => onMenuSelect?.('tuning')}
                >
                    AutoTune
                </button>
                <button
                    class="btn btn-ghost btn-sm normal-case"
                    onClick={() => onMenuSelect?.('brewlog')}
                >
                    Brew log
                </button>
                <button
                    class="btn btn-ghost btn-sm normal-case"
                    onClick={() => onMenuSelect?.('equipment')}
                >
                    Equipamento
                </button>
            </div>
        </div>
    );
}
