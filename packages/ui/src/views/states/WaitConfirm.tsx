// WaitConfirm.tsx — big single CTA (the WAIT_CONFIRM message).
import { Button, Card, Text, XStack, YStack } from 'tamagui';
import { confirmMessage, recipeName, brewingStepName, showToast } from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';

export function WaitConfirm() {
    function handleConfirm() {
        ConnectionManager.confirmRecipe()
            .then(() => showToast('Confirmado', 'success'))
            .catch((e: any) => showToast(e?.message || 'Falha ao confirmar', 'error'));
    }
    return (
        <Card elevate size="$4" padded theme="blue">
            <YStack ai="center" gap="$3">
                <XStack width="100%">
                    <Text fontSize="$1" opacity={0.5} textTransform="uppercase" letterSpacing={1}>
                        {recipeName.value || '—'} · {brewingStepName.value || 'Aguardando você'}
                    </Text>
                </XStack>
                <Text fontSize={40}>✋</Text>
                <Text fontSize="$6" fontWeight="700" textAlign="center">
                    {confirmMessage.value || 'Aguardando confirmação'}
                </Text>
                <Button size="$6" theme="active" width="100%" height={64} onPress={handleConfirm}>
                    <Text fontSize="$5" fontWeight="700">Continuar</Text>
                </Button>
                <XStack gap="$2" width="100%">
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
