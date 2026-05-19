// WatchdogSheet.tsx — Thermal Watchdog status + manual reset + full
// configuration (001-thermal-watchdog T032).
//
// Exposes every field accepted by req:watchdog:config so the operator
// can tune all detection thresholds from the UI. Ranges mirror the
// firmware constants (constants.h WATCHDOG_*_MIN/MAX) so we reject
// bad input client-side before sending. The firmware re-validates and
// returns wd_range:<field> if something slips through.

import { useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Input, Paragraph, Separator, Switch, Text, XStack, YStack } from 'tamagui';
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
    watchdogSensorFaultMs,
    watchdogLoopStuckMs,
    watchdogGradFactor,
    watchdogGradWindow,
    watchdogSafeAutoresetC,
    watchdogCoolMinMs,
} from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { WizardSheet } from '../wizards/WizardSheet';

interface Props { open: boolean; onClose: () => void }

// Range table — kept in lock-step with firmware/src/core/constants.h.
// Changing a bound on one side without the other will desync the
// validation; firmware is the source of truth, this is the mirror.
const RANGES = {
    hard_stop:          { min: 80,    max: 130,    unit: '°C', step: 'decimal' as const },
    sensor_fault_ms:    { min: 1000,  max: 60000,  unit: 'ms', step: 'integer' as const },
    loop_stuck_ms:      { min: 1000,  max: 30000,  unit: 'ms', step: 'integer' as const },
    grad_factor:        { min: 2,     max: 20,     unit: '×',  step: 'integer' as const },
    grad_window:        { min: 5,     max: 100,    unit: 'amostras', step: 'integer' as const },
    safe_autoreset_c:   { min: 20,    max: 80,     unit: '°C', step: 'decimal' as const },
    cool_min_ms:        { min: 60000, max: 1800000,unit: 'ms', step: 'integer' as const },
};

type FieldKey = keyof typeof RANGES;

function fmtUnix(ts: number): string {
    if (!ts) return '—';
    return new Date(ts * 1000).toLocaleString('pt-BR');
}

// Parse + range-check a draft string. Returns null when the draft is
// empty (meaning "leave as-is"), the parsed number when valid, or
// throws a string error tag matching the firmware's wd_range:<field>
// convention so the catch block renders a consistent message.
function parseDraft(key: FieldKey, raw: string): number | null {
    if (!raw.trim()) return null;
    const r = RANGES[key];
    const n = r.step === 'integer' ? parseInt(raw, 10) : parseFloat(raw);
    if (Number.isNaN(n)) throw `wd_range:${key}`;
    if (n < r.min || n > r.max) throw `wd_range:${key}`;
    return n;
}

interface RowProps {
    label: string;
    field: FieldKey;
    draft: string;
    current: number;
    onChange: (v: string) => void;
    fmtCurrent?: (v: number) => string;
}

function ConfigRow({ label, field, draft, current, onChange, fmtCurrent }: RowProps) {
    const r = RANGES[field];
    const display = fmtCurrent ? fmtCurrent(current) : String(current);
    return (
        <YStack gap="$1">
            <XStack ai="center" gap="$2">
                <Text width={140} fontSize="$2">{label}</Text>
                <Input flex={1} size="$3"
                    keyboardType={r.step === 'integer' ? 'number-pad' : 'decimal-pad'}
                    value={draft}
                    placeholder={display}
                    onChangeText={onChange} />
                <Text fontSize="$1" opacity={0.6} width={70}>{r.unit}</Text>
            </XStack>
            <Text fontSize="$1" opacity={0.5} paddingLeft={148}>
                atual: {display} · faixa: {r.min}–{r.max}
            </Text>
        </YStack>
    );
}

