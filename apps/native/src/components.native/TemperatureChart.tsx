// TemperatureChart.tsx — victory-native (Skia) version of the chart.
// Lives in `components.native/` so the cross-platform tree never
// reaches for Chart.js. Props surface and the signals it consumes
// (`tempHistory`, `targetHistory`, `outputHistory`, `timeLabels`)
// match the web version in apps/web/src/components.web — the only
// platform-specific concern is how we render lines, which on RN
// means Skia paths driven by victory-native's CartesianChart.

import { useMemo } from 'react';
import { View, Text } from 'react-native';
import { useSignals } from '@preact/signals-react/runtime';
import { CartesianChart, Line } from 'victory-native';
import {
    tempHistory, targetHistory, outputHistory, timeLabels,
} from '@inversa/stores';

// Victory-native works best with a single dataset whose rows carry
// every series. We zip the four signals into one array of points
// keyed by index — the X axis is just the sample number, which
// matches the web chart's behaviour (it uses timeLabels for ticks
// but the spacing is uniform per sample).
type Row = {
    i:      number;
    temp:   number;
    target: number;
    output: number;
};

export function TemperatureChart() {
    useSignals();

    const temps   = tempHistory.value;
    const targets = targetHistory.value;
    const outs    = outputHistory.value;

    const data: Row[] = useMemo(() => {
        const len = temps.length;
        const rows: Row[] = [];
        for (let i = 0; i < len; i++) {
            rows.push({
                i,
                temp:   temps[i]   ?? 0,
                target: targets[i] ?? 0,
                output: outs[i]    ?? 0,
            });
        }
        return rows;
    }, [temps, targets, outs]);

    if (data.length === 0) {
        return (
            <View style={{ width: '100%', height: 192,
                            alignItems: 'center', justifyContent: 'center' }}>
                <Text style={{ opacity: 0.4, fontSize: 12 }}>Aguardando telemetria…</Text>
            </View>
        );
    }

    return (
        <View style={{ width: '100%', height: 192 }}>
            <CartesianChart
                data={data}
                xKey="i"
                yKeys={['temp', 'target', 'output']}
                domainPadding={{ top: 8, bottom: 8, left: 4, right: 4 }}
                axisOptions={{
                    formatXLabel: (i) => {
                        const label = timeLabels.value[i as number];
                        return typeof label === 'string' ? label : '';
                    },
                    lineColor: 'rgba(255,255,255,0.08)',
                    labelColor: '#a6adba',
                }}
            >
                {({ points }) => (
                    <>
                        {/* Temperature — orange, thick */}
                        <Line points={points.temp}
                              color="#f59e0b"
                              strokeWidth={2}
                              curveType="natural" />
                        {/* Target — red dashed */}
                        <Line points={points.target}
                              color="#ef4444"
                              strokeWidth={1.5} />
                        {/* PID % — blue thin (shares the y axis with temp
                            here; a real dual-axis needs additional setup
                            in victory-native that's out of scope for the
                            first device build) */}
                        <Line points={points.output}
                              color="#3b82f6"
                              strokeWidth={1}
                              curveType="natural" />
                    </>
                )}
            </CartesianChart>
        </View>
    );
}
