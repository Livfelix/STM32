# Conexões atuais SG6 - seis entradas, nove gauges

Usar as posições verificadas em `PINOUT_PLACA_INTEGRADA.md` e os esquemas00/01/02. Continuidade e revisão da PCB são pré-requisitos físicos. Ganho inicial100 em cada CJ, ADS PGA1, J3 aberto, BOOT0/1=0.

## Energia: seguir os nós, sem atalhos entre rails

| Origem | Destino | Condição |
| --- | --- | --- |
| Célula 18650 1S | B+/B- somente se identificados no módulo real | Conferir proteção, polaridade e diagrama do fabricante; TP4056 sozinho não protege |
| OUT+ protegido | Chave -> SW_BAT+ -> MT3608 IN+ | SW_BAT+ também alimenta o topo do divisor PB0; carga só OFF |
| OUT- protegido | MT3608 IN-/retorno | Não bypassar proteção pelo B- |
| MT3608 OUT+ | J2-2 e LCD1 | Regular5,00V antes, depois com carga |
| MT3608 OUT- | J2-1 e LCD2 | Retorno digital/potência |
| C1+ após L1 | VEXC front-end | Apenas retirada do nó filtrado; não boost direto |
| P5 AGND | AGND front-end | Retorno pontes/INA/OPA/monitor/dreno |
| P9-1 / P9-2 | SD3V3 / GND | Não P9-12; nunca5V no SD |

100nF+10uF perto de cada um dos6 CJ e4 OPA (conferir os já onboard);47uF+100nF no cluster;100-470uF/10V+100nF no boost;47-100uF+100nF no SD, valores iniciais a validar. LCD J1 aberto para5V, IO3,3V. Carregar somente com aparelho OFF. Deixar os LEDs do TP4056 visíveis na caixa, rotulados CARREGANDO e CARGA TERMINADA; CHRG/STDBY não vão ao STM32. A tela não indica carga com o MCU desligado. Identificar revisão do módulo e corrente PROG compatível com a célula/dissipação antes de ligar. Não há PowerPath nem latch de auto-OFF; baixa bateria manda parar/fechar, o usuário desliga a chave. Diodos OR/1N5819 e zener antigos não são acrescentados automaticamente; queda/fuga podem prejudicar rail e precisão. AMS1117 externo somente como alternativa3,3V para SD após ensaio; nunca paralelizar sua saída com VDD.

Monitor bateria: SW_BAT+ (pós-chave, antes do boost) ->10k -> BAT_DIV ->10k -> OUT- protegido/GND; BAT_DIV ->PB0/P8-11 e100nF para retorno. Não usar VBAT/P9-12 do STM32.4,2V da célula dá2,1V nominal no PB0. Calibrar relação e VDDA. O resistor superior não sai do OUT+ pré-chave nem dos 5V regulados. Em OFF, conferir PB0/VDD e descarga dos 100nF pelo resistor inferior. Aviso abaixo de 3,50V por 500ms; abaixo de 3,35V ou monitor inválido por 500ms durante REC solicita fechamento. Limiares sob carga dependem de bancada; não instalar zener3,3V como remendo.

## Ponte e cabo de cada canal

Conector sensor5contatos:1=W1 corrente/excitação;2=W2 retorno;3=W3 sense;4=reserva isolada;5=SHIELD/dreno. São seis conjuntos completos de cinco contatos: metade de painel + metade de cabo (12 metades). Escolher família após medir a caixa, com trava, alívio de tração, aceitação de 0,50 mm² e vista de numeração do fabricante. Usar seis trechos até 1 m do cabo instalado 1×4×0,50 mm², blindagem total + dreno. Em Q e H são três vias ativas e uma reserva; três condutores isolados + dreno seriam o mínimo elétrico. O cabo 1×2×0,50 mm² + dreno não atende. W1/W2 iguais em bitola/comprimento; W3 só sense; quarto fio isolado nas duas pontas. Dreno no SHIELD do equipamento em um ponto AGND, aberto no sensor; não é retorno, sense nem ligação à peça.

Um par RC=RD=1k/0,1% por canal: VEXC ->RC ->S- ->RD ->AGND; S- ->CJ VIN-. Medir/casar os pares, baixo TCR compatível, mesma proximidade térmica e distância do boost; não compartilhar um único par entre canais. Se usar alternativa350/350, recontar18 resistores350 montados nos seis canais (6RA+12referência), três RA bypassados na configuração mista; consumo aumenta. A montagem recomendada permanece1k/1k.

| Tipo | Ligação completa ramo sensor | JP_QH |
| --- | --- | --- |
| Q CH3..5 | VEXC ->RA350 ->W1 ->A ->gauge350 ->B ->W2 ->AGND; A ->W3 ->VIN+ | W1 ligado à saída de RA |
| H CH0..2 | VEXC ->W1 ->gauge superior350 ->M ->gauge inferior350 ->W2 ->AGND; M ->W3 ->VIN+ | W1 diretamente VEXC, RA bypassado |

