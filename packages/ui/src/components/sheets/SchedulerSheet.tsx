// SchedulerSheet.tsx — UI for the "be ready at HH:MM" feature.
//
// Lets the operator program the firmware to start heating ahead of a
// target wall-clock time so the kettle reaches a chosen temperature at
// the chosen minute. Uses the existing req:sched:set / req:sched:stop
// BLE protocol; the firmware decides when to actually start heating
// based on its own thermal model.
//
// UI shape mirrors WatchdogSheet — read-only status block at the top,
// editable form below, footer with action buttons.

import { useEffect, useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Input, Paragraph, Separator, Text, XStack, YStack } from 'tamagui';
import {
    isConnected,
    showToast,
    schedulerActive,
    schedulerStatus,
    schedulerTargetHour,
    schedulerTargetMinute,
    schedulerTargetTemp,
    schedulerVolume,
    rtcAvailable,
    rtcTimestamp,
    rtcNtpSynced,
} from '@brewpilot/stores';
import { ConnectionManager } from '@brewpilot/services';
import { WizardSheet } from '../wizards/WizardSheet';

interface Props { open: boolean; onClose: () => void }

// Mirror of the firmware-side range checks in
// CommandHandler.h::REQ_SCHEDULER_SET. Kept here for client-side
// validation; firmware re-validates on submit.
const RANGES = {
    hour: { min: 0,  max: 23  },
    min:  { min: 0,  max: 59  },
    temp: { min: 20, max: 100 },
    vol:  { min: 1,  max: 100 },
} as const;

function pad(n: number): string { return String(n).padStart(2, '0'); }

function fmtTimestamp(unix: number): string {
    if (!unix) return '—';
    const d = new Date(unix * 1000);
    return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
}

function statusLabel(active: boolean, status: string): { text: string; tone: string } {
    if (!active) return { text: 'Inativo', tone: '$color' };
    switch (status) {
        case 'waiting':  return { text: '⏳ Aguardando início', tone: '$paused' };
        case 'heating':  return { text: '🔥 Aquecendo', tone: '$holding' };
        case 'ready':    return { text: '✅ Pronto', tone: '$holding' };
        case 'overdue':  return { text: '⚠️ Atrasado', tone: '$error' };
        default:         return { text: status || 'Ativo', tone: '$color' };
    }
}

