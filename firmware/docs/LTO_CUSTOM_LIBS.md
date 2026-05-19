# Habilitar LTO no firmware via rebuild local de `framework-arduinoespressif32-libs`

> **Status:** documentado mas **não validado end-to-end**. Veja seção
> "Tentativa anterior" antes de reusar este caminho — a estratégia
> "óbvia" (Kconfig sdkconfig) **não funciona em ESP-IDF v5.4**, e a
> alternativa via patch CMake está descrita abaixo mas precisa de
> testes que ninguém fez ainda.
>
> `-flto` continua **desligado** em `platformio.ini`. Os archives
> Espressif que o `arduino-esp32` (pioarduino fork) baixa pelo PIO
> vêm compilados **sem GIMPLE/fat-LTO**, então o linker reclama com
> `plugin needed to handle lto object` + `undefined reference to
> app_main` assim que `-flto` aparece em `build_flags`. Para
> destravar é preciso rebuildar essas archives localmente com flags
> LTO ativadas, substituir as cópias no cache do PIO, e só então
> reativar `-flto` no app.
>
> Ganho esperado (medido em projetos semelhantes): ~12–25 KB Flash +
> 2–5 KB IRAM. Pode variar para mais ou menos no Inversa.

---

## Tentativa anterior — não persegui-la

A primeira versão deste documento sugeria adicionar
`CONFIG_COMPILER_OPTIMIZATION_LTO=y` em `configs/sdkconfig.defaults`
do `esp32-arduino-lib-builder`. Isso **não funciona** em ESP-IDF
**v5.4**:

```
$ grep -rn "COMPILER_OPTIMIZATION_LTO" esp-idf/components/*/Kconfig
(nada)
```

O símbolo não existe no Kconfig da v5.4. O build completa, mas o
sdkconfig final fica em `CONFIG_COMPILER_OPTIMIZATION_DEBUG=y` e os
`.a` resultantes não trazem nenhuma seção `.gnu.lto_*` — i.e. a
flag passou batida e nada de LTO. Não desperdice 90 min de Docker
build testando isso de novo.

A rota que sobra é injetar `-flto -ffat-lto-objects` direto no
build system do ESP-IDF, fora do Kconfig.

---

## Pré-requisitos

| Ferramenta | Por que |
| --- | --- |
| Docker | O lib-builder roda em container reproduzível com toolchain Espressif fixo. Versão local de GCC não importa. |
| Espaço em disco | ~6 GB no host (imagem IDF + build cache). Build descarta tarballs depois. |
| Tempo | 90–180 min na primeira execução (download IDF + componentes + 3× build por chip). Re-runs vão pra ~30 min com cache. |
| Conexão estável | Build clona ~15 sub-repos durante setup. |

## Versão alvo

A versão atual do pioarduino que o projeto pina (`platformio.ini`,
`[common_esp].platform = ...#54.03.21-2`) corresponde a:

- arduino-esp32 **v3.2.1**
- esp-idf **v5.4.2** tag `release/v5.4` commit `858a988d6e`

Para manter compat binária com headers em
`~/.platformio/packages/framework-arduinoespressif32/cores/esp32/`,
**rebuildar contra exatamente essas versões**. Não pegar `master`.

---

## Processo (não validado E2E — leia tudo antes)

### 1. Clone do `esp32-arduino-lib-builder`

```bash
git clone https://github.com/espressif/esp32-arduino-lib-builder.git
cd esp32-arduino-lib-builder
git checkout release/v5.4
```

### 2. Patch no `CMakeLists.txt` — injetar LTO em todos componentes

Adicionar a linha marcada `+` logo após o `project(...)` no
`CMakeLists.txt` raiz do lib-builder:

```cmake
  cmake_minimum_required(VERSION 3.5)

  include($ENV{IDF_PATH}/tools/cmake/project.cmake)
  project(arduino-lib-builder)

+ # Inversa: force LTO + fat-LTO across every IDF component so the
+ # resulting archives carry both GIMPLE bytecode and native code.
+ # Without `-ffat-lto-objects`, anything *not* compiled with LTO
+ # (and there's a lot of it) can't link against the resulting `.a`.
+ idf_build_set_property(COMPILE_OPTIONS  "-flto;-ffat-lto-objects" APPEND)
+ idf_build_set_property(LINK_OPTIONS     "-flto;-fuse-linker-plugin" APPEND)

  idf_build_get_property(elf EXECUTABLE GENERATOR_EXPRESSION)
```

Notas:

