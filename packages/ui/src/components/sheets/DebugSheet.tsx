// DebugSheet.tsx — live trace of inbound BLE messages + action
// buttons that fire common req:* commands. Helps when the link is
// up but the UI isn't reacting; you can see what's actually
// arriving and trigger probes from inside the app instead of from
// a Mac terminal.

import { useSignals } from '@preact/signals-react/runtime';
import { Button, Paragraph, Text, XStack, YStack } from 'tamagui';
import { debugMessages, showToast, signalRssi, isConnected, deviceName } from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { WizardSheet } from '../wizards/WizardSheet';

interface Props { open: boolean; onClose: () => void }

const ACTIONS: { label: string; run: () => Promise<any> }[] = [
    { label: 'req:info',           run: () => ConnectionManager.getInfo() },
    { label: 'req:settings:get',   run: () => ConnectionManager.getSettings() },
    { label: 'req:recipe:list',    run: () => ConnectionManager.listRecipes() },
    { label: 'req:wifi:status',    run: () => ConnectionManager.getWiFiStatus() },
    { label: 'req:rtc:get',        run: () => ConnectionManager.getRtcTime() },
    { label: 'req:settings:thermal:get', run: () => ConnectionManager.getThermalParams() },
];

function fmtTime(ts: number): string {
    const d = new Date(ts);
    const hh = String(d.getHours()).padStart(2, '0');
    const mm = String(d.getMinutes()).padStart(2, '0');
    const ss = String(d.getSeconds()).padStart(2, '0');
    const ms = String(d.getMilliseconds()).padStart(3, '0');
    return `${hh}:${mm}:${ss}.${ms}`;
}

export function DebugSheet({ open, onClose }: Props) {
    useSignals();
    const msgs = debugMessages.value;
    // Newest first.
    const ordered = [...msgs].reverse();

    async function fire(label: string, run: () => Promise<any>) {
        try {
            const t0 = Date.now();
            const res = await run();
            const ms = Date.now() - t0;
            showToast(`${label} ✓ ${ms}ms`, 'success', 2000);
            console.log(`[debug] ${label} ✓ ${ms}ms`, res);
        } catch (e: any) {
            showToast(`${label}: ${e?.message ?? 'falha'}`, 'error', 4000);
        }
    }

    return (
        <WizardSheet title="Debug · BLE trace" open={open} onClose={onClose}
            footer={<Button size="$3" onPress={onClose}>Fechar</Button>}>
            <YStack gap="$3">
                {/* Status row */}
                <XStack gap="$2" ai="center" jc="space-between">
                    <Text fontSize="$1" opacity={0.6}>
                        {isConnected.value
                            ? `Conectado: ${deviceName.value || 'Inversa'}`
                            : 'Desconectado'}
                    </Text>
                    <Text fontSize="$1" fontFamily="$mono" opacity={0.5}>
                        RSSI: {signalRssi.value ?? '—'} dBm
                    </Text>
                </XStack>

                {/* Action grid — fire common req:* directly */}
                <YStack gap="$1.5">
                    <Text fontWeight="600" fontSize="$2">Disparar comandos</Text>
                    <XStack gap="$2" flexWrap="wrap">
                        {ACTIONS.map((a) => (
                            <Button key={a.label} size="$2" chromeless
                                disabled={!isConnected.value}
                                onPress={() => fire(a.label, a.run)}>
                                <Text fontFamily="$mono" fontSize="$1">{a.label}</Text>
                            </Button>
                        ))}
                    </XStack>
                </YStack>

                {/* Message trace */}
                <YStack gap="$1.5">
                    <XStack jc="space-between" ai="center">
                        <Text fontWeight="600" fontSize="$2">
                            Últimas mensagens ({msgs.length})
                        </Text>
                        <Button size="$1" chromeless
                            onPress={() => { debugMessages.value = []; }}>
                            <Text fontSize="$1">Limpar</Text>
                        </Button>
                    </XStack>
                    {ordered.length === 0 ? (
                        <Paragraph fontSize="$1" opacity={0.5}>
                            Nada recebido ainda.
                        </Paragraph>
                    ) : (
                        <YStack gap={2}>
                            {ordered.map((m, i) => (
                                <YStack key={msgs.length - i} padding="$1.5" br="$2"
                                        backgroundColor="$backgroundFocus">
                                    <XStack gap="$2" ai="baseline">
                                        <Text fontSize="$1" fontFamily="$mono" opacity={0.5}>
                                            {fmtTime(m.ts)}
                                        </Text>
                                        <Text fontSize="$1" fontFamily="$mono" fontWeight="700">
                                            {m.tp}
                                        </Text>
                                    </XStack>
                                    <Text fontSize="$1" fontFamily="$mono" opacity={0.6} numberOfLines={2}>
                                        {m.snippet}
                                    </Text>
                                </YStack>
                            ))}
                        </YStack>
                    )}
                </YStack>
            </YStack>
        </WizardSheet>
    );
}
