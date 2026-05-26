// AutoTune.tsx — progress + cancel during a relay-feedback tune.
import { Button, Card, Paragraph, Progress, Text, YStack } from 'tamagui';
import { useSignals } from '@preact/signals-react/runtime';
import { autoTuneActive, autoTuneProgress, targetTemp, showToast } from '@brewpilot/stores';
import { ConnectionManager } from '@brewpilot/services';

export function AutoTune() {
    useSignals();
    function cancel() {
        if (!confirm('Cancelar AutoTune?')) return;
        ConnectionManager.stopAutoTune()
            .then(() => showToast('AutoTune cancelado'))
            .catch(() => {});
    }
    return (
        <Card elevate size="$4" padded>
            <YStack ai="center" gap="$3">
                <Text fontSize={40}>🎯</Text>
                <Text fontWeight="700" fontSize="$7">AutoTune em andamento</Text>
                <Paragraph theme="alt2" textAlign="center" maxWidth={420}>
                    O firmware está fazendo o controle bang-bang em torno do
                    alvo <Text fontFamily="$mono">{targetTemp.value.toFixed(1)}°C</Text>
                    {' '}pra calcular Kp/Ki/Kd. Tipicamente leva 15-30 min
                    dependendo do volume e potência.
                </Paragraph>
                <YStack width="100%" gap="$1">
                    <Progress value={autoTuneProgress.value} backgroundColor="$backgroundFocus">
                        <Progress.Indicator backgroundColor="$primary" />
                    </Progress>
                    <Text fontSize="$1" opacity={0.5} textAlign="center">
                        {autoTuneProgress.value}% — não interrompa
                    </Text>
                </YStack>
                <Button theme="red" variant="outlined"
                        disabled={!autoTuneActive.value} onPress={cancel}>
                    Cancelar
                </Button>
            </YStack>
        </Card>
    );
}
