import { useRef, useEffect } from 'preact/hooks';
import { useSignalEffect } from '@preact/signals';
import { Chart, registerables } from 'chart.js';
import { tempHistory, targetHistory, outputHistory, timeLabels } from '../stores/state';

Chart.register(...registerables);

export function TemperatureChart() {
  const canvasRef = useRef(null);
  const chartRef = useRef(null);

  useEffect(() => {
    if (!canvasRef.current) return;

    const ctx = canvasRef.current.getContext('2d');
    chartRef.current = new Chart(ctx, {
      type: 'line',
      data: {
        labels: [],
        datasets: [
          {
            label: 'Temperatura',
            data: [],
            borderColor: '#f59e0b',
            backgroundColor: 'rgba(245,158,11,0.1)',
            borderWidth: 2,
            pointRadius: 0,
            tension: 0.3,
            fill: true,
            yAxisID: 'y',
          },
          {
            label: 'Alvo',
            data: [],
            borderColor: '#ef4444',
            borderWidth: 1.5,
            borderDash: [6, 3],
            pointRadius: 0,
            tension: 0,
            fill: false,
            yAxisID: 'y',
          },
          {
            label: 'PID %',
            data: [],
            borderColor: '#3b82f6',
            borderWidth: 1,
            pointRadius: 0,
            tension: 0.3,
            fill: false,
            yAxisID: 'y1',
          },
        ],
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        interaction: {
          mode: 'index',
          intersect: false,
        },
        plugins: {
          legend: {
            display: true,
            position: 'top',
            labels: {
              boxWidth: 12,
              padding: 8,
              font: { size: 10 },
              color: '#a6adba',
            },
          },
          tooltip: {
            enabled: true,
            bodyFont: { size: 11 },
          },
        },
        scales: {
          x: {
            display: true,
            ticks: {
              maxTicksLimit: 6,
              font: { size: 9 },
              color: '#6b7280',
            },
            grid: { display: false },
          },
          y: {
            display: true,
            position: 'left',
            title: {
              display: true,
              text: 'C',
              font: { size: 10 },
              color: '#a6adba',
            },
            ticks: {
              font: { size: 9 },
              color: '#a6adba',
            },
            grid: {
              color: 'rgba(255,255,255,0.05)',
            },
          },
          y1: {
            display: true,
            position: 'right',
            min: 0,
            max: 100,
            title: {
              display: true,
              text: '%',
              font: { size: 10 },
              color: '#a6adba',
            },
            ticks: {
              font: { size: 9 },
              color: '#6b7280',
            },
            grid: { display: false },
          },
        },
      },
    });

    return () => {
      chartRef.current?.destroy();
    };
  }, []);

  // P19 — useSignalEffect auto-subscribes to every signal read inside, so the
  // chart actually updates when telemetry comes in. The previous useEffect
  // (no deps) only ran on re-renders, which weren't triggered by signal
  // mutation alone — the chart silently froze.
  useSignalEffect(() => {
    const chart = chartRef.current;
    if (!chart) return;

    chart.data.labels = timeLabels.value;
    chart.data.datasets[0].data = tempHistory.value;
    chart.data.datasets[1].data = targetHistory.value;
    chart.data.datasets[2].data = outputHistory.value;
    chart.update('none'); // skip animations
  });

  return (
    <div class="w-full h-48">
      <canvas ref={canvasRef} />
    </div>
  );
}
