// Paused.tsx — yellow card while the recipe sits paused.
import { Button, Card, Text, XStack, YStack } from 'tamagui';
import { recipeName, recipeStep, recipeTotalSteps, brewingStepName } from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';

export function Paused() {
    return (
        <Card elevate size="$4" padded backgroundColor="$paused" opacity={0.95}>
            <YStack ai="center" gap="$3">
                <Text fontSize={40}>⏸</Text>
                <Text fontWeight="700" fontSize="$7" color="black">Receita pausada</Text>
                <Text fontSize="$2" color="black" opacity={0.8}>
                    <Text fontFamily="$mono" color="black">{recipeName.value || '—'}</Text>
                    {brewingStepName.value && <> · {brewingStepName.value}</>}
                    {recipeTotalSteps.value > 0 && <>
                        {' · passo '}
                        <Text fontFamily="$mono" color="black">
                            {recipeStep.value}/{recipeTotalSteps.value}
                        </Text>
                    </>}
                </Text>
                <Text fontSize="$1" color="black" opacity={0.7} textAlign="center" maxWidth={420}>
                    O timer interno é preservado — quando você retomar, o tempo
                    pausado não é descontado da contagem.
                </Text>
                <XStack gap="$2" width="100%" mt="$2">
                    <Button flex={1} theme="green"
                        onPress={() => ConnectionManager.resumeRecipe().catch(() => {})}>
                        Retomar
                    </Button>
                    <Button flex={1} variant="outlined" theme="red"
                        onPress={() => { if (confirm('Parar receita?')) ConnectionManager.stopRecipe(); }}>
                        Parar
                    </Button>
                </XStack>
            </YStack>
        </Card>
    );
}
