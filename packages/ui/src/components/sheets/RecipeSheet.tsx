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
        ConnectionManager.listRecipes()
            .then((r: any) => setRecipes(r.recipes || []))
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
                <XStack key={file} ai="center" gap="$2" padding="$2"
                        hoverStyle={{ backgroundColor: '$backgroundFocus' }} br="$2">
                    <Text fontFamily="$mono" fontSize="$2" flex={1} numberOfLines={1}>
                        {file}
                    </Text>
                    <Button size="$1" chromeless onPress={() => openPreview(file)}>Ver</Button>
                    <Button size="$1" theme="active" onPress={() => start(file)}>Iniciar</Button>
                    <Button size="$1" chromeless theme="red" onPress={() => del(file)}>🗑</Button>
                </XStack>
            ))}
        </WizardSheet>
    );
}
