// TopBar.tsx — Tamagui rewrite. Same affordances as the legacy one:
// brand, status chip (with stale state), mode pill, theme toggle, and
// an overflow popover that opens the wizard/sheet dispatcher.

import { useState } from 'react';
import { Platform } from '../platform';
import { useSignals } from '@preact/signals-react/runtime';
import { Button, Popover, Text, XStack, YStack, useTheme } from 'tamagui';
import { isConnected, deviceName, mode, modeLabel, showToast, signalRssi } from '@inversa/stores';
import { isStale } from '@inversa/stores';
import { theme as themeSignal, toggleTheme } from '@inversa/stores';
import { ConnectionManager } from '@inversa/services';
import { BleClient } from '@inversa/services';

export type MenuId =
    | 'recipes' | 'brewlog' | 'equipment' | 'calibration' | 'connectivity'
    | 'tuning'  | 'notifications' | 'about' | 'debug';

const MENU_ITEMS: { id: MenuId; label: string }[] = [
    { id: 'recipes',       label: 'Receitas salvas' },
    { id: 'brewlog',       label: 'Brew log' },
    { id: 'equipment',     label: 'Equipamento' },
    { id: 'calibration',   label: 'Calibrar sensor' },
    { id: 'connectivity',  label: 'WiFi & OTA' },
    { id: 'tuning',        label: 'PID & AutoTune' },
    { id: 'notifications', label: 'Notificações' },
    { id: 'about',         label: 'Sobre' },
    { id: 'debug',         label: 'Debug (BLE trace)' },
];

interface Props {
    onMenuSelect: (id: MenuId) => void;
}

