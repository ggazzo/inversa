// HopAlertOverlay.tsx — fullscreen takeover when a hop addition fires.
// Watches boilAlerts; pops over everything until the user acks.
//
// We keep the audio + vibration cues from the legacy version. Bouncing
// animation is gated by `prefers-reduced-motion` via main.css.

import { useEffect, useRef } from 'react';
import { Platform, Vibration } from '../platform';
import { useSignals } from '@preact/signals-react/runtime';
import { signal as createSignal } from '@preact/signals-react';
import { Button, Text, YStack } from 'tamagui';
import { boilAlerts } from '@brewpilot/stores';

// Index of the next alert in `boilAlerts` that the operator still has
// to acknowledge. When confirmedCount === boilAlerts.length the overlay
// is hidden. When more alerts arrive while one is on screen they queue
// up behind it — previously the overlay always jumped to the latest and
// silently swallowed earlier ones.
const confirmedCount = createSignal<number>(0);
const lastAlertedCount = createSignal<number>(0);

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
        // Reset when the alert list shrinks (e.g., new recipe started).
        if (count < lastAlertedCount.value) {
            confirmedCount.value = 0;
            lastAlertedCount.value = count;
            return;
        }
        if (count > lastAlertedCount.value) {
            lastAlertedCount.value = count;

            // Vibration: RN's Vibration.vibrate takes a pattern array
            // and accepts identical args on iOS/Android. On web,
            // react-native-web's shim falls through to navigator.vibrate
            // when available, so a single call covers both platforms.
            try { Vibration.vibrate([200, 100, 200, 100, 600]); } catch { /* noop */ }

            // Audio cue is web-only for now. On RN this needs `expo-av`
            // (or react-native-sound); deferred to a follow-up. The
            // vibration alone is enough to grab attention on a phone.
            if (Platform.OS === 'web') {
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
        }
    }, [count]);

    // Queue: show the oldest unacked alert; nothing if all are acked.
    const idx = confirmedCount.value;
    const alert = idx < count ? boilAlerts.value[idx] : null;
    if (!alert) return null;
    const pendingAfter = count - idx - 1;

    return (
        // Fullscreen takeover. `position: fixed` is web-only; on RN
        // `absolute` pinned to all corners produces the same effect
        // (the overlay is mounted near the root of the app tree, so
        // the absolute box covers the brew view too).
        <YStack
            style={Platform.OS === 'web'
                ? { position: 'fixed' as any, top: 0, left: 0, right: 0, bottom: 0, zIndex: 200 }
                : { position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, zIndex: 200 }}
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
            {pendingAfter > 0 && (
                <Text fontSize="$4" color="black" opacity={0.7}>
                    +{pendingAfter} adição{pendingAfter > 1 ? 'ões' : ''} na fila
                </Text>
            )}
            <Button size="$6" theme="dark"
                width="100%" maxWidth={420} height={80}
                onPress={() => { confirmedCount.value = idx + 1; }}>
                <Text fontSize="$6">Adicionei ✓</Text>
            </Button>
        </YStack>
    );
}
