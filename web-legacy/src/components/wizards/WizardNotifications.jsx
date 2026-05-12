// WizardNotifications.jsx — browser notification permission + per-event
// preferences. The actual notifications are fired from ConnectionManager
// when the relevant events arrive (hop addition, temp reached, etc.).

import { notificationsEnabled, notifyOnTempReached, notifyOnStepComplete, showToast } from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';
import { WizardSheet } from './WizardSheet';

function permState() {
    if (typeof Notification === 'undefined') return 'unsupported';
    return Notification.permission;
}

export function WizardNotifications({ onClose }) {
    const state = permState();

    return (
        <WizardSheet title="Notificações" onClose={onClose}
                     footer={<button class="btn btn-sm" onClick={onClose}>Fechar</button>}>
            <section class="space-y-2">
                <h4 class="font-semibold text-sm">Permissão do navegador</h4>
                <div class="text-xs">
                    Estado: <span class="font-mono">{state}</span>
                </div>
                {state === 'default' && (
                    <button class="btn btn-primary btn-sm"
                            onClick={() => ConnectionManager.requestNotificationPermission()
                                .then((granted) => showToast(granted ? 'Permissão concedida' : 'Negado',
                                                              granted ? 'success' : 'error'))
                                .catch(() => {})
                            }>
                        Solicitar permissão
                    </button>
                )}
                {state === 'denied' && (
                    <p class="text-xs text-warning">
                        Notificações foram negadas. Habilite manualmente nas
                        configurações do navegador para esse site.
                    </p>
                )}
                {state === 'unsupported' && (
                    <p class="text-xs text-base-content/60">
                        Este navegador não suporta a Notifications API.
                    </p>
                )}
            </section>

            <div class="divider my-1"/>

            <section class="space-y-1">
                <h4 class="font-semibold text-sm">Eventos</h4>

                <Toggle signal={notificationsEnabled}
                        label="Notificações ativas"
                        hint="Mestre on/off — desabilita todas mesmo com permissão" />
                <Toggle signal={notifyOnTempReached}
                        label="Temperatura atingida"
                        hint="Quando |currentTemp - target| < 0.5 °C (histerese)" />
                <Toggle signal={notifyOnStepComplete}
                        label="Passo de receita concluído"
                        hint="Ao avançar para o próximo step" />

                <p class="text-[10px] text-base-content/50 mt-2">
                    Adições de lúpulo sempre disparam o overlay fullscreen,
                    independente desses toggles.
                </p>
            </section>
        </WizardSheet>
    );
}

function Toggle({ signal, label, hint }) {
    return (
        <label class="flex items-start gap-3 py-2 cursor-pointer">
            <input type="checkbox" class="toggle toggle-primary mt-0.5"
                   checked={signal.value}
                   onChange={(e) => { signal.value = e.currentTarget.checked; }} />
            <div class="flex-1">
                <div class="text-sm font-medium">{label}</div>
                <div class="text-xs text-base-content/50">{hint}</div>
            </div>
        </label>
    );
}
