// TemperatureChart.tsx — Chart.js, web-only.
//
// Lives in `components.web/` so the cross-platform tree doesn't grow a
// dependency on DOM-based charts. When the native app arrives, swap for
// `react-native-svg-charts` (or equivalent) and keep the same prop
// surface. The signals it consumes (`tempHistory`, `targetHistory`,
// `outputHistory`, `timeLabels`) are platform-agnostic.

import { useEffect, useRef } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Chart, registerables } from 'chart.js';
import type { Chart as ChartType } from 'chart.js';
import {
    tempHistory, targetHistory, outputHistory, timeLabels,
} from '@inversa/stores';

Chart.register(...registerables);

export function TemperatureChart() {
    useSignals();   // belt + suspenders — the babel plugin should handle it too.

    const canvasRef = useRef<HTMLCanvasElement | null>(null);
    const chartRef  = useRef<ChartType | null>(null);

    // One-time chart construction.
    useEffect(() => {
        if (!canvasRef.current) return;
        const ctx = canvasRef.current.getContext('2d')!;
        chartRef.current = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'Temperatura', data: [], borderColor: '#f59e0b',
                        backgroundColor: 'rgba(245,158,11,0.1)', borderWidth: 2,
                        pointRadius: 0, tension: 0.3, fill: true, yAxisID: 'y',
                    },
                    {
                        label: 'Alvo', data: [], borderColor: '#ef4444',
                        borderWidth: 1.5, borderDash: [6, 3], pointRadius: 0,
                        tension: 0, fill: false, yAxisID: 'y',
                    },
                    {
                        label: 'PID %', data: [], borderColor: '#3b82f6',
                        borderWidth: 1, pointRadius: 0, tension: 0.3,
                        fill: false, yAxisID: 'y1',
                    },
                ],
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                interaction: { mode: 'index', intersect: false },
                plugins: {
                    legend: {
                        display: true, position: 'top',
                        labels: { boxWidth: 12, padding: 8, font: { size: 10 }, color: '#a6adba' },
                    },
                    tooltip: { enabled: true, bodyFont: { size: 11 } },
                },
                scales: {
                    x: {
                        display: true,
                        ticks: { maxTicksLimit: 6, font: { size: 9 }, color: '#6b7280' },
                        grid: { display: false },
                    },
                    y: {
                        display: true, position: 'left',
                        title: { display: true, text: 'C', font: { size: 10 }, color: '#a6adba' },
                        ticks: { font: { size: 9 }, color: '#a6adba' },
                        grid: { color: 'rgba(255,255,255,0.05)' },
                    },
                    y1: {
                        display: true, position: 'right', min: 0, max: 100,
                        title: { display: true, text: '%', font: { size: 10 }, color: '#a6adba' },
                        ticks: { font: { size: 9 }, color: '#6b7280' },
                        grid: { display: false },
                    },
                },
            },
        });
        return () => { chartRef.current?.destroy(); chartRef.current = null; };
    }, []);

    // Push signals into the chart on every render. The signals plugin
    // re-renders this component when any of them change, so a plain
    // effect (no deps) catches every update.
    useEffect(() => {
        const chart = chartRef.current;
        if (!chart) return;
        chart.data.labels = timeLabels.value;
        chart.data.datasets[0].data = tempHistory.value as any;
        chart.data.datasets[1].data = targetHistory.value as any;
        chart.data.datasets[2].data = outputHistory.value as any;
        chart.update('none');
    });

    const hasData = tempHistory.value.length > 0;
    return (
        <div style={{ width: '100%', height: 192, position: 'relative' }}>
            <canvas ref={canvasRef} />
            {!hasData && (
                <div style={{
                    position: 'absolute', inset: 0, display: 'grid',
                    placeItems: 'center', fontSize: 12, opacity: 0.4,
                    pointerEvents: 'none',
                }}>
                    Aguardando telemetria…
                </div>
            )}
        </div>
    );
}
