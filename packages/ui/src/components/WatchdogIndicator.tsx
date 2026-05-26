// WatchdogIndicator.tsx — compact chip showing the Thermal Watchdog
// state in the TopBar. Hidden when the firmware does not advertise the
// `wd` telemetry key (older builds).

import { useSignals } from '@preact/signals-react/runtime';
import { Text, XStack, useTheme } from 'tamagui';
import {
    isConnected,
    watchdogSupported,
    watchdogTripped,
    watchdogLastCause,
} from '@brewpilot/stores';

interface Props {
    onPress?: () => void;
}

export function WatchdogIndicator({ onPress }: Props) {
    useSignals();
    const t = useTheme();
    if (!isConnected.value) return null;
    if (!watchdogSupported.value) return null;

    const tripped = watchdogTripped.value;
    const cause   = watchdogLastCause.value;

    // Armed-and-fine state stays muted to avoid visual noise. Tripped
    // state goes bright red with the cause spelled out — the cervejeiro
    // needs to see this from across the kitchen.
    const bg     = tripped ? '$error'    : 'transparent';
    const border = tripped ? '$error'    : '$borderColor';
    const color  = tripped ? 'white'     : '$color';
    const label  = tripped ? `⛔ ${cause}` : '🛡 Armado';

    return (
        <XStack
            paddingHorizontal="$2"
            paddingVertical="$1"
            borderWidth={1}
            borderColor={border}
            backgroundColor={bg as any}
            br="$10"
            alignItems="center"
            gap="$1"
            onPress={onPress}
            cursor={onPress ? 'pointer' : undefined}
            aria-label={tripped ? `Watchdog disparado: ${cause}` : 'Watchdog armado'}
        >
            <Text fontSize="$1" color={color as any} fontWeight={tripped ? '700' : '400'}>
                {label}
            </Text>
        </XStack>
    );
}