export function SchedulerSheet({ open, onClose }: Props) {
    useSignals();

    const connected = isConnected.value;
    const active    = schedulerActive.value;
    const status    = schedulerStatus.value;
    const tH        = schedulerTargetHour.value;
    const tM        = schedulerTargetMinute.value;
    const tT        = schedulerTargetTemp.value;
    const tV        = schedulerVolume.value;

    // Local draft state. We do NOT mirror the active schedule into the
    // form on every render — the operator is configuring the NEXT one.
    // We seed the form when the sheet opens.
    const [hour,   setHour]   = useState<string>('07');
    const [minute, setMinute] = useState<string>('00');
    const [temp,   setTemp]   = useState<string>('65');
    const [volume, setVolume] = useState<string>('25');
    const [busy,   setBusy]   = useState(false);

    useEffect(() => {
        if (!open) return;
        // If a schedule is already active, seed the form with its
        // values so a "save again" pass keeps things consistent. If not,
        // keep whatever the operator last typed.
        if (active) {
            setHour(pad(tH));
            setMinute(pad(tM));
            setTemp(String(tT));
            setVolume(String(tV));
        }
    }, [open]);  // eslint-disable-line react-hooks/exhaustive-deps

    function validate(): { hour: number; min: number; temp: number; vol: number } | string {
        const h = parseInt(hour, 10);
        const m = parseInt(minute, 10);
        const t = parseFloat(temp);
        const v = parseFloat(volume);
        if (!Number.isFinite(h) || h < RANGES.hour.min || h > RANGES.hour.max)
            return `Hora deve estar entre ${RANGES.hour.min} e ${RANGES.hour.max}`;
        if (!Number.isFinite(m) || m < RANGES.min.min || m > RANGES.min.max)
            return `Minuto deve estar entre ${RANGES.min.min} e ${RANGES.min.max}`;
        if (!Number.isFinite(t) || t < RANGES.temp.min || t > RANGES.temp.max)
            return `Temperatura deve estar entre ${RANGES.temp.min} e ${RANGES.temp.max} °C`;
        if (!Number.isFinite(v) || v < RANGES.vol.min || v > RANGES.vol.max)
            return `Volume deve estar entre ${RANGES.vol.min} e ${RANGES.vol.max} L`;
        return { hour: h, min: m, temp: t, vol: v };
    }

    async function onSchedule(): Promise<void> {
        const r = validate();
        if (typeof r === 'string') { showToast(r, 'warning'); return; }
        setBusy(true);
        try {
            await ConnectionManager.setScheduler(r.hour, r.min, r.temp, r.vol);
            showToast(`Agendado para ${pad(r.hour)}:${pad(r.min)} @ ${r.temp}°C`, 'success');
        } catch (e: any) {
            showToast(e?.message ?? 'Falha ao agendar', 'error');
        } finally {
            setBusy(false);
        }
    }

    async function onCancel(): Promise<void> {
        setBusy(true);
        try {
            await ConnectionManager.stopScheduler();
            showToast('Agendamento cancelado', 'info');
        } catch (e: any) {
            showToast(e?.message ?? 'Falha ao cancelar', 'error');
        } finally {
            setBusy(false);
        }
    }

    const verdict = statusLabel(active, status);
    const rtcUnsynced = !rtcNtpSynced.value;

    return (
        <WizardSheet
            title="Agendar temperatura"
            open={open}
            onClose={onClose}
            footer={
                <>
                    <Button onPress={onClose} chromeless>Fechar</Button>
                    {active && (
                        <Button
                            theme="red"
                            onPress={onCancel}
                            disabled={!connected || busy}
                        >
                            Cancelar agendamento
                        </Button>
                    )}
                    <Button
                        theme="active"
                        onPress={onSchedule}
                        disabled={!connected || busy}
                    >
                        {active ? 'Atualizar' : 'Agendar'}
                    </Button>
                </>
            }
        >
            {/* Status block */}
            <YStack gap="$2">
                <Text fontWeight="600">Status</Text>
                <XStack gap="$2" ai="center">
                    <Text color={verdict.tone as any}>{verdict.text}</Text>
                </XStack>
                {active && (
                    <Paragraph theme="alt2">
                        Alvo: {pad(tH)}:{pad(tM)} @ {tT.toFixed(1)} °C ({tV.toFixed(0)} L)
                    </Paragraph>
                )}
                <Paragraph theme="alt2">
                    Relógio do controlador: {rtcAvailable.value
                        ? fmtTimestamp(rtcTimestamp.value)
                        : 'indisponível'}
                    {rtcAvailable.value && rtcUnsynced && ' (sem NTP)'}
                </Paragraph>
            </YStack>

            <Separator />

            {/* Form */}
            <YStack gap="$3">
                <Text fontWeight="600">Programar</Text>
                <YStack gap="$2">
                    <Text>Horário</Text>
                    <XStack gap="$2" ai="center">
                        <Input
                            value={hour}
                            onChangeText={setHour}
                            keyboardType="number-pad"
                            inputMode="numeric"
                            width={72}
                            textAlign="center"
                            placeholder="HH"
                            aria-label="Hora"
                        />
                        <Text>:</Text>
                        <Input
                            value={minute}
                            onChangeText={setMinute}
                            keyboardType="number-pad"
                            inputMode="numeric"
                            width={72}
                            textAlign="center"
                            placeholder="MM"
                            aria-label="Minuto"
                        />
                    </XStack>
                </YStack>

                <YStack gap="$2">
                    <Text>Temperatura alvo (°C)</Text>
                    <Input
                        value={temp}
                        onChangeText={setTemp}
                        keyboardType="decimal-pad"
                        inputMode="decimal"
                        width={120}
                        placeholder="65"
                        aria-label="Temperatura"
                    />
                </YStack>

                <YStack gap="$2">
                    <Text>Volume aproximado (L)</Text>
                    <Input
                        value={volume}
                        onChangeText={setVolume}
                        keyboardType="decimal-pad"
                        inputMode="decimal"
                        width={120}
                        placeholder="25"
                        aria-label="Volume"
                    />
                    <Paragraph theme="alt2" fontSize="$1">
                        Usado pelo controlador para estimar quanto tempo antes
                        do alvo começar a aquecer.
                    </Paragraph>
                </YStack>
            </YStack>

            {!connected && (
                <Paragraph theme="red">
                    Conecte o controlador para programar um agendamento.
                </Paragraph>
            )}
        </WizardSheet>
    );
}