* `idf_build_set_property(... APPEND)` é o hook documentado em
  ESP-IDF para acrescentar flags a *todos* os components depois que
  o `project.cmake` registrou o build env.
* `-ffat-lto-objects` faz com que cada `.o` carregue **ambos** o
  bytecode GIMPLE e o código nativo, o que destrava link contra
  componentes que (por qualquer razão) não passem por LTO. Sem
  isso o link continua com o mesmo erro de antes.
* Se algum componente do IDF (TinyUSB, NimBLE host, libsodium,
  etc.) tiver `target_compile_options(... NO_LTO)`, a flag deles
  ganha. Isso é OK — só significa que esses componentes ficam de
  fora do LTO global, mas o link continua funcionando.

### 3. Build via Docker

```bash
docker pull espressif/esp32-arduino-lib-builder:release-v5.4

docker run --rm \
  -v "$(pwd)":/lib-builder \
  -w /lib-builder \
  -e LIBBUILDER_GIT_SAFE_DIR=/lib-builder \
  espressif/esp32-arduino-lib-builder:release-v5.4 \
  ./build.sh -t esp32s3,esp32c3
```

Saída esperada: `out/tools/esp32-arduino-libs/<chip>/{lib,include,ld,…}`.

### 4. Sanity-check — *confirmar que LTO realmente foi aplicado*

Antes de copiar nada para o cache do PIO, valide que pelo menos um
archive carrega seções GIMPLE:

```bash
# RISC-V (C3):
~/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-objdump \
    -h out/tools/esp32-arduino-libs/esp32c3/lib/libfreertos.a | \
    grep -c "\.gnu\.lto"

# Xtensa (S3):
~/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump \
    -h out/tools/esp32-arduino-libs/esp32s3/qio_qspi/libfreertos.a | \
    grep -c "\.gnu\.lto"
```

Resultado esperado: **>0**. Se vier 0, o patch CMake não pegou —
veja a seção "Solução de problemas" abaixo antes de continuar.

### 5. Substituir archives locais que o PIO usa

```bash
cp -r ~/.platformio/packages/framework-arduinoespressif32-libs \
      ~/.platformio/packages/framework-arduinoespressif32-libs.backup-pre-lto

for CHIP in esp32s3 esp32c3; do
    rsync -a out/tools/esp32-arduino-libs/$CHIP/ \
              ~/.platformio/packages/framework-arduinoespressif32-libs/$CHIP/
done
```

Notas:
- **Não substituir `package.json`** — manter a versão do pioarduino
  para o PIO não tentar re-download na próxima `pio run`.
- Mexer só nos chips usados.

### 6. Reativar `-flto` no projeto

Em `firmware/platformio.ini`, dentro de `[common_esp].build_flags`:

```ini
; ── General size optimizations ──
-Os
-ffunction-sections
-fdata-sections
-Wl,--gc-sections
; LTO — só funciona depois de rebuild local dos archives Espressif
; com LTO habilitado via CMake patch. Ver firmware/docs/LTO_CUSTOM_LIBS.md.
-flto
-fuse-linker-plugin
```

E remova a nota "LTO is omitted" do comentário.

### 7. Build de validação

```bash
cd firmware
pio run -e wemos_s3_mini -t clean
pio run -e wemos_s3_mini
pio run -e wemos_c3_mini -t clean
pio run -e wemos_c3_mini
pio test -e native
```

Compare sizes via `objdump -h` (não use `xtensa-esp32s3-elf-size`
em ELFs com PSRAM — soma a região virtual como bss e te dá um
"+1.5 MB RAM" que é miragem):

```bash
~/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump \
    -h .pio/build/wemos_s3_mini/firmware.elf | \
    grep -E "\.flash\.text|\.iram0\.text|\.dram0\.bss|\.dram0\.data"
```

---

## Solução de problemas

- **`.gnu.lto_*` ainda ausente após Step 4** — o patch CMake não
  alcançou o componente. Verifique:
  - `cat build/compile_commands.json | python -m json.tool | grep -A1 freertos | head -3` deve mostrar `-flto -ffat-lto-objects` no compile.
  - Confirme que o `project(...)` no `CMakeLists.txt` aceitou o
    `idf_build_set_property` — algumas versões antigas exigem
    chamar **após** o `idf_build_process`, não antes.

- **`undefined reference to app_main` ainda no link da app** —
  algum archive grande está sem fat-LTO. Roda `nm libXXX.a | grep
  '__gnu_lto'` em cada um; o que aparecer vazio é o culpado.
  Force `-ffat-lto-objects` manualmente nesse componente via
  `target_compile_options(__idf_<comp> PRIVATE -ffat-lto-objects)`.

