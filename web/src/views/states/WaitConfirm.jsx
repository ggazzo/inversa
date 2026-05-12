// WaitConfirm.jsx — recipe is parked on WAIT_CONFIRM. The whole card
// becomes a single call to action so the user can poke it with wet
// hands from a meter away. Auxiliary pause/stop sit small below.

import { confirmMessage, recipeName, brewingStepName, showToast } from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';

export function WaitConfirm() {
    function handleConfirm() {
        ConnectionManager.confirmRecipe()
            .then(() => showToast('Confirmado', 'success'))
            .catch((e) => showToast(e?.message || 'Falha ao confirmar', 'error'));
    }

    return (
        <div class="card bg-info/10 border border-info shadow-sm">
            <div class="card-body p-4 sm:p-6 gap-4 items-center text-center">
                <div class="w-full text-xs uppercase tracking-wider text-base-content/50 text-left">
                    {recipeName.value || '—'} · {brewingStepName.value || 'Aguardando você'}
                </div>

                <div class="text-5xl">✋</div>
                <p class="text-xl font-semibold">
                    {confirmMessage.value || 'Aguardando confirmação'}
                </p>

                <button class="btn btn-primary btn-lg w-full h-16 text-lg"
                        onClick={handleConfirm} autofocus>
                    Continuar
                </button>

                <div class="grid grid-cols-2 gap-2 w-full">
                    <button class="btn btn-ghost btn-sm"
                            onClick={() => ConnectionManager.pauseRecipe().catch(() => {})}>
                        Pausar
                    </button>
                    <button class="btn btn-error btn-outline btn-sm"
                            onClick={() => { if (confirm('Parar receita?')) ConnectionManager.stopRecipe(); }}>
                        Parar
                    </button>
                </div>
            </div>
        </div>
    );
}
