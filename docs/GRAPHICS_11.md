# Floresta Viva — Graphics 11 / Systems 1.0

## O que mudou

- **Toolchain (rebuild validado em CI):** Android Gradle Plugin 8.13.2,
  Gradle 8.13, Python 3.12, Android NDK 28.2.13676358 e C++20.
  JDK 17 mantido por ser compatível com AGP 8.13; SDK Android 35 mantém
  comportamento Android verificado. APK **ARM64-only**.
  O uso de C++20 não implica que Vulkan substituiu OpenGL ES 3.0.
- **Foliage pivots (Pivot Painter-inspired):** a malha passa a armazenar em
  cada vértice pivot.xyz e um fator de flexão. O shader movimenta
  individualmente cada card de folha e galho a partir de seu pivô e
  faz o mesmo no passe de depth shadows, para reduzir descolamento.
  Dados são codificados na geração de malha, sem CPU skinning nem novos draw
  calls por folha. **Não** é um importador completo das texturas da Unreal
  Pivot Painter 2.0 nem reconstrução completa da hierarquia de quatro níveis.
  Referência: https://dev.epicgames.com/documentation/en-us/unreal-engine/pivot-painter-tool-2.0-in-unreal-engine
- **PBR fotográfico extra**: fundo com folhas e musgo de
  \`forest_leaves_03\`, Poly Haven CC0: base color, normal OpenGL, roughness.
  O build consulta a **Poly Haven public API**, identifica as variantes
  utilizáveis, verifica MD5 quando disponível, grava SHA256 e procedência.
  Se a API metadata falhar, tenta os URLs canônicos de distribuição.
  **Powered by Poly Haven** — https://polyhaven.com
  Também permanecem os materiais CC0 do ambientCG, baixados pelo endpoint
  público de assets, e as fotos Poly Haven incorporadas nas versões 6–10.
- **Fauna AI**: estados determinísticos REST / GRAZE / ROAM / ALERT / FLEE,
  com comportamento e velocidades por espécie, em custo fixo para 30 agentes.
  Não se trata de aprendizado por rede neural nem de navegação navmesh.
- **Sobrevivência inicial**: stamina que reduz ao correr e recupera ao
  caminhar, hidratação que cai lentamente e se recupera ao visitar o rio,
  inventário madeira/pedra/plantas obtido com o botão PEGAR ao lado de
  troncos, pedras, samambaias ou flores. A missão 5 completa ao coletar
  2 madeiras, 2 pedras, 1 planta. Sem internet, sem loot boxes.
- **Compatibilidade de progresso:** os campos de sobrevivência são anexados
  ao save textual existente. Arquivos das versões 7 a 10 ainda são lidos,
  com valores iniciais para novas propriedades.

## Experimentos de geometria de micropolígonos

\`forest_meshlets.hpp\` contém um **pré-processador real de meshlets no CPU**:
até 64 vértices únicos e 126 triângulos por agrupamento, com bounding spheres,
contagem de triângulos e metadados por espécie/LOD. O teste exporta
\`meshlet_lod_index.json\` junto dos 15 modelos OBJ.

**Limitação importante:** os clusters são validados e exportados, mas o motor
GLES continua desenhando malhas tradicionais e usando LOD por distância.
Não existe neste pacote o sistema virtualizado de geometria, streaming por
páginas de micropolígonos, rasterizador por compute, GPU mesh shading ou
Nanite/Mega Geometry. Implementá-lo na arquitetura mobile requer backend
e profiling próprios. O preprocessamento acrescenta verificabilidade, não
o custo de runtime de renderização virtualizada.

## API e licenciamento

A API Poly Haven funciona **apenas durante a compilação**.
O APK continua completamente offline. Os arquivos fornecidos por
Poly Haven são CC0; eles podem ser redistribuídos com o APK.
A licença da API pública e termos de marca são distintos da licença
dos assets e exigem crédito visível ao usar o serviço:
Powered by Poly Haven (https://polyhaven.com).

A API não injeta shaders ou código arbitrário no jogo; os arquivos têm
domínios restritos a \`api.polyhaven.com\` e \`dl.polyhaven.org\`,
e imagens com dimensões mínimas são validadas antes de substituir assets.

Referências:
- https://api.polyhaven.com (API)
- https://polyhaven.com/license (assets)
- https://developer.android.com/build/releases/agp-8-13-0-release-notes
- https://dev.epicgames.com/documentation/en-us/unreal-engine/pivot-painter-tool-2.0-in-unreal-engine

## Testes

GitHub Actions deve validar:
1. Consulta opcional à API e conjunto color/normal/rough CC0;
2. GLSL ES e SPIR-V;
3. Testes geométricos de folhas/pivôs e cobertura de meshlets;
4. Hydrology, biomes, stamina, gathering e inventory;
5. Android Gradle build e classificação ELF ARM64 dentro do APK.

**Não garante 60 FPS em todo aparelho.** Perfis ECO / ALTA / ULTRA
permanecem. Se a nova vegetação ficar instável, compare em mesmo lugar,
hora do jogo e perfil gráfico com a Graphics 10.

## Próxima etapa técnica

O motor já tem material e meshlet metadata. Um futuro backend dedicado
Vulkan (caso a GPU exponha os recursos necessários) pode experimentar
culling por cluster e GPU-driven indirect draws. É um projeto separado,
não um switch automático para Nanite de desktop.
