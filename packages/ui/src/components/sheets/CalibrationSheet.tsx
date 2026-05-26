// CalibrationSheet.tsx — multi-point linear calibration UI.
//
// The user fills up to three pairs of (controller, reference) readings,
// the sheet computes a least-squares slope+offset and pushes them as a
// single `{slope, offset}` write. The firmware applies
// `T_real = slope · T_medido + offset` after the Kalman filter — see
// `firmware/src/plugins/TemperaturePlugin.h::loop`.
//
// The quick offset-only path lives in WizardEquipment.tsx as a 6th input.
// This sheet is for users who have a couple reference thermometers and
// want to correct slope error too.

import { useEffect, useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Input, Label, Paragraph, Text, XStack, YStack } from 'tamagui';
import { ConnectionManager } from '@brewpilot/services';
import { currentTemp, showToast } from '@brewpilot/stores';
import { WizardSheet } from '../wizards/WizardSheet';

interface Props { open: boolean; onClose: () => void }

interface Row { measured: string; reference: string }

const EMPTY_ROW: Row = { measured: '', reference: '' };

// Match firmware-side bounds in CommandHandler::REQ_SETTINGS_CAL_SET.
const SLOPE_MIN  = 0.8;
const SLOPE_MAX  = 1.2;
const OFFSET_MIN = -10;
const OFFSET_MAX = 10;

function clamp(v: number, lo: number, hi: number) {
    return Math.max(lo, Math.min(hi, v));
}

// Fit y = a·x + b from up to N pairs using ordinary least squares.
// Degenerate cases:
//   * 0 valid pairs → identity {1, 0}
//   * 1 pair        → assume slope = 1, take offset = y - x
//   * variance(x)=0 → same as 1-pair fallback (two identical x's are
//                     useless for slope estimation)
function fitLine(rows: Row[]): { slope: number; offset: number; pts: number } {
    const pts: Array<{ x: number; y: number }> = [];
    for (const r of rows) {
        const x = parseFloat(r.measured);
        const y = parseFloat(r.reference);
        if (Number.isFinite(x) && Number.isFinite(y)) pts.push({ x, y });
    }
    if (pts.length === 0) return { slope: 1, offset: 0, pts: 0 };
    if (pts.length === 1) {
        return { slope: 1, offset: pts[0].y - pts[0].x, pts: 1 };
    }
    const n = pts.length;
    const mx = pts.reduce((s, p) => s + p.x, 0) / n;
    const my = pts.reduce((s, p) => s + p.y, 0) / n;
    let num = 0, den = 0;
    for (const p of pts) { num += (p.x - mx) * (p.y - my); den += (p.x - mx) * (p.x - mx); }
    if (den === 0) {
        return { slope: 1, offset: pts[0].y - pts[0].x, pts: n };
    }
    const slope  = num / den;
    const offset = my - slope * mx;
    return { slope, offset, pts: n };
}

