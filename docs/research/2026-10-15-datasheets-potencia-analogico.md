# Auditoria local: potência e condicionamento analógico

Data: 15/10/2026. Escopo: pesquisa documental e cálculos; nenhuma alteração de firmware, BOM, esquema ou guia de montagem. Páginas abaixo são as páginas do PDF, contadas desde 1. Parâmetros calculados são nominais; não constituem ensaio de bancada.

## Fontes e limites da evidência

| ID | Fonte primária local e localização relevante |
| --- | --- |
| T | [TP4056.pdf](../../uploads/TP4056.pdf), NanJing Top Power ASIC, p.1 Description/Features; p.2 descrição dos pinos e Electrical Characteristics; p.3 indicator light state, Rprog Current Setting e Typical Applications |
| M | [MT3608-Datasheet.pdf](../../uploads/MT3608-Datasheet.pdf), Aerosemi, versão V1.0: p.1 Features/Description; p.2 limites e pinos; p.3 Electrical Characteristics; p.4 Operation; p.5 curvas típicas; p.6 Application Information |
| O | [OPA2364.PDF](../../uploads/OPA2364.PDF), TI SBOS259B: p.2 limites e variantes; p.3 Pin Configurations; p.4 Electrical Characteristics; p.7 curvas de overshoot; p.8 Operating Voltage; p.9 Capacitive Load/Input and ESD Protection; p.11 Active Filtering |
| I | [ina333.pdf](../../uploads/ina333.pdf), TI SBOS445C: p.5-6 Electrical Characteristics; p.13 seção 7.4.2; p.14 seção 8.2; p.15 seção 8.2.2.1 |
| S | [ADS1256+STM32F103数据采集板原理图.pdf](../../uploads/ADS1256+STM32F103数据采集板原理图.pdf), desenho original datado 12/09/2016, p.1: quadrante superior esquerdo alimentação; inferior esquerdo ADC/filtros; inferior direito MCU/headers |

O esquema S é fonte primária do circuito desenhado, mas não prova a revisão da PCB comprada nem especificações internas dos CIs. O arquivo Esquema_Eletrico_Completo_ADS1256_STM32F103C8T6.pdf é documentação derivada; os artigos chineses de aplicação do ADS1256 não substituem o datasheet TI. O arquivo cartao_micro_SD.pdf começa com descrição de anúncio e não identifica fabricante/revisão: não foi usado como garantia elétrica do módulo.

Não foram encontrados no acervo local datasheets primários de ADS1256, ADR4525, AMS1117/ASM1117, STM32F103, da célula 18650 específica, circuito protetor, CJMCU-333 completo, cartão SD específico ou módulo carregador completo. Por isso não se certificam aqui: limites de entrada/BUFEN do ADS1256, correntes de injeção do STM32, margem de dropout/ESR/corrente contínua do LDO, capacidade e corrente admissível da célula/proteção, nem orçamento de consumo completo. Não houve consulta à internet.

## Achados classificados

