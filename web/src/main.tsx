// main.tsx — React 18 entry. Wraps the app in TamaguiProvider so theme
// tokens, fonts, and the animation driver are available everywhere.

import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { TamaguiProvider } from 'tamagui';
import tamaguiConfig from '../tamagui.config';
import { App } from './App';
import './main.css';

const rootEl = document.getElementById('app');
if (!rootEl) throw new Error('#app not found');

createRoot(rootEl).render(
    <StrictMode>
        <TamaguiProvider config={tamaguiConfig} defaultTheme="dark">
            <App />
        </TamaguiProvider>
    </StrictMode>
);
