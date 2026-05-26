// LossTune.tsx — drives the loss-coefficient auto-tune flow. Mirrors the
// AutoTune view layout: phase label, progress bar, status copy, cancel.
// On RESULT it swaps the cancel button for an accept/reject pair plus a
// summary card showing fitted h, R², τ, sample count and ambient source.
import { Button, Card, Paragraph, Progress, Text, XStack, YStack } from 'tamagui';
import { useSignals } from '@preact/signals-react/runtime';
import {
  lossTuneActive, lossTunePhase, lossTuneProgress, lossTuneTargetMode,
  lossTuneFittedCoeff, lossTuneR2, lossTuneTau, lossTuneSampleCount,
  lossTuneAmbientSource, lossTuneError,
  showToast,
} from '@brewpilot/stores';
import { ConnectionManager } from '@brewpilot/services';

const PHASE_COPY: Record<string, { title: string; hint: string }> = {
  PREFLIGHT: {
    title: 'Verificando condições',
    hint: 'Conferindo volume, diâmetro, ambiente e sensor.',
  },
  HEAT:      {
    title: 'Aquecendo',
    hint: 'Subindo a temperatura ~50°C acima do ambiente para gerar um ΔT mensurável.',
  },
  SOAK:      {
    title: 'Estabilizando',
    hint: 'Homogeneizando a temperatura antes de soltar a SSR.',
  },
  DECAY:     {
    title: 'Coletando curva de resfriamento',
    hint: 'SSR desligada. Amostrando T a cada 1 s até cair perto do ambiente.',
  },
  FIT:       { title: 'Ajustando exponencial',  hint: 'Calculando τ e h via mínimos quadrados.' },
  RESULT:    { title: 'Resultado pronto',       hint: 'Confira o ajuste antes de gravar.' },
  ERROR:     { title: 'Falha no auto-tune',     hint: 'Veja o motivo abaixo e tente novamente.' },
};

export function LossTune() {
  useSignals();

  const phase = lossTunePhase.value;
  const copy  = PHASE_COPY[phase] ?? { title: phase, hint: '' };
  const isResult = phase === 'RESULT';
  const isError  = phase === 'ERROR';

  function cancel() {
    if (!confirm('Cancelar LossTune?')) return;
    ConnectionManager.cancelLossTune()
      .then(() => showToast('LossTune cancelado'))
      .catch(() => {});
  }
  function accept() {
    ConnectionManager.acceptLossTune()
      .then(() => showToast('Coeficiente salvo', 'success'))
      .catch(() => showToast('Falha ao salvar', 'error'));
  }
  function reject() {
    ConnectionManager.rejectLossTune()
      .then(() => showToast('Ajuste descartado'))
      .catch(() => {});
  }

  return (
    <Card elevate size="$4" padded>
      <YStack ai="center" gap="$3">
        <Text fontSize={40}>🌡️</Text>
        <Text fontWeight="700" fontSize="$7">{copy.title}</Text>
        <Paragraph theme="alt2" textAlign="center" maxWidth={420}>
          {copy.hint}
        </Paragraph>

        <Text fontSize="$1" opacity={0.7}>
          Modo: <Text fontFamily="$mono">{lossTuneTargetMode.value === 'lidOn' ? 'tampa fechada' : 'tampa aberta'}</Text>
        </Text>

        <YStack width="100%" gap="$1">
          <Progress value={lossTuneProgress.value} backgroundColor="$backgroundFocus">
            <Progress.Indicator backgroundColor="$primary" />
          </Progress>
          <Text fontSize="$1" opacity={0.5} textAlign="center">
            {lossTuneProgress.value}% — não interrompa
          </Text>
        </YStack>

        {isResult && (
          <Card bordered padded>
            <YStack gap="$1">
              <Text fontWeight="700">Ajuste</Text>
              <Text fontFamily="$mono">h = {lossTuneFittedCoeff.value.toFixed(2)} W/m²K</Text>
              <Text fontFamily="$mono">τ = {lossTuneTau.value.toFixed(0)} s</Text>
              <Text fontFamily="$mono">R² = {lossTuneR2.value.toFixed(4)}</Text>
              <Text fontSize="$1" opacity={0.7}>
                {lossTuneSampleCount.value} amostras · ambiente {lossTuneAmbientSource.value === 'sensor' ? 'sensor' : 'manual'}
              </Text>
              {lossTuneAmbientSource.value === 'manual' && (
                <Paragraph fontSize="$1" theme="alt2">
                  Ambiente manual — não foi possível verificar drift durante o teste.
                </Paragraph>
              )}
            </YStack>
          </Card>
        )}

        {isError && (
          <Card bordered padded theme="red">
            <Text fontFamily="$mono">{lossTuneError.value || 'erro desconhecido'}</Text>
          </Card>
        )}

        {isResult ? (
          <XStack gap="$3">
            <Button theme="red" variant="outlined" onPress={reject}>Descartar</Button>
            <Button theme="green" onPress={accept}>Gravar coeficiente</Button>
          </XStack>
        ) : isError ? (
          <Button onPress={reject}>Fechar</Button>
        ) : (
          <Button theme="red" variant="outlined"
                  disabled={!lossTuneActive.value} onPress={cancel}>
            Cancelar
          </Button>
        )}
      </YStack>
    </Card>
  );
}
