// TemperatureChart.tsx — SVG-based version of the chart.
//
// We started on victory-native@41 (Skia-backed) but its render path
// is finicky with Reanimated worklets in dev and was producing a
// blank canvas on first boot. react-native-svg is simpler, lighter,
// and bullet-proof on both iOS and Android — and the chart we need
// here (three polylines + grid + temp scale) doesn't justify the
// Skia ceremony. Same prop surface, same signal inputs as the web
// Chart.js version.

import { useState } from 'react';
import { View, Text, type LayoutChangeEvent } from 'react-native';
import { useSignals } from '@preact/signals-react/runtime';
import Svg, { G, Line, Polyline, Text as SvgText } from 'react-native-svg';
import {
    tempHistory, targetHistory, outputHistory, timeLabels,
} from '@inversa/stores';

const HEIGHT = 208;
const PAD_L  = 28;  // left margin for °C labels
const PAD_R  = 8;
const PAD_T  = 8;
const PAD_B  = 28;  // bottom margin for °C + HH:MM tick labels

const COLOR_TEMP    = '#f59e0b';
const COLOR_TARGET  = '#ef4444';
const COLOR_OUTPUT  = '#3b82f6';
const COLOR_GRID    = 'rgba(255,255,255,0.08)';
const COLOR_AXIS    = '#a6adba';

function buildPoints(values: readonly number[], width: number, plotH: number, minY: number, maxY: number): string {
    if (values.length === 0) return '';
    const range = maxY - minY || 1;
    const step  = values.length > 1 ? width / (values.length - 1) : 0;
    const out: string[] = [];
    for (let i = 0; i < values.length; i++) {
        const x = PAD_L + i * step;
        const y = PAD_T + (1 - (values[i] - minY) / range) * plotH;
        out.push(`${x.toFixed(1)},${y.toFixed(1)}`);
    }
    return out.join(' ');
}