| Grau | Achado | Consequência e encaminhamento |
| --- | --- | --- |
| Crítico, condicional à montagem | O TP4056 documentado é carregador 1S de 4,2 V; o circuito de aplicação não contém proteção da saída contra descarga profunda/curto nem um caminho dedicado de alimentação da carga (T p.1-3). | Não autorizar a bateria somente pela inscrição TP4056. Identificar célula/protetor, B+/B-/OUT+/OUT-, limites e polaridade. Manter a regra atual de carregar com o aparelho desligado enquanto não existir power-path comprovado. |
| Importante | O divisor de bateria atualmente descrito toma OUT+ antes da chave. Com a chave aberta ele continua aplicando aproximadamente metade de VBAT a PB0, enquanto VDD pode estar em 0 V. | Há risco de injeção/back-power; admissibilidade e corrente máxima não verificadas por falta de datasheet STM32. Candidata mínima: tomar o topo do divisor na saída da chave, antes do boost; medir transientes de desligamento e carga USB. Depois do boost a medição seria dos 5 V, perdendo a informação de descarga da célula. |
| Importante | Open-drain em CHRG/STDBY não garante interface direta segura com GPIO de 3,3 V: LEDs/resistores podem vir de VCC=5 V (T p.3). | Inspecionar módulo e interface elétrica; validar pinos com MCU ligado e desligado antes de selecionar GPIO/interface. |
| Importante | Não há especificação local do AMS1117 concreto nem da célula/proteção/SD/LCD completos. | Corrente, estabilidade do 3,3 V, temperatura e autonomia permanecem hipóteses até medição e obtenção das fontes faltantes. |
| Importante | O filtro 2,15 k/220 n/100 n tem f0=499,08 Hz e Q=0,74162; seu ponto de -3 dB é 522,26 Hz. | Não confundir frequência natural com corte exato nem chamar de Butterworth. Banda final inclui INA, ADC, multiplexação e filtros digitais. |
| Importante | IN7 é monitor separado, com buffer/divisor e leitura sequencial; não é a referência física do ADC. | Correção pode compensar variação lenta da excitação; não garante rejeição de ripple, deriva de bias ou transientes entre conversões. |
| Menor | A capa de O menciona 500 µV/750 µA, mas a variante AI e a tensão utilizada exigem ler a tabela. | OPA2364AI tem offset até 2,5 mV; a tabela apresenta 1,1 mA típico/1,4 mA máximo por amplificador a 5,5 V, não 750 µA em qualquer alimentação (O p.4). |

## 1. Célula, carga e conversão

### 1.1 O que o TP4056 permite afirmar

O CI é carregador linear CC/CV para uma célula, com tensão nominal 4,2 V, corrente programável até 1 A, redução térmica e término C/10 após alcançar a tensão final (T p.1). A tabela de p.2 dá tensão de float 4,137-4,263 V nas condições declaradas, corrente de carga 950-1050 mA com RPROG=1,2 k e VBAT=4,0 V, e limiar de pré-carga 2,8-3,0 V. A compatibilidade dessa tensão/corrente com a célula adquirida não foi comprovada.

T p.3 associa RPROG=1,2 k a 1000 mA; 3 k a 400 mA; 5 k a 250 mA. O próprio PDF contém inconsistência: o texto da p.2 menciona 2 V no PROG, enquanto a equação da mesma página explicita VPROG=1 V. Não resolver isso por suposição: identificar revisão do CI/resistor real e medir a corrente. Dissipação linear aproximada, supondo 5 V/1 A e bateria em 3,7 V: P=(5-3,7)*1=1,3 W. Não significa corrente de 1 A sustentável em qualquer módulo; a regulação térmica pode reduzi-la (T p.1-2).

TEMP é o pino 1; aterrando-o desativa-se a supervisão térmica da bateria (T p.2). Portanto a proteção térmica do die não prova que a célula tenha monitoramento por NTC. Sem foto/esquema do módulo não se sabe se o recurso está presente.

Se o sistema consome corrente no mesmo nó BAT durante carga, a corrente fornecida pelo carregador inclui corrente da carga. Por conservação de corrente, essa carga interfere na interpretação de C/10 e pode impedir a terminação. Essa é uma dedução do circuito de T p.3, não uma certificação do comportamento do módulo comprado. Não há no PDF um pino SYS nem circuito de gerenciamento dedicado da carga.

### 1.2 Detectar carregando e carga terminada

| Pino CI | Sinal | Carregando | Terminação | Outros estados |
| --- | --- | --- | --- | --- |
| 7 | /CHRG | Baixo | Alta impedância | Ver tabela T p.3 |
| 6 | /STDBY | Alta impedância | Baixo | Ver tabela T p.3 |

