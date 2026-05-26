// RecipeSheet.jsx — lists recipes on the SD card, lets the user preview
// the DSL, start a brew, or delete. The "preview" step is intentional:
// the recipe text is the contract, and brewers want to read it before
// committing 4 hours to it.

import { useEffect } from 'preact/hooks';
import { useSignal } from '@preact/signals';
import { showToast, loadedRecipeContent } from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';
import { WizardSheet } from '../wizards/WizardSheet';

export function RecipeSheet({ onClose }) {
    const recipes = useSignal(null);   // null while loading
    const error   = useSignal(null);
    const preview = useSignal(null);   // { file, content } or null

    function refresh() {
        recipes.value = null;
        ConnectionManager.listRecipes()
            .then((r) => { recipes.value = r.recipes || []; })
            .catch((e) => { error.value = e?.message || 'Falha'; recipes.value = []; });
    }
    useEffect(refresh, []);

    function open(file) {
        ConnectionManager.loadRecipe(file)
            .then((r) => {
                preview.value = { file: r.file, content: r.content };
                // Cache for the RecipeTimeline so it can render right after start
                // without a second round-trip.
                loadedRecipeContent.value = r.content || '';
            })
            .catch((e) => showToast(e?.message || 'Falha', 'error'));
    }

    function start(file) {
        // Ensure the timeline has the DSL even if the user skipped "Ver".
        const ensureLoaded = (preview.value && preview.value.file === file)
            ? Promise.resolve()
            : ConnectionManager.loadRecipe(file).then((r) => {
                loadedRecipeContent.value = r.content || '';
              });
        ensureLoaded
            .then(() => ConnectionManager.startRecipe(file))
            .then(() => { showToast(`Iniciando ${file}`, 'success'); onClose(); })
            .catch((e) => showToast(e?.message || 'Falha', 'error'));
    }

    function del(file) {
        if (!window.confirm(`Apagar ${file}?`)) return;
        ConnectionManager.deleteRecipe(file)
            .then(() => { showToast('Apagado'); refresh(); })
            .catch((e) => showToast(e?.message || 'Falha', 'error'));
    }

    // ── Preview mode (overlays the list) ─────────────────────
    if (preview.value) {
        const p = preview.value;
        const lineCount = p.content.split('\n').filter(l => l.trim()).length;
        return (
            <WizardSheet
                title={p.file}
                onClose={() => { preview.value = null; }}
                footer={
                    <>
                        <button class="btn btn-ghost btn-sm" onClick={() => { preview.value = null; }}>
                            Voltar
                        </button>
                        <button class="btn btn-primary btn-sm" onClick={() => start(p.file)}>
                            Iniciar
                        </button>
                    </>
                }
            >
                <div class="text-xs text-base-content/50">{lineCount} comandos</div>
                <pre class="bg-base-200/60 rounded p-3 text-xs font-mono whitespace-pre-wrap break-words">
                    {p.content}
                </pre>
            </WizardSheet>
        );
    }

    return (
        <WizardSheet title="Receitas salvas" onClose={onClose}
                     footer={<button class="btn btn-sm" onClick={onClose}>Fechar</button>}>
            {recipes.value === null && (
                <div class="text-sm text-base-content/50">Listando do SD…</div>
            )}
            {recipes.value && recipes.value.length === 0 && (
                <div class="text-sm text-base-content/60">
                    {error.value
                        ? <>Erro: {error.value}</>
                        : 'Nenhuma receita no SD. Carregue uma via app/BrewPilot.'}
                </div>
            )}
            {recipes.value?.map((file) => (
                <div key={file}
                     class="flex items-center gap-2 p-2 rounded hover:bg-base-200/60">
                    <span class="font-mono text-sm flex-1 truncate">{file}</span>
                    <button class="btn btn-ghost btn-xs" onClick={() => open(file)}>
                        Ver
                    </button>
                    <button class="btn btn-primary btn-xs" onClick={() => start(file)}>
                        Iniciar
                    </button>
                    <button class="btn btn-ghost btn-xs text-error" onClick={() => del(file)}>
                        🗑
                    </button>
                </div>
            ))}
        </WizardSheet>
    );
}
