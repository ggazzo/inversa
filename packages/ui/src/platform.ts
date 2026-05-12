// platform.ts — small surface re-exported from `react-native` for the
// native bundle. Vite picks `platform.web.ts` instead (`.web.ts` is
// ahead of `.ts` in `resolve.extensions`) so the browser bundle never
// reaches for `react-native` — that single alias would pull in ~420
// modules from react-native-web for no payoff.

import { Alert, Platform, Vibration } from 'react-native';

export { Platform, Vibration };

/** Cross-platform yes/no prompt. Always returns a Promise even on
 *  web (where it's synchronous internally) so callers don't have to
 *  branch per platform. */
export function confirm(message: string, title: string = 'Confirmar'): Promise<boolean> {
    return new Promise((resolve) => {
        Alert.alert(title, message, [
            { text: 'Cancelar', style: 'cancel', onPress: () => resolve(false) },
            { text: 'OK',                       onPress: () => resolve(true) },
        ], { cancelable: true, onDismiss: () => resolve(false) });
    });
}
