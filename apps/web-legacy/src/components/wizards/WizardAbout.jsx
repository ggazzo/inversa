// WizardAbout.jsx — firmware identity + the destructive factory-reset
// flow. The firmware refuses anything other than the literal string
// "ERASE_ALL", and we enforce the same on the client by gating the
// destructive button on `confirm.value === 'ERASE_ALL'`.

import { useEffect } from 'preact/hooks';
import { useSignal } from '@preact/signals';
import { firmwareVersion, showToast } from '../../stores/state';
import { ConnectionManager } from '../../services/ConnectionManager';
import { WizardSheet } from './WizardSheet';

export function WizardAbout({ onClose }) {
    const info = useSignal(null);
    const confirm = useSignal('');

    useEffect(() => {
        ConnectionManager.getInfo()
            .then((r) => {
                info.value = r;
                if (r.fw) firmwareVersion.value = r.fw;
            })
            .catch(() => {});
    }, []);

    function doReset() {
        ConnectionManager.factoryReset(confirm.value)
            .then(() => {
                showToast('Reset realizado. Desconectando…', 'success');
                ConnectionManager.disconnect();
                onClose();
            })
            .catch((e) => showToast(e?.message || 'Falha (confirme exato)', 'error'));
    }

    return (
        <WizardSheet title="Sobre" onClose={onClose}
                     footer={<button class="btn btn-sm" onClick={onClose}>Fechar</button>}>
            <section class="space-y-1">
                <h4 class="font-semibold text-sm">Dispositivo</h4>
                {info.value ? (
                    <dl class="text-xs font-mono space-y-0.5">
                        <Row k="firmware" v={info.value.fw} />
                        <Row k="nome"     v={info.value.name} />
                        <Row k="build"    v={info.value.build} />
                        <Row k="heap"     v={`${info.value.heap} bytes`} />
                    </dl>
                ) : (
                    <div class="text-xs text-base-content/50">Lendo…</div>
                )}
            </section>

            <div class="divider my-1"/>

            <section class="space-y-2">
                <h4 class="font-semibold text-sm text-error">Zona de perigo</h4>
                <p class="text-xs text-base-content/70">
                    Factory reset apaga TODO o namespace NVS (PID, WiFi,
                    parâmetros térmicos, timezone). Receitas no SD não são
                    afetadas. Para confirmar, digite <code class="font-mono">ERASE_ALL</code> abaixo.
                </p>
                <input type="text" class="input input-sm input-bordered w-full font-mono"
                       placeholder="ERASE_ALL"
                       value={confirm.value}
                       onInput={(e) => { confirm.value = e.currentTarget.value; }} />
                <button class="btn btn-error btn-sm w-full"
                        disabled={confirm.value !== 'ERASE_ALL'}
                        onClick={() => {
                            if (window.confirm('Tem certeza? Esta ação é irreversível.'))
                                doReset();
                        }}>
                    Apagar tudo
                </button>
            </section>
        </WizardSheet>
    );
}

function Row({ k, v }) {
    return (
        <div class="grid grid-cols-[7rem_1fr] gap-2">
            <dt class="text-base-content/50">{k}</dt>
            <dd class="truncate">{v ?? '—'}</dd>
        </div>
    );
}