export function WatchdogSheet({ open, onClose }: Props) {
    useSignals();
    const [resetting, setResetting] = useState(false);

    // One draft string per editable field. Empty string means "no
    // change" so applying touches only fields the operator typed into.
    const [hardStopDraft,       setHardStopDraft]       = useState('');
    const [sensorFaultDraft,    setSensorFaultDraft]    = useState('');
    const [loopStuckDraft,      setLoopStuckDraft]      = useState('');
    const [gradFactorDraft,     setGradFactorDraft]     = useState('');
    const [gradWindowDraft,     setGradWindowDraft]     = useState('');
    const [safeAutoresetDraft,  setSafeAutoresetDraft]  = useState('');
    const [coolMinDraft,        setCoolMinDraft]        = useState('');
    const [autoResetDraft,      setAutoResetDraft]      = useState<boolean | null>(null);

    const supported = watchdogSupported.value;
    const tripped   = watchdogTripped.value;
    const armed     = watchdogArmed.value;
    const cause     = watchdogLastCause.value;
    const count     = watchdogTripCount.value;
    const lastUnix  = watchdogLastTripUnix.value;
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

    function clearDrafts() {
        setHardStopDraft('');
        setSensorFaultDraft('');
        setLoopStuckDraft('');
        setGradFactorDraft('');
        setGradWindowDraft('');
        setSafeAutoresetDraft('');
        setCoolMinDraft('');
        setAutoResetDraft(null);
    }

    async function handleApply() {
        const cfg: Record<string, any> = {};
        try {
            const hs  = parseDraft('hard_stop',        hardStopDraft);
            const sfm = parseDraft('sensor_fault_ms',  sensorFaultDraft);
            const lsm = parseDraft('loop_stuck_ms',    loopStuckDraft);
            const gf  = parseDraft('grad_factor',      gradFactorDraft);
            const gw  = parseDraft('grad_window',      gradWindowDraft);
            const sa  = parseDraft('safe_autoreset_c', safeAutoresetDraft);
            const cm  = parseDraft('cool_min_ms',      coolMinDraft);
            if (hs  !== null) cfg.hard_stop          = hs;
            if (sfm !== null) cfg.sensor_fault_ms    = sfm;
            if (lsm !== null) cfg.loop_stuck_ms      = lsm;
            if (gf  !== null) cfg.grad_factor        = gf;
            if (gw  !== null) cfg.grad_window        = gw;
            if (sa  !== null) cfg.safe_autoreset_c   = sa;
            if (cm  !== null) cfg.cool_min_ms        = cm;
            if (autoResetDraft !== null) cfg.auto_reset_enabled = autoResetDraft;
        } catch (tag: any) {
            const field = String(tag).startsWith('wd_range:') ? String(tag).slice('wd_range:'.length) : '?';
            showToast(`Valor fora de faixa: ${field}`, 'error', 6000);
            return;
        }

        if (Object.keys(cfg).length === 0) {
            showToast('Nada para aplicar', 'info');
            return;
        }
        try {
            await ConnectionManager.configureWatchdog(cfg);
            showToast('Config aplicada', 'success');
            clearDrafts();
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

                        {/* Limites térmicos */}
                        <YStack gap="$2">
                            <Text fontWeight="600">Limites Térmicos</Text>
                            <ConfigRow
                                label="Corte (hard-stop)"
                                field="hard_stop"
                                draft={hardStopDraft}
                                current={watchdogHardStopC.value}
                                onChange={setHardStopDraft}
                                fmtCurrent={(v) => v.toFixed(1)} />
                            <ConfigRow
                                label="Temp. segura"
                                field="safe_autoreset_c"
                                draft={safeAutoresetDraft}
                                current={watchdogSafeAutoresetC.value}
                                onChange={setSafeAutoresetDraft}
                                fmtCurrent={(v) => v.toFixed(1)} />
                        </YStack>

                        <Separator />

                        {/* Timeouts */}
                        <YStack gap="$2">
                            <Text fontWeight="600">Timeouts</Text>
                            <ConfigRow
                                label="Falha do sensor"
                                field="sensor_fault_ms"
                                draft={sensorFaultDraft}
                                current={watchdogSensorFaultMs.value}
                                onChange={setSensorFaultDraft} />
                            <ConfigRow
                                label="Loop travado"
                                field="loop_stuck_ms"
                                draft={loopStuckDraft}
                                current={watchdogLoopStuckMs.value}
                                onChange={setLoopStuckDraft} />
                            <ConfigRow
                                label="Resfriamento mín."
                                field="cool_min_ms"
                                draft={coolMinDraft}
                                current={watchdogCoolMinMs.value}
                                onChange={setCoolMinDraft} />
                        </YStack>

                        <Separator />

                        {/* Gradiente */}
                        <YStack gap="$2">
                            <Text fontWeight="600">Gradiente</Text>
                            <ConfigRow
                                label="Fator"
                                field="grad_factor"
                                draft={gradFactorDraft}
                                current={watchdogGradFactor.value}
                                onChange={setGradFactorDraft} />
                            <ConfigRow
                                label="Janela"
                                field="grad_window"
                                draft={gradWindowDraft}
                                current={watchdogGradWindow.value}
                                onChange={setGradWindowDraft} />
                        </YStack>

                        <Separator />

                        {/* Auto-reset */}
                        <YStack gap="$2">
                            <Text fontWeight="600">Auto-reset</Text>
                            <XStack ai="center" gap="$2">
                                <Text width={140} fontSize="$2">Habilitado</Text>
                                <Switch size="$3"
                                    checked={autoReset}
                                    onCheckedChange={(v) => setAutoResetDraft(!!v)}>
                                    <Switch.Thumb />
                                </Switch>
                                <Text fontSize="$1" opacity={0.6}>
                                    {autoReset ? 'libera quando esfriar' : 'só manual'}
                                </Text>
                            </XStack>
                        </YStack>

                        <XStack gap="$2">
                            <Button flex={1} size="$3" onPress={clearDrafts}>
                                <Text>Limpar</Text>
                            </Button>
                            <Button flex={2} size="$3" theme="active" onPress={handleApply}>
                                <Text fontWeight="700">Aplicar mudanças</Text>
                            </Button>
                        </XStack>
                    </>
                )}
            </YStack>
        </WizardSheet>
    );
}