As saídas são open-drain, ativas baixas (T p.2). T p.3 mostra vermelho ligado durante carga e verde ligado na terminação; ambos apagados podem indicar VIN insuficiente, temperatura fora da janela ou bateria ausente. Sem bateria, com 10 µF em BAT, o PDF descreve verde ligado e vermelho piscando em 1-4 s. Logo dois bits não são uma medição de estado de carga nem uma confirmação universal de bateria conectada. O rótulo prudente é "carga terminada pelo carregador", condicionado à presença de alimentação/bateria verificadas.

Possibilidade prática: expor CHRG e STDBY do módulo e ler por interface compatível com 3,3 V e com o estado OFF. Antes, conferir se há pads acessíveis, de onde vêm os resistores dos LEDs, tensão em alta impedância, referência de terra e estados com USB presente/ausente e proteção aberta. O PDF de três páginas não especifica claramente os limites de tensão/corrente desses dois drenos: não dimensionar uma conexão direta certificada a partir dele. Nenhuma interface/GPIO foi adicionada nesta pesquisa.

### 1.3 MT3608 e 5 V

M p.1/3 informa entrada operacional 2-24 V, comutação nominal 1,2 MHz, limite interno de switch de 4 A e eficiência de até 97%; M p.3 identifica o limite de corrente como garantido por projeto, não testado, nas condições da tabela. O título "2 A" não garante 2 A contínuos em 5 V para qualquer módulo, indutor, diodo, tensão da célula ou temperatura. Em carga leve há mudança automática para PFM (M p.1/3), portanto ruído não se resume a uma única senoide de 1,2 MHz.

M p.6 recomenda indutor de 4,7-22 µH, baixo DCR e corrente de saturação adequada; capacitores cerâmicos de entrada/saída de 22 µF, baixo ESR, X5R/X7R; diodo Schottky com tensão reversa superior à saída; caminhos curtos/largos, desacoplamento próximo, nó SW pequeno e feedback afastado do ruído. Componentes efetivos do módulo devem ser identificados; os capacitores extras sugeridos no projeto não substituem a conferência dessas condições.

O esquema S mostra entrada direta J2 no nó anterior a L1=6,8 µH, C1=47 µF e C2=100 nF após L1, e U3/3,3 V alimentado antes de L1. A frequência natural LC ideal é:

```text
fLC = 1 / (2*pi*sqrt(6,8e-6 * 47e-6)) = 8902,60 Hz
```

Não é 280 Hz. Não inferir ripple residual pela conta: ESR, DCR, ressonância, impedância das cargas e caminhos de terra afetam a resposta. Retirar VEXC de C1+ e não conectar ali outro fio do boost bruto; isso contornaria L1. O LDO 3,3 V recebe o nó bruto, portanto transientes do SD/MCU podem compartilhar caminhos de potência com o boost (S p.1, bloco alimentação). Não há medida de ripple no acervo.

Ensaios necessários: 5 V bruto, C1+/VEXC e 3,3 V com ponta curta, em bateria alta/baixa; ligar/apagar backlight, atualizar tela, escrever SD e transmitir UART; registrar ripple, undershoot, recuperação, aquecimento, consumo médio e picos. Repetir ADC com entradas conhecidas para verificar ruído referido à entrada. A estabilidade/precisão final de 5 V permanece não verificada.

### 1.4 AMS1117 e corrente disponível

S p.1 desenha U2 como regulador 5,0 V e U3 como 3,3 V, com pinos 3=IN, 1=GND, 2/4=OUT. A grafia no desenho é ASM1117. Isso não identifica o fabricante real nem autoriza usar especificações lembradas de outro 1117. Sem seu datasheet primário não se certificam dropout, estabilidade/ESR, proteção reversa, corrente máxima útil ou dissipação admissível.

