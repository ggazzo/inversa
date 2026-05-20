// WizardEquipment.tsx — P13 thermal params editor.
import { useEffect, useState } from 'react';
import { Button, Card, Input, Label, Paragraph, Text, XStack, YStack } from 'tamagui';
import { ConnectionManager } from '@inversa/services';
import {
    showToast,
    ambientEffectiveC, ambientSource, ambientSensorOk,
    lidState, lossTuneActive,
} from '@inversa/stores';
import { useSignals } from '@preact/signals-react/runtime';
import { WizardSheet } from './WizardSheet';

// Stored as strings so partial entries ("-", "0.", ".5") survive a
// re-render without being eaten by parseFloat. parseFloat happens at
// save() time; anything that fails to parse falls back to the persisted
// value (so a blank ambient field doesn't silently send NaN).
interface FormState {
    volumeL: string; powerW: string; ambientC: string;
    diameterM: string;
    lossCoeffLidOn: string;
    lossCoeffLidOff: string;
    tempOffsetC: string;
}

const FIELDS: Array<{ key: keyof FormState; label: string; min: number; max: number; step: number; hint: string }> = [
    { key: 'volumeL',         label: 'Volume (L)',                  min: 1,    max: 200,   step: 0.5,  hint: '1 - 200' },
    { key: 'powerW',          label: 'Potência (W)',                min: 500,  max: 10000, step: 50,   hint: '500 - 10000' },
    { key: 'ambientC',        label: 'Ambiente (°C)',               min: -10,  max: 50,    step: 1,    hint: '-10 - 50' },
    { key: 'diameterM',       label: 'Diâmetro (m)',                min: 0.1,  max: 1.0,   step: 0.01, hint: '0.1 - 1.0' },
    { key: 'lossCoeffLidOn',  label: 'Coef. perda — tampa fechada', min: 1,    max: 50,    step: 0.5,  hint: '1 - 50 W/m²K' },
    { key: 'lossCoeffLidOff', label: 'Coef. perda — tampa aberta',  min: 1,    max: 50,    step: 0.5,  hint: '1 - 50 W/m²K' },
    { key: 'tempOffsetC',     label: 'Offset sensor (°C)',          min: -10,  max: 10,    step: 0.1,  hint: '-10 - 10  •  ajuste rápido; para calibração com pontos múltiplos use "Calibrar sensor"' },
];

function num(s: string, fallback: number): number {
    const v = parseFloat(s);
    return Number.isFinite(v) ? v : fallback;
}

interface Props { open: boolean; onClose: () => void }

