# Auditoria documental da BOM e das ligações SG6

Data: 15/10/2026. Escopo somente leitura: README, guias atuais, especificação aprovada, três SVGs, BOM XLSX e manual final de 32 páginas. Nenhum arquivo de produto, esquema, planilha ou manual foi alterado. Páginas abaixo são as páginas físicas do PDF. Células pertencem à única aba `BOM_FINAL`.

## Parecer

As quantidades eletrônicas da montagem inicial mista fecham entre a BOM, os guias e o manual. A lista atual **não manda montar Rg externo de 10 kΩ, zener de 3,3 V ou diodos OR/1N5819**. Esses componentes estão corretamente segregados com necessário zero. A principal inconsistência literal é o cabo de três fios no esquema de sistema contra o cabo de quatro condutores mais dreno da BOM. Existe também um problema de sequência de alimentação no monitor de bateria que precisa ser resolvido antes da montagem: a ligação publicada mantém PB0 excitado com o instrumento OFF.

Não confundir coerência documental com liberação de fabricação: seleção comercial de conectores, chicotes, bateria/proteção e vários passivos continua expressamente pendente nos próprios arquivos.

## Achados e menor correção

### D1 — P1: divisor pré-chave mantém tensão em PB0 quando a placa está desligada

**Evidência:** `Projeto_Modulo_Portatil_StrainGauge/CONEXOES_SISTEMA.md:10` coloca a chave entre OUT+ protegido e boost. A linha 20 deriva o divisor de bateria diretamente do OUT+ protegido. `MONTAGEM_PASSO_A_PASSO.md:33`, `esquema/02_digital_power.svg:7,49` e `esquema/00_sistema.svg:47` repetem esses nós. Manual pp. 6, 11, 30 e 32. BOM `B19:I19`.

**Consequência dedutível da própria ligação:** com a célula conectada e a chave OFF, o topo do divisor continua energizado. Em circuito aberto o PB0 tende a 2,1 V para célula de 4,2 V, enquanto VDD/VDDA da placa desligada cai. O circuito não especifica isolamento do ADC para essa condição. Há risco de injeção e alimentação parasita durante OFF/carga; a análise de limites do STM32 e a medição da corrente em OFF são necessárias para quantificar o efeito. Não foi realizado ensaio físico e esta auditoria documental não certifica dano ao componente.

**Conferência primária adicional:** [ST DS5319 Rev20, STM32F103x8/xB](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf), tabela5 (PB0/ADC12_IN8 não é marcado FT) e seção5.3.12/tabela35, orienta evitar injeção em operação normal por tensão acima de VDD em I/O padrão. Os ensaios de robustez publicados não autorizam alimentar propositalmente um pino com o MCU desligado. A corrente e o efeito concretos continuam dependentes da placa.

**Menor correção proposta:** derivar o resistor superior do divisor do lado comutado da chave, ainda antes do boost, preservando a razão 10k/10k e o ADC PB0. Atualizar a identificação do nó nos dois SVGs, conexões, especificação, nota da BOM e PDF. Verificar partida/desligamento e descarga do capacitor de 100 nF; se a correção de sequência requerer isolamento adicional, defini-lo explicitamente. Não ligar um novo clamp genérico de 3,3 V como remendo.

### D2 — P2: compra do cabo fica contraditória entre desenho e lista

**Evidência:** `esquema/00_sistema.svg:6` e manual p. 30 dizem `3 fios + dreno`. `CONEXOES_SISTEMA.md:24`, `MONTAGEM_PASSO_A_PASSO.md:9`, BOM `B38:I38` e manual pp. 5, 11 e 28 especificam **4 condutores + dreno**, com quarto condutor isolado nas duas pontas.

Três condutores são eletricamente usados; a quarta via é reserva. Portanto não há curto ou erro de ponte intrínseco nessa legenda, mas há duas especificações comerciais de cabo.

