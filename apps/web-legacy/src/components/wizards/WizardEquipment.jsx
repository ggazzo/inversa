// WizardEquipment.jsx — P13 thermal params editor.
// Loads current values via req:settings:thermal:get (firmware also
// returns a `persisted` flag indicating whether NVS has them yet).
// Saves via req:settings:thermal:set; the firmware applies sanity
// ranges (1-200 L, 500-10000 W, etc.) and returns res:error if out of
// bounds — we surface that as a toast.

import { useEffect } from 'preact/hooks';
import { useSignal } from '@preact/signals';
import { ConnectionManager } from '../../services/ConnectionManager';
import { showToast } from '../../stores/state';
import { WizardSheet } from './WizardSheet';

const FIELDS = [
    { key: 'volumeL',   label: 'Volume (L)',          min: 1,    max: 200,   step: 0.5, hint: '1 - 200' },
    { key: 'powerW',    label: 'Potência (W)',        min: 500,  max: 10000, step: 50,  hint: '500 - 10000' },
    { key: 'ambientC',  label: 'Ambiente (°C)',       min: -10,  max: 50,    step: 1,   hint: '-10 - 50' },
    { key: 'diameterM', label: 'Diâmetro (m)',        min: 0.1,  max: 1.0,   step: 0.01,hint: '0.1 - 1.0' },
    { key: 'lossCoeff', label: 'Coef. perda (W/m²K)', min: 1,    max: 50,    step: 0.5, hint: '1 - 50' },
];

export function WizardEquipment({ onClose }) {
    const form      = useSignal(null);
    const persisted = useSignal(false);
    const loading   = useSignal(true);

    useEffect(() => {
        ConnectionManager.getThermalParams()
            .then((r) => {
                form.value = {
                    volumeL:   r.volumeL,
                    powerW:    r.powerW,
                    ambientC:  r.ambientC,
                    diameterM: r.diameterM,
                    lossCoeff: r.lossCoeff,
                };
                persisted.value = !!r.persisted;
            })
            .catch((e) => showToast(e?.message || 'Falha ao ler', 'error'))
            .finally(() => { loading.value = false; });
    }, []);

    function save() {
        const f = form.value;
        if (!f) return;
        ConnectionManager.setThermalParams(f.volumeL, f.powerW, f.ambientC, f.diameterM, f.lossCoeff)
            .then(() => { showToast('Parâmetros salvos', 'success'); persisted.value = true; })
            .catch((e) => showToast(e?.message || 'Falha', 'error'));
    }

    const dirty = false;  // We let user save anytime — no diff tracking required.

    return (
        <WizardSheet
            title="Equipamento"
            onClose={onClose}
            footer={
                <>
                    <button class="btn btn-ghost btn-sm" onClick={onClose}>Cancelar</button>
                    <button class="btn btn-primary btn-sm"
                            disabled={!form.value || loading.value}
                            onClick={save}>
                        Salvar (persistir)
                    </button>
                </>
            }
        >
            <p class="text-xs text-base-content/60">
                Esses parâmetros calibram o feed-forward do PID e o cálculo do
                "agendar pronto às HH:MM". São persistidos em NVS e
                aplicados no próximo boot.
                {' '}
                {persisted.value
                    ? <span class="badge badge-success badge-xs">persistido</span>
                    : <span class="badge badge-warning badge-xs">default</span>}
            </p>

            {loading.value && <div class="text-sm text-base-content/50">Lendo do dispositivo…</div>}

            {form.value && FIELDS.map((f) => (
                <label key={f.key} class="form-control">
                    <div class="label py-0.5">
                        <span class="label-text text-sm">{f.label}</span>
                        <span class="label-text-alt text-xs text-base-content/40">{f.hint}</span>
                    </div>
                    <input
                        type="number"
                        class="input input-sm input-bordered w-full"
                        min={f.min} max={f.max} step={f.step}
                        value={form.value[f.key]}
                        onInput={(e) => {
                            const v = parseFloat(e.currentTarget.value);
                            if (isNaN(v)) return;
                            form.value = { ...form.value, [f.key]: v };
                        }}
                    />
                </label>
            ))}
        </WizardSheet>
    );
}
