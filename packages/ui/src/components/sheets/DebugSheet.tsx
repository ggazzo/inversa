// DebugSheet.tsx — live trace of inbound BLE messages + action
// buttons that fire common req:* commands. Helps when the link is
// up but the UI isn't reacting; you can see what's actually
// arriving and trigger probes from inside the app instead of from
// a Mac terminal.

import { useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Paragraph, Text, XStack, YStack } from 'tamagui';
import { debugMessages, showToast, signalRssi, isConnected, deviceName } from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { WizardSheet } from '../wizards/WizardSheet';

interface Props { open: boolean; onClose: () => void }
interface ActionResult { label: string; status: 'pending' | 'ok' | 'err'; ms?: number; detail?: string }

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
    const [lastResult, setLastResult] = useState<ActionResult | null>(null);
    const msgs = debugMessages.value;
    // Newest first.
    const ordered = [...msgs].reverse();

    async function fire(label: string, run: () => Promise<any>) {
        console.log(`[debug] fire ${label}`);
        setLastResult({ label, status: 'pending' });
        const t0 = Date.now();
        try {
            const res = await run();
            const ms = Date.now() - t0;
            console.log(`[debug] ${label} ✓ ${ms}ms`, res);
            setLastResult({ label, status: 'ok', ms, detail: JSON.stringify(res).slice(0, 200) });
            showToast(`${label} ✓ ${ms}ms`, 'success', 2000);
        } catch (e: any) {
            const ms = Date.now() - t0;
            const msg = e?.message ?? String(e);
            console.warn(`[debug] ${label} ✗ ${ms}ms: ${msg}`);
            setLastResult({ label, status: 'err', ms, detail: msg });
            showToast(`${label}: ${msg}`, 'error', 4000);
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
                    {lastResult && (
                        <YStack padding="$2" br="$2" backgroundColor="$backgroundFocus" gap={2}>
                            <XStack gap="$2" ai="center">
                                <Text fontSize="$1" fontFamily="$mono" fontWeight="700">
                                    {lastResult.status === 'pending' ? '…' : lastResult.status === 'ok' ? '✓' : '✗'}
                                </Text>
                                <Text fontSize="$1" fontFamily="$mono">
                                    {lastResult.label}
                                </Text>
                                {lastResult.ms != null && (
                                    <Text fontSize="$1" opacity={0.6} fontFamily="$mono">
                                        {lastResult.ms}ms
                                    </Text>
                                )}
                            </XStack>
                            {lastResult.detail && (
                                <Text fontSize="$1" opacity={0.6} fontFamily="$mono" numberOfLines={3}>
                                    {lastResult.detail}
                                </Text>
                            )}
                        </YStack>
                    )}
                </YStack>

                {/* Per-tp counts since last clear */}
                {msgs.length > 0 && (
                    <YStack gap={2}>
                        <Text fontSize="$1" opacity={0.6} fontWeight="600">Contagem por tipo</Text>
                        <XStack gap="$2" flexWrap="wrap">
                            {Object.entries(msgs.reduce((acc: Record<string, number>, m) => {
                                acc[m.tp] = (acc[m.tp] || 0) + 1;
                                return acc;
                            }, {})).map(([tp, n]) => (
                                <Text key={tp} fontSize="$1" fontFamily="$mono" opacity={0.7}>
                                    {tp}={n}
                                </Text>
                            ))}
                        </XStack>
                    </YStack>
                )}

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
