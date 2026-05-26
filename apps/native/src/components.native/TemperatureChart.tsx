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
    tempHistory, targetHistory, outputHistory,
} from '@brewpilot/stores';

const HEIGHT = 192;
const PAD_L  = 28;  // left margin for °C labels
const PAD_R  = 8;
const PAD_T  = 8;
const PAD_B  = 16;  // bottom margin for tick labels

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
                </G>
            </Svg>
        </View>
    );
}
