// RecoveryPrompt.tsx — yellow warning card with Retomar/Descartar.
import { Button, Card, Paragraph, Text, XStack, YStack } from 'tamagui';
import { recoveryRecipeName, showToast } from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';

export function RecoveryPrompt() {
    return (
        <Card
            elevate size="$4" padded
            backgroundColor="$paused" opacity={0.95}
            borderColor="$paused" borderWidth={1}
        >
            <YStack gap="$3">
                <XStack alignItems="center" gap="$2">
                    <Text fontSize={24}>⚠️</Text>
                    <Text fontWeight="700" fontSize="$5" color="black">
                        Recuperação encontrada
                    </Text>
                </XStack>
                <Paragraph color="black">
                    O Inversa detectou uma receita interrompida no SD:
                    {' '}<Text fontFamily="$mono" fontWeight="700" color="black">
                        {recoveryRecipeName.value || '?'}
                    </Text>. Deseja retomar do passo onde parou?
                </Paragraph>
                <XStack gap="$2" flexWrap="wrap">
                    <Button flex={1} minWidth={120} theme="active"
                        onPress={() => ConnectionManager.resumeRecovery()
                            .then(() => showToast('Receita retomada', 'success'))
                            .catch((e: any) => showToast(e?.message || 'Falha', 'error'))}>
                        Retomar
                    </Button>
                    <Button flex={1} minWidth={120} variant="outlined"
                        onPress={() => ConnectionManager.discardRecovery()
                            .then(() => showToast('Recuperação descartada'))
                            .catch(() => {})}>
                        Descartar
                    </Button>
                </XStack>
            </YStack>
        </Card>
    );
}
