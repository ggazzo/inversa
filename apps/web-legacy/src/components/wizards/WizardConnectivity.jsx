// WizardConnectivity.jsx — WiFi config + OTA check/install in one place.
// Sim mode quietly degrades: WiFi never connects (fake stack returns false),
// OTA refuses to install (verifier returns false). UI still works.

import { useSignal } from '@preact/signals';
import {
    wifiConnected, wifiSSID, wifiIP, wifiConfiguredSSID,
    otaStatus, otaLatestVersion, otaProgress, otaError, firmwareVersion,
    showToast,
} from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';
import { WizardSheet } from './WizardSheet';

export function WizardConnectivity({ onClose }) {
    const ssid = useSignal(wifiConfiguredSSID.value || '');
    const pwd  = useSignal('');
    const showPwd = useSignal(false);

    function saveAndConnect() {
        if (!ssid.value) { showToast('SSID obrigatório', 'error'); return; }
        ConnectionManager.configureWiFi(ssid.value, pwd.value)
            .then(() => ConnectionManager.connectWiFi())
            .then(() => showToast('Conectando ao WiFi…', 'info'))
            .catch((e) => showToast(e?.message || 'Falha', 'error'));
    }

    return (
        <WizardSheet title="WiFi & OTA" onClose={onClose}
                     footer={<button class="btn btn-sm" onClick={onClose}>Fechar</button>}>
            {/* WiFi section */}
            <section class="space-y-2">
                <h4 class="font-semibold text-sm flex items-center gap-2">
                    WiFi
                    {wifiConnected.value
                        ? <span class="badge badge-success badge-xs">conectado</span>
                        : <span class="badge badge-ghost badge-xs">desconectado</span>}
                </h4>

                {wifiConnected.value && (
                    <div class="text-xs text-base-content/60">
                        Rede: <span class="font-mono">{wifiSSID.value}</span> ·
                        IP: <span class="font-mono">{wifiIP.value || '—'}</span>
                    </div>
                )}

                <label class="form-control">
                    <div class="label py-0.5"><span class="label-text text-sm">SSID</span></div>
                    <input class="input input-sm input-bordered" type="text"
                           value={ssid.value}
                           onInput={(e) => { ssid.value = e.currentTarget.value; }} />
                </label>

                <label class="form-control">
                    <div class="label py-0.5">
                        <span class="label-text text-sm">Senha</span>
                        <button type="button" class="label-text-alt text-xs underline"
                                onClick={() => { showPwd.value = !showPwd.value; }}>
                            {showPwd.value ? 'esconder' : 'mostrar'}
                        </button>
                    </div>
                    <input class="input input-sm input-bordered"
                           type={showPwd.value ? 'text' : 'password'}
                           value={pwd.value}
                           onInput={(e) => { pwd.value = e.currentTarget.value; }} />
                </label>

                <div class="flex gap-2">
                    <button class="btn btn-primary btn-sm flex-1" onClick={saveAndConnect}>
                        Salvar & Conectar
                    </button>
                    {wifiConnected.value && (
                        <button class="btn btn-ghost btn-sm"
                                onClick={() => ConnectionManager.disconnectWiFi().catch(() => {})}>
                            Desconectar
                        </button>
                    )}
                </div>
            </section>

            <div class="divider my-1"/>

            {/* OTA section */}
            <section class="space-y-2">
                <h4 class="font-semibold text-sm flex items-center gap-2">
                    Atualização (OTA)
                    <span class="badge badge-ghost badge-xs">
                        {otaStatus.value}
                    </span>
                </h4>

                <div class="text-xs text-base-content/60 space-y-0.5">
                    <div>Versão atual: <span class="font-mono">{firmwareVersion.value || '—'}</span></div>
                    {otaLatestVersion.value && (
                        <div>Mais recente: <span class="font-mono">{otaLatestVersion.value}</span></div>
                    )}
                    {otaError.value && (
                        <div class="text-error">Erro: {otaError.value}</div>
                    )}
                </div>

                {otaStatus.value === 'downloading' && (
                    <progress class="progress progress-info w-full" value={otaProgress.value} max="100"/>
                )}

                <div class="flex gap-2">
                    <button class="btn btn-outline btn-sm flex-1"
                            disabled={!wifiConnected.value}
                            onClick={() => ConnectionManager.checkForUpdates().catch(() => {})}>
                        Verificar
                    </button>
                    <button class="btn btn-warning btn-sm flex-1"
                            disabled={otaStatus.value !== 'available'}
                            onClick={() => {
                                if (confirm('Instalar atualização? O dispositivo reinicia.'))
                                    ConnectionManager.installUpdate().catch(() => {});
                            }}>
                        Instalar
                    </button>
                </div>
                <p class="text-[10px] text-base-content/40">
                    Binários OTA são verificados via ECDSA antes da instalação.
                    Releases sem `.bin.sig` são recusados.
                </p>
            </section>
        </WizardSheet>
    );
}