**Menor correção proposta:** trocar apenas a legenda do SVG00 por `cabo 4 condutores + dreno; 3 ativos, 1 isolado`, e regenerar sua figura no manual. Não alterar W1/W2/W3 nem usar o dreno como quarta via.

### D3 — P2: seis pares de conectores não estão distinguidos de seis peças

**Evidência:** BOM `B37:C37` e manual p. 28 pedem **6 pares** de conector de cinco contatos; `MONTAGEM_PASSO_A_PASSO.md:9` e manual p. 5 dizem apenas `seis conectores5contatos`. Não há família, código, sexo, orientação da vista de numeração, sistema de fixação ou bitola admitida. As linhas 24 de CONEXOES e 50 de MONTAGEM dão somente a função de cada contato.

**Efeito:** comprar seis peças individuais pode deixar metade da conexão faltando. A numeração funcional 1..5 não identifica a vista frontal/de solda de um modelo comercial. Isso pode inverter contatos mesmo obedecendo à tabela.

**Menor correção proposta:** escrever `6 conjuntos completos, cada um com conector de painel e correspondente de cabo (12 metades ao todo)` no passo de materiais. Após escolher o modelo com as dimensões físicas, registrar referência comercial e vista do fabricante junto à tabela 1..5. Não inferir o pinout pela fotografia.

### D4 — P2: a BOM é suficiente como levantamento funcional, mas o kit de montagem não está fechado

**Evidência:** BOM `B14:I14` identifica seis seletores JP_QH sem definir interruptor SPDT ou header de três pinos com jumper; `B38:I38` traz seis trechos de cabo sem bitola/modelo/comprimento individual; `B42:I42` agrega fiação, headers, espaçadores e prensa-cabos em **um conjunto**. `B40:I40` agrega gabinete e chave. `MONTAGEM_PASSO_A_PASSO.md:10,24` condiciona a fabricação às medidas reais e ao gabarito. Manual pp. 5 e 28.

**Efeito:** não se consegue reconciliar um carrinho de compra com número de terminais, cabos internos, suportes e peças mecânicas. Isso é uma pendência admitida, não prova de que tais itens foram esquecidos fisicamente.

**Menor correção proposta:** concluir somente os itens usados: seis seletores completos (se forem headers, incluir seis jumpers), seis conjuntos de conectores, seis trechos de cabo até 1 m, seis alívios de tração conforme solução mecânica, chicote da tela com 13 ligações, chicote SD com seis ligações, chicote UART com duas e chicote SWD com três ou quatro. São números de redes/terminações, não comprimentos ou peças comerciais já aprovados. Registrar as bitolas, comprimentos e quantidades reais após o gabarito. Não acrescentar uma arquitetura de conectores universal.

### D5 — P2: proteção de bateria é requisito aberto, sem diagrama da variante final

**Evidência:** BOM `B34:I36` confirma o carregador TP4056, mas não a proteção real nem célula. `CONEXOES_SISTEMA.md:8-11`, `MONTAGEM_PASSO_A_PASSO.md:9,33` e SVG02 dizem célula em B+/B- e saída OUT protegida, condicionando a proteção à conferência. Manual pp. 5, 11 e 28.

O texto corretamente avisa que TP4056 sozinho não é proteção. Contudo, para uma placa de carregador sem proteção e uma proteção 1S separada, ainda falta identificar a variante e mapear os terminais concretos célula/carregador/carga. Um TP4056 sem OUT+/- não se torna o módulo desenhado por equivalência de nome.

**Menor correção proposta:** identificar a revisão real do módulo e publicar uma única ligação pelos seus terminais reais; se a proteção for separada, incluir seus B/P ou equivalentes e o carregador conforme seu fabricante. Conferir corrente programada de carga contra a célula escolhida e capacidade dos fios/chave/boost. Manter a condição já escrita de carregar com aparelho OFF. Sem essa identificação, a linha da BOM continua pendente, não pronta para compra/montagem.

### D6 — P3: características de compra dos passivos novos ainda não estão determinadas