export function WizardEquipment({ open, onClose }: Props) {
    useSignals();
    const [form, setForm]               = useState<FormState | null>(null);
    const [persistedForm, setPersistedForm] = useState<{
        volumeL: number; powerW: number; ambientC: number;
        diameterM: number;
        lossCoeffLidOn: number; lossCoeffLidOff: number;
        offset: number; slope: number;
        ambientSource: 'manual' | 'sensor';
        lidState: 'lidOn' | 'lidOff';
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
                const lossOn  = thermal.lossCoeffLidOn  ?? thermal.lossCoeff ?? 10;
                const lossOff = thermal.lossCoeffLidOff ?? thermal.lossCoeff ?? 10;
                const ambSrc  = (thermal.ambientSource as 'manual' | 'sensor') ?? 'manual';
                const lid     = (thermal.lidState as 'lidOn' | 'lidOff') ?? 'lidOn';
                setForm({
                    volumeL:         String(thermal.volumeL),
                    powerW:          String(thermal.powerW),
                    ambientC:        String(thermal.ambientC),
                    diameterM:       String(thermal.diameterM),
                    lossCoeffLidOn:  String(lossOn),
                    lossCoeffLidOff: String(lossOff),
                    tempOffsetC:     String(offset),
                });
                setPersistedForm({
                    volumeL: thermal.volumeL, powerW: thermal.powerW,
                    ambientC: thermal.ambientC, diameterM: thermal.diameterM,
                    lossCoeffLidOn: lossOn, lossCoeffLidOff: lossOff,
                    offset, slope,
                    ambientSource: ambSrc, lidState: lid,
                });
                setPersisted(!!thermal.persisted);
                setCalPersisted(!!cal?.persisted);
            })
            .catch((e: any) => showToast(e?.message || 'Falha ao ler', 'error'))
            .finally(() => setLoading(false));
    }, [open]);

    function save() {
        if (!form || !persistedForm) return;
        const volumeL   = num(form.volumeL,         persistedForm.volumeL);
        const powerW    = num(form.powerW,          persistedForm.powerW);
        const ambientC  = num(form.ambientC,        persistedForm.ambientC);
        const diameterM = num(form.diameterM,       persistedForm.diameterM);
        const lossOn    = num(form.lossCoeffLidOn,  persistedForm.lossCoeffLidOn);
        const lossOff   = num(form.lossCoeffLidOff, persistedForm.lossCoeffLidOff);
        const offset    = num(form.tempOffsetC,     persistedForm.offset);
        Promise.all([
            ConnectionManager.setThermalParams({
                volumeL, powerW, ambientC, diameterM,
                lossCoeffLidOn: lossOn, lossCoeffLidOff: lossOff,
                ambientSource: persistedForm.ambientSource,
                lidState: persistedForm.lidState,
            }),
            ConnectionManager.setTempCalibration(persistedForm.slope, offset),
        ])
            .then(() => {
                showToast('Parâmetros salvos', 'success');
                setPersisted(true);
                setCalPersisted(true);
            })
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    function startTune(mode: 'lidOn' | 'lidOff') {
        ConnectionManager.startLossTune(mode)
            .then(() => { showToast('Auto-tune iniciado', 'success'); onClose(); })
            .catch((e: any) => showToast(e?.message || 'Falha ao iniciar', 'error'));
    }

    function setLid(mode: 'lidOn' | 'lidOff') {
        ConnectionManager.setLidState(mode)
            .then(() => showToast(`Tampa: ${mode === 'lidOn' ? 'fechada' : 'aberta'}`, 'success'))
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    const ambBadge = ambientSource.value === 'sensor'
        ? (ambientSensorOk.value ? 'Sensor (ok)' : 'Sensor (stale → manual)')
        : 'Manual';

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
                    <XStack jc="space-between" paddingVertical={2} ai="center">
                        <Label htmlFor={`fld-${f.key}`}>{f.label}</Label>
                        <XStack ai="center" gap="$2">
                            {f.key === 'ambientC' && (
                                <Text fontSize="$1" fontWeight="700"
                                    color={ambientSource.value === 'sensor' && ambientSensorOk.value
                                            ? '$holding' : '$paused'}>
                                    ● {ambBadge}
                                </Text>
                            )}
                            <Text fontSize="$1" opacity={0.4}>{f.hint}</Text>
                        </XStack>
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

            {form && (
                <Card bordered padded marginTop="$2">
                    <YStack gap="$2">
                        <Text fontWeight="700">Tampa em uso</Text>
                        <XStack gap="$2">
                            <Button size="$3"
                                theme={lidState.value === 'lidOn' ? 'active' : undefined}
                                variant={lidState.value === 'lidOn' ? undefined : 'outlined'}
                                onPress={() => setLid('lidOn')}>
                                Fechada
                            </Button>
                            <Button size="$3"
                                theme={lidState.value === 'lidOff' ? 'active' : undefined}
                                variant={lidState.value === 'lidOff' ? undefined : 'outlined'}
                                onPress={() => setLid('lidOff')}>
                                Aberta
                            </Button>
                        </XStack>
                        <Paragraph fontSize="$1" theme="alt2">
                            Define qual coeficiente o PID e o agendador usam agora.
                        </Paragraph>
                    </YStack>
                </Card>
            )}

            {form && (
                <Card bordered padded>
                    <YStack gap="$2">
                        <Text fontWeight="700">Auto-tune do coef. de perda</Text>
                        <Paragraph fontSize="$1" theme="alt2">
                            Aquece o líquido ~50°C acima do ambiente, desliga a SSR,
                            mede a curva de resfriamento e calcula h via Newton.
                            Duração tipica: 10-15 min. Faça em ambiente estável.
                        </Paragraph>
                        <XStack gap="$2">
                            <Button size="$3" disabled={lossTuneActive.value}
                                onPress={() => startTune('lidOn')}>
                                Calibrar tampa fechada
                            </Button>
                            <Button size="$3" disabled={lossTuneActive.value}
                                onPress={() => startTune('lidOff')}>
                                Calibrar tampa aberta
                            </Button>
                        </XStack>
                        {lossTuneActive.value && (
                            <Text fontSize="$1" opacity={0.7}>Auto-tune já em execução.</Text>
                        )}
                    </YStack>
                </Card>
            )}
        </WizardSheet>
    );
}