Conta térmica para qualquer LDO linear de 5 para 3,3 V, ignorando sua corrente própria: P=(5-3,3)*I. Em I=0,10/0,20/0,30 A resulta 0,17/0,34/0,51 W. Temperatura depende da PCB e ambiente, não apenas do nome do CI. Um LDO externo dedicado ao SD exige separar sua saída de VDD; não colocar duas saídas reguladas em paralelo sem arquitetura especificada. Não foi comprovada necessidade/capacidade de tal alternativa.

### 1.5 Orçamento de corrente e autonomia por capacidade

Dados demonstráveis por cálculo de circuito e tabela de CI:

```text
Por ponte: I = 5/(350+350) + 5/(1000+1000) = 9,642857 mA
Seis pontes: 57,857143 mA; P = 0,289286 W
Alternativa toda 350 ohms: 6*[5/700 + 5/700] = 85,714286 mA
Economia com referência 1k/1k: 27,857143 mA = 0,139286 W
Um gauge 350 ohms equilibrado: (5/700)^2*350 = 17,857143 mW
Divisor monitor 2k2+2k2: 5/4400 = 1,136364 mA
Seis INA333: 6*50 uA = 0,300 mA típico do CI, sem circuito CJ (I p.6)
Quatro OPA duais = oito amplificadores: 8*1,1 = 8,8 mA típico a 5,5 V;
  8*1,4 = 11,2 mA máximo nas condições de O p.4.
```

O oitavo amplificador, mesmo sem uso de sinal, consome corrente: OPA2364 não oferece o shutdown dos OPAx363 (O p.3-4/9). Os 8,8/11,2 mA são dados em 5,5 V e não uma especificação garantida transposta para 5,0 V. Não somar essas parcelas e declarar corrente total: faltam consumo/picos do ADS1256, MCU, LCD/backlight, SD, ADR4525, LEDs, reguladores e circuitos dos CJ. A corrente de saída também depende dos níveis dos sinais e cargas.

Para tornar a autonomia reproduzível, usa-se um **cenário**, sem capacidade real ou eficiência medidas: Vnom=3,7 V, energia aproveitável da célula=80%, eficiência média do boost=85%, carga média total I5=0,22-0,32 A. A faixa de corrente é hipótese de orçamento já presente no projeto, não valor obtido dos datasheets. Os 80% incorporam corte, queda sob carga e margem de capacidade; também são hipótese.

```text
E_nominal[Wh] = Vnom * C_mAh/1000
E_carga[Wh] = E_nominal * 0,80 * 0,85
t[h] = E_carga / (5 * I5)
Ibat[A] aproximada = 5*I5/(Vbat*0,85)
```

| Capacidade hipotética | Energia para carga no cenário | Autonomia a 0,32 A / 5 V | Autonomia a 0,22 A / 5 V |
| --- | --- | --- | --- |
| 2000 mAh | 5,032 Wh | 3,15 h | 4,57 h |
| 2600 mAh | 6,542 Wh | 4,09 h | 5,95 h |
| 3000 mAh | 7,548 Wh | 4,72 h | 6,86 h |
| 3500 mAh | 8,806 Wh | 5,50 h | 8,01 h |

A 3,7 V o cenário implica Ibat=0,350-0,509 A; a 3,35 V, 0,386-0,562 A, antes dos picos. Isso serve para planejar medições e confrontar com os limites reais da célula/proteção/boost; não certifica esses limites. A corrente de carga TP4056 e a capacidade em mAh da célula não são diretamente a corrente/tempo da carga em 5 V. Não há base primária local para prometer 8-10 h.

## 2. IN7, divisor e correção ratiométrica

S p.1 mostra P5 IN7 no pino 15, AGND no 16; IN7 entra em R27=100 ohms, CH7 tem R29=10 k para AGND e C35=1 nF; C32=10 nF liga CH6 a CH7. AINCOM é ligado a AGND por R16=0; a referência desenhada é ADR4525, não VEXC. O desenho não prova precisão/limites do ADS ou da referência.

Para a montagem bufferizada documentada: VEXC -> 2,2 k -> MEX -> 2,2 k -> AGND; MEX com 100 nF; seguidor OPA -> P5 IN7. Desprezando offsets/impedância de saída:

