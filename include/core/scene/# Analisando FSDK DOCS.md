# FVSDK 9.7.1 — oclusão e rosto cortado

Varri `C:\FVSDK_9_7_1\doc` (html C++, `dotnet/`, userguide) e os headers em `include/FRsdk`.

## Oclusão / rosto coberto

**Não existe API genérica de oclusão.** A busca literal por `occlu` em toda a documentação não retorna nada, e a lista completa de classes do wrapper .NET (`dotnet/annotated.html`) só tem `Face`, `Eyes`, `Portrait`, `ISO_19794_5`, `Enrollment`, `Verification`, `Identification` e utilitários de imagem — nenhum módulo de qualidade/oclusão.

O único caso de oclusão detectado explicitamente é **máscara facial**:

| API | Tipo | Onde |
|---|---|---|
| `Portrait.Characteristics.faceMaskScore()` | `float` — probabilidade de a pessoa usar máscara | exposto no .NET |
| `Portrait.Feature.Set.wearsFaceMask()` | `bool` | exposto no .NET |
| `Portrait::Feature::Boundaries::wearsFaceMaskThreshold()` | `float` — limiar configurável do bool acima | C++ |

Custo praticamente zero no seu código: em [ImageAnalysis.cs:521](HRF.Service.Business.NetCore/Cognitec/ImageAnalysis.cs:521) você já faz `Feature.Set features = featureTest.assess(portraitCharacteristics)` e lê `features.wearsGlasses()` — `features.wearsFaceMask()` está no mesmo objeto, e `portraitCharacteristics.faceMaskScore()` no que você já tem em mãos.

**Proxies indiretos de oclusão parcial**, todos já disponíveis e hoje não usados:

- `Face.Location.confidence` (`float`) — confiança da detecção do rosto. É o sinal **mais precoce** que existe: vem direto do `Face.Finder`, antes de olhos e antes do `Portrait.Analyzer`. Hoje o código descarta esse campo (usa só `boundingBox`).
- `Eyes.Location.rightConfidence` / `leftConfidence` — a doc diz faixa usual `[0..6]`, valores típicos `2..4`, perto de 0 = ruim. Um olho ocluso derruba um dos dois. Hoje o código só testa `eyesLocations.Length == 0` ([ImageAnalysis.cs:485](HRF.Service.Business.NetCore/Cognitec/ImageAnalysis.cs:485)) e joga fora as confianças.
- `Characteristics`: `rightEyeTinted()`/`leftEyeTinted()` (óculos escuros cobrindo os olhos), `rightEyeOpen()`/`leftEyeOpen()`, `mouthClosed()`, `hotSpots()`, `naturalSkinColour()`, `numberOfFaces()`.

## Rosto cortado para fora dos limites da imagem

Não há teste booleano pronto. Três rotas, em ordem de precisão:

**1) `PaddingRatioExceeded` no recorte ISO** — a mais direta e quantitativa. `ISO_19794_5.FullFrontal.Creator.extract()` recorta o full-frontal; os pixels que faltariam são preenchidos com cor de padding e, se a razão de pixels preenchidos passar do limite configurado, lança `PaddingRatioExceeded`, que expõe `paddingRatio()` — literalmente "quanto do enquadramento exigido está fora da imagem" (`include/FRsdk/fullfrontal.h:97-111`).
**Ressalva:** a doc .NET do `Creator` **não** documenta essa exceção (só a C++). Precisaria verificar na prática como ela aparece no wrapper .NET antes de contar com ela.

**2) Geometria a partir de `Characteristics`** — o `Characteristics` devolve `faceCenter()` (centro da linha dos olhos), `chin()`, `crown()`, `leftEar()`, `rightEar()` (distâncias em px a partir dessa referência) e `width()`/`height()` (dimensões da imagem). Dá para montar a caixa da cabeça e medir quanto dela cai fora de `[0,width] × [0,height]`. Em C++ existe até a função pronta `FRsdk::Portrait::earToEarChinCrownSurroundingBox(Characteristics)`, mas confirmei que ela **não está exposta no wrapper .NET** — em C# seria conta manual.

**3) O overflow que você já calcula** — em [ImageAnalysis.cs:625-628](HRF.Service.Business.NetCore/Cognitec/ImageAnalysis.cs:625) o crop já computa `leftXOverflow`, `rightXOverflow`, `leftYOverflow`, `rightYOverflow` a partir da `BoundingBox` vs. as dimensões da imagem. Hoje esses valores só servem para deslocar o recorte; eles já são a medida de "o enquadramento pedido não cabe na imagem".

Complementarmente, `FullFrontal.Compliance` tem `goodVerticalFacePosition()`, `horizontallyCenteredFace()`, `widthOfHead()`, `lengthOfHead()` — reprovam cabeça mal posicionada ou grande demais, o que cobre o caso de raspão, mas não é "cortado".

## Limitação que vale destacar

Você pediu "antes dele ser indetectável". `Characteristics` exige uma `AnnotatedImage` (rosto + olhos já localizados), então **faceMaskScore, chin/crown/ears e todos os testes ISO só existem depois que rosto e olhos foram detectados**. Os únicos sinais disponíveis antes disso são `Face.Location.confidence` e, em seguida, as confianças dos olhos — que são justamente os dois campos que o fluxo atual descarta.

Não mexi em nada disso; é só o levantamento.