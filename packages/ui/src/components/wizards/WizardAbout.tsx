// WizardAbout.tsx — device info + gated factory reset.
import { useEffect, useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Input, Paragraph, Separator, Text, XStack, YStack } from 'tamagui';
import { firmwareVersion, showToast } from '@brewpilot/stores';
import { ConnectionManager } from '@brewpilot/services';
import { WizardSheet } from './WizardSheet';
import { confirm } from '../../platform';

interface Props { open: boolean; onClose: () => void }

export function WizardAbout({ open, onClose }: Props) {
    useSignals();
    const [info, setInfo] = useState<any | null>(null);
    const [confirmText, setConfirmText] = useState('');

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

    return (
        <WizardSheet title="Sobre" open={open} onClose={onClose}
            footer={<Button size="$3" onPress={onClose}>Fechar</Button>}>
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

function Row({ k, v }: { k: string; v?: string }) {
    return (
        <XStack gap="$2">
            <Text fontSize="$1" fontFamily="$mono" opacity={0.5} width={112}>{k}</Text>
            <Text fontSize="$1" fontFamily="$mono" numberOfLines={1} flex={1}>{v ?? '—'}</Text>
        </XStack>
    );
}
