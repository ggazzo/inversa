// WatchdogSheet.tsx — Thermal Watchdog status + manual reset + limit
// configuration (001-thermal-watchdog T032).

import { useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Input, Paragraph, Switch, Text, XStack, YStack } from 'tamagui';
import {
    isConnected,
    showToast,
    watchdogSupported,
    watchdogArmed,
    watchdogTripped,
    watchdogTripCount,
    watchdogLastCause,
    watchdogLastTripUnix,
    watchdogHardStopC,
    watchdogAutoReset,
} from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { WizardSheet } from '../wizards/WizardSheet';

interface Props { open: boolean; onClose: () => void }

function fmtUnix(ts: number): string {
    if (!ts) return '—';
    return new Date(ts * 1000).toLocaleString('pt-BR');
}

export function WatchdogSheet({ open, onClose }: Props) {
    useSignals();
    const [resetting, setResetting] = useState(false);
    const [hardStopDraft, setHardStopDraft] = useState<string>('');
    const [autoResetDraft, setAutoResetDraft] = useState<boolean | null>(null);

    const supported = watchdogSupported.value;
    const tripped   = watchdogTripped.value;
    const armed     = watchdogArmed.value;
    const cause     = watchdogLastCause.value;
    const count     = watchdogTripCount.value;
    const lastUnix  = watchdogLastTripUnix.value;
    const hardStop  = watchdogHardStopC.value;
    const autoReset = autoResetDraft ?? watchdogAutoReset.value;

    async function handleReset() {
        if (resetting) return;
        setResetting(true);
        try {
            await ConnectionManager.resetWatchdog();
            showToast('Watchdog rearmado', 'success');
        } catch (e: any) {
            const msg = e?.message || 'falha';
            if (msg === 'wd_still_unsafe') {
                showToast('Temperatura ainda perigosa — espere esfriar', 'error', 6000);
            } else if (msg === 'wd_not_tripped') {
                showToast('Watchdog não está disparado', 'info');
            } else {
                showToast(`Reset falhou: ${msg}`, 'error');
            }
        } finally {
            setResetting(false);
        }
    }

    async function handleApply() {
        const cfg: Record<string, any> = {};
        const hs = parseFloat(hardStopDraft);
        if (hardStopDraft && !Number.isNaN(hs)) cfg.hard_stop = hs;
        if (autoResetDraft !== null) cfg.auto_reset_enabled = autoResetDraft;
        if (Object.keys(cfg).length === 0) {
            showToast('Nada para aplicar', 'info');
            return;
        }
        try {
            await ConnectionManager.configureWatchdog(cfg);
            showToast('Config aplicada', 'success');
            setHardStopDraft('');
            setAutoResetDraft(null);
        } catch (e: any) {
            const msg = e?.message || 'falha';
            if (msg.startsWith('wd_range:')) {
                const field = msg.slice('wd_range:'.length);
                showToast(`Valor fora de faixa: ${field}`, 'error', 6000);
            } else {
                showToast(`Config falhou: ${msg}`, 'error');
            }
        }
    }

    return (
        <WizardSheet title="Watchdog Térmico" open={open} onClose={onClose}
            footer={<Button size="$3" onPress={onClose}>Fechar</Button>}>
            <YStack gap="$3">
                {!isConnected.value && (
                    <Paragraph fontSize="$2" opacity={0.7}>
                        Conecte ao equipamento para ver o estado do watchdog.
                    </Paragraph>
                )}

                {isConnected.value && !supported && (
                    <Paragraph fontSize="$2" opacity={0.7}>
                        Firmware atual não expõe o watchdog. Atualize o firmware para usar esta camada de segurança.
                    </Paragraph>
                )}

                {isConnected.value && supported && (
                    <>
                        {/* Status block */}
                        <YStack
                            padding="$3" br="$3"
                            borderWidth={1}
                            borderColor={tripped ? '$error' : '$borderColor'}
                            backgroundColor={tripped ? '$error' : '$backgroundFocus'}
                            gap="$1.5"
                        >
                            <XStack ai="center" jc="space-between">
                                <Text fontWeight="700" color={tripped ? 'white' : '$color'}>
                                    {tripped ? `⛔ Disparado: ${cause || 'desconhecido'}`
                                             : armed ? '🛡 Armado' : 'Desconhecido'}
                                </Text>
                                <Text fontFamily="$mono" fontSize="$1"
                                      color={tripped ? 'white' : '$color'}>
                                    {count} {count === 1 ? 'disparo' : 'disparos'}
                                </Text>
                            </XStack>
                            <Text fontSize="$1" opacity={0.8}
                                  color={tripped ? 'white' : '$color'}>
                                Último disparo: {fmtUnix(lastUnix)}
                            </Text>
                        </YStack>

                        {/* Reset action */}
                        {tripped && (
                            <Button size="$3" theme="red"
                                disabled={resetting}
                                onPress={handleReset}>
                                <Text fontWeight="700">
                                    {resetting ? 'Rearmando…' : 'Rearmar (req:watchdog:reset)'}
                                </Text>
                            </Button>
                        )}

                        {/* Config form */}
                        <YStack gap="$2">
                            <Text fontWeight="600">Limites</Text>

                            <XStack ai="center" gap="$2">
                                <Text width={140} fontSize="$2">Corte (°C)</Text>
                                <Input flex={1} size="$3" keyboardType="decimal-pad"
                                    value={hardStopDraft}
                                    placeholder={String(hardStop)}
                                    onChangeText={setHardStopDraft} />
                                <Text fontSize="$1" opacity={0.6}>{`atual: ${hardStop.toFixed(1)}`}</Text>
                            </XStack>

                            <XStack ai="center" gap="$2">
                                <Text width={140} fontSize="$2">Auto-reset</Text>
                                <Switch size="$3"
                                    checked={autoReset}
                                    onCheckedChange={(v) => setAutoResetDraft(!!v)}>
                                    <Switch.Thumb />
                                </Switch>
                                <Text fontSize="$1" opacity={0.6}>
                                    {autoReset ? 'libera quando esfriar' : 'só manual'}
                                </Text>
                            </XStack>

                            <Button size="$3" onPress={handleApply}>
                                <Text>Aplicar mudanças</Text>
                            </Button>
                        </YStack>
                    </>
                )}
            </YStack>
        </WizardSheet>
    );
}