```text
k_onboard = 10000/(100+10000) = 0,9900990099
k_monitor = 0,5 * k_onboard = 0,4950495050
VADC7 em VEXC=5 = 2,475247525 V
VEXC_estimado = VADC7 / k_monitor
Rth_MEX = 2200 || 2200 = 1100 ohms
fc_MEX = 1/(2*pi*1100*100e-9) = 1446,86 Hz
```

Sem buffer, a carga 100+10 k participa do divisor; pela equivalência de Thévenin do divisor:

```text
k_monitor_passivo = 0,5 * 10000/(1100+100+10000)
                  = 0,4464285714
```

O valor legado 0,4504 não corresponde a essa rede nominal completa. A constante ativa `fw_pio/include/app_cfg.h` é `0.5f/1.01f`, coerente com buffer ideal. Calibrar o fator real inclui tolerâncias dos 2k2, 100/10k, offset OPA, referência e ADC; não basta calibrar o divisor isolado. C32 cria acoplamento entre IN6/IN7 em AC: IN6 não é eletricamente invisível, mesmo sem sensor.

Forma analítica de uma correção lenta:

```text
Vout_i = B_i + G_i * VEXC * r_i
r_i_estimado = (Vout_i - B_i)/(G_i * VEXC_estimado)
delta_r = r_i_estimado - r_i_zero
quarto de ponte, pequena deformação: epsilon ~= 4*delta_r/GF
meia ponte de flexão ideal: epsilon ~= 2*delta_r/GF
```

Aqui B_i inclui bias de saída e offsets relevantes; deve corresponder à calibração real. O firmware ativo faz essa subtração antes da normalização em `sg_measure.c:sg_bridge_ratio`; `app.c:app_measure_bias` mede o bias e `app_zero` mede a razão de zero. Isso compensa ganho de excitação lento sob hipóteses estáveis, mas não elimina deriva de B_i, tolerâncias/deriva de ganho, ruído comum que chega por outros caminhos ou erro temporal da varredura.

Exemplo calculado: 1 mV de mudança de bias, G=100, VEXC=5 V e GF=2 equivale a aproximadamente 4 microstrain em quarto de ponte. O offset de um OPA2364AI pode atingir 2,5 mV (O p.4), portanto ZERO e calibração reais importam. Offset constante pode ser removido no zero em condições constantes; a constância não é garantida ao variar alimentação/temperatura.

A divisão entre dois códigos do mesmo ADC pode cancelar uma referência comum multiplicativa; entretanto subtrair um bias armazenado em volts, medido em outra condição, impede afirmar cancelamento universal de deriva da referência. IN7 não amostra simultaneamente os seis sinais. Não extrapolar correção de nível médio para eliminação do ripple de 1,2 MHz ou componentes PFM. Não verificar a janela bufferizada do ADS1256 com o fundo de escala nominal: o datasheet primário desse CI está ausente; os limites citados nos guias atuais precisam dessa fonte e ensaio.

## 3. Sallen-Key de dois polos e OPA2364

Topologia analisada: INA OUT -> R1 -> X -> R2 -> Y -> OPA +IN; C2 de Y para AGND; C1 de X para OPA OUT; OPA -IN unido à própria saída; saída no P5. R1=R2=2150 ohms, C1=220 nF, C2=100 nF. Não tomar sinal do nó Y: ele é a entrada de alta impedância do seguidor, e a ligação de C1 à saída realiza a realimentação do filtro.

Derivação por análise nodal para seguidor ideal e sem carga na entrada:

```text
H(s) = 1 / [1 + s*C2*(R1+R2) + s^2*R1*R2*C1*C2]
omega0 = 1/sqrt(R1*R2*C1*C2)
f0 = omega0/(2*pi) = 499,080190 Hz
Q = sqrt(R1*R2*C1*C2)/[C2*(R1+R2)] = 0,741619849
|H(f)| = 1/sqrt[(1-(f/f0)^2)^2 + ((f/f0)/Q)^2]
f_-3dB = 522,256554 Hz
```

