// platform.web.ts — DOM-backed shims used by the web bundle in place
// of the `react-native` imports the native bundle resolves.

export const Platform = {
    OS: 'web' as const,
    select<T extends Record<string, any>>(specifics: T): T[keyof T] | undefined {
        return (specifics as any).web ?? (specifics as any).default;
    },
};

export const Vibration = {
    vibrate(pattern: number | number[]): void {
        const nav: any = typeof navigator !== 'undefined' ? navigator : null;
        if (nav && typeof nav.vibrate === 'function') {
            try { nav.vibrate(pattern as any); } catch { /* noop */ }
        }
    },
    cancel(): void {
        const nav: any = typeof navigator !== 'undefined' ? navigator : null;
        if (nav && typeof nav.vibrate === 'function') {
            try { nav.vibrate(0); } catch { /* noop */ }
        }
    },
};

export function confirm(message: string, _title: string = 'Confirmar'): Promise<boolean> {
    return Promise.resolve(typeof window !== 'undefined' && window.confirm(message));
}
