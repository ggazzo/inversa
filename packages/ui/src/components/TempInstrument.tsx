// TempInstrument.tsx — left-column instrument: large temperature
// reading + chart + heater/pump/PID strip.
//
// The chart itself is platform-specific (Chart.js on web, victory-native
// on RN) so the consumer injects it via the `chart` prop. This keeps
// `packages/ui` free of DOM imports and lets `apps/web` and
// `apps/native` each pass their own implementation.

import type { ReactNode } from 'react';
import { Card, H1, Text, XStack, YStack } from 'tamagui';
import {
    currentTemp, targetTemp, pidOutput, heaterOn, pumpOn, mode,
} from '@inversa/stores';
import { isStale } from '@inversa/stores';
import { tempStatus } from './util';

function Stat({ label, value, highlight }: { label: string; value: string; highlight?: string }) {
    return (
        <YStack
            flex={1}
            backgroundColor="$backgroundFocus"
            borderRadius="$2"
            paddingHorizontal="$2" paddingVertical="$1.5"
            alignItems="center"
        >
            <Text fontSize={9} opacity={0.5} textTransform="uppercase" letterSpacing={1}>
                {label}
            </Text>
            <Text fontFamily="$mono" fontWeight="700" color={highlight ?? '$color'}>
                {value}
            </Text>
        </YStack>
    );
}

export function TempInstrument({ chart }: { chart?: ReactNode } = {}) {
    const status = tempStatus(currentTemp.value, targetTemp.value, mode.value);
    const stale  = isStale.value;
    const pidPct = Math.round((pidOutput.value / 255) * 100);

    return (
        <Card elevate size="$4" opacity={stale ? 0.6 : 1} animation="quicker">
            <Card.Header padded={false} paddingHorizontal="$4" paddingTop="$4">
                <XStack alignItems="baseline" gap="$2">
                    <H1
                        aria-live="polite"
                        size="$11"
                        fontFamily="$mono"
                        color={status.color}
                        lineHeight={48}
                    >
                        {currentTemp.value.toFixed(1)}
                    </H1>
                    <Text fontSize="$6" opacity={0.6}>°C</Text>
                    {stale && (
                        <XStack
                            ml="auto" paddingHorizontal="$2" paddingVertical={2}
                            backgroundColor="$paused" br="$2"
                        >
                            <Text fontSize="$1" color="black" fontWeight="700">STALE</Text>
                        </XStack>
                    )}
                </XStack>

                <XStack jc="space-between" mt="$1">
                    <Text fontSize="$2" opacity={0.7}>
                        {targetTemp.value > 0 ? (
                            <>alvo <Text fontWeight="700">{targetTemp.value.toFixed(1)}°C</Text></>
                        ) : 'sem alvo'}
                    </Text>
                    <Text fontSize="$1" color={status.color} textTransform="uppercase" letterSpacing={1}>
                        {status.label}
                    </Text>
                </XStack>
            </Card.Header>

            <YStack paddingHorizontal="$4" paddingTop="$2">
                {chart}
            </YStack>

            <Card.Footer padded={false} paddingHorizontal="$4" paddingBottom="$4">
                <XStack gap="$2" width="100%" mt="$2">
                    <Stat label="PID"    value={`${pidPct}%`} />
                    <Stat label="Heater" value={heaterOn.value ? 'ON' : 'off'}
                          highlight={heaterOn.value ? '$heating' : undefined} />
                    <Stat label="Bomba"  value={pumpOn.value ? 'ON' : 'off'}
                          highlight={pumpOn.value ? '$cooling' : undefined} />
                </XStack>
            </Card.Footer>
        </Card>
    );
}
