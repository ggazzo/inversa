// HopAlertOverlay.tsx — fullscreen takeover when a hop addition fires.
// Watches boilAlerts; pops over everything until the user acks.
//
// We keep the audio + vibration cues from the legacy version. Bouncing
// animation is gated by `prefers-reduced-motion` via main.css.

import { useEffect, useRef } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { signal as createSignal } from '@preact/signals-react';
import { Button, Text, YStack } from 'tamagui';
import { boilAlerts } from '@inversa/stores';

const activeAlert = createSignal<any | null>(null);
const lastSeenCount = createSignal<number>(0);

export function HopAlertOverlay() {
    useSignals();
    const audioStarted = useRef(false);

    // Reading `boilAlerts.value.length` IN THE RENDER BODY is what
    // subscribes this component to the signal. Reading it only inside
    // the effect below isn't enough — the signals-react-transform
    // tracks signal reads on the synchronous render path, and effects
    // run after that. Without the line below the component never
    // re-rendered when a new hop alert was pushed.
    const count = boilAlerts.value.length;

    useEffect(() => {
        if (count > lastSeenCount.value) {
            activeAlert.value = boilAlerts.value[count - 1];
            lastSeenCount.value = count;
            try { (navigator as any).vibrate?.([200, 100, 200, 100, 600]); } catch { /* noop */ }
            try {
                const Ctx = (window as any).AudioContext || (window as any).webkitAudioContext;
                if (!Ctx) return;
                const ctx = new Ctx();
                const o = ctx.createOscillator(); o.type = 'sine'; o.frequency.value = 880;
                const g = ctx.createGain(); g.gain.value = 0.2;
                o.connect(g).connect(ctx.destination);
                o.start(); o.stop(ctx.currentTime + 0.4);
                audioStarted.current = true;
            } catch { /* noop */ }
        }
    }, [count]);

    const alert = activeAlert.value;
    if (!alert) return null;

    return (
        // `position: fixed` isn't part of the RN-style positioning Tamagui
        // models. Drop to a raw style for the overlay container — same
        // visual effect on web, ignored on RN (where this component
        // wouldn't render anyway).
        <YStack
            style={{ position: 'fixed', top: 0, left: 0, right: 0, bottom: 0, zIndex: 200 }}
            backgroundColor="$paused"
            ai="center" jc="center" gap="$5" padding="$5"
            role="alertdialog" aria-live="assertive"
        >
            <Text fontSize={96} color="black">🌿</Text>
            <Text fontSize={36} color="black" fontWeight="700" ta="center">
                Adicionar lúpulo!
            </Text>
            <Text fontFamily="$mono" fontSize={32} color="black" ta="center">
                {alert.name}
            </Text>
            <Text fontSize="$5" color="black" opacity={0.8}>
                @ {alert.min} min restantes
            </Text>
            <Button size="$6" theme="dark"
                width="100%" maxWidth={420} height={80}
                onPress={() => { activeAlert.value = null; }}>
                <Text fontSize="$6">Adicionei ✓</Text>
            </Button>
        </YStack>
    );
}
