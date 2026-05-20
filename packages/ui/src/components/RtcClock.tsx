// RtcClock.tsx — small clock chip in the TopBar showing the firmware's
// current RTC time. Renders nothing when disconnected or when the
// firmware reports `rtcAvailable=false`. A side dot indicates whether
// the firmware has done an NTP sync since boot (green) or is still on
// the local DS1307 alone (amber).
//
// The chip exists because, until now, there was no way to glance at the
// app and tell whether the brewing controller has a sane clock. The
// scheduler ("be ready at HH:MM") and recipe timers depend on it; if
// the operator can't see it, they can't trust it.

import { useSignals } from '@preact/signals-react/runtime';
import { Text, XStack } from 'tamagui';
import {
    isConnected,
    rtcAvailable,
    rtcTimestamp,
    rtcNtpSynced,
} from '@inversa/stores';

function fmtHHMM(unix: number): string {
    if (!unix || unix <= 0) return '--:--';
    const d = new Date(unix * 1000);
    return `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
}

interface Props {
    onPress?: () => void;
}

export function RtcClock({ onPress }: Props) {
    useSignals();
    if (!isConnected.value) return null;
    if (!rtcAvailable.value) return null;

    const synced = rtcNtpSynced.value;
    const dotColor = synced ? '$holding' : '$paused';
    const label = fmtHHMM(rtcTimestamp.value);
    const a11y = synced
        ? `Relógio do controlador: ${label}, sincronizado por NTP`
        : `Relógio do controlador: ${label}, sem sincronização NTP recente`;

    return (
        <XStack
            paddingHorizontal="$2"
            paddingVertical="$1"
            borderWidth={1}
            borderColor="$borderColor"
            br="$10"
            alignItems="center"
            gap="$1.5"
            onPress={onPress}
            cursor={onPress ? 'pointer' : undefined}
            aria-label={a11y}
        >
            <XStack
                width={6}
                height={6}
                br="$10"
                backgroundColor={dotColor as any}
            />
            <Text fontSize="$2" fontFamily="$mono">
                {label}
            </Text>
        </XStack>
    );
}
