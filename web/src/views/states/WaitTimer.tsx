// WaitTimer.tsx — austere giant countdown.
import { Button, Card, Text, XStack, YStack } from 'tamagui';
import {
    timerLeft, recipeName, brewingStepName, recipeStep, recipeTotalSteps,
} from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';
import { fmtMmSs } from '../../components/util';

export function WaitTimer() {
    return (
        <Card elevate size="$4" padded>
            <YStack ai="center" gap="$3">
                <XStack width="100%" jc="space-between">
                    <Text fontSize="$1" opacity={0.5}>{recipeName.value || '—'}</Text>
                    <Text fontSize="$1" opacity={0.5}>
                        Passo {recipeStep.value}/{recipeTotalSteps.value || '?'}
                    </Text>
                </XStack>

                <Text fontSize="$7" fontWeight="600">
                    {brewingStepName.value || 'Aguardando timer'}
                </Text>

                <Text fontFamily="$mono" fontSize={80} fontWeight="700" lineHeight={80}>
                    {fmtMmSs(timerLeft.value)}
                </Text>

                <Text fontSize="$1" opacity={0.5}>Restante até o próximo passo</Text>

                <XStack gap="$2" width="100%" mt="$2">
                    <Button flex={1} size="$2" chromeless
                        onPress={() => ConnectionManager.pauseRecipe().catch(() => {})}>
                        Pausar
                    </Button>
                    <Button flex={1} size="$2" variant="outlined" theme="red"
                        onPress={() => { if (confirm('Parar receita?')) ConnectionManager.stopRecipe(); }}>
                        Parar
                    </Button>
                </XStack>
            </YStack>
        </Card>
    );
}
