# Floresta Viva — Graphics 4.0: vegetação poligonal

## Referências
- NVIDIA GPU Gems 3, cap. 16, Tiago Sousa/Crytek: Vegetation Procedural Animation and Shading in Crysis.
- CRYENGINE Documentation: Vegetation 02 Grass (Merged Meshes).
- ARK Modding Wiki: Speedtree, LOD, normal maps e materiais separados.

Nenhum modelo, textura ou código proprietário desses jogos foi copiado. O motor é autoral em C++17 e OpenGL ES 3, com diagnóstico Vulkan, ainda sem renderizador Vulkan completo.

## Principais mudanças

O arquivo app/src/main/cpp/forest_tree_mesh.hpp gera malhas reais, não esferas pintadas com PNG:

- Troncos poligonais por tubos curvos Catmull-Rom, anéis e UVs repetidos por comprimento.
- Galhos principais, secundários, brotações e raízes estruturais.
- Três espécies de folhosas, duas coníferas com verticilos e ramos.
- Folhas próximas: geometrias trianguladas dobradas, nervuras, transparência alfa e atlas RGBA.
- Pinheiros: agulhas de conífera com atlas e normal map específicos, diferentes das folhas largas.
- Três LODs distintos para cada espécie: perto (madeira e folhas detalhadas), médio (malhas reduzidas) e longe (silhueta simplificada).
- Pedras com vértices irregulares e troncos caídos que formam parte do cenário.
- Malhas únicas compartilhadas entre instâncias, renderização instanciada, culling aproximado, vento em GPU, normals e translucidez de folhas aproximada.

### Triângulos por UMA instância em LOD alto

| Espécie | Madeira | Folhagem | Total |
|---|---:|---:|---:|
| Folhosa 0 | 2136 | 6144 | 8280 |
| Folhosa 1 | 2368 | 7104 | 9472 |
| Folhosa 2 | 2600 | 8064 | 10664 |
| Pinheiro 0 | 1984 | 7620 | 9604 |
| Pinheiro 1 | 1984 | 7620 | 9604 |

Esses números foram confirmados pelo teste nativo de integridade de malhas no GitHub Actions. Não representam o total desenhado por quadro.

## Controles de qualidade

| Perfil | LOD alto | LOD médio | Grama |
|---|---:|---:|---:|
| ECO | 32 m | 88 m | 42 m |
| ALTA (padrão) | 52 m | 118 m | 75 m |
| ULTRA | 74 m | 155 m | 107 m |

Use o botão Gráficos no canto superior direito. A preferência fica salva no Android.

## Artefatos 3D para o Blender

O GitHub Actions executa testes de vértices finitos, índices, UVs e redução de geometria, além de compilar o APK. A etapa de exportação cria o artefato FlorestaViva-3D-Trees-OBJ: 15 modelos Wavefront (.obj), as texturas correspondentes e o material forest_materials.mtl.

Para inspecionar uma espécie: no Blender, use File > Import > Wavefront (.obj), mantenha imagens e MTL na mesma pasta e abra broadleaf_0_LOD0.obj, por exemplo. Mapeamento de opacidade e normal do MTL pode requerer ajuste manual nos nós do Blender.

## Limitações e próximos passos

Melhor geometria não significa gráficos AAA automaticamente. A versão atual **não** tem materiais capturados por fotogrametria, sombras de copa em cascata, iluminação global, reflexos e volumetria calibrados, nem backend Vulkan do cenário. Vulkan é detectado e shaders SPIR-V são validados, mas o jogo segue em GLES3 para preservar um caminho funcional.

Próximos marcos técnicos recomendados: shadow maps/CSM com controle de qualidade; HBAO/SSAO móvel; transição suave e GPU-culling nos LODs; KTX2/BasisU e compactação de textura; instrumentos de FPS e overdraw; medição de performance Mali/Adreno; assets botânicos fotografados ou escaneados com licença adequada e comparação A/B no aparelho real.