**Evidência:** BOM `B15:I17` traz 2,15 kΩ, 220 nF e 100 nF sem tolerância/dielétrico/tensão de trabalho numéricos; as notas mandam conferir o lote. `B12:I13` pede 0,1% e TCR compatível, sem identificar lote/modelo ou limite de deriva admissível. `B22:I27` dá quantidades e algumas faixas nominais, ainda com adequação física pendente. Manual pp. 5 e 27. CONEXOES linhas 18, 26 e 45.

**Efeito:** os valores nominais calculam 499,1 Hz/Q0,742, mas uma compra apenas por capacitância não controla a frequência/temperatura da realização. Não há divergência de quantidade ou comprovação de que o estoque seja inadequado.

**Menor correção proposta:** preencher tolerância, dielétrico, tensão e lote efetivamente aceitos após inspeção, casando os pares de referência e validando a resposta do filtro. Não renomear os componentes já comprados como precisão ou baixo TCR sem identificação. Para os 10 kΩ antigos, declarar explicitamente quais cinco peças serão reaproveitadas (duas do monitor de bateria e três pull-ups), se forem adequadas; não montar seis resistores de ganho externos por causa da linha histórica.

## Reconciliação das quantidades

| Item físico da montagem inicial | Necessário e células da BOM | Guia/manual/esquema | Resultado |
| --- | --- | --- | --- |
| Placa ADS1256 + STM32 | 1, `C7` | README; manual pp. 3,27 | Coerente; Blue Pill avulsa fica reserva |
| Gauges de 350 Ω | 9, `C8` | 3 pares H CH0..2 + 3 Q CH3..5; SVG00/01; manual pp. 3,12,27 | Coerente; nove sensores físicos, seis entradas ADC |
| CJMCU-333 | 6, `C9`, confirmado `E9=6` | Um por entrada; manual pp. 12,27 | Coerente; ajuste interno de ganho |
| OPA2364AIDR SOIC-8 | 4, `C10`; `E10=12`, `G10=8` | 3 duais para filtros + 1 dual para monitor; manual pp. 12,18,27 | Coerente; meia seção livre termina em seguidor de MEX |
| Adaptadores SOIC-8 | 4, `C11` | Guia fabricação; manual pp. 5,27 | Coerente; estoque pendente |
| Referência RC/RD 1 kΩ/0,1% | 12, `C12` | Dois por canal, todos os seis; manual pp. 11,27 | Coerente; pares independentes |
| RA 350 Ω/0,1% | 6, `C13` | Um montado por seletor, três bypassados em H; manual pp. 11–12,27 | Coerente; não comprar 18 extras se usar referência 1k/1k |
| Seletores JP_QH | 6, `C14` | Centro W1, Q em RA, H em VEXC; manual pp. 12,27 | Quantidade coerente; variante/jumper pendente |
| Resistores de filtro 2,15 kΩ | 12, `C15` | Dois por filtro, seis filtros; SVG01; manual pp. 12,27 | Coerente |
| Capacitores de filtro 220 nF | 6, `C16` | C1 entre X e saída OPA; SVG01; manual pp. 12,27 | Coerente; não são capacitores para terra |
| Capacitores de filtro 100 nF | 6, `C17` | C2 entre Y e AGND; SVG01; manual pp. 12,27 | Coerente; Y somente na entrada + do OPA |
| Monitor VEXC 2,2 kΩ/0,1% | 2, `C18` | Divisor com buffer OPA4A, saída P5-15; manual pp. 12,27 | Coerente; 100 nF contado nos extras |
| Monitor bateria 10 kΩ | 2, `C19` | Divisor para PB0; manual pp. 11,27 | Quantidade coerente; corrigir sequência de alimentação D1 |
| Pull-ups CS 10 kΩ | 3, `C20` | LCD, touch e SD para 3,3 V; manual pp. 13,27 | Coerente; conferir os onboard sem contar duas vezes |
| Shunt 100 kΩ/0,1% | 1 reutilizável, `C21` | No gauge remoto Q ou inferior H; manual pp. 8,12,27 | Coerente; seis calibrações não exigem seis shunts simultâneos |
| Desacoplamento 100 nF + 10 µF | 10 de cada, `C22:C23` | Seis CJ + quatro OPAs; manual pp. 11,18,27 | Coerente; descontar os onboard após conferência física |
| 100 nF extras | 5, `C24` | VEXC monitor, bateria, cluster, boost, SD; manual pp. 11–12,27 | Coerente |
| Bulks cluster/boost/SD | 1 de cada, `C25:C27` | 47 µF; 100–470 µF/10 V; 47–100 µF; manual pp. 11,27–28 | Coerente; valores finais condicionados a ensaio |
| MSP2807 com XPT2046 | 1, `C28` | Touch integrado; SD da tela sem uso; manual pp. 17,28 | Coerente; não requer comprar XPT2046 separado |
| Módulo SD SPI 3,3 V e cartão SDHC | 1 de cada, `C29:C30` | Externo dedicado à gravação, SPI1 compartilhado; manual pp. 13,18,28 | Coerente; módulo não é barramento exclusivo |
| USB-UART e ST-Link | 1 de cada, `C31:C32` | UART dois fios; SWD sem alimentação pelo clone; manual pp. 19,28 | Coerente; USB-UART ainda pendente |
| MT3608/TP4056/proteção/célula | 1 de cada, `C33:C36` | SVG02 e guias; manual pp. 11,28,32 | Quantidade funcional coerente; proteção/célula abertas |
| Conectores e cabo | 6 pares, `C37`; 6 trechos, `C38` | Manual pp. 5,11,28,30 | D2/D3/D4 |
| AMS1117 externo/TJA1050 | Necessário 0, estoque 1 cada, `C43:G44` | Reserva; manual p. 28 | Coerente; nunca paralelizar LDOs |
| Rg 10 kΩ/zener 3,3 V/diodos OR | Necessário 0, `C45`; quantidade antiga pendente | Manual pp. 11,16,28 | Coerente; ausência de instalação é decisão explícita |

