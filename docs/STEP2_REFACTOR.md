# Etapa 2 — Plano executado e pontos de validação

**Base protegida:** tag `pre-refactor` = `939acb003af5436d4d3abbcbff795440ba74b195`.

A Etapa 1 foi integrada à `main` após o CI verde, inclusive 4 capturas
SwiftShader verificadas no
[run 38329848118](https://github.com/danilo631/FlorestaViva/actions/runs/38329848118).
Rollback da Etapa 2: `git reset --hard pre-refactor` **somente em clone
local de desenvolvimento**, nunca force-push na main compartilhada.

## Commits da etapa

- `358a651` — tag automática de rollback validada pelo CI.
- `f2a4cc8` — extrai 11 shaders GLSL (GLES3) para cinco
  unidades de compilação, preservando o código GLSL original.
- `066c9eb` — métricas de CPU/draw calls/triângulos/texturas isoladas.
- `ce12bf4` — captura posterior nas mesmas quatro coordenadas, vídeo
  e comparação com a Etapa 1 por média de diferença RGB.
- `c8f7de0` — ruído determinístico de terreno/biomas em módulo separado.
- `5746b01`, `a97715a` — modo LEGACY de iluminação Lambertiana,
  e correção de `distance` apontada pelo glslangValidator.
- `2035cb3` — cinco perfis com parâmetros centralizados e testes de
  distâncias, shadow taps, pós e compatibilidade com configuração antiga.
- Ajustes posteriores de permissões do workflow e relatórios são
  incluídos no histórico completo do GitHub.

## Estrutura realmente nova

| Área | Código compilado em módulo | Status |
|---|---|---|
| Terreno | `terrain/terrain_shaders.cpp`, `terrain/noise.cpp` | Integrado |
| Vegetação | `vegetation/vegetation_shaders.cpp` | Integrado |
| Céu | `sky/sky_shaders.cpp` | Integrado |
| Pós-processamento | `post/post_shaders.cpp` | Integrado |
| Sombras | `renderer/shadow_shaders.cpp` | Integrado |
| Instrumentação | `renderer/metrics.cpp` | Integrado |
| Qualidade | `renderer/quality.cpp` | Integrado |
| Assets | `assets/ktx2.cpp` | Já existia na etapa 1 |
| Mundo/câmera/JNI/IA | `game.cpp` e outros cabeçalhos | **Ainda parcialmente acoplados** |
| Áudio | Não há módulo com backend real | **Pendente** |

A extração preserva as strings GLSL, e não deve introduzir diferenças
visuais nos perfis antigos. A exceção intencional é o modo LEGACY
(iluminação simplificada) e a ampliação da tabela de perfis.

## Objetivos de desempenho (não medições)

| Perfil | Objetivo | Sombreamento | Vegetação |
|---|---|---|---|
| BAIXO | 60 FPS | Sem mapa de sombras/posFX avançados | Faixa curta |
| MÉDIO | 30 FPS Mali-G57 | Sombras + pós básicos | Moderada |
| ALTO | Qualidade aumentada | Sombras e detalhes ampliados | Maior |
| ULTRA | Máxima qualidade | God rays e PCF aumentado | Faixa máxima |
| LEGACY | Compatibilidade | Albedo + Lambert simples, sem pós | Faixa curta |

**Custo medido:** a instrumentação reporta FPS, tempo CPU, draw calls,
triângulos e estimativa de memória das texturas no Logcat. Comparações do
emulador registram dimensões e média de diferença RGB de quatro pares;
não medem tempo de GPU do aparelho real. Não há FPS real comprovado
na Mali-G57 nem Vulkan graphics swapchain.

## Gates exigidos antes de enviar para main

- APK ARM64 gera no GitHub Actions.
- Todos os shaders GLSL3 e SPIR-V validam.
- Política de qualidade passa nos testes de C++20.
- Captura de quatro screenshots + vídeo do jogo com SwiftShader e
  artefatos de comparação com a Etapa 1.
- `main` só move por fast-forward depois de verde.

A modularização segue por passos para permitir bissecção e rollback,
não como desculpa para parar o trabalho. Próximos alvos reais:
separar streaming de chunks/JNI em mais unidades, depois Cook–Torrance,
IBL HDR, CSM e AO, cada técnica com toggle e custo documentado.
