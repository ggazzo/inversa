// Scheduled.tsx — home view while a scheduler ("be ready at HH:MM") is
// armed. Owns the screen even after the firmware flips mode=Manual to
// actually heat (P17 in SchedulerPlugin::startHeating), so the operator
// always has a single, prominent Cancel and a live picture of the two
// targets that matter:
//   • clock target — the HH:MM the operator chose
//   • temperature target — the °C the operator chose
//
// Both are paired with a "now" value so the screen reads as a countdown
// (time) and a progress (temp).

import { useEffect, useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Paragraph, Separator, Text, XStack, YStack } from 'tamagui';
import {
    isConnected,
    currentTemp,
    rtcTimestamp,
    rtcAvailable,
    schedulerTargetHour,
    schedulerTargetMinute,
    schedulerTargetTemp,
    schedulerVolume,
    schedulerStatus,
    showToast,
} from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';

function pad(n: number): string { return String(n).padStart(2, '0'); }

// Format remaining time in a friendly way: "12 min", "1 h 4 min", "<1 min".
function fmtRemaining(seconds: number): string {
    if (!Number.isFinite(seconds) || seconds <= 0) return 'em instantes';
    const min = Math.round(seconds / 60);
    if (min < 1) return '<1 min';
    if (min < 60) return `${min} min`;
    const h = Math.floor(min / 60);
    const m = min % 60;
    return m === 0 ? `${h} h` : `${h} h ${m} min`;
}

// Compute seconds from "now" (controller wall clock) to the next
// occurrence of HH:MM. If the target HH:MM is already past today, the
// firmware schedules it for tomorrow, so we mirror that here.
function secondsUntilTarget(nowUnix: number, targetH: number, targetM: number): number {
    const d = new Date(nowUnix * 1000);
    const target = new Date(d);
    target.setHours(targetH, targetM, 0, 0);
    let delta = (target.getTime() - d.getTime()) / 1000;
    if (delta < 0) delta += 24 * 3600;
    return delta;
}

// Tick the local "second" so countdown updates without waiting on the
// next telemetry frame. Telemetry pushes rtcTimestamp once per second
// anyway, but on a slow link this keeps the digit moving.
function useSecondTick() {
    const [, force] = useState(0);
    useEffect(() => {
        const t = setInterval(() => force((n) => (n + 1) & 0xffff), 1000);
        return () => clearInterval(t);
    }, []);
}

export function Scheduled() {
    useSignals();
    useSecondTick();
    const [busy, setBusy] = useState(false);

    const h = schedulerTargetHour.value;
    const m = schedulerTargetMinute.value;
    const tT = schedulerTargetTemp.value;
    const vol = schedulerVolume.value;
    const cur = currentTemp.value;
    const rtcOk = rtcAvailable.value && rtcTimestamp.value > 0;
    const nowUnix = rtcTimestamp.value;
    const status = schedulerStatus.value;
    const connected = isConnected.value;

    // Countdown
    const secsLeft = rtcOk ? secondsUntilTarget(nowUnix, h, m) : NaN;
    const remaining = fmtRemaining(secsLeft);

    // Temperature progress — anchor "from" at 22°C since we don't keep
    // the starting temp; gives roughly the right visual without keeping
    // extra state. The number reads correctly even when the anchor is
    // off (it just compresses or expands the bar fill).
    const tempFrom = 22;
    const tempDelta = Math.max(0.1, tT - tempFrom);
    const tempPct = Math.max(0, Math.min(100, ((cur - tempFrom) / tempDelta) * 100));
    const tempReached = cur >= tT - 0.5;

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

    const nowH = rtcOk ? new Date(nowUnix * 1000).getHours()   : null;
    const nowM = rtcOk ? new Date(nowUnix * 1000).getMinutes() : null;

    return (
        <YStack gap="$4" ai="stretch">
            {/* Big status banner */}
            <YStack ai="center" gap="$1" paddingVertical="$2">
                <Text fontSize="$3" theme="alt2">Agendamento</Text>
                <Text fontSize="$8" fontWeight="700">
                    {status || (tempReached ? 'Pronto' : 'Aguardando')}
                </Text>
            </YStack>

            {/* Time target — now vs target HH:MM with countdown */}
            <YStack
                padding="$3"
                gap="$2"
                borderWidth={1}
                borderColor="$borderColor"
                br="$4"
            >
                <Text fontSize="$2" theme="alt2">Horário alvo</Text>
                <XStack ai="center" jc="space-between">
                    <YStack ai="flex-start" gap="$1">
                        <Text fontSize="$1" theme="alt2">Agora</Text>
                        <Text fontFamily="$mono" fontSize="$7">
                            {nowH !== null ? `${pad(nowH)}:${pad(nowM!)}` : '--:--'}
                        </Text>
                    </YStack>
                    <Text fontSize="$5" theme="alt2">→</Text>
                    <YStack ai="flex-end" gap="$1">
                        <Text fontSize="$1" theme="alt2">Alvo</Text>
                        <Text fontFamily="$mono" fontSize="$7">
                            {pad(h)}:{pad(m)}
                        </Text>
                    </YStack>
                </XStack>
                <Text fontSize="$2" theme="alt2" ta="center">
                    {rtcOk ? `Faltam ${remaining}` : 'Relógio do controlador indisponível'}
                </Text>
            </YStack>

            {/* Temp target — current vs target with bar */}
            <YStack
                padding="$3"
                gap="$2"
                borderWidth={1}
                borderColor="$borderColor"
                br="$4"
            >
                <Text fontSize="$2" theme="alt2">Temperatura alvo</Text>
                <XStack ai="center" jc="space-between">
                    <YStack ai="flex-start" gap="$1">
                        <Text fontSize="$1" theme="alt2">Atual</Text>
                        <Text fontFamily="$mono" fontSize="$7">
                            {cur.toFixed(1)}°C
                        </Text>
                    </YStack>
                    <Text fontSize="$5" theme="alt2">→</Text>
                    <YStack ai="flex-end" gap="$1">
                        <Text fontSize="$1" theme="alt2">Alvo</Text>
                        <Text fontFamily="$mono" fontSize="$7">
                            {tT.toFixed(1)}°C
                        </Text>
                    </YStack>
                </XStack>
                <YStack
                    height={6}
                    br="$10"
                    backgroundColor="$borderColor"
                    overflow="hidden"
                >
                    <YStack
                        height="100%"
                        width={`${tempPct}%`}
                        backgroundColor={tempReached ? '$holding' : '$paused'}
                    />
                </YStack>
                <Text fontSize="$2" theme="alt2" ta="center">
                    Volume: {vol.toFixed(0)} L
                </Text>
            </YStack>

            <Separator />

            <Paragraph theme="alt2" ta="center" fontSize="$2">
                Modo Manual, AutoTune e início de receita ficam bloqueados
                até o agendamento concluir. Cancele para desbloquear.
            </Paragraph>

            <Button
                size="$6"
                theme="red"
                onPress={onCancel}
                disabled={!connected || busy}
                aria-label="Cancelar agendamento"
            >
                <Text fontSize="$5" fontWeight="700">Cancelar agendamento</Text>
            </Button>
        </YStack>
    );
}
