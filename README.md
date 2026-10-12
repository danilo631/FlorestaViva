# Floresta Viva — Android ARM64 Survival (C++20 / OpenGL ES 3.0)

Floresta Viva é um jogo independente de exploração e sobrevivência na floresta.
O mundo procedural 2.048 × 2.048 m usa *chunks* de 64 m, rio com leito escavado,
vegetação instanciada, 5 espécies procedurais de árvores, 3 animais procedurais,
missões, salto, energia e hidratação.

**O projeto não usa Unreal Engine, Nanite, Lumen, ray tracing ou rasterizador
Vulkan para a cena.** O renderer é OpenGL ES 3.0. Vulkan Compute, quando disponível,
é utilizado apenas para gerar um campo de vento e tem fallback CPU.
As texturas fotográficas CC0 são reais; isso **não** torna as malhas procedurais
automaticamente modelos escaneados.

## Fase 0 — Auditoria e métricas

[Relatório de auditoria](docs/PHASE_0_AUDIT.md).

O aplicativo possui overlay em tela e linha de Logcat `METRICS` a cada
240 quadros:
- **FPS** filtrado a partir do intervalo dos frames do GLSurfaceView;
- **CPU ms** do envio de comandos e atualização — não é tempo de GPU;
- **DC** (draw calls GLES realmente emitidas, contando sombra e pós);
- **TRI** (triângulos submetidos, incluindo contagem de instâncias);
- **TEX MiB** (estimativa de upload de bitmaps RGBA8 com mipmaps).

Para coletar em um celular:
```sh
adb logcat -s FlorestaViva:E | grep METRICS
```

O usuário informou **60 FPS nas versões anteriores**. Nenhum teste de FPS
da versão em desenvolvimento foi executado pelo CI em Mali-G57/Adreno 610.
Nem o overlay nem o CI fornecem `gpu time`, consumo do driver ou energia.

## Fase 1 — Assets verificados, sem substitutos fictícios

O GitHub Actions usa [assets_manifest.json](assets_manifest.json)
e [tools/fetch_assets.py](tools/fetch_assets.py) para obter material fotográfico
CC0 pelo serviço oficial **Poly Haven**, validando MD5 de cada fonte e
registrando SHA256 em `assets_provenance.json`. Sem conexão/checksum válido,
a build falha. **Powered by Poly Haven** — https://polyhaven.com.

O pipeline gera 1K color + normal OpenGL (XY) + ORM (AO, rugosidade,
metallic=0), mais mapa original de altura quando disponível. Não se inventa
relevo a partir de albedo. As texturas de rocha, musgo, solo e madeira
possuem respostas distintas aos shaders. A casca das três árvores
broadleaf pode usar três fotografias CC0 diferentes.

[tools/fetch_models.py](tools/fetch_models.py) obtém glTFs CC0
(`tree_stump_01`, `dead_tree_trunk_02`), checa MD5 de cada arquivo,
simplifica 3 LODs e converte para um formato binário compacto verificado
para o Android. O renderer tem fallback seguro para geometrias antigas.
O carregador é do formato convertido, **não** o cgltf/tinygltf ainda.
Veja [CREDITS.md](CREDITS.md) para fontes e licenças.

### Antes e depois — alterações de código (não screenshots)

| Item | Graphics 11 | Etapas 0/1 |
|---|---|---|
| HUD FPS / draw calls / triângulos | Sem contadores visíveis | Overlay + Logcat implementados |
| Material base do solo | Foto CC0 em PNG 2K | Nova seleção de fotos CC0 em 1K; integrações sujeitas a CI |
| Casca de árvores de folhas largas | 1 família principal | 3 famílias de fotos diferenciadas por espécie |
| Roughness/AO | Mapas rough separados, AO parcial | ORM R=AO, G=rough, B=metal 0; shader utiliza R/G |
| Objetos escaneados glTF | Nenhum no runtime | Conversor/loader para toco e tronco, 3 LODs; exige verificação CI |
| Draw calls e triângulos reais | Sem dados | Contadores de GPU commands; números físicos pendentes |
| FPS Mali-G57/Adreno 610 | 60 informado pelo usuário em outro aparelho/versão | **Ainda não medido** |
| Screenshots reais no celular | Não anexados | **Pendentes** — preview de materiais não é screenshot |

O maior risco continua sendo excesso de alpha-overdraw, geração/upload de
chunks na thread gráfica e textura PNG decodificada como RGBA8. **Não há
ASTC/KTX2 em runtime nesta versão**, nem modelos realistas de fauna importados.
O pipeline de meshlets gera metadados e LOD, mas não executa Nanite.

## Compilar

Requisitos: Android SDK 35, NDK 28.2.13676358, JDK 17,
Android Gradle Plugin 8.13.2, Gradle 8.13, Python 3.12 para preparar assets.
O GitHub Actions instala as ferramentas e prepara assets/GLSL automaticamente.

```sh
gradle :app:assembleDebug --no-daemon
```

O comando local sem executar primeiro os scripts de assets recompila os
recursos já existentes no projeto, que podem não ser os materiais mais recentes.
Use Actions para o pipeline reproduzível. O APK é **somente ARM64-v8a**,
Android API 26+, assinatura debug para teste (pode não atualizar diretamente
uma versão com assinatura diferente, exigindo backup do progresso).

A qualidade interna 0/1/2 corresponde a ECO / ALTA / ULTRA;
os limites de LOD e pós-processamento diferem conforme a qualidade.
O orçamento-alvo é **30 FPS estáveis** num Android médio, ainda não aferido.

## Jogo

Joystick esquerdo = caminhar, lado direito = olhar, botões CORRER, PEGAR e PULAR.
PEGAR coleta fragmentos e recursos ao lado de troncos/pedras/plantas.
O rio, cervos, javalis, raposas e missões de exploração permanecem.
Salvar e carregar são automáticos, e partidas antigas continuam legíveis.

## Rastreabilidade e limitações

- CI: [GitHub Actions](https://github.com/danilo631/FlorestaViva/actions)
- Fontes/licenças: [CREDITS.md](CREDITS.md)
- Auditoria: [docs/PHASE_0_AUDIT.md](docs/PHASE_0_AUDIT.md)
- Recursos: [assets_manifest.json](assets_manifest.json)
- Render atual: OpenGL ES 3, sombra de mapa de profundidade, luz difusa e
  GGX simplificado, translucidez aproximada de folhas e pós-processamento LDR.
- Pendente: modelos glTF verdadeiros para todas as árvores e animais, ASTC
  comprimido carregado em runtime, IBL, CSM, HDR verdadeiro, Hi-Z/GTAO,
  cenários climáticos completos e benchmark físico verificável.
- Uma compilação CI `Success` prova compilação e asserts; não prova qualidade
  fotorrealista, consumo térmico ou 30/60 FPS no aparelho.
