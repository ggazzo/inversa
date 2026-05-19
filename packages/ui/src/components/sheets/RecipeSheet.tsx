// RecipeSheet.tsx — list, preview, start, delete.
import { useEffect, useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Paragraph, Text, XStack, YStack } from 'tamagui';
import { showToast, loadedRecipeContent } from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { WizardSheet } from '../wizards/WizardSheet';
import { confirm } from '../../platform';

interface Props { open: boolean; onClose: () => void }

export function RecipeSheet({ open, onClose }: Props) {
    useSignals();
    const [recipes, setRecipes] = useState<string[] | null>(null);
    const [err, setErr]         = useState<string | null>(null);
    const [preview, setPreview] = useState<{ file: string; content: string } | null>(null);

    function refresh() {
        if (!open) return;
        setRecipes(null); setErr(null);
        // ConnectionManager.listRecipes() returns the recipe array
        // directly (it already unwraps `res.recipes`). Previously this
        // expected an envelope object and read `r.recipes`, which was
        // always undefined → empty list in the UI.
        ConnectionManager.listRecipes()
            .then((r: string[]) => setRecipes(r ?? []))
            .catch((e: any) => { setErr(e?.message || 'Falha'); setRecipes([]); });
    }
    useEffect(refresh, [open]);

    function openPreview(file: string) {
        ConnectionManager.loadRecipe(file)
            .then((r: any) => {
                setPreview({ file: r.file, content: r.content });
                loadedRecipeContent.value = r.content || '';
            })
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    function start(file: string) {
        const ensure = (preview && preview.file === file)
            ? Promise.resolve()
            : ConnectionManager.loadRecipe(file).then((r: any) => {
                loadedRecipeContent.value = r.content || '';
            });
        ensure
            .then(() => ConnectionManager.startRecipe(file))
            .then(() => { showToast(`Iniciando ${file}`, 'success'); onClose(); })
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    async function del(file: string) {
        if (!(await confirm(`Apagar ${file}?`))) return;
        ConnectionManager.deleteRecipe(file)
            .then(() => { showToast('Apagado'); refresh(); })
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    if (preview) {
        const lineCount = preview.content.split('\n').filter(l => l.trim()).length;
        return (
            <WizardSheet title={preview.file} open={open} onClose={() => setPreview(null)}
                footer={<>
                    <Button size="$3" chromeless onPress={() => setPreview(null)}>Voltar</Button>
                    <Button size="$3" theme="active" onPress={() => start(preview.file)}>Iniciar</Button>
                </>}>
                <Text fontSize="$1" opacity={0.5}>{lineCount} comandos</Text>
                <YStack
                    backgroundColor="$backgroundFocus"
                    br="$3" padding="$3"
                >
                    <Text fontFamily="$mono" fontSize="$1" whiteSpace="pre-wrap">
                        {preview.content}
                    </Text>
                </YStack>
            </WizardSheet>
        );
    }

    return (
        <WizardSheet title="Receitas salvas" open={open} onClose={onClose}
            footer={<Button size="$3" onPress={onClose}>Fechar</Button>}>
            {recipes === null && <Text fontSize="$2" opacity={0.5}>Listando do SD…</Text>}
            {recipes && recipes.length === 0 && (
                <Paragraph fontSize="$2" opacity={0.6}>
                    {err ? <>Erro: {err}</> : 'Nenhuma receita no SD.'}
                </Paragraph>
            )}
            {recipes?.map((file) => (
                // The whole row is the "Ver" surface — that's by far the
                // most common action and using the row itself as the tap
                // target gives us a 44pt+ touch area without crowding the
                // buttons. Iniciar stays as the prominent CTA on the right
                // (still tappable as a discrete action), Apagar moves to a
                // smaller chrome-less icon button with an explicit
                // accessibility label since "🗑" alone isn't a label.
                <XStack
                    key={file}
                    ai="center"
                    gap="$3"
                    paddingVertical="$3"
                    paddingHorizontal="$3"
                    minHeight={56}
                    br="$3"
                    backgroundColor="$backgroundHover"
                    hoverStyle={{ backgroundColor: '$backgroundFocus' }}
                    pressStyle={{ backgroundColor: '$backgroundFocus', scale: 0.98 }}
                    cursor="pointer"
                    onPress={() => openPreview(file)}
                    accessibilityRole="button"
                    accessibilityLabel={`Visualizar receita ${file}`}
                    accessibilityHint="Toque para abrir o conteúdo da receita"
                >
                    <Text fontFamily="$mono" fontSize="$4" flex={1} numberOfLines={1}>
                        {file}
                    </Text>
                    <Button
                        size="$3"
                        theme="active"
                        onPress={(e: any) => { e?.stopPropagation?.(); start(file); }}
                        accessibilityLabel={`Iniciar receita ${file}`}
                    >
                        Iniciar
                    </Button>
                    <Button
                        size="$3"
                        chromeless
                        theme="red"
                        onPress={(e: any) => { e?.stopPropagation?.(); del(file); }}
                        accessibilityLabel={`Apagar receita ${file}`}
                        aria-label={`Apagar receita ${file}`}
                    >
                        🗑
                    </Button>
                </XStack>
            ))}
        </WizardSheet>
    );
}
