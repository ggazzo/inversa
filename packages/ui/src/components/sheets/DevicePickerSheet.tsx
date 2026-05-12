// DevicePickerSheet.tsx — lists nearby Inversa devices for RN real BLE.
//
// Web's `navigator.bluetooth.requestDevice` ships its own picker; sim
// mode hard-wires the WebSocket URL — neither case needs this sheet.
// On RN with real BLE, ble-plx only gives us a callback stream of
// discoveries, so we have to host the picker ourselves. The sheet
// starts a scan when it opens and stops it on close.

import { useEffect, useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Paragraph, Text, XStack, YStack } from 'tamagui';
import { devicePickerOpen, showToast } from '@inversa/stores';
import { ConnectionManager, type BleScanDevice } from '@inversa/services';
import { WizardSheet } from '../wizards/WizardSheet';

export function DevicePickerSheet() {
    useSignals();
    const open = devicePickerOpen.value;
    const [devices, setDevices] = useState<BleScanDevice[]>([]);
    const [error,   setError]   = useState<string | null>(null);
    const [busy,    setBusy]    = useState<string | null>(null);   // id currently connecting

    function close() {
        devicePickerOpen.value = false;
    }

    useEffect(() => {
        if (!open) {
            setDevices([]);
            setError(null);
            setBusy(null);
            return;
        }

        // Dedupe by id — ble-plx fires the callback repeatedly with
        // updated RSSI samples for the same device.
        const seen = new Map<string, BleScanDevice>();
        const stop = ConnectionManager.scanForDevices(
            (d: BleScanDevice) => {
                const prev = seen.get(d.id);
                // Skip if nothing changed; keeps React renders quiet.
                if (prev && prev.name === d.name && prev.rssi === d.rssi) return;
                seen.set(d.id, d);
                setDevices(Array.from(seen.values()));
            },
            (e: Error) => setError(e.message),
        );
        return () => stop();
    }, [open]);

    async function pick(d: BleScanDevice) {
        setBusy(d.id);
        try {
            await ConnectionManager.connectToDevice(d.id);
            close();
        } catch (e: any) {
            showToast(e?.message || 'Falha ao conectar', 'error');
        } finally {
            setBusy(null);
        }
    }

    return (
        <WizardSheet title="Procurando dispositivos" open={open} onClose={close}
            footer={<Button size="$3" onPress={close}>Cancelar</Button>}>
            {error ? (
                <Paragraph fontSize="$2" color="$error">{error}</Paragraph>
            ) : devices.length === 0 ? (
                <YStack ai="center" jc="center" paddingVertical="$6" gap="$2">
                    <Text fontSize={28}>📡</Text>
                    <Paragraph fontSize="$2" opacity={0.7} textAlign="center">
                        Buscando dispositivos Inversa…
                    </Paragraph>
                    <Paragraph fontSize="$1" opacity={0.4} textAlign="center" maxWidth={300}>
                        Certifique-se que o controlador está ligado e o
                        Bluetooth do telefone está ativo.
                    </Paragraph>
                </YStack>
            ) : (
                <YStack gap="$2">
                    {devices.map((d) => (
                        <Button key={d.id} size="$4" disabled={busy !== null}
                            onPress={() => pick(d)}
                            justifyContent="flex-start" flexDirection="column"
                            alignItems="stretch" paddingVertical="$3">
                            <XStack jc="space-between" ai="center" width="100%">
                                <Text fontWeight="600">
                                    {d.name || '(sem nome)'}
                                </Text>
                                {busy === d.id ? (
                                    <Text fontSize="$1" opacity={0.6}>conectando…</Text>
                                ) : d.rssi != null ? (
                                    <Text fontSize="$1" opacity={0.5} fontFamily="$mono">
                                        {d.rssi} dBm
                                    </Text>
                                ) : null}
                            </XStack>
                            <Text fontSize="$1" opacity={0.4} fontFamily="$mono" numberOfLines={1}>
                                {d.id}
                            </Text>
                        </Button>
                    ))}
                </YStack>
            )}
        </WizardSheet>
    );
}
