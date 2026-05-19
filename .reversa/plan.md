# Plano de Exploração — inversa

> Criado pelo Reversa em 2026-05-11
> Marque cada tarefa com ✅ quando concluída.
> Você pode editar este plano antes de iniciar: adicione, remova ou reordene tarefas conforme necessário.

---

## Fase 1: Reconhecimento 🔍

- [x] ✅ **Scout** — Mapeamento de estrutura de pastas e tecnologias
- [x] ✅ **Scout** — Análise de dependências e gerenciadores de pacotes
- [x] ✅ **Scout** — Identificação de entry points, CI/CD e configurações

## Decisão de organização das specs 🗂️

> Entre o Scout e o Arqueólogo, o Reversa pergunta como você quer organizar as specs (por módulo, caso de uso, endpoint, híbrida, por features ou customizada). A escolha fica persistida em `.reversa/config.toml` na seção `[specs]` e não será reperguntada em execuções futuras. Para reapresentar o menu, remova manualmente a seção.

## Fase 2: Escavação 🏗️

> Organização escolhida: **por feature** (`granularity = "feature"` em `.reversa/config.toml`).
> Cada item gera uma spec em `_reversa_sdd/<feature>/`.

- [x] ✅ **Arqueólogo** — `controle-de-temperatura` (Temperature, PID, Heater, Pump, AutoTune, Ramp)
- [x] ✅ **Arqueólogo** — `execucao-de-receitas` (RecipePlugin + DSL de 22 comandos + mash-out)
- [x] ✅ **Arqueólogo** — `timer-de-fervura` (BoilTimerPlugin + adições de lúpulo)
- [x] ✅ **Arqueólogo** — `agendamento-inteligente` (SchedulerPlugin + ThermalCalc + RTCPlugin/NTP)
- [x] ✅ **Arqueólogo** — `timer-generico` (TimerPlugin)
- [x] ✅ **Arqueólogo** — `logging-de-brassagem` (BrewLogPlugin + export CSV/JSON)
- [x] ✅ **Arqueólogo** — `recuperacao-pos-queda` (RecoveryManager + SDCardPlugin/recovery.bin)
- [x] ✅ **Arqueólogo** — `conectividade-bluetooth` (BLEPlugin + CommandHandler + protocol.h + web/services)
- [x] ✅ **Arqueólogo** — `atualizacao-wifi-ota` (WiFiPlugin + OTAPlugin + GitHub Releases)
- [x] ✅ **Arqueólogo** — `armazenamento-de-receitas` (SDCardPlugin /recipes + upload/save/delete via BLE)
- [x] ✅ **Arqueólogo** — `app-web-pwa` (pages, components, stores, service worker, manifest)
- [x] ✅ **Arqueólogo** — `infraestrutura-de-plugins` (Plugin, PluginManager, EventBus, NVSStorage, MachineState)

## Fase 3: Interpretação 🧠

- [x] ✅ **Detetive** — Arqueologia Git e ADRs retroativos
- [x] ✅ **Detetive** — Regras de negócio implícitas e máquinas de estado
- [x] ✅ **Detetive** — Matriz de permissões (RBAC/ACL)
- [x] ✅ **Arquiteto** — Diagramas C4 (Contexto, Containers, Componentes)
- [x] ✅ **Arquiteto** — ERD completo e integrações externas
- [x] ✅ **Arquiteto** — Spec Impact Matrix

## Fase 4: Geração 📝

- [x] ✅ **Redator** — Specs SDD por componente (12 units, 47 arquivos)
- [x] N/A **Redator** — OpenAPI (não aplicável; BLE GATT em vez de HTTP)
- [x] ✅ **Redator** — User Stories (3 fluxos completos)
- [x] ✅ **Redator** — Code/Spec Matrix (cobertura 100%)

## Fase 5: Revisão ✅

- [x] ✅ **Revisor** — Revisão cruzada de specs (sem Codex; revisão manual interna)
- [x] ✅ **Revisor** — Resolução de lacunas com o usuário (21/22 resolvidas; P15 adiada)
- [x] ✅ **Revisor** — Relatório de confiança final (**91%**, partindo de 64.9% inicial)

---

## Agentes Independentes

> Execute estes agentes quando os recursos estiverem disponíveis — podem rodar em qualquer fase.

- [ ] **Visor** — Análise de interface via screenshots
- [ ] **Data Master** — Análise completa do banco de dados
- [ ] **Design System** — Extração de tokens de design
- [ ] **Tracer** — Análise dinâmica (requer sistema acessível)

---

## Próximo passo

Após o Time de Descoberta concluir e o `_reversa_sdd/` estar populado, você pode disparar um dos fluxos seguintes:

- `/reversa-migrate`: orquestrador do **Time de Migração** (Paradigm Advisor → Curator → Strategist → Designer → Screen Translator → Inspector). Gera as specs do sistema novo. Saída em `_reversa_sdd/migration/` e `_reversa_sdd/screens/`.
- `/reversa-reconstructor`: gera plano bottom-up para reimplementar o software a partir das specs do legado (uma tarefa por sessão).