Totais derivados, sem incluir componentes já soldados nos módulos: **38 resistores discretos funcionais**, incluindo o shunt reutilizável e seis RA montados (três bypassados); **21 capacitores de 100 nF** (6 filtro + 10 desacoplamento + 5 extras), **6 de 220 nF**, **10 de 10 µF**, **3 bulks**, totalizando **40 capacitores funcionais**. Os totais são requisitos nominais; não são declaração de compra adicional nem contagem física de estoque.

O shunt alternativo de 330 kΩ do manual p. 8 é opcional para ganhos altos. A BOM inicial de G100 não é contraditória por listar somente 100 kΩ. Se esse modo de calibração for adotado fisicamente, incluir a peça medida como opção e seu registro, sem mudar silenciosamente o padrão.

## Ligações cruzadas confirmadas

- **Energia:** J2-2 recebe boost 5 V e J2-1 o retorno. VEXC é retirado de C1+ após L1; nenhum guia atual manda curto entre esse nó e boost bruto. Pontes/CJ/OPA/monitor retornam em P5/AGND. LCD usa 5 V bruto/J1 aberto/IO 3,3 V. SD usa P9-1 VDD e P9-2 GND; P9-12 é VBAT, sem uso.
- **Canal Q:** VEXC → RA350 → W1 → A → gauge350 → B → W2 → AGND. A → W3 → VIN+. Referência própria 1k/1k → VIN-. W1/W3 juntam no sensor, sem junção local em VIN+.
- **Canal H:** VEXC → W1 → gauge superior → M → gauge inferior → W2 → AGND. M → W3 → VIN+. RA bypassado pelo JP_QH. Gauge inferior tracionado dá sinal positivo. Troca H/Q exige sensor/cabo/seletor/menu e recalibração.
- **Filtro:** CJ OUT → 2,15k → X → 2,15k → Y → OPA +IN. C1=220n de X a OUT, C2=100n de Y a AGND, -IN em OUT. P5 recebe OUT, nunca Y. SVG01 e manual p. 31 correspondem às instruções textuais.
- **P5:** filtros CH0..5 em pinos 1/3/5/7/9/11, AGND nos pares adjacentes; IN6 pino13 reserva; monitor em IN7 pino15/AGND16. Monitor bufferizado usa fator nominal 0,49505; não há autorização atual para usar divisor passivo antigo com essa constante.
- **SPI:** ADS usa SPI2 PB12..15, DRDY PB10 e RESET PB11. LCD/touch/SD externo compartilham SPI1 PA5/6/7. CS LCD PA4, touch PA0, SD PA8; IRQ PB1, RESET LCD PA2, DC PA3, LED PA1. LCD SDO9 aberto, slot SD da tela vazio. SD externo precisa liberar MISO em CS alto. Nenhum documento atual promete SPI exclusivo para o SD.
- **PC/programação:** PA9/P4-1 → RX do USB-UART e P4-3 → GND, TX/VCC do adaptador abertos. ST-Link clone4 → JTAG1-7, clone2 → JTAG1-9, reset opcional clone1 → JTAG1-15, terra em um par4..20. Saídas de energia do clone abertas. CAN/TJA1050 e MAX232/DB9 sem uso.

