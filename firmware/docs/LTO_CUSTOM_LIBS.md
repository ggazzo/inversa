# Habilitar LTO no firmware via rebuild local de `framework-arduinoespressif32-libs`

> Estado atual: **`-flto` está desligado** em `platformio.ini`. Os archives
> Espressif que o `arduino-esp32` (pioarduino fork) baixa pelo PIO vêm
> compilados **sem GIMPLE/fat-LTO**, então o linker reclama com
> `plugin needed to handle lto object` + `undefined reference to app_main`
> assim que `-flto` aparece em `build_flags`.
>
> Este documento descreve **como rebuildar essas libs com LTO habilitado**,
> substituir as cópias locais que o PlatformIO usa, e finalmente reativar
> `-flto` no app. Esperar ganho de ~12–25 KB Flash + 2–5 KB IRAM (medido em
> projetos semelhantes; pode variar para mais ou menos no Inversa).

---

## Pré-requisitos

| Ferramenta | Por que |
| --- | --- |
| Docker | O lib-builder roda em container reproduzível com toolchain Espressif fixo. Versão local de GCC não importa. |
| Espaço em disco | ~6 GB no host (imagem IDF + build cache). Build descarta tarballs depois. |
| Tempo | 90–180 min na primeira execução (download IDF + componentes + 3× build por chip). Re-runs vão pra ~30 min com cache. |
| Conexão estável | Build clona ~15 sub-repos durante setup. |

Sem Docker é possível rodar nativamente, mas requer ter exatamente
`esp-idf v5.4.2`, `xtensa-esp-elf-gcc 14.2`, e dependências Python na
mesma versão que o lib-builder espera. Não vale o atrito.

## Versão alvo

A versão atual do pioarduino que o projeto pina (`platformio.ini`,
`[common_esp].platform = ...#54.03.21-2`) corresponde a:

- arduino-esp32 **v3.2.1**
- esp-idf **v5.4.2** tag `release/v5.4` commit `858a988d6e`

Para manter compat binária com headers em `~/.platformio/packages/framework-arduinoespressif32/cores/esp32/`, **temos que rebuildar contra exatamente essas versões**. Não pegar `master`.

## Processo (manual)

### 1. Clone do `esp32-arduino-lib-builder`

```bash
git clone --recursive https://github.com/espressif/esp32-arduino-lib-builder.git
cd esp32-arduino-lib-builder
git checkout release/v5.4   # ou commit 858a988d6e exato
```

### 2. Habilitar LTO em `configs/sdkconfig.defaults`

Adicionar (ou alterar) as linhas:

```ini
# Habilita LTO no compilador esp-idf
CONFIG_COMPILER_OPTIMIZATION_LTO=y
# Garante que .o levam tanto GIMPLE quanto código nativo,
# o que destrava link contra .o emitidos por outros componentes.
CONFIG_COMPILER_OPTIMIZATION_DEFAULT=y
# (opcional, mais agressivo) Force Os (já era default, deixar explícito):
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
```

Tem variantes por chip em `configs/defconfig.common`. Replicar lá se
quiser garantir que aplica em todos.

### 3. Build via Docker (recomendado)

O projeto upstream tem um wrapper `tools/docker-build.sh`. Resumo dos
chips que este projeto usa:

```bash
# ESP32-S3 Mini
./tools/docker-build.sh -t esp32s3

# ESP32-C3 Mini
./tools/docker-build.sh -t esp32c3
```

Saída: `out/tools/esp32-arduino-libs/<chip>/{lib,include,ld,…}`.

### 4. Substituir archives locais que o PIO usa

```bash
# Backup do que veio do pioarduino
cp -r ~/.platformio/packages/framework-arduinoespressif32-libs \
      ~/.platformio/packages/framework-arduinoespressif32-libs.backup

# Substituir só os archives por chip que usamos
for CHIP in esp32s3 esp32c3; do
    rsync -a out/tools/esp32-arduino-libs/$CHIP/ \
              ~/.platformio/packages/framework-arduinoespressif32-libs/$CHIP/
done
```

Notas:
- **Não substituir `package.json`** — manter a versão do pioarduino para
  o PIO não tentar re-download na próxima `pio run`.
- Mexer só nos chips usados: `esp32s3`, `esp32c3`. Os outros (`esp32`,
  `esp32c6`, `esp32h2`, `esp32p4`) ficam intactos.

### 5. Reativar `-flto` no projeto

Em `firmware/platformio.ini`, dentro de `[common_esp].build_flags`:

```ini
; ── General size optimizations ──
-Os
-ffunction-sections
-fdata-sections
-Wl,--gc-sections
; LTO — só funciona depois de rebuild local dos archives Espressif com
; LTO habilitado. Ver firmware/docs/LTO_CUSTOM_LIBS.md.
-flto
-fuse-linker-plugin
```

E remover o comentário "LTO is omitted…" pra refletir o novo estado.

### 6. Build de validação

```bash
cd firmware
pio run -e wemos_s3_mini -t clean
pio run -e wemos_s3_mini
pio run -e wemos_c3_mini -t clean
pio run -e wemos_c3_mini
pio test -e native
```