export function TopBar({ onMenuSelect }: Props) {
    useSignals();
    const connected = isConnected.value;
    const stale     = isStale.value;
    const supported = BleClient.isSupported();
    const t         = useTheme();
    // Controlled popover so we can dismiss it the moment a wizard
    // (or sheet) is requested — otherwise the dropdown would still be
    // floating above the newly-opened Sheet.
    const [menuOpen, setMenuOpen] = useState(false);

    function handleSelect(id: MenuId) {
        setMenuOpen(false);
        onMenuSelect(id);
    }

    let chipColor:  '$background' | '$holding' | '$paused' | '$error' = '$background';
    let chipBg:     string = t.background?.val ?? '';
    let chipText:   string = 'Conectar';

    if (!supported) {
        chipText = 'Web BT?';
    } else if (!connected) {
        chipText = 'Conectar';
    } else if (stale) {
        chipText = 'Sem dados';
        chipColor = '$paused';
    } else {
        chipText = deviceName.value || 'Conectado';
        chipColor = '$holding';
    }

    const themeName = themeSignal.value;

    return (
        <XStack
            tag="header"
            // position:sticky is web-only; RN's StyleSheet rejects it and
            // warns. On RN the TopBar lives inside a flex column above
            // the brew screen, so a static layout is fine — no sticky
            // needed. zIndex stays low because the Sheet portal layers
            // above us via its own zIndex ladder; sticky creates a
            // stacking context on web so a high value here would compete.
            style={Platform.OS === 'web'
                ? { position: 'sticky' as any, top: 0, zIndex: 10 }
                : undefined}
            height={48}
            backgroundColor="$background"
            borderBottomWidth={1} borderBottomColor="$borderColor"
            paddingHorizontal="$3"
            alignItems="center" gap="$3"
        >
            {/* Brand */}
            <XStack alignItems="center" gap="$2">
                <YStack
                    width={28} height={28} br="$2"
                    backgroundColor="$primary"
                    alignItems="center" justifyContent="center"
                >
                    <Text color="white" fontWeight="700" fontSize="$3">I</Text>
                </YStack>
                <Text fontWeight="600" $sm={{ display: 'none' }}>Inversa</Text>
            </XStack>

            {/* Connection chip */}
            <Button
                size="$2"
                backgroundColor={chipColor !== '$background' ? chipColor : undefined}
                disabled={!supported}
                onPress={() => connected
                    ? ConnectionManager.disconnect()
                    : ConnectionManager.connect().catch((err: any) => {
                        showToast(err?.message || 'Falha na conexão', 'error');
                    })}
                aria-label={connected ? 'Desconectar' : 'Conectar'}
            >
                <XStack ai="center" gap="$1.5">
                    {connected && signalRssi.value != null && (
                        <SignalBars rssi={signalRssi.value} on="white" off="rgba(255,255,255,0.35)" />
                    )}
                    <Text fontSize="$1" color={chipColor !== '$background' ? 'white' : '$color'}>
                        {chipText}
                    </Text>
                </XStack>
            </Button>

            {/* Mode pill */}
            {connected && mode.value !== 'idle' && (
                <XStack
                    paddingHorizontal="$2" paddingVertical="$1"
                    borderWidth={1} borderColor="$borderColor" br="$10"
                    $sm={{ display: 'none' }}
                >
                    <Text fontSize="$1">{modeLabel.value}</Text>
                </XStack>
            )}

            <XStack flex={1} />

            {/* Theme toggle */}
            <Button size="$2" circular
                    onPress={toggleTheme}
                    aria-label={themeName === 'dark' ? 'Tema claro' : 'Tema escuro'}>
                <Text>{themeName === 'dark' ? '☀️' : '🌙'}</Text>
            </Button>

            {/* Overflow menu — controlled so it closes synchronously when
                we open a wizard. Without this the popover stayed mounted
                above the Sheet, producing the "two layers" effect. */}
            <Popover
                size="$5" allowFlip placement="bottom-end"
                open={menuOpen}
                onOpenChange={setMenuOpen}
            >
                <Popover.Trigger asChild>
                    <Button size="$2" circular aria-label="Menu">
                        <Text>⋮</Text>
                    </Button>
                </Popover.Trigger>
                <Popover.Content
                    borderWidth={1} borderColor="$borderColor"
                    backgroundColor="$background"
                    enterStyle={{ y: -10, opacity: 0 }}
                    exitStyle={{ y: -10, opacity: 0 }}
                    elevate animation="quick"
                >
                    <Popover.Arrow borderWidth={1} borderColor="$borderColor" />
                    <YStack gap="$1" minWidth={200}>
                        {MENU_ITEMS.map((m) => (
                            <Button
                                key={m.id} size="$3"
                                justifyContent="flex-start"
                                backgroundColor="transparent"
                                onPress={() => handleSelect(m.id)}
                            >
                                <Text>{m.label}</Text>
                            </Button>
                        ))}
                        {connected && (
                            <Button size="$3" theme="red"
                                justifyContent="flex-start"
                                backgroundColor="transparent"
                                onPress={() => {
                                    setMenuOpen(false);
                                    ConnectionManager.disconnect();
                                }}>
                                <Text>Desconectar</Text>
                            </Button>
                        )}
                    </YStack>
                </Popover.Content>
            </Popover>
        </XStack>
    );
}

// 4-bar signal indicator. RSSI is in dBm (negative, closer to 0 is
// stronger). Thresholds match the conventional Wi-Fi/BLE bucketing:
//   >= -55 dBm: 4 bars (excellent)
//   >= -67 dBm: 3 bars (good)
//   >= -75 dBm: 2 bars (fair)
//   >= -85 dBm: 1 bar  (weak)
//    < -85 dBm: 0 bars (unusable)
function rssiToBars(rssi: number): number {
    if (rssi >= -55) return 4;
    if (rssi >= -67) return 3;
    if (rssi >= -75) return 2;
    if (rssi >= -85) return 1;
    return 0;
}

function SignalBars({ rssi, on, off }: { rssi: number; on: string; off: string }) {
    const bars    = rssiToBars(rssi);
    const heights = [4, 7, 10, 13];
    return (
        <XStack ai="flex-end" gap={1.5} height={14}>
            {heights.map((h, i) => (
                <YStack key={i} width={2.5} height={h} br={1}
                        backgroundColor={i < bars ? on : off} />
            ))}
        </XStack>
    );
}
