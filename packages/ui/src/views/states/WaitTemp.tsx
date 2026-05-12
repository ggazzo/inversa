// WaitTemp.tsx — "aguardando temperatura" with delta + ETA.
import { Card, Progress, Text, XStack, YStack } from 'tamagui';
import { currentTemp, targetTemp, recipeName, brewingStepName } from '@inversa/stores';

function approxEtaMin(curr: number, target: number): number | null {
    const delta = target - curr;
    if (!isFinite(delta) || delta <= 0.5) return null;
    const slopePerMin = 1.4;   // see legacy WaitTemp comment for rationale
    return Math.max(1, Math.round(delta / slopePerMin));
}

export function WaitTemp() {
    const curr   = currentTemp.value;
    const target = targetTemp.value;
    const delta  = target - curr;
    const eta    = approxEtaMin(curr, target);
    const pct    = target > 0 ? Math.max(0, Math.min(100, (curr / target) * 100)) : 0;

    const dirColor = delta > 0.5 ? '$heating' : delta < -0.5 ? '$cooling' : '$holding';
    const dirLabel = delta > 0.5 ? 'aquecendo' : delta < -0.5 ? 'esfriando' : 'estabilizando';

    return (
        <Card elevate size="$4" padded>
            <YStack ai="center" gap="$3">
                <XStack width="100%" jc="space-between">
                    <Text fontSize="$1" opacity={0.5} textTransform="uppercase" letterSpacing={1}>
                        Receita · {recipeName.value || '—'}
                    </Text>
                    <Text fontSize="$1" color={dirColor} textTransform="uppercase">
                        {dirLabel}
                    </Text>
                </XStack>

                <Text fontSize="$7" fontWeight="600">
                    {brewingStepName.value || 'Aguardando temperatura'}
                </Text>

                <Text fontSize="$1" opacity={0.6}>Alvo</Text>
                <Text fontFamily="$mono" fontSize={56} fontWeight="700" color="$primary" lineHeight={56}>
                    {target.toFixed(1)}°C
                </Text>

                <Text fontSize="$1" opacity={0.6}>Falta</Text>
                <Text fontFamily="$mono" fontSize={32} color={dirColor}>
                    {delta > 0 ? '+' : ''}{delta.toFixed(1)}°C
                </Text>

                {eta !== null && (
                    <Text fontSize="$1" opacity={0.5}>
                        ETA aprox. <Text fontWeight="700">{eta} min</Text>
                    </Text>
                )}

                <Progress value={pct} width="100%" mt="$2" backgroundColor="$backgroundFocus">
                    <Progress.Indicator backgroundColor="$primary" />
                </Progress>
            </YStack>
        </Card>
    );
}
