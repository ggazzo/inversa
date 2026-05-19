// App.tsx — web shell. Imports the cross-platform UI from `@inversa/ui`
// and only handles things that are DOM-specific (the Chart.js
// TemperatureChart, the TamaguiProvider, the ToastProvider config).

import { useEffect, useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Toast, ToastProvider, ToastViewport } from '@tamagui/toast';
import { Theme, YStack } from 'tamagui';
import { ConnectionManager } from '@inversa/services';
import { theme, manualIntent } from '@inversa/stores';
import {
    TopBar, BrewView, HopAlertOverlay, ToastBridge,
    RecipeSheet, BrewLogSheet, DevicePickerSheet, DebugSheet,
    CalibrationSheet,
    WizardEquipment, WizardConnectivity, WizardTuning,
    WizardNotifications, WizardAbout,
    type MenuId,
} from '@inversa/ui';
import { TemperatureChart } from './components.web/TemperatureChart';

export function App() {
    useSignals();   // subscribe to the theme signal so re-renders flow
    const [openSheet, setOpenSheet] = useState<MenuId | null>(null);

    useEffect(() => { ConnectionManager.init(); }, []);

    const close = () => setOpenSheet(null);
    const isOpen = (id: MenuId) => openSheet === id;

    // `<Theme>` is Tamagui's runtime theme switch — `TamaguiProvider`'s
    // `defaultTheme` only seeds the initial value, so we read the theme
    // signal here and re-wrap on every toggle. The signal is also
    // mirrored to `<html data-theme>` (in stores/theme.ts).
    return (
        <Theme name={theme.value}>
        <ToastProvider swipeDirection="horizontal" duration={3000} native={[]}>
            <YStack minHeight="100vh" backgroundColor="$background">
                <TopBar onMenuSelect={setOpenSheet} />

                <YStack flex={1}>
                    {/* `chart` is the platform-specific slot — Chart.js on
                        web, victory-native (or similar) on RN. */}
                    <BrewView
                        onMenuSelect={setOpenSheet}
                        onStartManual={() => { manualIntent.value = true; }}
                        chart={<TemperatureChart />}
                    />
                </YStack>

                <RecipeSheet     open={isOpen('recipes')}       onClose={close} />
                <BrewLogSheet    open={isOpen('brewlog')}       onClose={close} />
                <WizardEquipment open={isOpen('equipment')}     onClose={close} />
                <CalibrationSheet open={isOpen('calibration')}  onClose={close} />
                <WizardConnectivity open={isOpen('connectivity')} onClose={close} />
                <WizardTuning    open={isOpen('tuning')}        onClose={close} />
                <WizardNotifications open={isOpen('notifications')} onClose={close} />
                <WizardAbout     open={isOpen('about')}         onClose={close} />
                <DebugSheet      open={isOpen('debug')}         onClose={close} />

                <DevicePickerSheet />

                <HopAlertOverlay />
                <ToastBridge />
            </YStack>

            <ToastViewport top={8} right={8} />
            <Toast />
        </ToastProvider>
        </Theme>
    );
}
