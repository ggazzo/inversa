// HopAlertOverlay.jsx — fullscreen takeover when a hop addition fires.
//
// Watches `boilAlerts.length`. The newest alert triggers an overlay
// the user must dismiss; this guarantees the brewer notices even when
// the device is across the room. Optional vibration on supported
// devices. Dismiss button is huge and the only escape — closing it
// records the ack, no further state needed.

import { useSignalEffect, signal } from '@preact/signals';
import { useRef } from 'preact/hooks';
import { boilAlerts } from '../stores/state';

// Top-level so reopening the component doesn't reset the ack state.
const activeAlert = signal(null);
const lastSeenAlertCount = signal(0);

export function HopAlertOverlay() {
    const audioRef = useRef(null);

    useSignalEffect(() => {
        const count = boilAlerts.value.length;
        if (count > lastSeenAlertCount.value) {
            const newest = boilAlerts.value[count - 1];
            activeAlert.value = newest;
            lastSeenAlertCount.value = count;
            try { navigator.vibrate?.([200, 100, 200, 100, 600]); } catch {}
            // Best-effort audio cue (no asset; uses Web Audio synth tick).
            try {
                const ctx = new (window.AudioContext || window.webkitAudioContext)();
                const o = ctx.createOscillator(); o.type = 'sine'; o.frequency.value = 880;
                const g = ctx.createGain(); g.gain.value = 0.2;
                o.connect(g).connect(ctx.destination);
                o.start(); o.stop(ctx.currentTime + 0.4);
            } catch {}
        }
    });

    const alert = activeAlert.value;
    if (!alert) return null;

    return (
        <div class="fixed inset-0 z-[200] bg-warning/95 text-warning-content
                    flex flex-col items-center justify-center p-6 gap-6"
             role="alertdialog" aria-live="assertive">
            <div class="text-8xl motion-safe:animate-bounce">🌿</div>
            <h1 class="text-3xl sm:text-5xl font-bold text-center">
                Adicionar lúpulo!
            </h1>
            <div class="text-2xl sm:text-4xl font-mono text-center break-words max-w-xl">
                {alert.name}
            </div>
            <div class="text-lg opacity-80">
                @ {alert.min} min restantes
            </div>
            <button
                class="btn btn-neutral btn-lg w-full max-w-md h-20 text-xl mt-4"
                onClick={() => { activeAlert.value = null; }}
                autofocus
            >
                Adicionei ✓
            </button>
        </div>
    );
}
