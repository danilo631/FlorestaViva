# Credits — Floresta Viva

**Jogo e código próprio:** FlorestaViva, licença MIT no repositório.

**Materiais externos incorporados durante o build:** Poly Haven — CC0-1.0.
Os mapas não são arquivos originais da Unreal Engine nem de jogos comerciais.
**Powered by Poly Haven** — https://polyhaven.com (uso da API pública).

| Arquivo de destino | Asset original | Autor(es) | URL | Licença |
|---|---|---|---|---|
| `grass*.png` | Forest Ground 01 | Rob Tuytel | https://polyhaven.com/a/forrest_ground_01 | CC0-1.0 |
| `soil*.png` | Leaves Forest Ground | Dimitrios Savva (fotografia), Dario Barresi (processamento) | https://polyhaven.com/a/leaves_forest_ground | CC0-1.0 |
| `moss*.png` | Forest Leaves 02 | Rob Tuytel | https://polyhaven.com/a/forest_leaves_02 | CC0-1.0 |
| `bark*.png` (família principal) | Bark Brown 02 | Rob Tuytel | https://polyhaven.com/a/bark_brown_02 | CC0-1.0 |
| `pine_bark*.png` | Pine Bark | Dimitrios Savva | https://polyhaven.com/a/pine_bark | CC0-1.0 |
| `rock*.png` | Mossy Rock | Rob Tuytel | https://polyhaven.com/a/mossy_rock | CC0-1.0 |
| `riverbed*.png` | Clean Pebbles | Charlotte Baglioni (fotografia), Dario Barresi (processamento) | https://polyhaven.com/a/clean_pebbles | CC0-1.0 |
| `bark_platanus*.png` | Bark Platanus | Dimitrios Savva | https://polyhaven.com/a/bark_platanus | CC0-1.0 |
| `bark_willow*.png` | Bark Willow | Dario Barresi, Dimitrios Savva | https://polyhaven.com/a/bark_willow | CC0-1.0 |

Estes materiais são baixados com MD5 fornecido pelo provedor e SHA256 registrado em
`app/src/main/assets/assets_provenance.json` dentro do APK (quando o CI passar).

**Outros materiais licenciados já existentes da Graphics 3–11:**
- ambientCG *Leaf001*, texturas fotográficas botânicas de cor, normal e transparência usadas no atlas de folhas — https://ambientcg.com/view?id=Leaf001 — CC0.
- ambientCG *LeafSet024*, nove folhas fotografadas usadas no atlas — https://ambientcg.com/view?id=LeafSet024 — CC0.
- Galhos, terreno procedural, animais antigos e fungos do projeto: código/objetos geométricos autorais; **não** são assets escaneados.

**Pacotes ainda NÃO incorporados ao renderizador nesta etapa:** árvore Tree Small 02,
Pine Tree 01, toco Tree Stump 01, Dead Tree Trunk 02 (Poly Haven CC0),
modelo Fox do Khronos glTF Sample Assets (modelo CC0, animação/rig CC-BY 4.0).
Esses modelos são **candidatos**, não devem ser apresentados como instalados
enquanto não existirem malhas otimizadas carregadas em tempo de execução.
Consulte `assets_manifest.json`.

Formato de saída do build: `*_rough.png` contém atualmente **ORM** (R: AO,
G: roughness, B: metallic=0) para os materiais listados. Os mapas de altura
são guardados para futuras técnicas de blend/displacement; não há tesselação
dinâmica na versão atual. Os mapas de normais são RGB PNG com os dois
componentes X/Y provenientes da captura; o hardware faz upload como RGBA8.

Licenças oficiais: https://polyhaven.com/license e https://ambientcg.com/license .
