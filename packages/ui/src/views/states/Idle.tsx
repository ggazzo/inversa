// Idle.tsx — entry CTAs (Iniciar Receita / Modo Manual) + shortcuts.
import { Button, Text, XStack, YStack } from 'tamagui';
import type { MenuId } from '../../components/TopBar';

interface Props {
    onMenuSelect: (id: MenuId) => void;
    onStartManual: () => void;
}

export function Idle({ onMenuSelect, onStartManual }: Props) {
    return (
        // Two main CTAs share a row with `flexBasis={0}` so neither
        // button's intrinsic content (solid vs. outlined variant has a
        // slightly different padding model) shifts the 50/50 split.
        // The shortcut row underneath does NOT pretend to be a 3-column
        // grid — that fights the 2-column row above and the edges never
        // line up. It's a centered chip row.
        <YStack gap="$3">
            <XStack gap="$3" flexWrap="wrap">
                <Button
                    flex={1} flexBasis={0} minWidth={140}
                    size="$6" theme="active"
                    onPress={() => onMenuSelect('recipes')}
                >
                    <Text fontSize="$4" fontWeight="700">Iniciar Receita</Text>
                </Button>
                <Button
                    flex={1} flexBasis={0} minWidth={140}
                    size="$6" variant="outlined"
                    onPress={onStartManual}
                >
                    <Text fontSize="$4" fontWeight="700">Modo Manual</Text>
                </Button>
            </XStack>
            <XStack gap="$2" jc="center" flexWrap="wrap">
                <Button size="$2" chromeless onPress={() => onMenuSelect('tuning')}>
                    AutoTune
                </Button>
                <Button size="$2" chromeless onPress={() => onMenuSelect('brewlog')}>
                    Brew log
                </Button>
                <Button size="$2" chromeless onPress={() => onMenuSelect('equipment')}>
                    Equipamento
                </Button>
            </XStack>
        </YStack>
    );
}
