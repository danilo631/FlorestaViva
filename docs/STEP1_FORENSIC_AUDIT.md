# Etapa 1 — Auditoria forense do FlorestaViva (12/10/2026)

## Como reproduzir

Baseline pré-Fase 0: `eaca26c8b34d5287c4278e0cecaf8ffd2b7b5084`.
Commit pré-Etapa 1: `c58078c495e0d20fb955265a34e974d389a25bb1`.
Tag de rollback: `pre-step1` aponta para `c58078c`.

No CI (`phase1-source-audit`):

```sh
git log --oneline -n 30
git diff --stat eaca26c8b34d5287c4278e0cecaf8ffd2b7b5084..HEAD
python tools/audit_repository.py --output build/audit/source_assets.json
```

Execução com saída de auditoria: https://github.com/danilo631/FlorestaViva/actions/runs/38322934192

## Comparação byte a byte dos assets versionados

Dados realmente extraídos de `git show BASE:<path>` + SHA256 de bytes
atuais, com dimensões PNG lidas por Pillow. Coluna idêntico referente ao
commit pré-Fase 0, **não** aos novos assets do APK.

| Arquivo | Bytes no Git | Resolução | Formato | SHA-256 atual (= SHA-256 antigo) | Idêntico? |
|---|---:|---|---|---|---|
| `bark.png` | 2553614 | [1024, 1024] | PNG | `081c675d224af0771e7a48800b53cc3031e89303464c758506ca8e79e0c9b099` | **True** |
| `bark_normal.png` | 620685 | [512, 512] | PNG | `8c0601845c08317a600f042d34a43b32deb318ae972349087e746fd55daf1d73` | **True** |
| `blades.png` | 1062065 | [512, 1024] | PNG | `9af608d7a544be5809d10aea4d283267605d53eef779466b4130767cfc606665` | **True** |
| `bush.png` | 1592541 | [1024, 1024] | PNG | `b09f715c31278185d68fa111cca686a0b3c74e5d6345bd09de6f1f66a1026148` | **True** |
| `canopy.png` | 2146615 | [1024, 1024] | PNG | `1da97633581549c20bcbc372ed310ac362a1dc1b7b38c213e819229b2605031c` | **True** |
| `grass.png` | 2653661 | [1024, 1024] | PNG | `c3c6f6330b2f58272e661977e5c2146827a8c28dc634b97b0bd08f8235afb75b` | **True** |
| `grass_normal.png` | 621484 | [512, 512] | PNG | `b4203eee8259f1e0847a4033db382bee0c38dec07a25f0c372a02efd2039cb51` | **True** |
| `moss.png` | 2569761 | [1024, 1024] | PNG | `45154029d7b3b1d6be0523d785658777db7136782f1c4a5010378bef1935dba1` | **True** |
| `moss_normal.png` | 608874 | [512, 512] | PNG | `542249bb9234da451062c05c165cbeb089bedb08b26301f3cb25cf858cba5cc7` | **True** |
| `soil.png` | 2528118 | [1024, 1024] | PNG | `f12d2f1c29e8e2ec6dd127890b674f8c0a851fbefc1e4384dcb0b2f50044098b` | **True** |
| `soil_normal.png` | 573190 | [512, 512] | PNG | `c28a57fec33ec83c483e19ebb25be284c3c2b4f9d3430ba2fe33d45bd1e6b1c4` | **True** |

**Achado crítico: os 11 PNGs tracked são idênticos ao baseline.** O pipeline
substitui várias texturas APENAS no workspace efêmero do GitHub Actions.
Arquivos de assets produzidos no CI estão no artefato separado
`phase1-baked-audit` (SHA-256, resolução e tamanho incluídos).
A diferença precisa ser comunicada: `git checkout` sem executar o pipeline
não possui os novos arquivos fotográficos.

## Licenças e fontes externas

`CREDITS.md` e `assets_manifest.json` existem no repositório; este último
contém as famílias fotografadas pelo Poly Haven, URL, licença CC0 e 45
hashes SHA-256 pinados. `tools/fetch_assets.py` consulta metadados
oficiais por API, verifica MD5 e SHA256 e reempacota os canais
`rough.png` como ORM (AO vermelho, roughness verde, metallic 0).
Dois modelos GLTF CC0 foram incorporados somente depois de redução de
3 LODs cada, convertidos num formato nativo `FVME`.

**Problemas de proveniência a resolver:** no manifesto alguns autores
são registrados como `Poly Haven contributors (verify asset metadata)`,
não crédito preciso de pessoas. Os `blades` e `bush` ainda são fontes
RGBA antigas para montagem de atlas; suas normais são estimadas de cor.
O atlas fotográfico de folhas ambientCG tem normais realmente de fonte,
mas o atlas de pinheiro contém conteúdo ilustrado/gerado por algoritmo.
Essas limitações impedem descrever toda a vegetação como fotogrametria.

## Evidência dos shaders reais

No `app/src/main/cpp/game.cpp` a compilação nativa usa strings
`TERRAIN_FS` e `INST_FS` (os arquivos `shaders/foliage*.glsl`
têm finalidade de teste Vulkan, não são o shader principal GLES):

- Linha 255–257: leitura de normal por `uGrassNormal`,
  `uMossNormal`, `uDirtNormal`, XY do mapa.
- Linha 264–269: leitura de rugosidade `.g` e AO `.r`
  dos mapas `uGrassRough`, `uDirtRough`, `uMossRough`.
- Linha 404: normal de material `uMaterialNormal`.
- Linha 442: ORM de instâncias `uMaterialRough`.
- Usa GGX/Smith/Schlick de um termo escalar simplificado;
  **não** há PBR metálico fisicamente completo, IBL pré-filtrado,
  LUT BRDF nem GI baseada em probes. Portanto “PBR completo”
  era exagero.

## Estado operacional

O pipeline principal do commit pré-Etapa 1
https://github.com/danilo631/FlorestaViva/actions/runs/38321105020
concluiu **success** e gerou APK 64-bit.
A auditoria instrumentada recompila o mesmo renderer com ABI x86_64 para
testar imagens no emulador, pois a produção é arm64-v8a.

A execução de captura usa SwiftShader e deve publicar 4 screenshots
em pontos fixos + vídeo curto + Logcat, sem convertê-los em
benchmarks de Mali/Adreno. Até um artefato ser confirmado,
**não há screenshots validadas** nesta auditoria.

## Dívidas priorizadas

1. Persistir fontes fotográficas auditáveis no Git ou adotar vendoring
   com hash e cache reprodutível; eliminar assets sintéticos.
2. Trocar geração falsa de normais por mapas reais, inclusive vegetação.
3. Implementar KTX2/ASTC com decodificador real e fallback ETC2/GLES.
4. Renderização modular em unidades separadas; arquivo `game.cpp`
   de aproximadamente 93 KB ainda concentra renderer/terreno/JNI.
5. PBR energia-conservativo e IBL; atualizar pipeline HDR.
6. Capturar cena comparativa em emulador e dispositivos Mali/Adreno;
   o FPS antigo reportado pelo usuário é relato, não teste CI.
