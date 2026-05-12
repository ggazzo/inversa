// Idle.tsx — entry CTAs (Iniciar Receita / Modo Manual) + shortcuts.
import { Button, Text, XStack, YStack } from 'tamagui';
import type { MenuId } from '../../components/TopBar';

interface Props {
    onMenuSelect: (id: MenuId) => void;
    onStartManual: () => void;
}

export function Idle({ onMenuSelect, onStartManual }: Props) {
    return (
        <YStack gap="$3">
            <XStack gap="$3" flexWrap="wrap">
                <Button
                    flex={1} minWidth={140}
                    size="$6" theme="active"
                    onPress={() => onMenuSelect('recipes')}
                    flexDirection="column" gap="$1" paddingVertical="$5"
                >
                    <Text fontSize={24}>🍺</Text>
                    <Text fontSize="$4" fontWeight="700">Iniciar Receita</Text>
                    <Text fontSize="$1" opacity={0.7}>Escolher do SD card</Text>
                </Button>
                <Button
                    flex={1} minWidth={140}
                    size="$6" variant="outlined"
                    onPress={onStartManual}
                    flexDirection="column" gap="$1" paddingVertical="$5"
                >
                    <Text fontSize={24}>🎛️</Text>
                    <Text fontSize="$4" fontWeight="700">Modo Manual</Text>
                    <Text fontSize="$1" opacity={0.7}>Setpoint direto</Text>
                </Button>
            </XStack>
            <XStack gap="$2">
                <Button flex={1} size="$2" chromeless onPress={() => onMenuSelect('tuning')}>
                    AutoTune
                </Button>
                <Button flex={1} size="$2" chromeless onPress={() => onMenuSelect('brewlog')}>
                    Brew log
                </Button>
                <Button flex={1} size="$2" chromeless onPress={() => onMenuSelect('equipment')}>
                    Equipamento
                </Button>
            </XStack>
        </YStack>
    );
}
