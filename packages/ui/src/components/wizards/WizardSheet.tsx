// WizardSheet.tsx — wraps Tamagui's <Sheet> (a native-feeling bottom
// sheet on mobile, a modal-like container on desktop) with a title bar,
// scrollable body, and an optional footer. Closing via backdrop, ESC,
// or the X button. Focus restoration is handled by Sheet by default.

import type { ReactNode } from 'react';
import { Button, Sheet, Text, XStack, YStack } from 'tamagui';

interface Props {
    title: string;
    open: boolean;
    onClose: () => void;
    children: ReactNode;
    footer?: ReactNode;
}

export function WizardSheet({ title, open, onClose, children, footer }: Props) {
    // Explicit zIndex ladder above whatever the TopBar (sticky, zi=30)
    // and the BrewView panels paint. The scrim gets its own hard
    // backgroundColor because Tamagui's default scrim renders behind the
    // frame and can leave a sticky TopBar painting over it.
    return (
        <Sheet
            modal
            open={open}
            onOpenChange={(o: boolean) => { if (!o) onClose(); }}
            snapPoints={[90]}
            dismissOnSnapToBottom
            zIndex={100_000}
            animation="medium"
        >
            <Sheet.Overlay
                animation="lazy"
                enterStyle={{ opacity: 0 }}
                exitStyle={{ opacity: 0 }}
                backgroundColor="rgba(0,0,0,0.6)"
                zIndex={99_999}
            />
            <Sheet.Frame backgroundColor="$background" padding="$0">
                <Sheet.Handle />
                <YStack flex={1}>
                    <XStack
                        ai="center" jc="space-between"
                        paddingHorizontal="$4" paddingVertical="$3"
                        borderBottomWidth={1} borderBottomColor="$borderColor"
                    >
                        <Text fontWeight="600" fontSize="$5">{title}</Text>
                        <Button size="$2" circular onPress={onClose} aria-label="Fechar">
                            <Text>✕</Text>
                        </Button>
                    </XStack>

                    <Sheet.ScrollView contentContainerStyle={{ padding: 16, gap: 12 }}>
                        {children}
                    </Sheet.ScrollView>

                    {footer && (
                        <XStack
                            paddingHorizontal="$4" paddingVertical="$3"
                            borderTopWidth={1} borderTopColor="$borderColor"
                            gap="$2" jc="flex-end"
                        >
                            {footer}
                        </XStack>
                    )}
                </YStack>
            </Sheet.Frame>
        </Sheet>
    );
}