| Frequência | Ganho ideal do filtro | Atenuação ideal |
| --- | --- | --- |
| 80 Hz | 1,002012 | +0,01746 dB |
| 100 Hz | 1,002856 | +0,02477 dB |
| 200 Hz | 1,001709 | +0,01483 dB |
| 260 Hz | 0,988061 | -0,10432 dB |
| 500 Hz | 0,740253 | -2,61240 dB |
| 1000 Hz | 0,247020 | -12,14535 dB |

É um filtro nominal próximo de 500 Hz, com Q acima de 1/sqrt(2); não é Butterworth exato. Os números não incluem a carga onboard de ganho DC 10000/10100, o INA, tolerâncias/dielétricos dos capacitores, GBW/offset do OPA, acoplamentos onboard ou filtro digital ADC/firmware. Capacitores de valores iguais a esses números, porém com forte dependência de tensão/temperatura, mudam a resposta. Não declarar rejeição RF de centenas de dB extrapolando o modelo ideal: parasitas e GBW dominam fora da banda útil.

O p.8 confirma estabilidade em ganho unitário e desacoplamento 100 nF perto dos pinos; p.4 dá GBW típico 7 MHz e slew típico 5 V/us. Para comparação, uma senoide de 500 Hz e 2 V de pico exige 2*pi*500*2=0,006283 V/us: isso indica folga nominal de slew, sem validar carga/ruído/layout. O p.11 apresenta aplicação Sallen-Key, mas não este circuito/valores exatos; estes resultados são cálculos próprios.

O p.9 adverte que ganho unitário é sensível à carga capacitiva; menciona bom comportamento com carga puramente capacitiva até aproximadamente 1000 pF e sugere resistor série de 10-20 ohms em certas cargas. A rede real tem capacitores de 1 nF e 10 nF entre pares após 100 ohms; não é o teste simples de uma carga puramente capacitiva. Verificar estabilidade/overshoot real com osciloscópio; não instalar automaticamente resistores que alterem o ganho DC e a calibração.

INA333 acrescenta limites próprios: ganho 1+100k/RG (I p.15), banda típica 3,5 kHz em G=100 e 350 Hz em G=1000 (I p.6). Não usar o filtro de 500 Hz como prova de banda útil independente do ganho. I p.13, seção 7.4.2 explica que a janela de modo comum depende também da saída e alimentação e que sobrecarga pode produzir saída aparentemente normal. VCC=5 V no CJ não prova alimentação de 5 V no CI: conferir o módulo real.

## 4. Pinagens e limites críticos

