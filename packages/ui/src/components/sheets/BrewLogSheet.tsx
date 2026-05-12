// BrewLogSheet.tsx — start/stop the BrewLog + CSV/JSON multi-chunk export.
import { useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, RadioGroup, Separator, Text, XStack, YStack } from 'tamagui';
import { brewLogActive, brewLogEntries, showToast } from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { WizardSheet } from '../wizards/WizardSheet';

interface Props { open: boolean; onClose: () => void }

export function BrewLogSheet({ open, onClose }: Props) {
    useSignals();
    const [fmt, setFmt]       = useState<'csv' | 'json'>('csv');
    const [busy, setBusy]     = useState(false);
    const [chunks, setChunks] = useState<string[]>([]);

    async function fetchAll() {
        setBusy(true); setChunks([]);
        try {
            let i = 0;
            const acc: string[] = [];
            for (;;) {
                const res: any = await ConnectionManager.fetchLogChunk(fmt, i);
                if (res.data) acc.push(res.data);
                i++;
                if (!res.total || i >= res.total) break;
            }
            setChunks(acc);
            showToast(`Export pronto (${i} chunks)`, 'success');
        } catch (e: any) {
            showToast(e?.message || 'Falha no export', 'error');
        } finally {
            setBusy(false);
        }
    }

    function download() {
        const text = chunks.join('');
        if (!text) { showToast('Nada pra baixar — faça o export primeiro', 'error'); return; }
        const blob = new Blob([text], { type: fmt === 'json' ? 'application/json' : 'text/csv' });
        const url  = URL.createObjectURL(blob);
        const a    = document.createElement('a');
        a.href = url; a.download = `brewlog.${fmt}`;
        a.click();
        URL.revokeObjectURL(url);
    }

    return (
        <WizardSheet title="Brew log" open={open} onClose={onClose}
            footer={<Button size="$3" onPress={onClose}>Fechar</Button>}>
            <YStack gap="$2">
                <XStack jc="space-between" ai="center">
                    <Text fontSize="$2" opacity={0.7}>Gravação</Text>
                    <Text fontSize="$1"
                        backgroundColor={brewLogActive.value ? '$holding' : '$backgroundFocus'}
                        color={brewLogActive.value ? 'white' : '$color'}
                        paddingHorizontal="$2" paddingVertical={2} br="$10">
                        {brewLogActive.value ? 'ativa' : 'parada'}
                    </Text>
                </XStack>
                <Text fontSize="$1" opacity={0.6}>
                    Entradas atuais: <Text fontFamily="$mono">{brewLogEntries.value}</Text>
                </Text>
                <XStack gap="$2">
                    <Button flex={1} size="$3" theme="active"
                        disabled={brewLogActive.value}
                        onPress={() => ConnectionManager.startBrewLog()
                            .then(() => showToast('Log iniciado'))
                            .catch(() => {})}>
                        Iniciar
                    </Button>
                    <Button flex={1} size="$3" chromeless
                        disabled={!brewLogActive.value}
                        onPress={() => ConnectionManager.stopBrewLog()
                            .then(() => showToast('Log parado'))
                            .catch(() => {})}>
                        Parar
                    </Button>
                </XStack>
            </YStack>

            <Separator marginVertical="$2" />

            <YStack gap="$2">
                <Text fontWeight="700">Exportar</Text>
                <RadioGroup value={fmt} onValueChange={(v) => { setFmt(v as 'csv' | 'json'); setChunks([]); }}>
                    <XStack gap="$3" ai="center">
                        <XStack ai="center" gap="$1">
                            <RadioGroup.Item value="csv" id="fmt-csv" size="$3">
                                <RadioGroup.Indicator />
                            </RadioGroup.Item>
                            <Text fontSize="$2">CSV</Text>
                        </XStack>
                        <XStack ai="center" gap="$1">
                            <RadioGroup.Item value="json" id="fmt-json" size="$3">
                                <RadioGroup.Indicator />
                            </RadioGroup.Item>
                            <Text fontSize="$2">JSON</Text>
                        </XStack>
                    </XStack>
                </RadioGroup>

                <XStack gap="$2">
                    <Button flex={1} size="$3" variant="outlined" disabled={busy} onPress={fetchAll}>
                        {busy ? 'Buscando…' : 'Buscar'}
                    </Button>
                    <Button flex={1} size="$3" theme="active"
                        disabled={!chunks.length} onPress={download}>
                        Baixar
                    </Button>
                </XStack>

                {chunks.length > 0 && (
                    <Text fontSize="$1" opacity={0.5}>
                        {chunks.length} chunks (~{chunks.join('').length} bytes)
                    </Text>
                )}
            </YStack>
        </WizardSheet>
    );
}
