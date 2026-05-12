// RecipeTimeline.jsx — full vertical timeline of the loaded recipe.
//
// Lazy-loads the DSL via `req:recipe:load` if `loadedRecipeContent` is
// empty (typical after `req:recovery:resume` where the firmware loads
// the file internally but doesn't push the content back).
//
// Auto-scrolls the current step into view so the brewer always sees
// "now" without scrolling, even on long recipes.

import { useEffect, useRef } from 'preact/hooks';
import { useComputed, useSignalEffect } from '@preact/signals';
import {
    recipeName, recipeStep, recipeTotalSteps, loadedRecipeContent,
} from '../stores/state';
import { ConnectionManager } from '../services/ConnectionManager';
import { parseRecipe, KIND_ICON } from '../utils/parseRecipe';

export function RecipeTimeline() {
    const steps   = useComputed(() => parseRecipe(loadedRecipeContent.value));
    const current = recipeStep.value;
    const listRef = useRef(null);

    // Lazy fetch if we don't have the DSL yet but a recipe is set
    // (recovery resume or hot-reload of the page mid-brew).
    useEffect(() => {
        if (!loadedRecipeContent.value && recipeName.value) {
            ConnectionManager.loadRecipe(recipeName.value)
                .then((r) => { loadedRecipeContent.value = r.content || ''; })
                .catch(() => {});
        }
    }, [recipeName.value]);

    // Auto-scroll current step into view.
    useSignalEffect(() => {
        // touch the dep so the effect re-runs on step change
        const _ = recipeStep.value;
        const el = listRef.current?.querySelector('[data-current="true"]');
        if (el && typeof el.scrollIntoView === 'function') {
            el.scrollIntoView({ block: 'center', behavior: 'smooth' });
        }
    });

    if (!steps.value.length) {
        return (
            <section class="card bg-base-100 shadow-sm">
                <div class="card-body p-4 sm:p-6">
                    <h2 class="font-semibold text-sm">Roteiro da receita</h2>
                    <p class="text-xs text-base-content/50">
                        Carregando DSL{recipeName.value ? ` de ${recipeName.value}` : ''}…
                    </p>
                </div>
            </section>
        );
    }

    return (
        <section class="card bg-base-100 shadow-sm">
            <div class="card-body p-4 sm:p-6 gap-2">
                <div class="flex items-baseline justify-between">
                    <h2 class="font-semibold text-sm">Roteiro da receita</h2>
                    <span class="text-xs text-base-content/50 font-mono">
                        {Math.min(current + 1, steps.value.length)} / {recipeTotalSteps.value || steps.value.length}
                    </span>
                </div>

                <ol ref={listRef}
                    class="relative max-h-80 overflow-y-auto pr-2 pl-6 space-y-1">
                    {/* spine */}
                    <span class="absolute left-3 top-1 bottom-1 w-px bg-base-300" aria-hidden="true" />

                    {steps.value.map((s, i) => (
                        <Row key={i} idx={i} step={s} state={statusOf(i, current)} />
                    ))}
                </ol>
            </div>
        </section>
    );
}

function statusOf(idx, current) {
    if (idx < current)  return 'done';
    if (idx === current) return 'current';
    return 'pending';
}

function Row({ idx, step, state }) {
    const isMarker = step.kind === 'marker';
    const styles = {
        done:    'text-base-content/40 line-through',
        current: 'text-base-content font-semibold',
        pending: 'text-base-content/70',
    }[state];

    const dot = {
        done:    'bg-success',
        current: 'bg-primary ring-4 ring-primary/30',
        pending: 'bg-base-300',
    }[state];

    return (
        <li class={`relative flex items-start gap-2 py-1 ${styles} ${isMarker ? 'mt-3' : ''}`}
            data-current={state === 'current'}>
            <span class={`absolute -left-[1.05rem] top-2 w-2.5 h-2.5 rounded-full ${dot}`}
                  aria-hidden="true" />
            <span class="text-base leading-none mt-0.5 select-none">
                {KIND_ICON[step.kind] || '·'}
            </span>
            {isMarker ? (
                <span class="uppercase tracking-wider text-xs">{step.label}</span>
            ) : (
                <span class="text-sm flex-1 break-words">
                    {step.label}
                    {step.kind === 'hop' && step.value !== undefined && (
                        <span class="text-base-content/50 ml-1">@ {step.value} min</span>
                    )}
                </span>
            )}
            <span class="text-[10px] text-base-content/40 font-mono shrink-0 mt-1">{idx + 1}</span>
        </li>
    );
}
