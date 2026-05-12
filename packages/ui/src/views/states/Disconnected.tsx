// Disconnected.tsx — single CTA card; adapts text for sim mode.
import { Button, Card, H2, Paragraph, Text, YStack } from 'tamagui';
import { showToast, devicePickerOpen } from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { BleClient } from '@inversa/services';

export function Disconnected() {
    const supported = BleClient.isSupported();
    const simMode   = typeof location !== 'undefined'
        && !!new URLSearchParams(location.search).get('sim');

    function onTap() {
        // RN real BLE: open the in-app picker so the user can choose
        // between nearby devices. Web's `requestDevice` and the sim
        // WebSocket path don't need a picker — they fall through to
        // a direct connect().
        if (BleClient.needsPicker()) {
            devicePickerOpen.value = true;
            return;
        }
        ConnectionManager.connect().catch((err: any) => {
            showToast(err?.message || 'Falha na conexão', 'error');
        });
    }

    return (
        <Card elevate size="$4" padded>
            <YStack alignItems="center" gap="$4" paddingVertical="$6">
                <YStack
                    width={64} height={64} borderRadius={32}
                    backgroundColor="$backgroundFocus"
                    alignItems="center" justifyContent="center"
                >
                    <Text fontSize={28}>📡</Text>
                </YStack>
                <YStack alignItems="center" gap="$1" maxWidth={420}>
                    <H2 size="$8">
                        {simMode ? 'Conectar ao simulador' : 'Conectar Inversa'}
                    </H2>
                    <Paragraph theme="alt2" textAlign="center">
                        {simMode
                            ? 'O simulador está rodando em ws://localhost:8765. Clique abaixo para abrir a sessão virtual.'
                            : supported
                                ? 'Aproxime o dispositivo Inversa e clique abaixo para parear via Bluetooth.'
                                : 'Este navegador não suporta Web Bluetooth. Tente Chrome ou Edge em desktop, ou Bluefy no iOS.'}
                    </Paragraph>
                </YStack>
                <Button
                    size="$5" theme="active"
                    disabled={!supported}
                    onPress={onTap}
                >
                    {simMode ? 'Iniciar sessão simulada' : 'Conectar'}
                </Button>
            </YStack>
        </Card>
    );
}
