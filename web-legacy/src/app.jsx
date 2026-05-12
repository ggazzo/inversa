// app.jsx — root of the redesigned PWA. Single-page contextual UI; no
// preact-router. Layout: TopBar on top, BrewView grid below, transient
// toast at the bottom. Wizards/sheets are rendered conditionally based
// on the `openSheet` signal (Phase 3 will wire each one to a real form).

import { useEffect } from 'preact/hooks';
import { signal } from '@preact/signals';
import { ConnectionManager } from './services/ConnectionManager';
import { mode, toastMessage, toastType } from './stores/state';
import { TopBar } from './components/TopBar';
import { BrewView } from './views/BrewView';
import { HopAlertOverlay } from './components/HopAlertOverlay';
import { RecipeSheet } from './components/sheets/RecipeSheet';
import { BrewLogSheet } from './components/sheets/BrewLogSheet';
import { WizardEquipment }    from './components/wizards/WizardEquipment';
import { WizardConnectivity } from './components/wizards/WizardConnectivity';
import { WizardTuning }       from './components/wizards/WizardTuning';
import { WizardNotifications } from './components/wizards/WizardNotifications';
import { WizardAbout }        from './components/wizards/WizardAbout';

// Which sheet/wizard is open (null = none). Top-level so any handler can set it.
const openSheet = signal(null);

export function App() {
    useEffect(() => { ConnectionManager.init(); }, []);

    return (
        <div class="min-h-screen bg-base-200 flex flex-col">
            <TopBar onMenuSelect={(id) => { openSheet.value = id; }} />

            <main class="flex-1">
                <BrewView
                    onMenuSelect={(id) => { openSheet.value = id; }}
                    onStartManual={() => { mode.value = 'manual'; }}
                />
            </main>

            <SheetHost />
            <Toast />
            <HopAlertOverlay />
        </div>
    );
}

// Sheet host — routes openSheet ids to the wizard/sheet component.
// Each wizard handles its own data load/save via ConnectionManager and
// signals state to the rest of the app via shared signals.
function SheetHost() {
    const which = openSheet.value;
    if (!which) return null;
    const close = () => { openSheet.value = null; };

    switch (which) {
        case 'recipes':       return <RecipeSheet      onClose={close} />;
        case 'brewlog':       return <BrewLogSheet     onClose={close} />;
        case 'equipment':     return <WizardEquipment  onClose={close} />;
        case 'connectivity':  return <WizardConnectivity onClose={close} />;
        case 'tuning':        return <WizardTuning     onClose={close} />;
        case 'notifications': return <WizardNotifications onClose={close} />;
        case 'about':         return <WizardAbout      onClose={close} />;
        default:              return null;
    }
}

function Toast() {
    const msg = toastMessage.value;
    if (!msg) return null;
    const cls = {
        info:    'alert-info',
        success: 'alert-success',
        error:   'alert-error',
    }[toastType.value] || 'alert-info';
    return (
        <div class="toast toast-top toast-center z-[100]">
            <div class={`alert ${cls} py-2 px-4 text-sm shadow-lg`}>
                <span>{msg}</span>
            </div>
        </div>
    );
}
