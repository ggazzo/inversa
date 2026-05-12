// RecoveryPrompt.jsx — shown when the device reports `hasRecovery` true
// after connect. Asks the operator whether to resume the in-progress
// brew or discard the recovery file. Top of the queue: no other CTAs
// should pull attention away.

import { recoveryRecipeName, showToast } from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';

export function RecoveryPrompt() {
    return (
        <div class="card bg-warning/10 border border-warning shadow-sm">
            <div class="card-body p-4 sm:p-6 gap-4">
                <div class="flex items-center gap-3">
                    <span class="text-2xl">⚠️</span>
                    <h2 class="font-semibold">Recuperação encontrada</h2>
                </div>
                <p class="text-sm">
                    O Inversa detectou uma receita interrompida no SD:
                    <span class="font-mono font-semibold ml-1">
                        {recoveryRecipeName.value || '?'}
                    </span>.
                    Deseja retomar do passo onde parou?
                </p>
                <div class="flex flex-col sm:flex-row gap-2">
                    <button
                        class="btn btn-primary flex-1"
                        onClick={() => ConnectionManager.resumeRecovery()
                            .then(() => showToast('Receita retomada', 'success'))
                            .catch(e => showToast(e.message || 'Falha ao retomar', 'error'))
                        }
                    >
                        Retomar
                    </button>
                    <button
                        class="btn btn-outline flex-1"
                        onClick={() => ConnectionManager.discardRecovery()
                            .then(() => showToast('Recuperação descartada'))
                            .catch(() => {})
                        }
                    >
                        Descartar
                    </button>
                </div>
            </div>
        </div>
    );
}
