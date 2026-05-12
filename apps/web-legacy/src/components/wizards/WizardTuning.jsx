// WizardTuning.jsx — PID gains + AutoTune trigger.
// Reading via req:settings:get; persisting via req:settings:set.
// AutoTune is a long-running operation owned by the firmware; we just
// kick it off and let the live state (autoTuneActive / autoTuneProgress)
// drive the BrewView's AutoTune sub-state.

import { useEffect } from 'preact/hooks';
import { useSignal } from '@preact/signals';
import {
    pidKp, pidKi, pidKd, autoTuneActive, currentTemp, showToast,
} from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';
import { WizardSheet } from './WizardSheet';

export function WizardTuning({ onClose }) {
    const kp = useSignal(pidKp.value);
    const ki = useSignal(pidKi.value);
    const kd = useSignal(pidKd.value);
    const atTarget = useSignal(Math.max(40, Math.round(currentTemp.value + 10)));
    const loaded = useSignal(false);

    useEffect(() => {
        ConnectionManager.getSettings()
            .then((r) => {
                kp.value = r.kp; ki.value = r.ki; kd.value = r.kd;
                pidKp.value = r.kp; pidKi.value = r.ki; pidKd.value = r.kd;
                loaded.value = true;
            })
            .catch((e) => showToast(e?.message || 'Falha ao ler', 'error'));
    }, []);

    function save() {
        ConnectionManager.saveSettings(kp.value, ki.value, kd.value)
            .then(() => {
                pidKp.value = kp.value; pidKi.value = ki.value; pidKd.value = kd.value;
                showToast('PID salvo', 'success');
            })
            .catch((e) => showToast(e?.message || 'Falha (range?)', 'error'));
    }

    function startAt() {
        if (atTarget.value < 30 || atTarget.value > 100) {
            showToast('AutoTune: 30-100°C', 'error'); return;
        }
        ConnectionManager.startAutoTune(atTarget.value)
            .then(() => { showToast('AutoTune iniciado', 'success'); onClose(); })
            .catch((e) => showToast(e?.message || 'Falha', 'error'));
    }

    return (
        <WizardSheet title="PID & AutoTune" onClose={onClose}
                     footer={<button class="btn btn-sm" onClick={onClose}>Fechar</button>}>
            {/* Manual PID */}
            <section class="space-y-2">
                <h4 class="font-semibold text-sm">Ganhos PID</h4>
                <PidField label="Kp" value={kp} step={0.1}  max={500}   />
                <PidField label="Ki" value={ki} step={0.001} max={10}    />
                <PidField label="Kd" value={kd} step={1}    max={50000} />
                <button class="btn btn-primary btn-sm w-full"
                        disabled={!loaded.value}
                        onClick={save}>
                    Salvar & persistir
                </button>
                <p class="text-[10px] text-base-content/50">
                    Os limites de range são aplicados pelo firmware:
                    Kp ∈ [1, 500], Ki ∈ [0.0001, 10], Kd ∈ [0, 50000].
                </p>
            </section>

            <div class="divider my-1"/>

            {/* AutoTune */}
            <section class="space-y-2">
                <h4 class="font-semibold text-sm flex items-center gap-2">
                    AutoTune (Ziegler-Nichols)
                    {autoTuneActive.value && <span class="badge badge-warning badge-xs">rodando</span>}
                </h4>
                <p class="text-xs text-base-content/60">
                    Bang-bang em torno do alvo até 4 ciclos. Encontra Kp/Ki/Kd
                    e persiste automaticamente. Tipicamente 15-30 min.
                </p>
                <label class="form-control">
                    <div class="label py-0.5"><span class="label-text text-sm">Temperatura-alvo</span></div>
                    <input type="number" class="input input-sm input-bordered"
                           min="30" max="100" step="1"
                           value={atTarget.value}
                           onInput={(e) => { atTarget.value = parseInt(e.currentTarget.value, 10) || 65; }} />
                </label>
                <button class="btn btn-warning btn-sm w-full"
                        disabled={autoTuneActive.value}
                        onClick={startAt}>
                    {autoTuneActive.value ? 'Já em execução' : 'Iniciar AutoTune'}
                </button>
            </section>
        </WizardSheet>
    );
}

function PidField({ label, value, step, max }) {
    return (
        <label class="form-control">
            <div class="label py-0.5">
                <span class="label-text text-sm">{label}</span>
                <span class="label-text-alt text-xs text-base-content/40">≤ {max}</span>
            </div>
            <input type="number" class="input input-sm input-bordered font-mono"
                   min="0" max={max} step={step}
                   value={value.value}
                   onInput={(e) => { value.value = parseFloat(e.currentTarget.value) || 0; }} />
        </label>
    );
}
