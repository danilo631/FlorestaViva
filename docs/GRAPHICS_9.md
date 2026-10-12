# Floresta Viva Graphics 9 — rios e iluminação raster aprimorados

## Causa dos defeitos da Graphics 8

A versão anterior calculava a largura da malha de água no eixo X sem considerar
a inclinação da curva e depois limitava cada vértice aos extremos dos chunks.
Isso criava triângulos de área zero, sobreposições e lacunas nas margens.
Além disso, o terreno descia abruptamente abaixo da água perto das bordas.

**Solução:** os patches da água são divididos em células menores, recortados
geometricamente por dois planos X e triangulados. Vértices nas bordas são
interpolados de forma contínua entre os chunks. O leito usa a mesma métrica
lateral ajustada à curvatura do rio, com transição suave até a margem.

## Rasterização e materiais

- Água com duas camadas de normal maps em movimento, ondulação geométrica leve,
  reflexão especular GGX aproximada, Fresnel/Schlick, absorção Beer-Lambert
  aproximada e espuma irregular calculada na margem.
- Sombreamento por mapa de profundidade das árvores também aplicado à água.
  Água renderizada depois de superfícies opacas, com depth test e alpha blend.
- Material **Clean Pebbles** (Poly Haven, CC0), mapa difuso e normal OpenGL
  em 2K, fotografado e inserido na geração do APK.
- Material fotografado de seixos integrado também ao shader do solo nas margens,
  com mistura espacial baseada na *mesma distância do rio* usada pela física.
- Luz de ambiente hemisférica na rasterização: céu frio e reflexão terrestre
  mais quente. PCF 3x3 opcional na qualidade ULTRA para suavizar sombras do solo.
- Malhas de terreno e água passam por frustum culling por chunk, uma redução
  importante da rasterização fora da câmera em GPUs móveis.

## Uso do ForestGenerator como referência

Referência: https://github.com/callumnewlands/ForestGenerator

O projeto de Callum Newlands utiliza OpenGL, árvores L-systems, quadtree com
culling, g-buffer/deferred raster shading, SSAO, luz e sombras e translucidez
de folhas. É distribuído sob **GPLv3**, com exigências próprias. Nenhuma
implementação/fonte, shader, recurso, binário ou arquivo do ForestGenerator
foi copiado para este jogo MIT. As técnicas foram estudadas e implementadas
de forma independente no motor C++17/GLES3 original do FlorestaViva.

Este APK não executa o ForestGenerator nem a Unreal Engine.

## Compilação e teste

Android ARM64 (única arquitetura do APK), assets internos e Vulkan Compute
para o vento. GLES3 continua responsável pela renderização da floresta/rios.
O GitHub Actions executa compilação real GLES com glslangValidator (incluindo
ambos os shaders do rio), testes geométricos que rejeitam triângulos de área
zero ou invertidos, testes de alinhamento de margem em centenas de posições,
downloads dos mapas CC0, validação ELF64 e montagem da Release.

Os testes automatizados não verificam o FPS nem o comportamento no telefone.
Para comparar, visite o mesmo trecho do rio nas versões 8/9, observando
bordas de chunks, transições de areia/pedras, sombra sobre a água, fluidez e
temperatura do aparelho.

Materiais: https://polyhaven.com/a/clean_pebbles (CC0 1.0, autores Charlotte
Baglioni e Dario Barresi). Uma build que não puder validar os recursos
fotográficos falha, sem publicar texturas substitutas desatualizadas.