Fontes deste cruzamento: `CONEXOES_SISTEMA.md`, `PINOUT_PLACA_INTEGRADA.md`, SVG00/01/02 e manual pp. 11–19,30–32. As fontes originais de módulos não foram revalidadas nesta subauditoria; a correspondência constatada é entre os documentos correntes do projeto. A exceção é a consulta pontual do datasheet oficial STM32 para D1, identificada acima.

## Estoque, fórmulas, histórico e limites

As 39 linhas da BOM são registros funcionais, não 39 peças. Há 10 linhas com quantidade confirmada numérica e 29 com `Pendente`; 28 destas 29 têm necessidade positiva. O cadastro da célula/suporte não quantifica separadamente o suporte declarado; isso limita o estoque, sem alterar a necessidade de uma célula e um suporte. Não se pode calcular um total de peças a comprar usando pendente como zero.

Foram lidas as 78 fórmulas de falta/reserva, duas por linha, e seus valores armazenados. Não há célula de erro no XML da planilha. OPA: necessário4/confirmado12/falta0/reserva8; AMS1117 e TJA1050: necessário0/confirmado1/reserva1. Os pendentes preservam o rótulo, coerente com README. Não foi executado recálculo no Excel nem alterada a planilha.

`AUDITORIA_2026-10-13.md` e os cinco comparativos/decisões antigos têm aviso explícito de histórico na primeira linha. Neles ainda existem seis gauges, 18 resistores350, Rg10k e zener como propostas antigas. Por estarem marcados, não contam como requisitos atuais nem como contradições correntes. O PDF de auditoria anterior também é histórico; não deve ser entregue isoladamente como manual.

Método: extração direta do XLSX com ZIP/XML, leitura textual integral do manual final com pypdf e inspeção visual das páginas 27,28,30,31,32 renderizadas a partir do arquivo atual; leitura dos três SVGs e dos guias. Não foi avaliado todo o layout das outras páginas, não houve compra, medição, montagem ou teste de tensão/continuidade. Família dos conectores, revisão CJMCU, proteção/célula e desempenho de SD/rails permanecem dependências físicas.

Identificação dos artefatos auditados (SHA-256):

- BOM: `f4a3122922ecba90951fcc7fc00ab17fce3359bee470c7329cb791513bb70884`.
- Manual: `e5a4642d56b32b2db32cea7d325e713fcd620e0c8ccbbc46d7f2ba5e497172e3`.

Risco residual principal: a montagem não está liberada sem resolver a sequência OFF do divisor de bateria, identificar proteção real e conferir alimentação/modo comum do CJMCU concreto. A transparência de `Pendente` não substitui essas verificações.
