# Floresta Viva — Graphics 7: diferença visual verificável

## Motivo desta atualização

Na Graphics 6, tronco e chão já usavam materiais CC0 fotografados, porém as
folhas vinham de formas processuais pintadas por cima de uma única textura de
canopy. Esse visual se repetia e dava aparência de recortes artificiais.
Também faltavam antisserrilhamento de bordas e gradação global de imagem.

**Melhorias no código principal**
- Pipeline Python/Pillow gera atlas **2K de folhas efetivamente fotografadas**
  usando Color + NormalGL + Opacity CC0 de ambientCG Leaf001 e LeafSet024.
  A atlas com 16 clusters deriva de onze espécimes botânicos escaneados.
- Quadrantes reunidos em um único **atlas GPU RGBA 2048×2048**, acompanhado
  por atlas de normais RGBA 2048×2048; não há arquivos PNG individuais por
  árvore. Materiais de tronco/solo continuam separados por propriedades.
- Mistura contínua de dois comprimentos de onda de solo fotografado, musgo e
  terra, usando **ruído contínuo** em vez de blocos quadriculados.
- **Renderização em framebuffer de cena** (cor RGBA8 + profundidade) seguida
  de pós-processamento: filtro antisserrilhado de tela baseado em FXAA, contraste
  neutro, dessaturação sutil, vinheta contida e pequena oclusão de contato
  derivada da profundidade no preset ULTRA.
- O sistema de sombras direcionais da Graphics 6 e o Vulkan Compute para vento
  permanecem, com fallback para o modo ECO e para dispositivos incompatíveis.

## Repositórios e referências consultados

- https://github.com/google/filament — documentação PBR, parâmetros de
  materiais e gradação de cor. A arquitetura do motor NÃO foi copiada nem
  migrada para Filament; apenas princípios públicos foram usados.
- https://github.com/MarcelloMorettoni/mapgen — exemplo de workflow CC0
  ambientCG, descarga de Color/NormalGL/Opacity e prevenção de "fringes".
  O script desta atualização é independente.
- https://ambientcg.com/view?id=Leaf001 — duas folhas fotografadas CC0.
- https://ambientcg.com/view?id=LeafSet024 — nove folhas fotografadas CC0.
- https://polyhaven.com/license — materiais do terreno/casca PBR CC0
  da Graphics 6, mantidos.

## Testes e limitações

O GitHub Actions gera e valida os shaders GLES/SPIR-V, fotos, alpha atlases,
teste de malhas, APK e o artefato de imagem **Graphics7_Materials_Preview**
(composição de materiais, não screenshot do jogo).

**Não implementado:** Nanite, Lumen, ray tracing, GI em tempo real, renderer
Vulkan da cena, TAA temporal, fotogrametria de árvores inteiras.
O pós-processamento é LDR para compatibilidade com GPUs OpenGL ES 3.0 e não
equivale a HDR de 16 bits. O SSAO simplificado é uma sombra de contato
limitada pela profundidade, não SSAO volumétrico multiescala.

A build bem-sucedida não comprova melhoria estética ou desempenho no Mali de
um celular real. Avalie árvore perto, arbustos, solo/sombras, horizonte e FPS
antes/depois, de preferência na mesma posição e qualidade gráfica.