JP_QH é seletor de três contatos no front-end: centro=W1, posiçãoQ=saídaRA350, posiçãoH=VEXC. Montar seis RA, mesmo os três bypassados. Não fechar simultaneamente H/Q. VIN+ não recebe outra conexão local ao nó após RA. Em Q, W1 e W3 terminam juntos no terminalA do gauge, no sensor. Em H, W1 termina no topo do par e W3 no meioM. **Trocar H/Q também exige reconfigurar fisicamente sensor/cabo, não só jumper e menu.** Aparelho desligado, tipo por canal atualizado, ZERO/shunt repetidos; ganho alterado exige bias também.

Gauge inferior H na face tracionada para sinal positivo; gauge superior na oposta. Com deformações opostas iguais, H indica a componente de flexão. Com axial uniforme ideal, H cancela aproximadamente; não interpretar como deformação total da face. Não substituir o gauge superior por um sensor arbitrário ou por configuração de Poisson.

Shunt manual100k em paralelo ao único gauge Q, entreA/B, ou ao gauge inferior H, entreM/terminal inferior, **no sensor**. Não entre VEXC/AGND do par inteiro nem entreW1/W3 no gabinete. Remover shunt e confirmar retorno antes de aceitar escala; o botão não instala resistor nem aciona relé. UI usa100k nominal; medir esse resistor e registrar desvio. Valor diferente exige rotina/calibração correspondente, não alterar apenas a etiqueta.

6 pontes com ramo sensor350+350 e referência1k+1k: corrente ideal total57,86mA em5V,85,71mA na alternativa350/350. Dissipação por gauge~17,86mW também em H: são nove fontes de aquecimento. Não aumentar excitação presumindo compensação térmica universal.

## CJMCU -> Sallen-Key -> ADC

CJ VCC=VEXC5V, GND=AGND, VIN+=W3, VIN-=S-, Vout=entradaR1. Vref é saída monitor nominal1,65V e3.3V saída auxiliar: ambos abertos, não unidos entre módulos. Nenhum RG externo; ajuste por trimpot e ganho informado100 por canal. Conferir alimentação efetiva do CI/mode comum~2,5V e excursões;5V no terminal não prova5V no INA.

Filtro por canal: Vout ->R1=2,15k ->X ->R2=2,15k ->Y ->OPA +IN. C2=100nF entreY/AGND; C1=220nF entreX/OPA OUT. OPA -IN emOUT; OUT no P5 INn. **Y não vai ao ADC.** OPA V+=VEXC eV-=AGND. Três OPA duais dão seis filtros; quarto OPA dá monitor. Valores ideais: frequência natural f0=499,08 Hz, Q=0,74162 e ponto de −3 dB=522,26 Hz; não é Butterworth. A banda final inclui INA, ADS multiplexado e filtros digitais; carga onboard causa atenuação DC nominal10000/10100 corrigida uma única vez pelo firmware.

| Saída filtro | P5 IN | P5 AGND |
| --- | --- | --- |
| CH0 / OPA1A | 1 | 2 |
| CH1 / OPA1B | 3 | 4 |
| CH2 / OPA2A | 5 | 6 |
| CH3 / OPA2B | 7 | 8 |
| CH4 / OPA3A | 9 | 10 |
| CH5 / OPA3B | 11 | 12 |
| Reserva IN6 | 13 aberto | 14 |
| Monitor OPA4A | 15 | 16 |

Monitor: VEXC ->2,2k/0,1% ->MEX ->2,2k/0,1% ->AGND; MEX ->100nF ->AGND; MEX ->OPA4A +IN; -IN_A ->OUT_A; OUT_A ->P5-15. Valor no ADC=VEXC*0,5/1,01≈0,4950495*VEXC; calibrar fator. IN7 é lido uma vez a cada oito varreduras, com valor retido entre leituras: correção de deriva lenta da excitação, sem garantia de corrigir ripple/transientes entre conversões. Não montar o antigo divisor passivo sem buffer com a constante atual. OPA4B termina como seguidor de MEX sem carga na saída, sem unir saídas. Pinagem SOIC no guia.

## Digital, PC e programação

Ligar todos os14 pinos da MSP conforme a tabela no guia, exceto SDO9 aberto. SPI1:PA5 clock ->LCD7/T_CLK10/SD CLK; PA7 ->LCD6/T_DIN12/SD MOSI; PA6 <-T_DO13/SD MISO. CS LCDPA4/P8-7, touchPA0/P8-3, SDPA8/P9-3; pull-ups10k para3,3V. IRQPB1/P8-12, RESET PA2/P8-5, DC PA3/P8-6, LED PA1/P8-4. Slot SD da tela sem cartão; não remover R3 (é SD_CLK). Conferir que o módulo externo libera MISO com CS alto.

USB-UART3,3V: P4-1/PA9 ->RX; P4-3 ->GND. TX eVCC adaptador abertos, P4-2 não usado por estar também no MAX232. PC recebe dados, não alimenta aparelho. ST-Link clone:SWDIO porta4 ->JTAG1-7; SWCLK2 ->JTAG1-9; RST1 ->JTAG1-15 se necessário; GND5/6 ->um par4..20. Sem3,3/5V do clone. Placa porJ2. CAN/TJA1050 e DB9 sem uso.
