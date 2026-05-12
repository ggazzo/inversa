// BrewView.jsx — context-driven layout. Picks which sub-state component
// to render based on what the firmware tells us.
//
// Phase 2: full dispatcher across all known sub-states. The unifying
// invariant: TempInstrument on the left always, ContextPanel on the
// right; the right side switches body based on `recipeState`,
// `boilActive`, `mode`, and `autoTuneActive`.

import { useComputed } from '@preact/signals';
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

// Sub-states that mean "a recipe is actively running" — the only times
// the timeline block is meaningful (and the only times the firmware has
// a recipe loaded that we can fetch the DSL for).
const RECIPE_SUBS = new Set(['paused', 'waitconfirm', 'boil', 'waittemp', 'waittimer', 'mash']);

// Returns one of the sub-state identifiers, derived purely from state.
// Order matters — earlier checks win when multiple flags are set.
function deriveSubState() {
    if (!isConnected.value)              return 'disconnected';
    if (hasRecovery.value)               return 'recovery';
    if (autoTuneActive.value)            return 'autotune';
    if (mode.value === 'manual')         return 'manual';

    if (mode.value === 'recipe' && recipeState.value !== 'idle' && recipeState.value !== 'completed') {
        if (recipeState.value === 'paused')          return 'paused';
        if (recipeState.value === 'waiting_confirm') return 'waitconfirm';
        if (boilActive.value)                        return 'boil';
        if (recipeState.value === 'waiting_temp')    return 'waittemp';
        if (recipeState.value === 'waiting_timer')   return 'waittimer';
        return 'mash';
    }
    return 'idle';
}

export function BrewView({ onMenuSelect, onStartManual }) {
    const sub = useComputed(deriveSubState);

    // Disconnected fills the whole viewport — no temp panel.
    if (sub.value === 'disconnected') {
        return (
            <div class="max-w-md mx-auto px-3 sm:px-6 py-6">
                <Disconnected />
            </div>
        );
    }

    return (
        <div class="px-3 sm:px-6 py-3 sm:py-4">
            <div class="max-w-5xl lg:max-w-6xl mx-auto space-y-3">
                <div class="grid grid-cols-1 md:grid-cols-2 gap-3">
                    <TempInstrument />
                    {/* `key` forces a remount when the sub-state changes, which
                        in turn replays the `subview-enter` keyframe animation.
                        Reduced-motion users opt out via the CSS media query. */}
                    <div key={sub.value} class="subview-enter">
                        <ContextPanel sub={sub.value}
                                      onMenuSelect={onMenuSelect}
                                      onStartManual={onStartManual} />
                    </div>
                </div>

                {/* Timeline only when a recipe is alive. It auto-scrolls to
                    the current step; on mobile this sits below the context
                    panel and is collapsible by its own scroll affordance. */}
                {RECIPE_SUBS.has(sub.value) && <RecipeTimeline />}
            </div>
        </div>
    );
}

function ContextPanel({ sub, onMenuSelect, onStartManual }) {
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
