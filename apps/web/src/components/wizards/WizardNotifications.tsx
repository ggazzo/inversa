// WizardNotifications.tsx — browser permission + per-event toggles.
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Paragraph, Separator, Switch, Text, XStack, YStack } from 'tamagui';
import {
    notificationsEnabled, notifyOnTempReached, notifyOnStepComplete, showToast,
} from '../../stores/state';
import type { Signal } from '@preact/signals-react';
import { ConnectionManager } from '../../services/ConnectionManager';
import { WizardSheet } from './WizardSheet';

function permState(): NotificationPermission | 'unsupported' {
    if (typeof Notification === 'undefined') return 'unsupported';
    return Notification.permission;
}

interface Props { open: boolean; onClose: () => void }

export function WizardNotifications({ open, onClose }: Props) {
    useSignals();
    const state = permState();

    return (
        <WizardSheet title="Notificações" open={open} onClose={onClose}
            footer={<Button size="$3" onPress={onClose}>Fechar</Button>}>
            <YStack gap="$2">
                <Text fontWeight="700">Permissão do navegador</Text>
                <Text fontSize="$1">
                    Estado: <Text fontFamily="$mono">{state}</Text>
                </Text>
                {state === 'default' && (
                    <Button size="$3" theme="active"
                        onPress={() => ConnectionManager.requestNotificationPermission()
                            .then((granted: boolean) => showToast(
                                granted ? 'Permissão concedida' : 'Negado',
                                granted ? 'success' : 'error'))
                            .catch(() => {})}>
                        Solicitar permissão
                    </Button>
                )}
                {state === 'denied' && (
                    <Paragraph fontSize="$1" color="$paused">
                        Notificações foram negadas. Habilite manualmente nas
                        configurações do navegador para esse site.
                    </Paragraph>
                )}
                {state === 'unsupported' && (
                    <Paragraph fontSize="$1" opacity={0.6}>
                        Este navegador não suporta a Notifications API.
                    </Paragraph>
                )}
            </YStack>

            <Separator marginVertical="$2" />

            <YStack>
                <Text fontWeight="700">Eventos</Text>
                <Toggle signal={notificationsEnabled}
                        label="Notificações ativas"
                        hint="Mestre on/off — desabilita todas mesmo com permissão" />
                <Toggle signal={notifyOnTempReached}
                        label="Temperatura atingida"
                        hint="Quando |currentTemp - target| < 0.5 °C (histerese)" />
                <Toggle signal={notifyOnStepComplete}
                        label="Passo de receita concluído"
                        hint="Ao avançar para o próximo step" />
                <Paragraph fontSize="$1" opacity={0.5} mt="$2">
                    Adições de lúpulo sempre disparam o overlay fullscreen,
                    independente desses toggles.
                </Paragraph>
            </YStack>
        </WizardSheet>
    );
}

function Toggle({ signal, label, hint }:
    { signal: Signal<boolean>; label: string; hint: string }) {
    return (
        <XStack ai="flex-start" gap="$3" paddingVertical="$2">
            <Switch size="$3" checked={signal.value}
                onCheckedChange={(v: boolean) => { signal.value = v; }}>
                <Switch.Thumb animation="quicker" />
            </Switch>
            <YStack flex={1}>
                <Text fontSize="$3" fontWeight="600">{label}</Text>
                <Text fontSize="$1" opacity={0.5}>{hint}</Text>
            </YStack>
        </XStack>
    );
}