| Elemento | Evidência verificada | Limite de interpretação |
| --- | --- | --- |
| OPA2364 SO-8 | 1 OUT_A, 2 -IN_A, 3 +IN_A, 4 V-, 5 +IN_B, 6 -IN_B, 7 OUT_B, 8 V+ (O p.3) | Conferir vista superior/pino 1; não confundir com o OPA364 simples ou OPA2363 com enable. |
| OPA2364 alimentação/entrada | Operação 1,8-5,5 V; máximo absoluto alimentação 5,5 V; entradas -0,5 a V++0,5 V e corrente limitada a 10 mA fora dos rails (O p.2/4/8/9) | Máximo absoluto não é faixa operacional recomendada. Saída perto de rail depende da carga; 10/20 mV de O p.4 usam RL=10k e condições declaradas. |
| OPA2364AI | Offset típico 1 mV/máximo 2,5 mV; 17 nV/sqrt(Hz) em 10 kHz e 10 µVpp em 0,1-10 Hz (O p.4) | Não usar o dado de 10 kHz como ruído branco garantido em toda banda; OPA da variante AI não tem máximo de offset 500 µV. |
| TP4056 | 1 TEMP, 2 PROG, 3 GND, 4 VCC, 5 BAT, 6 /STDBY, 7 /CHRG, 8 CE (T p.2-3) | Pinos do CI não são pads do módulo; OUT protegido não é um pino do TP4056. |
| MT3608 SOT23-6 | 1 SW, 2 GND, 3 FB, 4 EN, 5 IN, 6 NC (M p.2) | SW não é OUT. Identificar pinagem/pads físicos do módulo separadamente. |
| MT3608 | Entrada operação 2-24 V; FB nominal 0,6 V; EN alto >=1,5 V, baixo <=0,4 V; UVLO típico 1,98 V (M p.3) | UVLO do boost não substitui proteção da célula; não usar 1,98 V como limite seguro de descarga 18650. |
| Placa J2/P5 | J2-1 GND/J2-2 entrada direta; P5 ímpares IN0..IN7, pares AGND; R1=0R une terras; R16=0R põe AINCOM em AGND (S p.1) | Confirmar continuidade/revisão. P5 não é entrada diferencial direta de ponte sem condicionamento. |
| ADC/MCU | ADS DRDY21 -> PB10; RESET15 -> PB11; CS20 -> PB12; SCLK24 -> PB13; DOUT22 -> PB14; DIN23 -> PB15 (S p.1, bloco inferior) | Números dos pinos ADS no símbolo, não posições de um header externo. |

O símbolo S mostra SYNC/PDWN no pino 14 ligado a VDD, AINCOM no 5, AIN7 no 13, VREFP no 4 e VREFN no 3. Essas conexões foram verificadas no desenho; sua admissibilidade elétrica depende dos datasheets faltantes e da revisão física. P9-12 é VBAT do STM32, não terra nem uma entrada genérica de medição da célula.

## 5. Inspeções físicas e medições pendentes

1. Identificar célula: fabricante/modelo, capacidade medida, estado, limites de carga/descarga, proteção incorporada; identificar CI protetor/MOSFETs e verificar se usar OUT- preserva a proteção.
2. Carregador: foto frente/verso, resistência PROG, TEMP/NTC, disponibilidade de CHRG/STDBY, circuito dos LEDs, pads USB/BAT/OUT, eventual power-path. Não soldar fios em pads desconhecidos por semelhança de anúncio.
3. Monitor PB0: confirmar nó acima da chave, reduzir risco OFF com nó comutado antes do boost ou isolamento adequado; medir VDD/PB0 nos quatro estados bateria/USB e durante desligamento. Datasheet STM32 faltante impede certificar injeção admissível.
4. Boost: verificar CI, indutor/saturação, diodo, capacitores e polaridade, ajuste de 5,00 V, picos e temperatura; inspecionar fios que possam bypassar L1.
5. Placa: confirmar J2, C1+, AGND/GND, LDO real e capacitores/ESR; medir VEXC, 3,3 V e referência durante SD/backlight; investigar alimentação reversa com J2 direto enquanto U2 fica sem entrada.
6. CJ/OPA: identificar revisão/pinos do CJ, medir rails e bias no modo comum real, ganho por canal e margem de saída; conferir OPA AI versus I, orientação e desacoplamento; terminar amplificador livre sem paralelizar saídas.
7. IN7/filtro: medir fator monitor completo em pontos de VEXC; obter datasheet ADS1256 e conferir BUFEN/entrada/referência; varrer amplitude/fase, settling, acoplamento CH6/7 e alias nas taxas reais.
8. Autonomia: medir energia/corrente reais, picos de SD, limiar da proteção e tempo de fechamento do arquivo com bateria baixa; só então preencher o orçamento e declarar duração operacional.

Não foram executados ensaios elétricos, de descarga, térmicos ou de firmware. O risco remanescente principal é tratar limites de módulo/célula desconhecidos e cálculos nominais como uma montagem eletricamente validada.
