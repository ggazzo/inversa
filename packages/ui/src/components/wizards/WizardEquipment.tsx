// WizardEquipment.tsx — P13 thermal params editor.
import { useEffect, useState } from 'react';
import { Button, Input, Label, Paragraph, Text, XStack, YStack } from 'tamagui';
import { ConnectionManager } from '@inversa/services';
import { showToast } from '@inversa/stores';
import { WizardSheet } from './WizardSheet';

interface FormState {
    volumeL: number; powerW: number; ambientC: number;
    diameterM: number; lossCoeff: number;
}

const FIELDS: Array<{ key: keyof FormState; label: string; min: number; max: number; step: number; hint: string }> = [
    { key: 'volumeL',   label: 'Volume (L)',          min: 1,    max: 200,   step: 0.5,  hint: '1 - 200' },
    { key: 'powerW',    label: 'Potência (W)',        min: 500,  max: 10000, step: 50,   hint: '500 - 10000' },
    { key: 'ambientC',  label: 'Ambiente (°C)',       min: -10,  max: 50,    step: 1,    hint: '-10 - 50' },
    { key: 'diameterM', label: 'Diâmetro (m)',        min: 0.1,  max: 1.0,   step: 0.01, hint: '0.1 - 1.0' },
    { key: 'lossCoeff', label: 'Coef. perda (W/m²K)', min: 1,    max: 50,    step: 0.5,  hint: '1 - 50' },
];

interface Props { open: boolean; onClose: () => void }

export function WizardEquipment({ open, onClose }: Props) {
    const [form, setForm]           = useState<FormState | null>(null);
    const [persisted, setPersisted] = useState(false);
    const [loading, setLoading]     = useState(true);

    useEffect(() => {
        if (!open) return;
        setLoading(true);
        ConnectionManager.getThermalParams()
            .then((r: any) => {
                setForm({
                    volumeL: r.volumeL, powerW: r.powerW, ambientC: r.ambientC,
                    diameterM: r.diameterM, lossCoeff: r.lossCoeff,
                });
                setPersisted(!!r.persisted);
            })
            .catch((e: any) => showToast(e?.message || 'Falha ao ler', 'error'))
            .finally(() => setLoading(false));
    }, [open]);

    function save() {
        if (!form) return;
        ConnectionManager.setThermalParams(form.volumeL, form.powerW, form.ambientC, form.diameterM, form.lossCoeff)
            .then(() => { showToast('Parâmetros salvos', 'success'); setPersisted(true); })
            .catch((e: any) => showToast(e?.message || 'Falha', 'error'));
    }

    return (
        <WizardSheet title="Equipamento" open={open} onClose={onClose}
            footer={<>
                <Button size="$3" chromeless onPress={onClose}>Cancelar</Button>
                <Button size="$3" theme="active" disabled={!form || loading} onPress={save}>
                    Salvar (persistir)
                </Button>
            </>}>
            <Paragraph theme="alt2" fontSize="$1">
                Esses parâmetros calibram o feed-forward do PID e o cálculo do
                "agendar pronto às HH:MM". São persistidos em NVS e
                aplicados no próximo boot.{' '}
                <Text fontSize="$1" fontWeight="700"
                    color={persisted ? '$holding' : '$paused'}>
                    {persisted ? '● persistido' : '● default'}
                </Text>
            </Paragraph>

            {loading && <Text fontSize="$2" opacity={0.5}>Lendo do dispositivo…</Text>}

            {form && FIELDS.map((f) => (
                <YStack key={f.key}>
                    <XStack jc="space-between" paddingVertical={2}>
                        <Label htmlFor={`fld-${f.key}`}>{f.label}</Label>
                        <Text fontSize="$1" opacity={0.4}>{f.hint}</Text>
                    </XStack>
                    <Input
                        id={`fld-${f.key}`}
                        size="$3"
                        keyboardType="numeric"
                        value={String(form[f.key])}
                        onChangeText={(s: string) => {
                            const v = parseFloat(s);
                            if (!isNaN(v)) setForm({ ...form, [f.key]: v });
                        }}
                    />
                </YStack>
            ))}
        </WizardSheet>
    );
}
