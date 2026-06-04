// WizardAbout.tsx — device info + gated factory reset.
import { useEffect, useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Input, Paragraph, Separator, Text, XStack, YStack } from 'tamagui';
import { firmwareVersion, showToast } from '@brewpilot/stores';
import { ConnectionManager } from '@brewpilot/services';
import { WizardSheet } from './WizardSheet';
import { confirm } from '../../platform';
import { getAppBundleInfo, type AppBundleInfo } from '../../appInfo';
import { KNOWN_CHANNELS, isChannelOverrideSupported, currentChannel, applyChannel } from '../../updateChannel';

interface Props { open: boolean; onClose: () => void }

export function WizardAbout({ open, onClose }: Props) {
    useSignals();
    const [info, setInfo] = useState<any | null>(null);
    const [confirmText, setConfirmText] = useState('');
    const [advanced, setAdvanced] = useState(false);
    const [customCh, setCustomCh] = useState('');
    const channelSupported = isChannelOverrideSupported();

    async function switchChannel(ch: string) {
        const name = ch.trim();
        if (!name) return;
        if (!(await confirm(`Trocar para o canal "${name}"? O app reinicia para aplicar.`))) return;
        // applyChannel reloads the app on success; we only return here on failure.
        applyChannel(name).catch((e: any) => showToast(e?.message || 'Falha ao trocar canal', 'error'));
    }

    useEffect(() => {
        if (!open) return;
        ConnectionManager.getInfo()
            .then((r: any) => {
                setInfo(r);
                if (r.fw) firmwareVersion.value = r.fw;
            })
            .catch(() => {});
    }, [open]);

    function doReset() {
        ConnectionManager.factoryReset(confirmText)
            .then(() => {
                showToast('Reset realizado. Desconectando…', 'success');
                ConnectionManager.disconnect();
                onClose();
            })
            .catch((e: any) => showToast(e?.message || 'Falha (confirme exato)', 'error'));
    }

    const bundle: AppBundleInfo | null = getAppBundleInfo();

    return (
        <WizardSheet title="Sobre" open={open} onClose={onClose}
            footer={<Button size="$3" onPress={onClose}>Fechar</Button>}>
            {bundle && (
                <>
                    <YStack>
                        <Text fontWeight="700">App</Text>
                        <YStack mt="$2" gap={2}>
                            <Row k="bundle"   v={shortId(bundle.updateId) || 'embedded'} />
                            <Row k="publicado" v={formatDate(bundle.createdAt)} />
                            <Row k="runtime"  v={shortId(bundle.runtimeVersion)} />
                            <Row k="canal"    v={bundle.channel} />
                            <Row k="origem"
                                v={bundle.isEmbeddedLaunch ? 'APK embarcado' : 'EAS Update'} />
                            <Row k="ota ativo"
                                v={bundle.isEnabled ? 'sim' : 'não'} />
                        </YStack>
                    </YStack>
                    <Separator marginVertical="$2" />
                </>
            )}
            {channelSupported && (
                <>
                    <YStack gap="$2">
                        <XStack ai="center" jc="space-between">
                            <Text fontWeight="700">Canal de atualização</Text>
                            <Button size="$2" chromeless onPress={() => setAdvanced((a) => !a)}>
                                {advanced ? 'ocultar' : 'avançado'}
                            </Button>
                        </XStack>
                        <Text fontSize="$1" opacity={0.6}>
                            Canal atual: <Text fontFamily="$mono">{currentChannel() || '—'}</Text>
                        </Text>
                        {advanced && (
                            <YStack gap="$2">
                                <XStack gap="$2" flexWrap="wrap">
                                    {KNOWN_CHANNELS.map((ch) => (
                                        <Button key={ch} size="$2"
                                            theme={ch === currentChannel() ? 'orange' : undefined}
                                            onPress={() => switchChannel(ch)}>
                                            {ch}
                                        </Button>
                                    ))}
                                </XStack>
                                <XStack gap="$2">
                                    <Input flex={1} size="$3" autoCapitalize="none"
                                        placeholder="canal customizado (ex: pr-123)"
                                        value={customCh} onChangeText={setCustomCh} />
                                    <Button size="$3" disabled={!customCh.trim()}
                                        onPress={() => switchChannel(customCh)}>
                                        Aplicar
                                    </Button>
                                </XStack>
                                <Paragraph fontSize="$1" opacity={0.4}>
                                    Troca o canal em runtime e baixa o último bundle dele. Só
                                    aplica se o fingerprint nativo bater com este APK. Anti-brick
                                    desativado neste build — use com cuidado.
                                </Paragraph>
                            </YStack>
                        )}
                    </YStack>

                    <Separator marginVertical="$2" />
                </>
            )}

            <YStack>
                <Text fontWeight="700">Dispositivo</Text>
                {info ? (
                    <YStack mt="$2" gap={2}>
                        <Row k="firmware" v={info.fw} />
                        <Row k="nome"     v={info.name} />
                        <Row k="build"    v={info.build} />
                        <Row k="heap"     v={info.heap ? `${info.heap} bytes` : '—'} />
                    </YStack>
                ) : (
                    <Text fontSize="$1" opacity={0.5}>Lendo…</Text>
                )}
            </YStack>

            <Separator marginVertical="$2" />

            <YStack gap="$2">
                <Text fontWeight="700" color="$error">Zona de perigo</Text>
                <Paragraph fontSize="$1" opacity={0.7}>
                    Factory reset apaga TODO o namespace NVS (PID, WiFi,
                    parâmetros térmicos, timezone). Receitas no SD não são
                    afetadas. Para confirmar, digite <Text fontFamily="$mono">ERASE_ALL</Text> abaixo.
                </Paragraph>
                <Input size="$3" fontFamily="$mono"
                    placeholder="ERASE_ALL"
                    value={confirmText}
                    onChangeText={setConfirmText} />
                <Button size="$3" theme="red"
                    disabled={confirmText !== 'ERASE_ALL'}
                    onPress={async () => {
                        if (await confirm('Tem certeza? Esta ação é irreversível.'))
                            doReset();
                    }}>
                    Apagar tudo
                </Button>
            </YStack>
        </WizardSheet>
    );
}

function Row({ k, v }: { k: string; v?: string | null }) {
    return (
        <XStack gap="$2">
            <Text fontSize="$1" fontFamily="$mono" opacity={0.5} width={112}>{k}</Text>
            <Text fontSize="$1" fontFamily="$mono" numberOfLines={1} flex={1}>{v ?? '—'}</Text>
        </XStack>
    );
}

// Truncate long hashes to a glanceable prefix. `null` → "—" via Row.
function shortId(id: string | null): string | null {
    if (!id) return null;
    return id.length > 12 ? id.slice(0, 12) + '…' : id;
}

// EAS Update gives us an ISO timestamp; render in the user's locale
// so it lines up with when they actually published / received it.
function formatDate(iso: string | null): string | null {
    if (!iso) return null;
    try {
        const d = new Date(iso);
        if (Number.isNaN(d.getTime())) return iso;
        return d.toLocaleString();
    } catch {
        return iso;
    }
}
