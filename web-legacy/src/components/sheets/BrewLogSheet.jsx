// BrewLogSheet.jsx — start/stop the BrewLog ring buffer and fetch a
// CSV/JSON export. Per P18 the firmware persists to `/brewlog.bin` so
// power loss only costs the last batch (~30 entries).

import { useSignal } from '@preact/signals';
import { brewLogActive, brewLogEntries, showToast } from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';
import { WizardSheet } from '../wizards/WizardSheet';

export function BrewLogSheet({ onClose }) {
    const fmt    = useSignal('csv');
    const busy   = useSignal(false);
    const chunks = useSignal([]);   // accumulated string parts

    async function fetchAll() {
        busy.value = true;
        chunks.value = [];
        try {
            // The firmware paginates by chunkIndex; we fetch sequentially
            // until it tells us the running total chunk count is exhausted.
            let i = 0;
            for (;;) {
                const res = await ConnectionManager.fetchLogChunk(fmt.value, i);
                if (res.data) chunks.value = [...chunks.value, res.data];
                i++;
                if (!res.total || i >= res.total) break;
            }
            showToast(`Export pronto (${i} chunks)`, 'success');
        } catch (e) {
            showToast(e?.message || 'Falha no export', 'error');
        } finally {
            busy.value = false;
        }
    }

    function download() {
        const text = chunks.value.join('');
        if (!text) { showToast('Nada pra baixar — faça o export primeiro', 'error'); return; }
        const blob = new Blob([text], { type: fmt.value === 'json' ? 'application/json' : 'text/csv' });
        const url  = URL.createObjectURL(blob);
        const a    = document.createElement('a');
        a.href = url;
        a.download = `brewlog.${fmt.value}`;
        a.click();
        URL.revokeObjectURL(url);
    }

    return (
        <WizardSheet title="Brew log" onClose={onClose}
                     footer={<button class="btn btn-sm" onClick={onClose}>Fechar</button>}>
            <section class="space-y-2">
                <div class="flex items-center justify-between text-sm">
                    <span class="text-base-content/70">Gravação</span>
                    <span class={`badge badge-sm ${brewLogActive.value ? 'badge-success' : 'badge-ghost'}`}>
                        {brewLogActive.value ? 'ativa' : 'parada'}
                    </span>
                </div>
                <div class="text-xs text-base-content/60">
                    Entradas atuais: <span class="font-mono">{brewLogEntries.value}</span>
                </div>
                <div class="grid grid-cols-2 gap-2">
                    <button class="btn btn-primary btn-sm"
                            disabled={brewLogActive.value}
                            onClick={() => ConnectionManager.startBrewLog()
                                .then(() => showToast('Log iniciado'))
                                .catch(() => {})}>
                        Iniciar
                    </button>
                    <button class="btn btn-ghost btn-sm"
                            disabled={!brewLogActive.value}
                            onClick={() => ConnectionManager.stopBrewLog()
                                .then(() => showToast('Log parado'))
                                .catch(() => {})}>
                        Parar
                    </button>
                </div>
            </section>

            <div class="divider my-1"/>

            <section class="space-y-2">
                <h4 class="font-semibold text-sm">Exportar</h4>
                <div class="flex gap-2 items-center">
                    <label class="label cursor-pointer gap-1">
                        <input type="radio" name="fmt" class="radio radio-sm"
                               checked={fmt.value === 'csv'}
                               onChange={() => { fmt.value = 'csv'; chunks.value = []; }}/>
                        <span class="label-text text-sm">CSV</span>
                    </label>
                    <label class="label cursor-pointer gap-1">
                        <input type="radio" name="fmt" class="radio radio-sm"
                               checked={fmt.value === 'json'}
                               onChange={() => { fmt.value = 'json'; chunks.value = []; }}/>
                        <span class="label-text text-sm">JSON</span>
                    </label>
                </div>

                <div class="grid grid-cols-2 gap-2">
                    <button class="btn btn-outline btn-sm" disabled={busy.value} onClick={fetchAll}>
                        {busy.value ? 'Buscando…' : 'Buscar'}
                    </button>
                    <button class="btn btn-primary btn-sm"
                            disabled={!chunks.value.length} onClick={download}>
                        Baixar
                    </button>
                </div>

                {chunks.value.length > 0 && (
                    <div class="text-xs text-base-content/50">
                        {chunks.value.length} chunks (~{chunks.value.join('').length} bytes)
                    </div>
                )}
            </section>
        </WizardSheet>
    );
}
