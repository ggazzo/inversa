// ToastBridge.tsx — listens to the `toastMessage` signal (set by the
// legacy showToast helper) and forwards it to Tamagui's ToastController.
// This keeps `showToast('text', 'success')` working from anywhere in the
// codebase without forcing each call site to use hooks.

import { useEffect } from 'react';
import { useToastController } from '@tamagui/toast';
import { toastMessage, toastType } from '../stores/state';

export function ToastBridge() {
    const controller = useToastController();
    useEffect(() => {
        const unsub = toastMessage.subscribe((msg) => {
            if (!msg) return;
            controller.show(msg, {
                duration: 3000,
                burntOptions: { preset: toastType.value === 'error' ? 'error' : 'done' as any },
                customData: { type: toastType.value },
            });
        });
        return () => unsub();
    }, [controller]);
    return null;
}
