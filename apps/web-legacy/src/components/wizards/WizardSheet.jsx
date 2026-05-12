// WizardSheet.jsx — responsive container for the contextual wizards.
//
// Mobile (<sm): bottom-sheet that slides up to ~90% of the viewport.
// Desktop (≥sm): centered modal up to 32rem wide.
// In both cases: clicking the backdrop closes; ESC closes via the
// browser-native modal handling (DaisyUI / dialog).
//
// Body content is the wizard itself (children). The wizard signals
// completion either by calling `onClose` or by reaching a terminal
// state and using `<WizardSheet.Footer>`.

import { useEffect, useRef } from 'preact/hooks';

export function WizardSheet({ title, onClose, children, footer = null }) {
    const panelRef    = useRef(null);
    const previousRef = useRef(null);

    // Lock body scroll, focus the first input/button, restore focus on close,
    // and capture ESC at the document level.
    useEffect(() => {
        previousRef.current = document.activeElement;
        const prevOverflow  = document.body.style.overflow;
        document.body.style.overflow = 'hidden';

        const t = setTimeout(() => {
            const first = panelRef.current?.querySelector(
                'input, select, textarea, button, [tabindex]:not([tabindex="-1"])'
            );
            (first || panelRef.current)?.focus?.();
        }, 0);

        function onKey(e) {
            if (e.key === 'Escape') { e.preventDefault(); onClose?.(); }
        }
        document.addEventListener('keydown', onKey);

        return () => {
            clearTimeout(t);
            document.removeEventListener('keydown', onKey);
            document.body.style.overflow = prevOverflow;
            previousRef.current?.focus?.();
        };
    }, []);

    return (
        <div
            class="fixed inset-0 z-50 bg-black/60 flex sm:items-center items-end justify-center"
            onClick={onClose}
            role="dialog" aria-modal="true" aria-label={title}
        >
            <div
                ref={panelRef} tabIndex={-1}
                class="bg-base-100 w-full sm:max-w-lg sm:rounded-2xl rounded-t-2xl
                       max-h-[90vh] flex flex-col shadow-xl outline-none"
                onClick={(e) => e.stopPropagation()}
            >
                <div class="flex items-center justify-between px-4 py-3 border-b border-base-300">
                    <h3 class="font-semibold text-base">{title}</h3>
                    <button class="btn btn-ghost btn-sm btn-circle"
                            onClick={onClose} aria-label="Fechar">
                        ✕
                    </button>
                </div>
                <div class="flex-1 overflow-y-auto p-4 space-y-3">
                    {children}
                </div>
                {footer && (
                    <div class="px-4 py-3 border-t border-base-300 flex gap-2 justify-end">
                        {footer}
                    </div>
                )}
            </div>
        </div>
    );
}