export function CalibrationSheet({ open, onClose }: Props) {
    useSignals();
    const [rows, setRows]                 = useState<Row[]>([{ ...EMPTY_ROW }, { ...EMPTY_ROW }, { ...EMPTY_ROW }]);
    const [persistedSlope, setPSlope]     = useState(1);
    const [persistedOffset, setPOffset]   = useState(0);
    const [persisted, setPersisted]       = useState(false);
    const [loading, setLoading]           = useState(true);

    useEffect(() => {
        if (!open) return;
        setLoading(true);
        ConnectionManager.getTempCalibration()
            .then((r: any) => {
                setPSlope(typeof r?.slope === 'number' ? r.slope : 1);
                setPOffset(typeof r?.offset === 'number' ? r.offset : 0);
                setPersisted(!!r?.persisted);
            })
            .catch((e: any) => showToast(e?.message || 'Falha ao ler', 'error'))
            .finally(() => setLoading(false));
    }, [open]);

    function updateRow(i: number, patch: Partial<Row>) {
        setRows((rs) => rs.map((r, idx) => (idx === i ? { ...r, ...patch } : r)));
    }

    function useReading(i: number) {
        updateRow(i, { measured: currentTemp.value.toFixed(2) });
    }

    function save() {
        const fit = fitLine(rows);
        if (fit.pts === 0) {
            showToast('Preencha ao menos um par de leituras', 'error');
            return;
        }
        const slope  = clamp(fit.slope,  SLOPE_MIN,  SLOPE_MAX);
        const offset = clamp(fit.offset, OFFSET_MIN, OFFSET_MAX);
        const clamped = slope !== fit.slope || offset !== fit.offset;

        ConnectionManager.setTempCalibration(slope, offset)
            .then(() => {
                setPSlope(slope);
                setPOffset(offset);
                setPersisted(true);
                const msg = `Calibração salva (slope ${slope.toFixed(4)}, offset ${offset.toFixed(2)} °C)`;
                showToast(clamped ? `${msg} — ajustada ao limite seguro` : msg,
                          clamped ? 'warning' : 'success');
            })
            .catch((e: any) => showToast(e?.message || 'Falha ao salvar', 'error'));
    }

    function reset() {
        ConnectionManager.setTempCalibration(1.0, 0.0)
            .then(() => {
                setPSlope(1);
                setPOffset(0);
                setPersisted(true);
                setRows([{ ...EMPTY_ROW }, { ...EMPTY_ROW }, { ...EMPTY_ROW }]);
                showToast('Calibração restaurada (identidade)', 'success');
            })
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    return (
        <WizardSheet
            title="Calibrar sensor"
            open={open}
            onClose={onClose}
            footer={<>
                <Button size="$3" chromeless onPress={reset} disabled={loading}>
                    Resetar
                </Button>
                <Button size="$3" theme="active" onPress={save} disabled={loading}>
                    Calcular & salvar
                </Button>
            </>}
        >
            <Paragraph theme="alt2" fontSize="$1">
                Coloque um termômetro de referência em até 3 temperaturas
                conhecidas (ex.: água com gelo, ambiente, fervura). Para
                cada ponto, informe o que o controlador está lendo (botão
                "usar leitura atual" copia o valor em tempo real) e o
                que o termômetro de referência mostrou.{' '}
                <Text fontSize="$1" fontWeight="700"
                      color={persisted ? '$holding' : '$paused'}>
                    {persisted ? '● calibração persistida' : '● identidade'}
                </Text>
            </Paragraph>

            <YStack
                backgroundColor="$backgroundFocus" br="$3" padding="$3"
                gap="$1"
            >
                <Text fontSize="$1" opacity={0.6}>Leitura atual do controlador</Text>
                <Text fontFamily="$mono" fontSize="$6">
                    {currentTemp.value.toFixed(2)} °C
                </Text>
            </YStack>

            {loading && <Text fontSize="$2" opacity={0.5}>Lendo calibração atual…</Text>}

            {rows.map((row, i) => (
                <YStack key={i} gap="$1" paddingVertical="$2">
                    <XStack jc="space-between">
                        <Label htmlFor={`cal-m-${i}`}>Ponto {i + 1}</Label>
                        <Button size="$1" chromeless onPress={() => useReading(i)}>
                            Usar leitura atual
                        </Button>
                    </XStack>
                    <XStack gap="$2" ai="center">
                        <YStack flex={1}>
                            <Text fontSize="$1" opacity={0.5}>Controlador (°C)</Text>
                            <Input
                                id={`cal-m-${i}`}
                                size="$3"
                                keyboardType="numbers-and-punctuation"
                                inputMode="decimal"
                                value={row.measured}
                                onChangeText={(s: string) => updateRow(i, { measured: s })}
                                placeholder="ex. 24.6"
                            />
                        </YStack>
                        <Text fontSize="$2" opacity={0.4}>→</Text>
                        <YStack flex={1}>
                            <Text fontSize="$1" opacity={0.5}>Referência (°C)</Text>
                            <Input
                                id={`cal-r-${i}`}
                                size="$3"
                                keyboardType="numbers-and-punctuation"
                                inputMode="decimal"
                                value={row.reference}
                                onChangeText={(s: string) => updateRow(i, { reference: s })}
                                placeholder="ex. 25.0"
                            />
                        </YStack>
                    </XStack>
                </YStack>
            ))}

            <Paragraph theme="alt2" fontSize="$1" opacity={0.7}>
                Atualmente persistido: slope={persistedSlope.toFixed(4)},
                offset={persistedOffset.toFixed(2)} °C. A regressão linear
                usa os pontos válidos preenchidos acima; valores fora dos
                limites do firmware (slope {SLOPE_MIN}–{SLOPE_MAX}, offset
                ±{OFFSET_MAX} °C) são arredondados para a fronteira.
            </Paragraph>
        </WizardSheet>
    );
}
