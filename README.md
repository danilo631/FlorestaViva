# Floresta Viva — exploração 3D Android (C++ / OpenGL ES 3)

**Jogo jogável de exploração em primeira pessoa em uma floresta procedural, com código-fonte incluído.**

> Importante: este é um jogo independente/protótipo 3D completo no sentido de poder ser aberto e compilado como aplicativo, com loop jogável, controles e objetivo. **Não é uma produção AAA fotorrealista**: as árvores são modelos procedurais e as texturas de base são difusas, criadas por algoritmo. Para aproximar qualidade fotográfica, substitua as texturas e amplie o motor com PBR, mapas normais, sombras e modelos de escaneamento.

## O que já funciona

- Terreno de **2.048 × 2.048 metros** (aproximadamente **4,19 km²**).
- Geração determinística de relevo, gramado, árvores largas, pinheiros, pedras e arbustos.
- Streaming de blocos de terreno **64 × 64 m**, renderizando somente os blocos ao redor da câmera.
- Renderização com **OpenGL ES 3.0** por **C++17/Android NDK**, index buffers e desenho instanciado para vegetação.
- **7 texturas PNG** integradas ao APK, sem downloads em tempo de execução.
- Vegetação cutout com transparência, balanço de folhas, iluminação difusa, névoa e ciclo lento de luz.
- Câmera em primeira pessoa, movimento, corrida, obstáculos nos troncos e limites no mapa.
- Missão: encontre e colete **6 fragmentos**, com contador, distância ao mais próximo e minimapa.
- Controles por toque com joystick, área de arrastar câmera, botões **CORRER** e **PEGAR**.
- Salvamento local automático de posição, direção e fragmentos coletados, usando preferências privadas do Android.
- Funcionamento **offline**, sem contas e sem permissões de internet.

## Estrutura do projeto

```text
FlorestaViva/
├── settings.gradle
├── build.gradle
├── gradle.properties
├── app/
│   ├── build.gradle
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── cpp/
│       │   ├── CMakeLists.txt
│       │   └── game.cpp                # Motor 3D, mapa, renderizador, gameplay, JNI
│       ├── java/com/florestaviva/game/
│       │   └── MainActivity.java      # Android, GLES surface, UI de toque, minimapa
│       └── assets/
│           ├── grass.png
│           ├── soil.png
│           ├── moss.png
│           ├── bark.png
│           ├── canopy.png
│           ├── blades.png
│           └── bush.png
├── tools/
│   └── generate_textures.py
└── .github/workflows/
    └── android-apk.yml                # Compilação automática de APK no GitHub
```

## Como compilar no Android Studio (recomendado)

1. Instale uma versão recente do **Android Studio** e tenha o **JDK 17** configurado para o Gradle.
2. Descompacte `FlorestaViva.zip` e abra a pasta `FlorestaViva` no Android Studio (**Open**).
3. Permita a sincronização Gradle e a instalação de SDK 35, CMake 3.22.1 e NDK 27.2.12479018, se solicitado.
4. Conecte um celular Android compatível com OpenGL ES 3.0 (Android 8+, API 26+) com depuração USB, ou use um emulador com aceleração gráfica.
5. Clique em **Run ▶** para instalar e executar diretamente; ou em **Build → Build Bundle(s) / APK(s) → Build APK(s)**.
6. Para distribuir o APK de teste, encontre: `app/build/outputs/apk/debug/app-debug.apk`.

**Pelo terminal**, se `gradle` versão 8.7 estiver instalado e o Android SDK configurado:

```sh
gradle :app:assembleDebug
```

Este ZIP não traz APK pré-compilado nem o Android SDK. **Cxxdroid sozinho não constrói este APK Android**: você precisa de SDK/NDK/Gradle ou usar a compilação no GitHub.

## Compilar a partir do celular, pelo GitHub Actions

1. Crie um repositório GitHub e envie o **conteúdo descompactado** da pasta `FlorestaViva` (preserve as subpastas, inclusive `.github`).
2. Na aba **Actions**, selecione **Gerar APK Android**. Se necessário, clique em **Run workflow**.
3. Após a execução bem-sucedida, baixe o artefato **FlorestaViva-APK** com `app-debug.apk`.
4. Instale o APK no Android (autorize instalação de fontes externas quando solicitado).

A primeira compilação pode demorar porque o SDK e dependências precisam ser baixados. Esse fluxo utiliza runners do GitHub; a disponibilidade e limites dependem da conta/repositório.

