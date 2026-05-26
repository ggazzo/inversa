// HopTimeline.tsx — horizontal strip of triggered hops along the boil.
import { Text, XStack, YStack } from 'tamagui';
import { useSignals } from '@preact/signals-react/runtime';
import { boilAlerts, boilTotal, boilRemaining, boilAdditions } from '@brewpilot/stores';
import { fmtMmSs } from './util';

export function HopTimeline() {
    useSignals();
    const total = boilTotal.value;
    const left  = boilRemaining.value;
    const elapsed = total > 0 ? total - left : 0;
    const pct = total > 0 ? Math.max(0, Math.min(100, (elapsed / total) * 100)) : 0;

    const alerts: any[] = boilAlerts.value;
    const remaining = Math.max(0, boilAdditions.value - alerts.length);

    return (
        <YStack backgroundColor="$backgroundFocus" br="$3" padding="$3" gap="$2">
            <XStack jc="space-between">
                <Text fontSize="$1" opacity={0.5} textTransform="uppercase" letterSpacing={1}>
                    Adições
                </Text>
                <Text fontSize="$1" opacity={0.5}>
                    {alerts.length} feitas · {remaining} pendentes
                </Text>
            </XStack>

            {/* spine */}
            <YStack position="relative" height={8} backgroundColor="$borderColor" br="$2">
                <YStack
                    position="absolute" top={0} left={0} bottom={0}
                    backgroundColor="$primary" opacity={0.4} br="$2"
                    width={`${pct}%`}
                />
                {alerts.map((a: any, i: number) => {
                    const minutesIn = total > 0 ? (total / 60) - a.min : 0;
                    const leftPct   = total > 0 ? (minutesIn * 60 / total) * 100 : 0;
                    return (
                        <YStack key={i}
                            position="absolute" top={-6}
                            width={20} height={20} br={10}
                            backgroundColor="$paused"
                            borderWidth={2} borderColor="$background"
                            left={`calc(${leftPct}% - 10px)`}
                        />
                    );
                })}
            </YStack>

            {alerts.length > 0 && (
                <YStack gap={2} mt="$1">
                    {alerts.slice(-3).reverse().map((a: any, i: number) => (
                        <XStack key={i} jc="space-between" gap="$2">
                            <Text fontSize="$1" numberOfLines={1}>🌿 {a.name}</Text>
                            <Text fontSize="$1" opacity={0.5}>{a.min} min</Text>
                        </XStack>
                    ))}
                </YStack>
            )}

            <Text textAlign="center" mt="$2" fontFamily="$mono" fontSize="$6">
                {fmtMmSs(left)}
                <Text fontSize="$1" opacity={0.5}> restantes</Text>
            </Text>
        </YStack>
    );
}
