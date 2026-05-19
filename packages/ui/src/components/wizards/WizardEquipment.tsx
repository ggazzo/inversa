// WizardEquipment.tsx — P13 thermal params editor.
import { useEffect, useState } from 'react';
import { Button, Input, Label, Paragraph, Text, XStack, YStack } from 'tamagui';
import { ConnectionManager } from '@inversa/services';
import { showToast } from '@inversa/stores';
import { WizardSheet } from './WizardSheet';

// Stored as strings so partial entries ("-", "0.", ".5") survive a
// re-render without being eaten by parseFloat. parseFloat happens at
// save() time; anything that fails to parse falls back to the persisted
// value (so a blank ambient field doesn't silently send NaN).
interface FormState {
    volumeL: string; powerW: string; ambientC: string;
    diameterM: string; lossCoeff: string;
    // Temperature calibration "fast path": this wizard only exposes the
    // additive offset. The slope is held in `currentSlope` (state below)
    // and preserved across saves — to recompute it from multiple
    // calibration points the user opens the dedicated Calibration sheet.
    tempOffsetC: string;
}

const FIELDS: Array<{ key: keyof FormState; label: string; min: number; max: number; step: number; hint: string }> = [
    { key: 'volumeL',     label: 'Volume (L)',          min: 1,    max: 200,   step: 0.5,  hint: '1 - 200' },
    { key: 'powerW',      label: 'Potência (W)',        min: 500,  max: 10000, step: 50,   hint: '500 - 10000' },
    { key: 'ambientC',    label: 'Ambiente (°C)',       min: -10,  max: 50,    step: 1,    hint: '-10 - 50' },
    { key: 'diameterM',   label: 'Diâmetro (m)',        min: 0.1,  max: 1.0,   step: 0.01, hint: '0.1 - 1.0' },
    { key: 'lossCoeff',   label: 'Coef. perda (W/m²K)', min: 1,    max: 50,    step: 0.5,  hint: '1 - 50' },
    { key: 'tempOffsetC', label: 'Offset sensor (°C)',  min: -10,  max: 10,    step: 0.1,  hint: '-10 - 10  •  ajuste rápido; para calibração com pontos múltiplos use "Calibrar sensor"' },
];

function num(s: string, fallback: number): number {
    const v = parseFloat(s);
    return Number.isFinite(v) ? v : fallback;
}

interface Props { open: boolean; onClose: () => void }

export function WizardEquipment({ open, onClose }: Props) {
    const [form, setForm]               = useState<FormState | null>(null);
    // The slope is held in a hidden state alongside the form (this
    // wizard only edits the offset). The persisted snapshot lets save()
    // fall back to the last good number when a string fails to parse.
    const [persistedForm, setPersistedForm] = useState<{
        volumeL: number; powerW: number; ambientC: number;
        diameterM: number; lossCoeff: number; offset: number; slope: number;
    } | null>(null);
    const [persisted, setPersisted]     = useState(false);
    const [calPersisted, setCalPersisted] = useState(false);
    const [loading, setLoading]         = useState(true);

    useEffect(() => {
        if (!open) return;
        setLoading(true);
        // Two independent endpoints. Fire in parallel — the wizard is
        // unusable until both come back.
        Promise.all([
            ConnectionManager.getThermalParams(),
            ConnectionManager.getTempCalibration(),
        ])
            .then(([thermal, cal]: any[]) => {
                const offset = typeof cal?.offset === 'number' ? cal.offset : 0;
                const slope  = typeof cal?.slope  === 'number' ? cal.slope  : 1.0;
                setForm({
                    volumeL:     String(thermal.volumeL),
                    powerW:      String(thermal.powerW),
                    ambientC:    String(thermal.ambientC),
                    diameterM:   String(thermal.diameterM),
                    lossCoeff:   String(thermal.lossCoeff),
                    tempOffsetC: String(offset),
                });
                setPersistedForm({
                    volumeL: thermal.volumeL, powerW: thermal.powerW,
                    ambientC: thermal.ambientC, diameterM: thermal.diameterM,
                    lossCoeff: thermal.lossCoeff, offset, slope,
                });
                setPersisted(!!thermal.persisted);
                setCalPersisted(!!cal?.persisted);
            })
            .catch((e: any) => showToast(e?.message || 'Falha ao ler', 'error'))
            .finally(() => setLoading(false));
    }, [open]);

    function save() {
        if (!form || !persistedForm) return;
        const volumeL   = num(form.volumeL,   persistedForm.volumeL);
        const powerW    = num(form.powerW,    persistedForm.powerW);
        const ambientC  = num(form.ambientC,  persistedForm.ambientC);
        const diameterM = num(form.diameterM, persistedForm.diameterM);
        const lossCoeff = num(form.lossCoeff, persistedForm.lossCoeff);
        const offset    = num(form.tempOffsetC, persistedForm.offset);
        // Send both writes in parallel. Thermal params + the calibration
        // pair are persisted in independent NVS keys; ordering doesn't
        // matter, and either failing surfaces as a toast without rolling
        // the other back (firmware state stays consistent because each
        // SET handler validates on its own).
        Promise.all([
            ConnectionManager.setThermalParams(volumeL, powerW, ambientC, diameterM, lossCoeff),
            ConnectionManager.setTempCalibration(persistedForm.slope, offset),
        ])
            .then(() => {
                showToast('Parâmetros salvos', 'success');
                setPersisted(true);
                setCalPersisted(true);
            })
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    return (
        <WizardSheet title="Equipamento" open={open} onClose={onClose}
            footer={<>
                <Button size="$3" chromeless onPress={onClose}>Cancelar</Button>
                <Button size="$3" theme="active" disabled={!form || loading} onPress={save}>
                    Salvar (persistir)
                </Button>
            </>}>
            <Paragraph theme="alt2" fontSize="$1">
                Esses parâmetros calibram o feed-forward do PID e o cálculo do
                "agendar pronto às HH:MM". São persistidos em NVS e
                aplicados no próximo boot.{' '}
                <Text fontSize="$1" fontWeight="700"
                    color={persisted ? '$holding' : '$paused'}>
                    {persisted ? '● persistido' : '● default'}
                </Text>
            </Paragraph>

            {loading && <Text fontSize="$2" opacity={0.5}>Lendo do dispositivo…</Text>}

            {form && FIELDS.map((f) => (
                <YStack key={f.key}>
                    <XStack jc="space-between" paddingVertical={2}>
                        <Label htmlFor={`fld-${f.key}`}>{f.label}</Label>
                        <Text fontSize="$1" opacity={0.4}>{f.hint}</Text>
                    </XStack>
                    <Input
                        id={`fld-${f.key}`}
                        size="$3"
                        keyboardType="numbers-and-punctuation"
                        inputMode="decimal"
                        value={form[f.key]}
                        onChangeText={(s: string) => setForm({ ...form, [f.key]: s })}
                    />
                </YStack>
            ))}
        </WizardSheet>
    );
}
