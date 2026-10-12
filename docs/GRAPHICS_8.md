# Floresta Viva — Graphics 8: rios, animais, biomas, missões e ARM64

## O que é jogável

O APK atual é um **jogo de exploração em primeira pessoa** para Android 64 bits
com mundo procedural de 2048×2048 metros. Esta atualização acrescenta:

- **Rio contínuo e navegável**: uma spline determinística cruza o mundo, escava
  o solo em cada chunk, gera um leito abaixo do nível da água e uma malha de
  superfície em tiras. O shader OpenGL ES 3.0 usa duas camadas de normais em
  movimento, Fresnel aproximado, reflexo especular, espuma de margem e neblina.
  A água é transparente e desenhada depois dos elementos opacos. Não há
  simulação hidrodinâmica tridimensional nem reflexos planar/SSR.
- **Quatro biomas**: mata de folhas largas, coníferas, vegetação ribeirinha
  (samambaias e taboas) e campos abertos com flores. Árvores não nascem dentro
  da água; vegetação é selecionada conforme bioma, ruído e distância do rio.
- **Três espécies de fauna** em polígonos distintos: cervo (corpo, quatro
  patas, orelhas e galhadas), javali (tronco largo e presas) e raposa (orelhas
  e cauda volumosa). Cada espécie usa parte de um atlas de pelagem. As malhas
  são autorais e instanciadas. A IA local passeia, evita entrar na água,
  aproxima-se do próprio habitat e foge ao detectar o jogador. A população
  limitada a 30 agentes é reciclada durante a exploração.
- **Salto com gravidade** (botão PULAR): velocidade vertical e gravidade em
  C++17, pouso, movimento reduzido em água, colisão com troncos existentes.
  Trata-se de física cinemática simples, não da biblioteca Chaos.
- **Quatro missões sequenciais**: localizar rio, saltar três vezes, observar
  três espécies, recolher seis fragmentos antigos. O estado é salvo no
  dispositivo e continua compatível com as seis informações do arquivo da
  Graphics 7.
- **Minimapa fluvial**: curva azul desenhada a partir de pontos vindos da
  mesma função que gera o rio do mundo.

## Assets/linguagens

- C++17: mundo, física, fauna, instancing, malhas, colisões e objetivos.
- GLSL ES 3: céu, terreno, transparência da água, iluminação, folhas e
  pós-processamento FXAA/contato.
- Vulkan compute: campo de vento (ou fallback CPU); **não** é o renderizador
  completo de água e mundo.
- Python 3, NumPy, Pillow: geração determinística de mapas normais da água,
  textura de cascalho, atlas de pelagem de 3 espécies e atlas de flores.
- Java: interface táctil, HUD, botão PULAR, minimapa, carregamento das
  texturas e persistência.

Materiais fotográficos CC0 de Poly Haven e ambientCG das versões 6–7
permanecem. Recursos carregam de **dentro do APK**, sem diretórios adicionais.

## Requisito obrigatório ARM64

O Gradle usa unicamente \`abiFilters 'arm64-v8a'\` e as bibliotecas resultantes
são ELF 64-bit. O GitHub Actions verifica o único caminho
\`lib/arm64-v8a/libflorestaviva.so\` e a classe ELF64. O APK **não suporta**
Android ARM32.

## Tecnologias de engines AAA utilizadas como referência

- Streamed world chunks, instancing e LOD, equivalentes de conceito às
  ferramentas de foliage de engines contemporâneos.
- Biome-aware PCG: volume fluvial, dispersão filtrada por distância,
  inclinação e ruído, inspirado na documentação de PCG da Unreal.
- Rendering transparent water after opaque passes, dual-scrolling normal maps,
  Fresnel approximation and foam.
- Autonomous AI limited by player-centric streaming for affordable mobile CPU.

Referências:
- https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine
- https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-in-unreal-engine
- https://developer.android.com/games/develop/vulkan/native-engine-support
- https://developer.android.com/ndk/guides/abis

**Não é a Unreal Engine, ARK Mobile nem implementação de Nanite/Lumen.**
Não contém assets proprietários desses jogos, iluminação global dinâmica,
modelos escaneados de animais, IA de combate sofisticada, sistema de
inventário, crafting ou física Chaos. O salto, as missões e a água são
implementações jogáveis iniciais, não equivalentes à profundidade do ARK.

## Como testar

1. Baixe o arquivo APK na Release \`graphics8\`.
2. Abra em Android ARM64 (sem pacotes de dados separados).
3. Alterne ECO, ALTA e ULTRA.
4. Mova-se para leste a partir do ponto inicial até encontrar água.
5. Use PULAR três vezes; aproxime-se dos três tipos de animais sem colidir.
6. Recolha os seis cristais amarelos e verifique o HUD.
7. Use o minimapa para navegar pelo rio (curva azul).

O CI valida estrutura dos shaders, malhas de animais, geometria dos rios,
hidrologia, atlas de texturas, compilação JNI e arquitetura ELF64.
A execução do APK não comprova estabilidade ou FPS no dispositivo físico:
depois da instalação, deve-se observar se a água aparece e as transições
das margens, fauna e objetivos se comportam como esperado.
