// Disconnected.tsx — single CTA card; adapts text for sim mode.
import { Button, Card, H2, Paragraph, Text, YStack } from 'tamagui';
import { ConnectionManager } from '../../services/ConnectionManager';
import { BLEService } from '../../services/BLEService';

export function Disconnected() {
    const supported = BLEService.isSupported();
    const simMode   = typeof location !== 'undefined'
        && !!new URLSearchParams(location.search).get('sim');

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
                    onPress={() => ConnectionManager.connect().catch(() => {})}
                >
                    {simMode ? 'Iniciar sessão simulada' : 'Conectar'}
                </Button>
            </YStack>
        </Card>
    );
}