Capture sizes via `xtensa-esp32s3-elf-objdump -h firmware.elf | grep dram0` e
`xtensa-esp32s3-elf-objdump -h firmware.elf | grep flash` para comparar com
o baseline.

---

## Custos / armadilhas conhecidos

1. **CI quebra.** GitHub Actions vai puxar archives originais via PIO,
   sem LTO. `-flto` no `build_flags` vai voltar a falhar com `plugin
   needed to handle lto object`. Opções:
   - **(a)** publicar archives custom como release tarball no nosso
     próprio repo e configurar PIO custom platform script para baixar
     dali (pioarduino aceita override via `board_build.arduino.libs_url`
     em alguns forks — verificar viabilidade).
   - **(b)** rodar o lib-builder na própria pipeline CI antes do `pio
     run`. Adiciona ~30 min ao CI mas é reproduzível.
   - **(c)** manter `-flto` desligado no `platformio.ini` versionado e
     adicionar override local via `.env` ou `platformio.local.ini`.
     Cada dev/CI que quiser LTO precisa do rebuild local. **Caminho
     mais barato, menos benefício.**

2. **IRAM pode estourar.** LTO inline agressivo pode levar funções
   `IRAM_ATTR` cross-module pra fora do IRAM, ou trazer outras pra
   dentro, gerando `region 'iram0_0_seg' overflowed`. Tem `noinline`
   markers em ESP-IDF, mas vale `objdump -h | grep iram0` antes/depois.

3. **Updates pioarduino.** Toda vez que o `platformio.ini` pinar uma
   versão nova do `pioarduino/platform-espressif32`, PIO baixa as
   archives sem LTO de novo e sobrescreve seu rebuild local. **Tem que
   rodar o lib-builder a cada upgrade.** Script de Step 4 + ChangeLog
   ajudam.

4. **Tempo de build local sobe 2-3×.** `pio run -e wemos_s3_mini`
   passa de ~30s pra ~60-90s em cold cache. CI já usa cache de `.pio`
   então o impacto é menor.

5. **Debugging mais difícil.** Stack traces pós-LTO podem ter funções
   inlined que somem do `addr2line`. Manter symbol file (`firmware.elf`)
   pareado com o binário flashed é mais importante que antes.

---

## Verificação end-to-end (depois de tudo rodar)

1. `pio run -e wemos_s3_mini` + `pio run -e wemos_c3_mini` limpos.
2. `xtensa-esp32s3-elf-objdump -h firmware.elf | grep -E "\\.flash\\.text|\\.iram0\\.text|\\.dram0\\.bss"` antes e depois.
3. `pio test -e native` 69/69.
4. Flash em **ambos** S3 e C3 reais. Validar:
   - Boot OK.
   - BLE connect via `tools/ble-probe/probe.mjs`.
   - Recipe curta (`samples/quick.txt`).
   - OTA dry-run.
   - `ESP.getFreeHeap()` estável por ≥30 min.
5. Se IRAM apertar, considerar `__attribute__((noinline))` em handlers
   `IRAM_ATTR` ou ajustar `CONFIG_COMPILER_OPTIMIZATION_LTO_PARTITIONED`
   no sdkconfig do lib-builder.

---

## Quando NÃO fazer isso

O Inversa hoje tem ~2.5 MB de Flash livre num app de ~1.3 MB. **Esses
12–25 KB economizados pelo LTO não destravam feature.** Manter este
processo só vale se:

- Uma feature futura encostar nos limites de Flash.
- Migrar para chip mais apertado (C6 inicial tinha 4 MB também — sem
  problema).
- Habilitar PSRAM-less builds onde tudo cabe em SRAM interno.

Caso contrário, deixar o LTO ligado adiciona uma dependência de build
(o rebuild local das libs) sem retorno proporcional. **A decisão
documentada hoje é: deixar `-flto` desligado e este doc no repo como
referência se o quadro mudar.**

---

## Referência rápida (TL;DR)

```bash
# Pasta-irmã, fora do repo Inversa
git clone --recursive https://github.com/espressif/esp32-arduino-lib-builder.git
cd esp32-arduino-lib-builder && git checkout release/v5.4

# Adicionar CONFIG_COMPILER_OPTIMIZATION_LTO=y em configs/sdkconfig.defaults

./tools/docker-build.sh -t esp32s3
./tools/docker-build.sh -t esp32c3

# Substituir archives locais
for C in esp32s3 esp32c3; do
  rsync -a out/tools/esp32-arduino-libs/$C/ \
       ~/.platformio/packages/framework-arduinoespressif32-libs/$C/
done

# Reativar -flto -fuse-linker-plugin no firmware/platformio.ini
cd ~/dev/inversa/firmware
pio run -e wemos_s3_mini -t clean && pio run -e wemos_s3_mini
pio run -e wemos_c3_mini -t clean && pio run -e wemos_c3_mini
pio test -e native
```

Se algum dia automatizar isso no nosso CI, salvar os outputs em
`firmware/scripts/lto-libs/<sha>.tar.gz` ou similar e fazer o CI baixar
de release próprio — não há outro caminho limpo.
