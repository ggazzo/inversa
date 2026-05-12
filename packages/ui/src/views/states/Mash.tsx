// Mash.tsx — generic "recipe is running, calm middle" view.
import { Button, Card, Progress, Text, XStack, YStack } from 'tamagui';
import { recipeName, recipeStep, recipeTotalSteps, brewingStepName, showToast } from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { confirm } from '../../platform';

export function Mash() {
    const total = recipeTotalSteps.value;
    const step  = recipeStep.value;
    const pct   = total > 0 ? Math.min(100, (step / total) * 100) : 0;

    return (
        <Card elevate size="$4" padded>
            <YStack gap="$3">
                <XStack jc="space-between" ai="center">
                    <YStack>
                        <Text fontSize="$1" opacity={0.5} textTransform="uppercase" letterSpacing={1}>
                            Receita
                        </Text>
                        <Text fontWeight="700">{recipeName.value || '—'}</Text>
                    </YStack>
                    <Text
                        paddingHorizontal="$2" paddingVertical={2}
                        backgroundColor="$primary" color="white"
                        br="$10" fontSize="$1"
                    >
                        Executando
                    </Text>
                </XStack>

                <YStack gap="$1">
                    <XStack jc="space-between">
                        <Text fontSize="$2" opacity={0.7}>Passo</Text>
                        <Text fontFamily="$mono">
                            <Text fontWeight="700">{step}</Text>
                            <Text opacity={0.5}> / {total || '?'}</Text>
                        </Text>
                    </XStack>
                    <Progress value={pct} backgroundColor="$backgroundFocus">
                        <Progress.Indicator backgroundColor="$primary" />
                    </Progress>
                </YStack>

                {brewingStepName.value && (
                    <Text textAlign="center" fontSize="$8" fontWeight="600" paddingVertical="$3">
                        {brewingStepName.value}
                    </Text>
                )}

                <XStack gap="$2" mt="$1">
                    <Button flex={1} size="$2" chromeless
                        onPress={() => ConnectionManager.pauseRecipe().catch(() => {})}>
                        Pausar
                    </Button>
                    <Button flex={1} size="$2" variant="outlined" theme="red"
                        onPress={async () => {
                            if (await confirm('Parar receita?'))
                                ConnectionManager.stopRecipe()
                                    .then(() => showToast('Receita parada'))
                                    .catch(() => {});
                        }}>
                        Parar
                    </Button>
                </XStack>
            </YStack>
        </Card>
    );
}
