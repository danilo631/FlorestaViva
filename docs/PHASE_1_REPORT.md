# Fase 1 — relatório do build verificado

Build de verificação: https://github.com/danilo631/FlorestaViva/actions/runs/38320647187
Estado: **Success** — compilação e validação nativa Android 64 bits.

## Assets efetivamente obtidos e incorporados

**9 famílias PBR fotográficas CC0 1K**, baixadas pelo Poly Haven com MD5
de metadados e SHA256 fixado no repositório, com 45 arquivos de origem
(ver `assets_manifest.json`) e mapas de albedo, normais e ORM
(AO/Roughness/Metallic). Usados no shader de chão, rochas e três cascas.

- Forest Ground 01 — solo vegetado
- Leaves Forest Ground — terra com folhas
- Forest Leaves 02 — folhas secas/musgo
- Bark Brown 02 — casca folhosa A
- Pine Bark — coníferas
- Mossy Rock — rocha
- Clean Pebbles — leito fluvial
- Bark Platanus — casca folhosa B
- Bark Willow — casca folhosa C

Além desses mapas, folhas botânicas `Leaf001` e `LeafSet024` do ambientCG
seguem presentes no atlas 2K, e recursos de folhas e arbustos do pipeline
existente permanecem.

**2 modelos glTF CC0 de objetos fotografados**, convertidos em **6
malhas móveis**. Contagem real produzida pelo conversor e verificada por
testes de cobertura/indexação:

| Modelo | LOD perto | LOD médio | LOD longe |
|---|---:|---:|---:|
| Tree Stump 01 (toco) | 4.799 triângulos | 1.699 | 480 |
| Dead Tree Trunk 02 (tronco) | 5.999 triângulos | 1.900 | 669 |

Tamanho bruto individual dos 6 FVME: toco 193.492 / 73.156 / 23.872
bytes; tronco 251.476 / 84.832 / 34.252 bytes. Estão dentro do APK.
O formato é uma conversão offline de fonte glTF: o runtime usa o
loader binário `forest_scanned_loader.hpp` e não glTF diretamente.
Os materiais originais específicos do modelo não são carregados;
há shading de casca fotográfica genérica do jogo.

## O que NÃO foi baixado/integrado nesta fase

- Árvores escaneadas completas em três espécies, 9 variações e 4 LODs: não.
- Raposa, cervo, javali de fonte fotogramétrica, rig e animação glTF: não.
- Pedra, samambaia, cogumelo e arbusto como malhas externas: não;
  permanecem geometrias procedurais do motor.
- ASTC/KTX2, meshoptimizer de produção, cgltf/tinygltf runtime: não.
- AO/roughness/normal fotografados em 1K sim; HDR/IBL/CSM/froxel raymarch
  e Nanite real: não. Os shaders atuais são implementações GLES limitadas.

## Métricas reais vs pendências

- Quantidade de PBR de fonte fotográfica validada: 9 famílias.
- Malhas 3D externas presentes e testadas: 2 modelos × 3 LODs.
- Native `libflorestaviva.so`: Android ELF64 ARM64 verificado por CI.
- Render FPS, draw calls, triângulos e uploads reais: instrumentação de
  Fase 0 pronta, **sem números de celular físico disponíveis**.
- Captura visual do jogo: **não disponível no ambiente de CI**; a
  prévia dos materiais gerada pelo CI não deve ser chamada screenshot.
- Meta Mali-G57 / Adreno 610: 30 FPS, **não garantidos nem medidos**.

Próxima medição: instalar o APK Graphics 12 e capturar duas cenas fixas
(área densamente arborizada e margem do rio) em ECO/ALTA/ULTRA com o
overlay (FPS / CPU ms / DC / TRI / TEX). Correlacionar com timestamp,
densidade vegetal, temperatura e gráficos do aparelho.

© Poly Haven assets CC0. Powered by Poly Haven.
Consulte `CREDITS.md` e `assets_manifest.json`.
