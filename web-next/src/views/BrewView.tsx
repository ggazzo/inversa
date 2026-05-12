// BrewView.tsx — state-driven layout. Picks which sub-state component
// to render based on what the firmware tells us, mirroring the legacy
// dispatcher in `web/src/views/BrewView.jsx`.

import { useSignals } from '@preact/signals-react/runtime';
import { XStack, YStack } from 'tamagui';
import {
    isConnected, hasRecovery, mode, recipeState, autoTuneActive, boilActive,
} from '../stores/state';
import { TempInstrument } from '../components/TempInstrument';
import { RecipeTimeline } from '../components/RecipeTimeline';
import { Disconnected }   from './states/Disconnected';
import { RecoveryPrompt } from './states/RecoveryPrompt';
import { Idle }           from './states/Idle';
import { Mash }           from './states/Mash';
import { WaitTemp }       from './states/WaitTemp';
import { WaitTimer }      from './states/WaitTimer';
import { WaitConfirm }    from './states/WaitConfirm';
import { BoilActive }     from './states/BoilActive';
import { Paused }         from './states/Paused';
import { Manual }         from './states/Manual';
import { AutoTune }       from './states/AutoTune';
import type { MenuId } from '../components/TopBar';

const RECIPE_SUBS = new Set([
    'paused', 'waitconfirm', 'boil', 'waittemp', 'waittimer', 'mash',
]);

type Sub =
    | 'disconnected' | 'recovery' | 'autotune' | 'manual'
    | 'paused' | 'waitconfirm' | 'boil' | 'waittemp' | 'waittimer' | 'mash'
    | 'idle';

function deriveSubState(): Sub {
    if (!isConnected.value)              return 'disconnected';
    if (hasRecovery.value)               return 'recovery';
    if (autoTuneActive.value)            return 'autotune';
    if (mode.value === 'manual')         return 'manual';

    if (mode.value === 'recipe' &&
        recipeState.value !== 'idle' && recipeState.value !== 'completed') {
        if (recipeState.value === 'paused')          return 'paused';
        if (recipeState.value === 'waiting_confirm') return 'waitconfirm';
        if (boilActive.value)                        return 'boil';
        if (recipeState.value === 'waiting_temp')    return 'waittemp';
        if (recipeState.value === 'waiting_timer')   return 'waittimer';
        return 'mash';
    }
    return 'idle';
}

interface Props {
    onMenuSelect: (id: MenuId) => void;
    onStartManual: () => void;
}

export function BrewView({ onMenuSelect, onStartManual }: Props) {
    useSignals();
    const sub = deriveSubState();

    if (sub === 'disconnected') {
        return (
            <YStack maxWidth={460} marginHorizontal="auto" paddingHorizontal="$3" paddingVertical="$6">
                <Disconnected />
            </YStack>
        );
    }

    return (
        <YStack paddingHorizontal="$3" paddingVertical="$3" gap="$3">
            <YStack maxWidth={1100} marginHorizontal="auto" width="100%" gap="$3">
                <XStack
                    gap="$3" flexWrap="wrap"
                    $gtSm={{ flexWrap: 'nowrap' }}
                >
                    <YStack flex={1} minWidth={300} $sm={{ minWidth: '100%' }}>
                        <TempInstrument />
                    </YStack>
                    <YStack flex={1} minWidth={300} $sm={{ minWidth: '100%' }}>
                        <ContextPanel sub={sub}
                                      onMenuSelect={onMenuSelect}
                                      onStartManual={onStartManual} />
                    </YStack>
                </XStack>
                {RECIPE_SUBS.has(sub) && <RecipeTimeline />}
            </YStack>
        </YStack>
    );
}

function ContextPanel({ sub, onMenuSelect, onStartManual }:
    { sub: Sub; onMenuSelect: (id: MenuId) => void; onStartManual: () => void }) {
    switch (sub) {
        case 'recovery':    return <RecoveryPrompt />;
        case 'autotune':    return <AutoTune />;
        case 'manual':      return <Manual />;
        case 'paused':      return <Paused />;
        case 'waitconfirm': return <WaitConfirm />;
        case 'boil':        return <BoilActive />;
        case 'waittemp':    return <WaitTemp />;
        case 'waittimer':   return <WaitTimer />;
        case 'mash':        return <Mash />;
        case 'idle':
        default:            return <Idle onMenuSelect={onMenuSelect}
                                         onStartManual={onStartManual} />;
    }
}