## Controles e missão

- Arraste o joystick esquerdo para caminhar.
- Deslize no lado direito para olhar ao redor.
- Segure **CORRER** para se deslocar mais rápido.
- Aproxime-se de um fragmento luminoso (até ~3,6 m) e toque em **PEGAR**.
- No minimapa, os pontos amarelos indicam fragmentos; pontos além do alcance aparecem na borda.
- Colete os seis para terminar a missão. O progresso é salvo automaticamente e restaurado quando o aplicativo abre novamente.

## Ajustar desempenho

Na primeira parte de `app/src/main/cpp/game.cpp`:

| Ajuste | Padrão | Efeito |
|---|---:|---|
| `RADIUS` | `3` | 49 blocos de terreno ao redor do jogador; mudar para `2` reduz para 25 blocos |
| `GRID` | `32` | 32 × 32 células por bloco; 16 reduz geometria do solo |
| `CHUNK` | `64` | Tamanho de cada bloco em metros; mudar requer rever streaming |
| `for(int i=0;i<13;i++)` | `13` | Número máximo de árvores geradas por bloco |
| `for(int i=0;i<235;i++)` | `235` | Número de plantas/arbustos gerados por bloco |

Para aparelhos mais fracos, use raio 2, grama 100 e árvores 8. Quanto maior a densidade, mais trabalho para a GPU. Qualidade e FPS variam conforme aparelho.

## Trocar para texturas fotográficas / PBR

As texturas incluídas são originais e procedurais, **não fotografias nem conjuntos PBR reais**. Você pode substituí-las por texturas com licença compatível, por exemplo do [ambientCG](https://ambientcg.com/) (materiais CC0). Escolha resolução 512 ou 1024 para não sobrecarregar a memória do celular.

- `grass.png`: solo gramado (albedo/diffuse).
- `soil.png`: terra/pedrinhas.
- `moss.png`: musgo.
- `bark.png`: casca do tronco.
- `canopy.png`: textura opaca de folhas das copas.
- `blades.png`: **RGBA com transparência**, cartas de grama vistas de lado.
- `bush.png`: **RGBA com transparência**, folhas e arbustos vistos de lado.

**Atenção:** trocar por mapas de normal ou roughness não produz automaticamente iluminação PBR, pois os shaders atuais usam apenas albedo. Um pipeline PBR precisa de UV, tangentes, normais/mapas e iluminação apropriada. O gerador de texturas original pode ser reexecutado usando:

```sh
python -m pip install pillow numpy
python tools/generate_textures.py
```

O Python só é necessário para **regenerar** as imagens; as imagens prontas estão incluídas no jogo.

## Tecnologias e limitações

- Java é usado somente para `Activity`, tela, entrada por toque e carregamento dos bitmaps.
- O terreno, a lógica do jogador, a geração procedural, o desenho e a missão são implementados em C++17.
- O motor usa iluminação difusa + ambiente, névoa, sombreamento de contato simplificado e folhas com alpha-test; **não usa** sombras dinâmicas de alta resolução, iluminação global, HDR ou PBR completo.
- Sem backend, anúncios, login, assets baixados dinamicamente ou conexão de rede.
- O executável Android e o desempenho gráfico **não foram testados em dispositivo físico neste ambiente**. O fonte C++ passou por análise de sintaxe com Clang usando cabeçalhos substitutos; isso não equivale a uma compilação NDK/APK nem a um teste funcional da GPU.

## Licença

Código e recursos gerados nesta versão podem ser reutilizados sob os termos do arquivo `LICENSE` (MIT). Para recursos externos futuros, respeite as licenças correspondentes.


## Graphics 2.0

Renderização OpenGL ES 3.0 aprimorada: normal mapping gerado das imagens, iluminação especular GGX simplificada, casca de árvore com bump mapping, iluminação de transmissão nas folhas, copas e raízes extras, densidade de vegetação variável e filtro anisotrópico opcional. Não há ray tracing nem PBR medido; estas técnicas são aproximações móveis. Os mapas de normais são gerados no build via Python + Pillow + NumPy pelo workflow.

Para atualizar as cores com as texturas fotográficas da versão 2.0, envie os sete PNGs do pacote FlorestaViva_Graphics_2 para `app/src/main/assets/`. O workflow também funciona com os PNGs antigos caso você ainda não tenha enviado as novas imagens.
