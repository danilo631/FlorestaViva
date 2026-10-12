# Floresta Viva Graphics 5.0 — floresta densa + vento Vulkan

Esta versão mantém o jogo Android em C++17/OpenGL ES 3.0, com JNI/Java para
interface e controle e Python (Pillow/NumPy) para o pré-processamento de materiais.
A computação GPU Vulkan agora possui uma tarefa real, opcional e mensurável.

## Mudanças estruturais

- Arquivo \`forest_undergrowth.hpp\`: arbustos com galhos sólidos afilados,
  folhas individuais trianguladas, samambaias com ramos e folíolos, plantas
  rasteiras com geometrias 3D. Instancing GPU reaproveita as malhas.
- Terreno procedural com microclimas: distribuição de plantas varia por umidade,
  sombreamento procedural e inclinação do solo; eleva a quantidade de plantas
  por bloco para cerca de 340 vezes o multiplicador local, mantendo LOD.
- Arbusto 3D só próximo da câmera; em distância intermediária, usa impostor
  mais leve. Samambaia e plântulas aparecem somente no raio detalhado.
- Os controles ECO/ALTA/ULTRA afetam as distâncias dos detalhes, permitindo
  comparar a qualidade sem baixar APKs separados.

## Textura unificada: dois arquivos de vegetação na GPU

A ferramenta \`tools/foliage_pack.py\` combina quatro famílias em quadrantes
do mesmo material:

| Quadrante | Classe | Origem no build |
|---|---|---|
| 0,0 | Capim | blades.png |
| 1,0 | Arbustos | bush.png |
| 0,1 | Folhas largas | leaves_atlas.png |
| 1,1 | Agulhas | pine_atlas.png |

O produto final tem \`foliage_color.png\` e \`foliage_normal.png\` de
1536×1536 pixels, mapeados pelos shaders por UV retangular com \`uAtlasRect\`.
Texturas intermediárias de folhas e agulhas são removidas **do APK gerado**
após o processamento. As imagens-fonte continuam no Git para que os atlas
possam ser regenerados quando o material mudar.

Troncos, solo e pedras continuam com materiais independentes por possuírem
parâmetros próprios de normal mapping e rugosidade. Não é um único arquivo
gigantesco para toda a cena; é o empacotamento mais útil para este motor.

## Vulkan real para simulação de vento

- \`shaders/wind.comp\`: compute shader GLSL 450, 16×16 grupos de 8×8,
  gerando uma textura RGBA8 128×128 com turbulência e rajadas.
- O GitHub Actions compila o shader em SPIR-V via \`glslc\` (NDK).
- \`vulkan_wind.cpp\`: cria VkInstance, seleciona GPU e fila compute,
  VkDevice, VkBuffer SSBO host-visible, VkDescriptorSet, shader module,
  compute pipeline, command buffer e fence.
- O resultado é copiado para textura OpenGL ES após a execução GPU.
- \`INST_VS\` amostra o campo ao mover a vegetação e combina o vento com
  animação secundária das folhas.
- Se o dispositivo Vulkan não suportar o pipeline, o programa usa geração
  determinística em C++ e mantém a floresta jogável.

**Importante:** isto é um backend Vulkan *compute*, não um renderizador de
cenário Vulkan. O terreno, árvores, materiais e céu continuam desenhados
em OpenGL ES 3.0. Alternar o renderer inteiro para Vulkan requer uma
implementação separada de surface, swapchain, render passes/pipelines,
texturas, memória, sincronização e ciclo de vida no Android.

## Técnicas inspiradas pela Unreal Engine

- Batching/instancing de vegetação e LOD, equivalentes de conceito aos
  princípios de Foliage Mode e ISM/HISM — sem usar código da Epic.
- Folhagem com iluminação dos dois lados e transmissão aproximada,
  inspirada em Two Sided Foliage.
- Material atlas com normals, máscara alfa, TBN aproximado e vento procedural
  na GPU.
- Culling aproximado por objeto/bloco, neblina atmosférica e céu procedural.

**Não foi implementado** Nanite, Lumen, Virtual Shadow Maps, CSM, foliage
voxelizado ou o renderizador da Unreal Engine; não seria correto anunciar
esses recursos por analogia.

## Testes

O GitHub Actions testa malhas 3D por integridade de índices/UVs/vértices,
exporta as árvores em OBJ, valida os quadrantes do atlas, mapeamento de
materiais Java/JNI e compilação do APK. A pipeline compilada não garante
sucesso da execução compute em cada GPU, nem prova desempenho/visuais
num aparelho Android — verificar o HUD (campo \`Vento: Vulkan\` ou
\`Vento: CPU\`), FPS, velocidade de abertura e renderização.

Fontes técnicas: Unreal Engine Foliage Mode, Instanced Static Mesh Components,
Two Sided Foliage; Vulkan Guide Tile Based Rendering; Android Vulkan Guide.
