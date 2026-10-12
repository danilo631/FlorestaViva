# Floresta Viva — Graphics 6 (materiais e sombras reais)

## Mudança visual, não aumento artificial do APK

O renderizador anteriormente carregava texturas de casca/rochas com UVs limitadas
ao intervalo 0..1 — mesmo quando a malha pedia repetição. Isso esticava
texturas ao longo dos troncos e piorava o nível de detalhe. Foi corrigido
no fragment shader com tratamento distinto para materiais de atlas e materiais
repetitivos.

A versão Graphics 6 usa **quatro pares de texturas fotografadas CC0 em 2K**:
- forrest_ground_01 → grass.png e grass_normal.png
- forest_floor → soil.png e soil_normal.png
- bark_brown_01 → bark.png e bark_normal.png
- pine_bark → pine_bark.png e pine_bark_normal.png

Fonte e termos: https://polyhaven.com/license. Os autores originais e as páginas
dos materiais permanecem listados no script tools/photogrammetry_materials.py.
Cada mapa é recuperado e validado na compilação do GitHub Actions. A compilação
falha se o CDN não retornar os quatro materiais completos; não publica um
APK silenciosamente com materiais antigos ou incompletos.

Os restantes atlas de vegetação (folhas, capim, arbustos e agulhas) seguem
como arquivos unificados, gerados offline por Python, evitando dezenas de
PNG individuais no pacote final.

## Sombras direcionais

- Profundidade real OpenGL ES 3.0: FBO com GL_DEPTH_COMPONENT24 e teste de
  comparação de profundidade (sampler2DShadow).
- Matrizes view/projection da luz direcional atualizadas em torno do jogador.
- Troncos instanciados e folhagem com alpha-cutout projetam sombra.
- Sombreamento real do solo via PCF de 4 amostras; árvores usam sombra de uma
  amostra para limitar custo de fragment shader.
- Sombras arredondadas antigas só aparecem como fallback quando depth-map não
  está disponível.
- ECO: usa fallback leve; ALTA: mapa de 1536 px; ULTRA: 2048 px.
- Pode haver artefatos de resolução/aliasing e queda de FPS no ULTRA.
- Isto não é Cascaded Shadow Maps, ray tracing, Lumen ou Virtual Shadow Maps.

## Entrega pronta para instalar

A branch main faz o GitHub Actions gerar o APK completo e publicá-lo em
GitHub Releases, no arquivo `FlorestaViva-Graphics6.apk`. Não é necessário
transferir nenhum pacote adicional de textura nem executar Python no celular.

**Observação:** o arquivo APK precisa ser assinado para instalações Android.
O instalador automatizado é uma build debug assinada pela chave de debug do
runner; pode haver incompatibilidade de assinatura ao atualizar um APK
instalado previamente. Se o Android indicar "Aplicativo não instalado" ao
tentar atualizar, faça backup da progressão e verifique a assinatura, pois
reinstalar pode remover dados locais. Para atualizações consistentes futuras,
recomenda-se criar uma chave Release privada e guardar os segredos
exclusivamente no GitHub Actions.

## Validação

O CI compila os shaders GLSL com glslangValidator, verifica geometria,
gera texturas CC0, compila Vulkan SPIR-V e empacota C++/Android. A compilação
não substitui teste visual em GPU Mali: sombras, impacto no FPS, temperatura
e gerenciamento de memória ainda precisam ser verificados no dispositivo.

**Tamanho:** 1 GB é um número arbitrário. Este projeto concentra o espaço
nos recursos visuais úteis, não em arquivos de preenchimento. Materiais 4K/8K,
assets PBR completos, flora fotogramétrica e renderização Vulkan da cena
só devem ser acrescentados após testes de GPU e streaming/LOD.
