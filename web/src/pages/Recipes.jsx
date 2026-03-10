import { useState } from 'preact/hooks';
import { ConnectionManager } from '../services/ConnectionManager';
import {
  isConnected, isRecipeRunning, recipeName, recipeStep,
  recipeTotalSteps, recipeState, formattedTimer, showToast,
} from '../stores/state';

export function Recipes() {
  const connected = isConnected.value;
  const [recipes, setRecipes] = useState([]);
  const [loading, setLoading] = useState(false);
  const [selectedRecipe, setSelectedRecipe] = useState(null);
  const [recipeContent, setRecipeContent] = useState('');
  const [viewMode, setViewMode] = useState('list'); // list, view, edit, create

  async function fetchRecipes() {
    setLoading(true);
    try {
      const list = await ConnectionManager.listRecipes();
      setRecipes(list);
    } catch (e) {
      showToast(e.message, 'error');
    } finally {
      setLoading(false);
    }
  }

  async function loadRecipe(filename) {
    setLoading(true);
    try {
      const res = await ConnectionManager.loadRecipe(filename);
      setSelectedRecipe(filename);
      setRecipeContent(res.content || '');
      setViewMode('view');
    } catch (e) {
      showToast(e.message, 'error');
    } finally {
      setLoading(false);
    }
  }

  async function startRecipe(filename) {
    try {
      await ConnectionManager.startRecipe(filename);
      showToast('Receita iniciada!', 'success');
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  async function stopRecipe() {
    try {
      await ConnectionManager.stopRecipe();
      showToast('Receita parada.', 'info');
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  async function pauseRecipe() {
    try {
      await ConnectionManager.pauseRecipe();
      showToast('Receita pausada.', 'info');
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  async function resumeRecipe() {
    try {
      await ConnectionManager.resumeRecipe();
      showToast('Receita retomada.', 'success');
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  async function confirmStep() {
    try {
      await ConnectionManager.confirmRecipe();
      showToast('Passo confirmado.', 'success');
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  async function saveRecipe() {
    if (!selectedRecipe || !recipeContent.trim()) {
      showToast('Nome e conteudo necessarios', 'error');
      return;
    }
    try {
      await ConnectionManager.saveRecipe(selectedRecipe, recipeContent);
      showToast('Receita salva!', 'success');
      setViewMode('list');
      fetchRecipes();
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  async function createRecipe(filename) {
    if (!filename || !recipeContent.trim()) {
      showToast('Nome e conteudo necessarios', 'error');
      return;
    }
    const name = filename.endsWith('.txt') ? filename : `${filename}.txt`;
    try {
      await ConnectionManager.saveRecipe(name, recipeContent);
      showToast('Receita criada!', 'success');
      setViewMode('list');
      setSelectedRecipe(null);
      setRecipeContent('');
      fetchRecipes();
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  async function deleteRecipe(filename) {
    try {
      await ConnectionManager.deleteRecipe(filename);
      showToast('Receita excluida.', 'info');
      setViewMode('list');
      setSelectedRecipe(null);
      fetchRecipes();
    } catch (e) {
      showToast(e.message, 'error');
    }
  }

  if (!connected) {
    return (
      <div class="flex flex-col items-center justify-center py-16 text-base-content/50">
        <svg xmlns="http://www.w3.org/2000/svg" class="h-12 w-12 mb-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5" d="M18.364 5.636a9 9 0 11-12.728 0M12 3v9" />
        </svg>
        <p>Conecte ao dispositivo para ver receitas.</p>
      </div>
    );
  }

  return (
    <div class="flex flex-col gap-4">
      {/* Active Recipe Controls */}
      {isRecipeRunning.value && (
        <ActiveRecipeCard
          onStop={stopRecipe}
          onPause={pauseRecipe}
          onResume={resumeRecipe}
          onConfirm={confirmStep}
        />
      )}

      {/* List / View / Edit / Create */}
      {viewMode === 'list' && (
        <RecipeList
          recipes={recipes}
          loading={loading}
          onFetch={fetchRecipes}
          onLoad={loadRecipe}
          onStart={startRecipe}
          onCreate={() => {
            setSelectedRecipe('');
            setRecipeContent(RECIPE_TEMPLATE);
            setViewMode('create');
          }}
        />
      )}

      {viewMode === 'view' && (
        <RecipeView
          filename={selectedRecipe}
          content={recipeContent}
          onBack={() => setViewMode('list')}
          onEdit={() => setViewMode('edit')}
          onStart={() => startRecipe(selectedRecipe)}
          onDelete={() => deleteRecipe(selectedRecipe)}
        />
      )}

      {viewMode === 'edit' && (
        <RecipeEditor
          filename={selectedRecipe}
          content={recipeContent}
          onContentChange={setRecipeContent}
          onSave={saveRecipe}
          onCancel={() => setViewMode('view')}
        />
      )}

      {viewMode === 'create' && (
        <RecipeCreator
          content={recipeContent}
          onContentChange={setRecipeContent}
          onSave={createRecipe}
          onCancel={() => setViewMode('list')}
        />
      )}
    </div>
  );
}

// ─── Sub-components ─────────────────────────────────────────

function ActiveRecipeCard({ onStop, onPause, onResume, onConfirm }) {
  const state = recipeState.value;
  const isPaused = state === 'paused';
  const needsConfirm = state === 'waiting_confirm';

  return (
    <div class="card bg-primary/10 border border-primary/30 shadow-md">
      <div class="card-body p-4">
        <div class="flex items-center justify-between mb-2">
          <div>
            <div class="text-xs uppercase text-primary font-medium">Receita Ativa</div>
            <div class="font-semibold">{recipeName.value || 'Sem nome'}</div>
          </div>
          <div class="text-right">
            <div class="font-mono text-xl">{formattedTimer.value}</div>
            <div class="text-xs text-base-content/50">
              Passo {recipeStep.value}/{recipeTotalSteps.value}
            </div>
          </div>
        </div>

        <div class="flex gap-2 mt-1">
          {needsConfirm && (
            <button class="btn btn-success btn-sm flex-1" onClick={onConfirm}>
              Confirmar
            </button>
          )}
          {isPaused ? (
            <button class="btn btn-primary btn-sm flex-1" onClick={onResume}>
              Retomar
            </button>
          ) : (
            <button class="btn btn-warning btn-sm flex-1" onClick={onPause}>
              Pausar
            </button>
          )}
          <button class="btn btn-error btn-sm" onClick={onStop}>
            Parar
          </button>
        </div>
      </div>
    </div>
  );
}

function RecipeList({ recipes, loading, onFetch, onLoad, onStart, onCreate }) {
  return (
    <div class="card bg-base-100 shadow-md">
      <div class="card-body p-4">
        <div class="flex items-center justify-between mb-3">
          <h3 class="text-sm font-semibold uppercase text-base-content/60">Receitas no SD</h3>
          <div class="flex gap-2">
            <button class="btn btn-ghost btn-xs" onClick={onFetch} disabled={loading}>
              {loading ? <span class="loading loading-spinner loading-xs" /> : 'Atualizar'}
            </button>
            <button class="btn btn-primary btn-xs" onClick={onCreate}>
              + Nova
            </button>
          </div>
        </div>

        {recipes.length === 0 ? (
          <div class="text-center text-base-content/40 py-4 text-sm">
            {loading ? 'Carregando...' : 'Nenhuma receita. Clique "Atualizar" para buscar.'}
          </div>
        ) : (
          <ul class="space-y-1">
            {recipes.map((filename) => (
              <li key={filename} class="flex items-center justify-between py-2 px-2 rounded hover:bg-base-200 transition-colors">
                <button class="text-sm text-left flex-1 truncate" onClick={() => onLoad(filename)}>
                  {filename}
                </button>
                <button class="btn btn-success btn-xs ml-2" onClick={() => onStart(filename)}>
                  Iniciar
                </button>
              </li>
            ))}
          </ul>
        )}
      </div>
    </div>
  );
}

function RecipeView({ filename, content, onBack, onEdit, onStart, onDelete }) {
  const lines = content.split('\n');

  return (
    <div class="card bg-base-100 shadow-md">
      <div class="card-body p-4">
        <div class="flex items-center justify-between mb-3">
          <div class="flex items-center gap-2">
            <button class="btn btn-ghost btn-xs" onClick={onBack}>&#8592; Voltar</button>
            <h3 class="text-sm font-semibold">{filename}</h3>
          </div>
          <div class="flex gap-1">
            <button class="btn btn-ghost btn-xs" onClick={onEdit}>Editar</button>
            <button class="btn btn-error btn-xs" onClick={onDelete}>Excluir</button>
          </div>
        </div>

        <div class="bg-base-200 rounded p-3 font-mono text-xs leading-relaxed max-h-64 overflow-y-auto">
          {lines.map((line, i) => (
            <div key={i} class={`${line.startsWith('#') ? 'text-base-content/30' : ''}`}>
              {line || '\u00A0'}
            </div>
          ))}
        </div>

        <button class="btn btn-success btn-sm mt-3 w-full" onClick={onStart}>
          Iniciar Receita
        </button>
      </div>
    </div>
  );
}

function RecipeEditor({ filename, content, onContentChange, onSave, onCancel }) {
  return (
    <div class="card bg-base-100 shadow-md">
      <div class="card-body p-4">
        <div class="flex items-center justify-between mb-3">
          <h3 class="text-sm font-semibold">Editando: {filename}</h3>
          <button class="btn btn-ghost btn-xs" onClick={onCancel}>Cancelar</button>
        </div>
        <textarea
          class="textarea textarea-bordered font-mono text-xs w-full h-56"
          value={content}
          onInput={(e) => onContentChange(e.target.value)}
        />
        <CommandReference />
        <button class="btn btn-primary btn-sm mt-3 w-full" onClick={onSave}>
          Salvar
        </button>
      </div>
    </div>
  );
}

function RecipeCreator({ content, onContentChange, onSave, onCancel }) {
  const [filename, setFilename] = useState('');

  return (
    <div class="card bg-base-100 shadow-md">
      <div class="card-body p-4">
        <div class="flex items-center justify-between mb-3">
          <h3 class="text-sm font-semibold">Nova Receita</h3>
          <button class="btn btn-ghost btn-xs" onClick={onCancel}>Cancelar</button>
        </div>
        <input
          type="text"
          class="input input-bordered input-sm w-full mb-2"
          placeholder="Nome do arquivo (ex: pilsen.txt)"
          value={filename}
          onInput={(e) => setFilename(e.target.value)}
        />
        <textarea
          class="textarea textarea-bordered font-mono text-xs w-full h-56"
          value={content}
          onInput={(e) => onContentChange(e.target.value)}
        />
        <CommandReference />
        <button
          class="btn btn-primary btn-sm mt-3 w-full"
          onClick={() => onSave(filename)}
          disabled={!filename.trim()}
        >
          Criar Receita
        </button>
      </div>
    </div>
  );
}

function CommandReference() {
  return (
    <div class="collapse collapse-arrow bg-base-200 mt-2">
      <input type="checkbox" />
      <div class="collapse-title text-xs font-medium">Referencia de Comandos</div>
      <div class="collapse-content text-xs font-mono space-y-1">
        <p><strong>SET_TEMP 65</strong> — Define temperatura alvo</p>
        <p><strong>WAIT_TEMP</strong> — Aguarda atingir temperatura</p>
        <p><strong>WAIT_TIMER 60</strong> — Aguarda N minutos</p>
        <p><strong>WAIT_CONFIRM "mensagem"</strong> — Aguarda confirmacao</p>
        <p><strong>PUMP_ON</strong> — Liga bomba</p>
        <p><strong>PUMP_OFF</strong> — Desliga bomba</p>
        <p><strong># comentario</strong> — Linha ignorada</p>
      </div>
    </div>
  );
}

const RECIPE_TEMPLATE = `# Nome da Receita
# Descricao...

SET_TEMP 65
WAIT_TEMP
PUMP_ON
WAIT_TIMER 60
PUMP_OFF

SET_TEMP 72
WAIT_TEMP
PUMP_ON
WAIT_TIMER 15
PUMP_OFF

SET_TEMP 78
WAIT_TEMP
WAIT_CONFIRM "Mashout completo. Prosseguir?"
`;
