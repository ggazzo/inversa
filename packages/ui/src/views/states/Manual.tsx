// Manual.tsx — direct setpoint + actuator toggles.
import { useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Card, Slider, Text, XStack, YStack } from 'tamagui';
import { targetTemp, heaterOn, pumpOn, mode, manualIntent, showToast } from '@brewpilot/stores';
import { ConnectionManager } from '@brewpilot/services';

export function Manual() {
    useSignals();
    // Local mirror so the slider feels responsive without round-tripping
    // every move. Confirmed by the user with "Aplicar".
    const [draft, setDraft] = useState<number>(targetTemp.value || 65);

    function applyTemp() {
        const v = Math.max(0, Math.min(110, Math.round(draft)));
        ConnectionManager.setTemp(v)
            .then(() => showToast(`Alvo: ${v}°C`, 'success'))
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    function emergencyOff() {
        ConnectionManager.heaterOff()
            .then(() => ConnectionManager.pumpOff().catch(() => {}))
            .then(() => {
                mode.value = 'idle';
                manualIntent.value = false;
                showToast('Desligado');
            })
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    return (
        <Card elevate size="$4" padded>
            <YStack gap="$3">
                <XStack jc="space-between" ai="center">
                    <Text fontWeight="700">Modo Manual</Text>
                    <Button size="$1" chromeless
                        onPress={() => { mode.value = 'idle'; manualIntent.value = false; }}>
                        Sair
                    </Button>
                </XStack>

                <XStack ai="baseline" gap="$2">
                    <Text fontSize="$1" opacity={0.5} textTransform="uppercase">Alvo</Text>
                    <Text fontFamily="$mono" fontSize={32}>{Math.round(draft)}</Text>
                    <Text opacity={0.6}>°C</Text>
                </XStack>

                <Slider min={0} max={110} step={1}
                        value={[Math.round(draft)]}
                        onValueChange={(v) => setDraft(v[0])}>
                    <Slider.Track backgroundColor="$backgroundFocus">
                        <Slider.TrackActive backgroundColor="$primary" />
                    </Slider.Track>
                    <Slider.Thumb size="$2" index={0} circular />
                </Slider>

                <XStack jc="space-between" paddingHorizontal="$1">
                    {[0, 30, 65, 100, 110].map((v) => (
                        <Text key={v} fontSize="$1" opacity={0.4}>{v}</Text>
                    ))}
                </XStack>

                <Button theme="active"
                        disabled={Math.round(draft) === Math.round(targetTemp.value)}
                        onPress={applyTemp}>
                    Aplicar alvo
                </Button>

                <XStack gap="$2" mt="$2">
                    <Button flex={1}
                        backgroundColor={heaterOn.value ? '$heating' : undefined}
                        variant={heaterOn.value ? undefined : 'outlined'}
                        onPress={() => (heaterOn.value
                            ? ConnectionManager.heaterOff()
                            : ConnectionManager.heaterOn()).catch(() => {})}>
                        Aquecedor {heaterOn.value ? 'ON' : 'OFF'}
                    </Button>
                    <Button flex={1}
                        backgroundColor={pumpOn.value ? '$cooling' : undefined}
                        variant={pumpOn.value ? undefined : 'outlined'}
                        onPress={() => (pumpOn.value
                            ? ConnectionManager.pumpOff()
                            : ConnectionManager.pumpOn()).catch(() => {})}>
                        Bomba {pumpOn.value ? 'ON' : 'OFF'}
                    </Button>
                </XStack>

                <Button theme="red" onPress={emergencyOff}>
                    Desligar tudo
                </Button>
            </YStack>
        </Card>
    );
}
