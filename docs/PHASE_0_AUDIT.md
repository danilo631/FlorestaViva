# Fase 0 — Auditoria do motor (2026-10-12)

**Alvo de desempenho:** Android ARM64, GPUs classe Mali-G57 / Adreno 610,
meta inicial de **30 FPS estáveis**. O relato de 60 FPS anterior vem do
usuário; **não é um benchmark executado pelo CI**.

## Arquitetura e estado real

- Jogo renderiza por **OpenGL ES 3.0** (C++20/NDK 28) com JNI/Java para
  UI e texturas PNG; Vulkan Compute opcional somente para o vento.
- Em `game.cpp` há um renderizador forward, sombra depth atlas, céu em
  fullscreen triangle, água alpha blended e pós-processamento LDR RGBA8.
- Renderização de fauna e plantas usa instancing GLES; porém uploads de
  buffer com `glBufferData(..., GL_STREAM_DRAW)` repetem a cada grupo.
- Dezenas de texturas fotográficas **2K PNG RGBA_8888** carregadas na
  inicialização, com mipmaps — custo de memória alto. Muitas texturas são
  RGBA mesmo quando a fonte é RGB/escala de cinza. Não são KTX2/ASTC.
- Atlas fotográficos são construídos **no CI** por Python; ficheiros de
  autoria antigos continuam no Git, mas não necessariamente entram no APK.
- `game.cpp` tem ~86 KB e várias strings GLSL embutidas, tornando
  manutenção e testes difíceis. Não há loader glTF ou modelo escaneado
  de animal/árvore instalado; as malhas são autorais/procedurais.
- O código atual já tem 3 níveis de LOD para árvores e metadados de
  meshlets para exportação, **não** possui GPU-driven meshlet renderer
  nem rasterizador de micropolígonos.
- Falta medição por GPU timer queries, contagem do overdraw, memória do
  driver e gravação de screenshots capturadas no próprio Android.

## Instrumentação desta fase

Foi adicionado um overlay de CPU render thread:
`FPS`, `CPU ms` do envio de comandos, `DC` de chamadas GLES,
`TRI` de triângulos submetidos contando instâncias e `TEX` estimativa
de MiB de RGBA8 + mipmaps efetivamente enviados em JNI.

*FPS é derivado dos intervalos de frames recebidos do SurfaceView.*
*CPU ms NÃO é tempo de GPU; não se chama `glFinish()`.*
*Memória TEX é aproximação dos bitmaps carregados, não inclui FBOs,
driver allocations e vídeo/Vulkan compute.*

O motor registra uma linha `METRICS` no Logcat a cada 240 quadros.
Uma captura física com o overlay permitirá preencher a tabela
"antes/depois" com os mesmos pontos de teste. CI não cria screenshots
do jogo nem mede FPS em GPU real.

## Ordem de riscos

1. **Limite de VRAM e bandwidth**: reduzir imagens de 2K para 1K
   e usar compressão nativa ASTC/KTX2 quando implementada com fallback.
2. **Overdraw de alpha cutouts** na vegetação próxima; otimizar
   cartas e separação dos passes de material.
3. **Draw calls/CPU**: atribuições repetidas de uniformes GLES, buffers
   de instância atualizados toda vez, sombreamento em 1-9 PCF taps.
4. **Sombras e god rays ULTRA**: sombra até 2048², postFX full resolution,
   iluminação de 4 passos sem profiling real.
5. **Streaming síncrono**: geração de chunks e uploads de malha na GL
   thread; picos de CPU durante a caminhada.
6. **Escala do código**: GLSL e mundo/IA misturados no game.cpp, cobertura
   insuficiente de testes em aparelhos GPU físicos.

## Critérios para próxima fase

Build Android + testes nativos verdes no CI; captura ou Logcat real
com FPS/DC/TRI/TEX em ECO, ALTA e ULTRA para medição comparável.
Substituição de ativos por URL/ID CC0 com checksum e CREDITS, sem
atribuir qualidade AAA a um build sem teste visual.
