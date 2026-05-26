// WizardConnectivity.tsx — WiFi + OTA.
import { useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Input, Label, Paragraph, Progress, Separator, Text, XStack, YStack } from 'tamagui';
import {
    wifiConnected, wifiSSID, wifiIP, wifiConfiguredSSID,
    otaStatus, otaLatestVersion, otaProgress, otaError, firmwareVersion, showToast,
    deviceName,
} from '@brewpilot/stores';
import { ConnectionManager } from '@brewpilot/services';
import { WizardSheet } from './WizardSheet';
import { confirm } from '../../platform';

interface Props { open: boolean; onClose: () => void }

// Same set the firmware validates in CommandHandler::REQ_DEVICE_RENAME.
const VALID_NAME_RE = /^[A-Za-z0-9 _-]*$/;
const MAX_NAME_LEN  = 31;

export function WizardConnectivity({ open, onClose }: Props) {
    useSignals();
    const [ssid, setSsid]       = useState(wifiConfiguredSSID.value || '');
    const [pwd, setPwd]         = useState('');
    const [showPwd, setShowPwd] = useState(false);
    const [name, setName]       = useState(deviceName.value || '');

    function saveAndConnect() {
        if (!ssid) { showToast('SSID obrigatório', 'error'); return; }
        ConnectionManager.configureWiFi(ssid, pwd)
            .then(() => ConnectionManager.connectWiFi())
            .then(() => showToast('Conectando ao WiFi…', 'info'))
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    async function saveName() {
        const next = name.trim();
        if (next.length > MAX_NAME_LEN) {
            showToast(`Máx ${MAX_NAME_LEN} caracteres`, 'error');
            return;
        }
        if (!VALID_NAME_RE.test(next)) {
            showToast('Use só letras, números, espaço, "-" ou "_"', 'error');
            return;
        }
        if (!(await confirm(
            'O dispositivo vai reiniciar para aplicar o novo nome. Continuar?'
        ))) return;
        try {
            await ConnectionManager.renameDevice(next);
            showToast('Reiniciando com novo nome…', 'info');
        } catch (e: any) {
            // `name_too_long` / `name_invalid_char` come from the firmware.
            // The disconnect that follows ESP.restart() also bubbles up as
            // a rejection — surface it but don't treat it as an error.
            const msg = e?.message || '';
            if (msg.includes('disconnect') || msg.includes('Disconnected')) {
                showToast('Reiniciando com novo nome…', 'info');
            } else {
                showToast(msg || 'Falha ao renomear', 'error');
            }
        }
    }

    return (
        <WizardSheet title="Conectividade" open={open} onClose={onClose}
            footer={<Button size="$3" onPress={onClose}>Fechar</Button>}>
            {/* Device name */}
            <YStack gap="$2">
                <XStack ai="center" gap="$2">
                    <Text fontWeight="700">Nome do equipamento</Text>
                </XStack>
                <Paragraph fontSize="$1" opacity={0.6}>
                    Aparece no Bluetooth e no <Text fontFamily="$mono">.local</Text> da rede.
                    Letras, números, espaço, <Text fontFamily="$mono">-</Text> ou <Text fontFamily="$mono">_</Text>; até {MAX_NAME_LEN} chars.
                </Paragraph>
                <Input size="$3"
                    value={name}
                    onChangeText={setName}
                    placeholder="BrewPilot" />
                <Button size="$3" theme="active"
                    disabled={name.trim() === (deviceName.value || '')}
                    onPress={saveName}>
                    Salvar & Reiniciar
                </Button>
            </YStack>

            <Separator marginVertical="$2" />

            {/* WiFi */}
            <YStack gap="$2">
                <XStack ai="center" gap="$2">
                    <Text fontWeight="700">WiFi</Text>
                    <Text
                        fontSize="$1"
                        backgroundColor={wifiConnected.value ? '$holding' : '$backgroundFocus'}
                        color={wifiConnected.value ? 'white' : '$color'}
                        paddingHorizontal="$2" paddingVertical={2} br="$10"
                    >
                        {wifiConnected.value ? 'conectado' : 'desconectado'}
                    </Text>
                </XStack>

                {wifiConnected.value && (
                    <Text fontSize="$1" opacity={0.6}>
                        Rede: <Text fontFamily="$mono">{wifiSSID.value}</Text> ·
                        IP: <Text fontFamily="$mono">{wifiIP.value || '—'}</Text>
                    </Text>
                )}

                <Label>SSID</Label>
                <Input size="$3" value={ssid} onChangeText={setSsid} />

                <XStack jc="space-between">
                    <Label>Senha</Label>
                    <Button size="$1" chromeless onPress={() => setShowPwd(!showPwd)}>
                        <Text fontSize="$1" textDecorationLine="underline">
                            {showPwd ? 'esconder' : 'mostrar'}
                        </Text>
                    </Button>
                </XStack>
                <Input size="$3"
                    secureTextEntry={!showPwd}
                    value={pwd} onChangeText={setPwd} />

                <XStack gap="$2">
                    <Button flex={1} size="$3" theme="active" onPress={saveAndConnect}>
                        Salvar & Conectar
                    </Button>
                    {wifiConnected.value && (
                        <Button size="$3" chromeless
                            onPress={() => ConnectionManager.disconnectWiFi().catch(() => {})}>
                            Desconectar
                        </Button>
                    )}
                </XStack>
            </YStack>

            <Separator marginVertical="$2" />

            {/* OTA */}
            <YStack gap="$2">
                <XStack ai="center" gap="$2">
                    <Text fontWeight="700">Atualização (OTA)</Text>
                    <Text fontSize="$1"
                        backgroundColor="$backgroundFocus"
                        paddingHorizontal="$2" paddingVertical={2} br="$10">
                        {otaStatus.value}
                    </Text>
                </XStack>

                <YStack gap={2}>
                    <Text fontSize="$1" opacity={0.6}>
                        Versão atual: <Text fontFamily="$mono">{firmwareVersion.value || '—'}</Text>
                    </Text>
                    {otaLatestVersion.value && (
                        <Text fontSize="$1" opacity={0.6}>
                            Mais recente: <Text fontFamily="$mono">{otaLatestVersion.value}</Text>
                        </Text>
                    )}
                    {otaError.value && (
                        <Text fontSize="$1" color="$error">Erro: {otaError.value}</Text>
                    )}
                </YStack>

                {otaStatus.value === 'downloading' && (
                    <Progress value={otaProgress.value}>
                        <Progress.Indicator backgroundColor="$cooling" />
                    </Progress>
                )}

                <XStack gap="$2">
                    <Button flex={1} size="$3" variant="outlined"
                        disabled={!wifiConnected.value}
                        onPress={() => ConnectionManager.checkForUpdates().catch(() => {})}>
                        Verificar
                    </Button>
                    <Button flex={1} size="$3" theme="orange"
                        disabled={otaStatus.value !== 'available'}
                        onPress={async () => {
                            if (await confirm('Instalar atualização? O dispositivo reinicia.'))
                                ConnectionManager.installUpdate().catch(() => {});
                        }}>
                        Instalar
                    </Button>
                </XStack>
                <Paragraph fontSize="$1" opacity={0.4}>
                    Binários OTA são verificados via ECDSA antes da instalação.
                    Releases sem `.bin.sig` são recusados.
                </Paragraph>
            </YStack>
        </WizardSheet>
    );
}
