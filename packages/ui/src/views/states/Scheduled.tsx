// Scheduled.tsx — substate shown while a scheduler ("be ready at HH:MM")
// is armed but not yet heating. The brewing controller is going to take
// over the heater at the scheduled moment, so locking the other entry
// points (Iniciar Receita / Modo Manual / AutoTune) keeps the operator
// from kicking off something that would collide with the scheduled run.
//
// The only action allowed here is "Cancelar agendamento". Tapping it
// sends req:sched:stop; the next telemetry tick clears `schedulerActive`
// and the view falls back to Idle.

import { useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Paragraph, Separator, Text, XStack, YStack } from 'tamagui';
import {
    isConnected,
    schedulerTargetHour,
    schedulerTargetMinute,
    schedulerTargetTemp,
    schedulerVolume,
    schedulerStatus,
    showToast,
} from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';

function pad(n: number): string { return String(n).padStart(2, '0'); }

export function Scheduled() {
    useSignals();
    const [busy, setBusy] = useState(false);

    const h = schedulerTargetHour.value;
    const m = schedulerTargetMinute.value;
    const t = schedulerTargetTemp.value;
    const v = schedulerVolume.value;
    const status = schedulerStatus.value;
    const connected = isConnected.value;

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

    return (
        <YStack gap="$3" ai="stretch">
            <YStack gap="$1" ai="center" paddingVertical="$2">
                <Text fontSize="$3" theme="alt2">Agendamento ativo</Text>
                <Text fontSize="$10" fontWeight="700" fontFamily="$mono">
                    {pad(h)}:{pad(m)}
                </Text>
                <Text fontSize="$5" theme="alt2">
                    {t.toFixed(1)} °C · {v.toFixed(0)} L
                </Text>
                {status ? (
                    <Text fontSize="$3" theme="alt2" marginTop="$2">{status}</Text>
                ) : null}
            </YStack>

            <Separator />

            <Paragraph theme="alt2" ta="center">
                Modo Manual, AutoTune e início de receita ficam bloqueados
                até o agendamento concluir. Para usar outra função,
                cancele primeiro.
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
