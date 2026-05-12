// App.tsx — root of the Tamagui-based PWA. Mirrors the legacy
// web/src/app.jsx structure but uses Tamagui primitives, Sheets for
// wizards, and React 18+ for the renderer.

import { useEffect, useState } from 'react';
import { useSignals } from '@preact/signals-react/runtime';
import { Toast, ToastProvider, ToastViewport } from '@tamagui/toast';
import { Theme, YStack } from 'tamagui';
import { ConnectionManager } from './services/ConnectionManager';
import { theme } from './stores/theme';
import { mode } from './stores/state';
import { TopBar, type MenuId } from './components/TopBar';
import { BrewView } from './views/BrewView';
import { HopAlertOverlay } from './components/HopAlertOverlay';
import { ToastBridge } from './components/ToastBridge';
import { RecipeSheet }   from './components/sheets/RecipeSheet';
import { BrewLogSheet }  from './components/sheets/BrewLogSheet';
import { WizardEquipment }    from './components/wizards/WizardEquipment';
import { WizardConnectivity } from './components/wizards/WizardConnectivity';
import { WizardTuning }       from './components/wizards/WizardTuning';
import { WizardNotifications } from './components/wizards/WizardNotifications';
import { WizardAbout }        from './components/wizards/WizardAbout';

export function App() {
    useSignals();   // subscribe to the theme signal so re-renders flow
    const [openSheet, setOpenSheet] = useState<MenuId | null>(null);

    useEffect(() => { ConnectionManager.init(); }, []);

    const close = () => setOpenSheet(null);
    const isOpen = (id: MenuId) => openSheet === id;

    // `<Theme>` is Tamagui's runtime theme switch — `TamaguiProvider`'s
    // `defaultTheme` only seeds the initial value, so we read the theme
    // signal here and re-wrap on every toggle. The signal is also
    // mirrored to `<html data-theme>` (in stores/theme.ts) for any
    // legacy CSS that targets it.
    return (
        <Theme name={theme.value}>
        <ToastProvider swipeDirection="horizontal" duration={3000} native={[]}>
            <YStack minHeight="100vh" backgroundColor="$background">
                <TopBar onMenuSelect={setOpenSheet} />

                <YStack flex={1}>
                    <BrewView
                        onMenuSelect={setOpenSheet}
                        onStartManual={() => { mode.value = 'manual'; }}
                    />
                </YStack>

                {/* Wizards / sheets */}
                <RecipeSheet     open={isOpen('recipes')}       onClose={close} />
                <BrewLogSheet    open={isOpen('brewlog')}       onClose={close} />
                <WizardEquipment open={isOpen('equipment')}     onClose={close} />
                <WizardConnectivity open={isOpen('connectivity')} onClose={close} />
                <WizardTuning    open={isOpen('tuning')}        onClose={close} />
                <WizardNotifications open={isOpen('notifications')} onClose={close} />
                <WizardAbout     open={isOpen('about')}         onClose={close} />

                <HopAlertOverlay />
                <ToastBridge />
            </YStack>

            <ToastViewport top={8} right={8} />
            <Toast />
        </ToastProvider>
        </Theme>
    );
}
