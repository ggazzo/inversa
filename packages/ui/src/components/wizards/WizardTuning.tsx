// WizardTuning.tsx — PID gains + AutoTune trigger.
import { useEffect, useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Input, Label, Paragraph, Separator, Text, XStack, YStack } from 'tamagui';
import { pidKp, pidKi, pidKd, autoTuneActive, currentTemp, showToast } from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { WizardSheet } from './WizardSheet';

interface Props { open: boolean; onClose: () => void }

export function WizardTuning({ open, onClose }: Props) {
    useSignals();
    const [kp, setKp] = useState(pidKp.value);
    const [ki, setKi] = useState(pidKi.value);
    const [kd, setKd] = useState(pidKd.value);
    const [atTarget, setAtTarget] = useState(Math.max(40, Math.round(currentTemp.value + 10)));
    const [loaded, setLoaded] = useState(false);

    useEffect(() => {
        if (!open) return;
        ConnectionManager.getSettings()
            .then((r: any) => {
                setKp(r.kp); setKi(r.ki); setKd(r.kd);
                pidKp.value = r.kp; pidKi.value = r.ki; pidKd.value = r.kd;
                setLoaded(true);
            })
            .catch((e: any) => showToast(e?.message || 'Falha ao ler', 'error'));
    }, [open]);

    function save() {
        ConnectionManager.saveSettings(kp, ki, kd)
            .then(() => {
                pidKp.value = kp; pidKi.value = ki; pidKd.value = kd;
                showToast('PID salvo', 'success');
            })
            .catch((e: any) => showToast(e?.message || 'Falha (range?)', 'error'));
    }

    function startAt() {
        if (atTarget < 30 || atTarget > 100) { showToast('AutoTune: 30-100°C', 'error'); return; }
        ConnectionManager.startAutoTune(atTarget)
            .then(() => { showToast('AutoTune iniciado', 'success'); onClose(); })
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    return (
        <WizardSheet title="PID & AutoTune" open={open} onClose={onClose}
            footer={<Button size="$3" onPress={onClose}>Fechar</Button>}>
            {/* PID */}
            <YStack gap="$2">
                <Text fontWeight="700">Ganhos PID</Text>
                <PidField label="Kp" max={500}   value={kp} onChange={setKp} />
                <PidField label="Ki" max={10}    value={ki} onChange={setKi} />
                <PidField label="Kd" max={50000} value={kd} onChange={setKd} />
                <Button size="$3" theme="active" disabled={!loaded} onPress={save}>
                    Salvar & persistir
                </Button>
                <Paragraph fontSize="$1" opacity={0.5}>
                    Limites do firmware: Kp ∈ [1, 500], Ki ∈ [0.0001, 10], Kd ∈ [0, 50000].
                </Paragraph>
            </YStack>

            <Separator marginVertical="$2" />

            {/* AutoTune */}
            <YStack gap="$2">
                <XStack ai="center" gap="$2">
                    <Text fontWeight="700">AutoTune (Ziegler-Nichols)</Text>
                    {autoTuneActive.value && (
                        <Text fontSize="$1" backgroundColor="$paused" color="black"
                            paddingHorizontal="$2" paddingVertical={2} br="$10">
                            rodando
                        </Text>
                    )}
                </XStack>
                <Paragraph fontSize="$1" opacity={0.6}>
                    Bang-bang em torno do alvo até 4 ciclos. Encontra Kp/Ki/Kd
                    e persiste automaticamente. Tipicamente 15-30 min.
                </Paragraph>
                <Label>Temperatura-alvo</Label>
                <Input size="$3" keyboardType="numeric"
                    value={String(atTarget)}
                    onChangeText={(s: string) => {
                        const v = parseInt(s, 10);
                        if (!isNaN(v)) setAtTarget(v);
                    }} />
                <Button size="$3" theme="orange"
                    disabled={autoTuneActive.value} onPress={startAt}>
                    {autoTuneActive.value ? 'Já em execução' : 'Iniciar AutoTune'}
                </Button>
            </YStack>
        </WizardSheet>
    );
}

function PidField({ label, max, value, onChange }:
    { label: string; max: number; value: number; onChange: (n: number) => void }) {
    return (
        <YStack>
            <XStack jc="space-between" paddingVertical={2}>
                <Label>{label}</Label>
                <Text fontSize="$1" opacity={0.4}>≤ {max}</Text>
            </XStack>
            <Input size="$3" fontFamily="$mono" keyboardType="numeric"
                value={String(value)}
                onChangeText={(s: string) => onChange(parseFloat(s) || 0)} />
        </YStack>
    );
}