- **`region 'iram0_0_seg' overflowed`** — LTO inline trouxe
  símbolos `IRAM_ATTR` cross-module pra DRAM ou vice-versa. Marca
  os handlers de ISR críticos com
  `__attribute__((noinline, optimize("no-tree-loop-im")))` para
  proteger das mudanças de inlining.

- **`pioarduino@nova-versão` baixou archives stock por cima** — o
  PIO re-baixa o pacote quando muda a versão pinada. Re-rodar Step
  3 + 4 a cada upgrade.

---

## Custos / armadilhas conhecidos

1. **CI quebra.** Actions vai puxar archives originais via PIO,
   sem LTO. `-flto` no `build_flags` volta a falhar. Opções:
   - **(a)** publicar archives custom como release tarball no nosso
     repo + override em pioarduino. Manutenção pesada.
   - **(b)** rodar o lib-builder na própria pipeline CI antes do
     `pio run`. Adiciona ~30 min ao CI mas é reproduzível.
   - **(c)** manter `-flto` desligado no `platformio.ini`
     versionado + override local. Cada dev/CI que quiser LTO roda
     o patch + rebuild. **Caminho mais barato.**

2. **Atualização do pioarduino sobrescreve cache.** A cada bump de
   versão em `platformio.ini`, PIO baixa archives stock por cima
   do rebuild local. Tem que rodar lib-builder de novo.

3. **Tempo de build local sobe 2–3×.** `pio run -e wemos_s3_mini`
   passa de ~30 s pra ~60–90 s em cold cache. CI já usa cache de
   `.pio`, impacto menor lá.

4. **Debugging mais difícil.** Stack traces pós-LTO podem ter
   funções inlined que somem do `addr2line`. Manter o `.elf`
   pareado com o binário flashado é mais importante.

---

## Quando NÃO fazer isso

O Inversa hoje tem ~2.5 MB de Flash livre num app de ~1.3 MB.
**Esses 12–25 KB economizados não destravam feature.** Manter este
processo só vale se:

- Uma feature futura encostar nos limites de Flash.
- Migrar para chip mais apertado (C6/H2 com 4 MB de flash + RAM
  apertada — aí sim os KB ajudam).
- Habilitar builds PSRAM-less onde tudo cabe em SRAM interno.

Caso contrário: **deixa LTO desligado e mantém este documento aqui
como referência se o quadro mudar.** O caminho está mapeado; basta
seguir Step 2–7.

---

## Referência rápida (TL;DR)

```bash
# Pasta-irmã, fora do repo Inversa
git clone https://github.com/espressif/esp32-arduino-lib-builder.git
cd esp32-arduino-lib-builder && git checkout release/v5.4

# Patch CMakeLists.txt (manual ou via sed/awk):
#   - logo após `project(arduino-lib-builder)`:
#     idf_build_set_property(COMPILE_OPTIONS "-flto;-ffat-lto-objects" APPEND)
#     idf_build_set_property(LINK_OPTIONS    "-flto;-fuse-linker-plugin"  APPEND)

docker run --rm \
  -v "$(pwd)":/lib-builder -w /lib-builder \
  -e LIBBUILDER_GIT_SAFE_DIR=/lib-builder \
  espressif/esp32-arduino-lib-builder:release-v5.4 \
  ./build.sh -t esp32s3,esp32c3

# Sanity check — must be >0:
~/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-objdump \
    -h out/tools/esp32-arduino-libs/esp32c3/lib/libfreertos.a | grep -c "\.gnu\.lto"

# Drop into PIO cache (backup first):
cp -r ~/.platformio/packages/framework-arduinoespressif32-libs \
      ~/.platformio/packages/framework-arduinoespressif32-libs.backup-pre-lto
for C in esp32s3 esp32c3; do
  rsync -a out/tools/esp32-arduino-libs/$C/ \
       ~/.platformio/packages/framework-arduinoespressif32-libs/$C/
done

# Re-enable -flto -fuse-linker-plugin in firmware/platformio.ini
cd ~/dev/inversa/firmware
pio run -e wemos_s3_mini -t clean && pio run -e wemos_s3_mini
pio run -e wemos_c3_mini -t clean && pio run -e wemos_c3_mini
pio test -e native
```

Se algum dia automatizar isso no CI, salvar os outputs em release
do próprio repo e fazer o CI baixar de lá — não há outro caminho
limpo.
