// BoilActive.tsx — big countdown + HopTimeline + pause/stop.
import { Button, Card, Text, XStack, YStack } from 'tamagui';
import { recipeName, brewingStepName, boilRemaining, recipeState } from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { HopTimeline } from '../../components/HopTimeline';
import { fmtMmSs } from '../../components/util';
import { confirm } from '../../platform';

export function BoilActive() {
    const paused = recipeState.value === 'paused';
    return (
        <Card elevate size="$4" padded>
            <YStack gap="$3">
                <XStack jc="space-between" ai="center">
                    <Text fontSize="$1" opacity={0.5} textTransform="uppercase" letterSpacing={1}>
                        {recipeName.value || 'Fervura'} · {brewingStepName.value || 'Fervura'}
                    </Text>
                    <Text
                        paddingHorizontal="$2" paddingVertical={2}
                        backgroundColor={paused ? '$paused' : '$heating'}
                        color="white" br="$10" fontSize="$1"
                    >
                        {paused ? 'pausada' : 'ferver'}
                    </Text>
                </XStack>

                <YStack ai="center" paddingVertical="$2">
                    <Text fontSize="$1" opacity={0.5} textTransform="uppercase" letterSpacing={1}>
                        Restante
                    </Text>
                    <Text fontFamily="$mono" fontSize={80} fontWeight="700" lineHeight={80}>
                        {fmtMmSs(boilRemaining.value)}
                    </Text>
                </YStack>

                <HopTimeline />

                <XStack gap="$2" mt="$2">
                    {paused ? (
                        <Button flex={1} theme="green"
                            onPress={() => ConnectionManager.resumeRecipe().catch(() => {})}>
                            Retomar
                        </Button>
                    ) : (
                        <Button flex={1} chromeless
                            onPress={() => ConnectionManager.pauseRecipe().catch(() => {})}>
                            Pausar
                        </Button>
                    )}
                    <Button flex={1} variant="outlined" theme="red"
                        onPress={async () => { if (await confirm('Parar receita?')) ConnectionManager.stopRecipe(); }}>
                        Parar
                    </Button>
                </XStack>
            </YStack>
        </Card>
    );
}