export function TemperatureChart() {
    useSignals();
    const [width, setWidth] = useState(0);

    function onLayout(e: LayoutChangeEvent) {
        const w = e.nativeEvent.layout.width;
        if (w !== width) setWidth(w);
    }

    const temps   = tempHistory.value;
    const targets = targetHistory.value;
    const outs    = outputHistory.value;
    const labels  = timeLabels.value;

    const hasData = temps.length > 0;

    if (!hasData || width === 0) {
        return (
            <View
                onLayout={onLayout}
                style={{ width: '100%', height: HEIGHT,
                          alignItems: 'center', justifyContent: 'center' }}
            >
                {!hasData && (
                    <Text style={{ opacity: 0.4, fontSize: 12 }}>
                        Aguardando telemetria…
                    </Text>
                )}
            </View>
        );
    }

    const plotW = Math.max(0, width - PAD_L - PAD_R);
    const plotH = Math.max(0, HEIGHT - PAD_T - PAD_B);

    // Temperature range with a small floor/ceiling padding so the
    // line doesn't kiss the edge of the plot.
    const allTemps = [...temps, ...targets.filter((v) => v > 0)];
    const rawMin   = Math.min(...allTemps);
    const rawMax   = Math.max(...allTemps);
    const pad      = Math.max(2, (rawMax - rawMin) * 0.1);
    const minY     = Math.floor(rawMin - pad);
    const maxY     = Math.ceil(rawMax + pad);

    // PID output is 0-255 in the firmware; normalise to the same Y
    // scale by rescaling to the temp range so the % line shares the
    // axis. The dual axis is web-only luxury; on the phone we just
    // overlay the line as a hint.
    const outsNorm = outs.map((v) => minY + (v / 255) * (maxY - minY));

    const tempPoints   = buildPoints(temps,    plotW, plotH, minY, maxY);
    const targetPoints = buildPoints(targets,  plotW, plotH, minY, maxY);
    const outputPoints = buildPoints(outsNorm, plotW, plotH, minY, maxY);

    // Horizontal grid lines at min, midpoint, max.
    const grid = [minY, (minY + maxY) / 2, maxY];

    // X-axis time ticks. Up to 5 evenly spaced timestamps; if the
    // sample buffer is tiny just show start and end so labels don't
    // collide. timeLabels.value tracks tempHistory 1:1 (see
    // packages/stores/src/state.ts::addTelemetryPoint), so labels[i]
    // is the HH:MM:SS that produced temps[i]. We render HH:MM only
    // — full precision would overlap on narrow phones.
    const tickCount = Math.min(5, Math.max(2, labels.length));
    const tickIdx: number[] = [];
    if (labels.length > 0) {
        for (let i = 0; i < tickCount; i++) {
            const idx = Math.round(((labels.length - 1) * i) / (tickCount - 1 || 1));
            tickIdx.push(idx);
        }
    }
    const step = temps.length > 1 ? plotW / (temps.length - 1) : 0;
    function toHHMM(s: string | undefined): string {
        if (!s) return '';
        // Source format is `HH:MM:SS` from toLocaleTimeString pt-BR.
        // Trim the seconds so we fit comfortably under the chart.
        return s.length >= 5 ? s.slice(0, 5) : s;
    }

    return (
        <View onLayout={onLayout} style={{ width: '100%', height: HEIGHT }}>
            <Svg width={width} height={HEIGHT}>
                <G>
                    {grid.map((y) => {
                        const yPx = PAD_T + (1 - (y - minY) / (maxY - minY || 1)) * plotH;
                        return (
                            <G key={y}>
                                <Line
                                    x1={PAD_L} x2={width - PAD_R}
                                    y1={yPx}   y2={yPx}
                                    stroke={COLOR_GRID} strokeWidth={1}
                                />
                                <SvgText
                                    x={PAD_L - 4} y={yPx + 3}
                                    fontSize={9} fill={COLOR_AXIS}
                                    textAnchor="end"
                                >
                                    {y.toFixed(0)}°
                                </SvgText>
                            </G>
                        );
                    })}

                    {/* Target — drawn first so the temperature line wins
                        in the visual stack. */}
                    {targetPoints && (
                        <Polyline
                            points={targetPoints}
                            stroke={COLOR_TARGET} strokeWidth={1.5}
                            strokeDasharray="6,3"
                            fill="none"
                        />
                    )}

                    {/* PID output (rescaled). */}
                    {outputPoints && (
                        <Polyline
                            points={outputPoints}
                            stroke={COLOR_OUTPUT} strokeWidth={1}
                            fill="none"
                        />
                    )}

                    {/* Temperature, on top. */}
                    {tempPoints && (
                        <Polyline
                            points={tempPoints}
                            stroke={COLOR_TEMP} strokeWidth={2}
                            fill="none"
                        />
                    )}

                    {/* X-axis time ticks: small vertical mark + HH:MM
                        below the plot. Edge labels are anchored to the
                        edge so they don't run off the chart bounds. */}
                    {tickIdx.map((idx, k) => {
                        const x = PAD_L + idx * step;
                        const yTop = PAD_T + plotH;
                        const yLabel = HEIGHT - 4;
                        const isFirst = k === 0;
                        const isLast  = k === tickIdx.length - 1;
                        const anchor  = isFirst ? 'start' : isLast ? 'end' : 'middle';
                        return (
                            <G key={`t-${idx}`}>
                                <Line
                                    x1={x} x2={x}
                                    y1={yTop} y2={yTop + 4}
                                    stroke={COLOR_AXIS} strokeWidth={1}
                                />
                                <SvgText
                                    x={x} y={yLabel}
                                    fontSize={9} fill={COLOR_AXIS}
                                    textAnchor={anchor}
                                >
                                    {toHHMM(labels[idx])}
                                </SvgText>
                            </G>
                        );
                    })}
                </G>
            </Svg>
        </View>
    );
}
