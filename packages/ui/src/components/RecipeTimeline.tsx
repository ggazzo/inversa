// RecipeTimeline.tsx — full vertical timeline of the loaded recipe.
import { useEffect, useRef } from 'react';
import { Platform } from '../platform';
import { useSignals } from '@preact/signals-react/runtime';
import { Card, ScrollView, Text, XStack, YStack } from 'tamagui';
import {
    recipeName, recipeStep, recipeTotalSteps, loadedRecipeContent,
} from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { parseRecipe, KIND_ICON, type RecipeStep, type StepKind } from '@inversa/utils';

function statusOf(idx: number, current: number): 'done' | 'current' | 'pending' {
    if (idx < current)   return 'done';
    if (idx === current) return 'current';
    return 'pending';
}

interface RowProps { idx: number; step: RecipeStep; state: 'done' | 'current' | 'pending' }
function Row({ idx, step, state }: RowProps) {
    const isMarker = step.kind === 'marker';
    const dotBg    = state === 'done' ? '$holding' : state === 'current' ? '$primary' : '$borderColor';
    const opacity  = state === 'done' ? 0.5 : state === 'current' ? 1 : 0.8;
    const fontWeight = state === 'current' ? '700' : '400';
    return (
        <XStack
            ai="flex-start" gap="$2" paddingVertical="$1"
            opacity={opacity}
            marginTop={isMarker ? '$3' : 0}
            data-current={state === 'current' ? 'true' : undefined}
        >
            <YStack
                marginLeft={2} marginTop={6}
                width={10} height={10} borderRadius={5}
                backgroundColor={dotBg}
                style={state === 'current' ? { boxShadow: '0 0 0 4px rgba(249,115,22,0.3)' } : undefined}
            />
            <Text fontSize="$5" lineHeight={20} userSelect="none">
                {KIND_ICON[step.kind as StepKind] || '·'}
            </Text>
            {isMarker ? (
                <Text textTransform="uppercase" letterSpacing={1} fontSize="$1">
                    {step.label}
                </Text>
            ) : (
                <Text flex={1} fontSize="$3" fontWeight={fontWeight}>
                    {step.label}
                    {step.kind === 'hop' && step.value !== undefined && (
                        <Text opacity={0.5}> @ {step.value} min</Text>
                    )}
                </Text>
            )}
            <Text fontSize={10} opacity={0.4} fontFamily="$mono">{idx + 1}</Text>
        </XStack>
    );
}

export function RecipeTimeline() {
    useSignals();
    const listRef = useRef<HTMLDivElement | null>(null);
    const steps   = parseRecipe(loadedRecipeContent.value);
    const current = recipeStep.value;

    // Lazy fetch on recovery resume (firmware loaded the file but didn't
    // re-broadcast the content).
    useEffect(() => {
        if (!loadedRecipeContent.value && recipeName.value) {
            ConnectionManager.loadRecipe(recipeName.value)
                .then((r: any) => { loadedRecipeContent.value = r.content || ''; })
                .catch(() => {});
        }
    }, [recipeName.value]);

    // Auto-scroll the current step into view. Web uses scrollIntoView
    // on the DOM child marked with `data-current`. RN's equivalent
    // (`ScrollView.scrollTo` with a measured offset) needs per-row
    // refs and `onLayout` plumbing — deferred. Manual scroll on
    // mobile is acceptable for the timeline.
    useEffect(() => {
        if (Platform.OS !== 'web') return;
        const el = (listRef.current as any)?.querySelector?.(
            '[data-current="true"]',
        ) as HTMLElement | null;
        if (el && typeof el.scrollIntoView === 'function') {
            el.scrollIntoView({ block: 'center', behavior: 'smooth' });
        }
    }, [current]);

    if (!steps.length) {
        return (
            <Card elevate size="$4" padded>
                <Text fontWeight="700">Roteiro da receita</Text>
                <Text fontSize="$1" opacity={0.5} marginTop="$1">
                    Carregando DSL{recipeName.value ? ` de ${recipeName.value}` : ''}…
                </Text>
            </Card>
        );
    }

    return (
        <Card elevate size="$4" padded>
            <YStack gap="$2">
                <XStack ai="baseline" jc="space-between">
                    <Text fontWeight="700">Roteiro da receita</Text>
                    <Text fontSize="$1" opacity={0.5} fontFamily="$mono">
                        {Math.min(current + 1, steps.length)} / {recipeTotalSteps.value || steps.length}
                    </Text>
                </XStack>

                <ScrollView ref={listRef as any} maxHeight={320}>
                    <YStack paddingLeft="$5" position="relative">
                        {/* spine */}
                        <YStack
                            position="absolute" left={12} top={4} bottom={4} width={1}
                            backgroundColor="$borderColor"
                        />
                        {steps.map((s, i) => (
                            <Row key={i} idx={i} step={s} state={statusOf(i, current)} />
                        ))}
                    </YStack>
                </ScrollView>
            </YStack>
        </Card>
    );
}
