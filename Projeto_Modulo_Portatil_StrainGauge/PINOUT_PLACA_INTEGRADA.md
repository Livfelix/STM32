# Pinagem atual - placa integrada e módulos

Fonte primária: esquema original chinês de12/09/2016, uma folha, em `../uploads/ADS1256+STM32F103数据采集板原理图.pdf`. Tabelas conferidas com esse desenho e a especificação aprovada. Confirmar cada conexão na placa concreta sem bateria. Posição significa número do símbolo; não contar pela esquerda de uma foto. A serigrafia dos GPIOs mostra o número físico do STM32.

## Alimentação e redes onboard

| Ponto | Função | Uso atual |
| --- | --- | --- |
| J2-1 | GND | MT3608 OUT- |
| J2-2 | Entrada5V direta | MT3608 OUT+ regulado5,00V |
| P1-1/2 | GND/VIN via regulador5V | Sem ligação nesta montagem |
| C1+ após L1 | Rail analógico VEXC | Saída para pontes, INA, OPA; nunca bypass de L1 |
| P5 pares | AGND | Retorno analógico; R1=0R une AGND/GND onboard |
| TP1 | VREF ADR4525 | Referência nominal2,5V; apenas medir |
| TP2 / TP3 | CLKOUT / DRDY | Testpoints ADC |
| R16=0R | AINCOM para AGND | Não conecta VREF ao AINCOM |
| J3 | Injeta potenciômetro em IN0 | Aberto/removido |
| BOOT0/BOOT1 | Boot STM32 | 0/0 para executar flash |

L1=6,8uH/C1=47uF: frequência natural ideal8,90kHz; isso não garante atenuação/ripple. MT3608 datasheet1,2MHz, não150kHz. U3 gera VDD3,3V a partir do nó anterior a L1. D1 onboard não comprova proteção da bateria nem de toda a montagem.

Ligação externa da bateria: OUT+ protegido -> chave -> **SW_BAT+** -> MT3608 IN+. O topo do divisor 10k/10k sai de SW_BAT+, após a chave e antes do boost. Centro em PB0/P8-11, resistor inferior e 100nF em OUT- protegido/GND. Verificar PB0/VDD no desligamento; P9-12 não mede a célula. Carga só OFF, LEDs TP4056 visíveis, CHRG/STDBY sem ligação ao STM32; terminais reais do carregador/protetor exigem identificação.

## P8 completo

| Posição | Serigrafia STM32 | Rede | Destino |
| --- | --- | --- | --- |
| 1 | 3 | PC14 | Reserva |
| 2 | 4 | PC15 | Reserva |
| 3 | 10 | PA0 | MSP11 T_CS |
| 4 | 11 | PA1 | MSP8 LED/PWM |
| 5 | 12 | PA2 | MSP4 RESET |
| 6 | 13 | PA3 | MSP5 DC |
| 7 | 14 | PA4 | MSP3 CS |
| 8 | 15 | PA5 | MSP7 SCK/MSP10 T_CLK/SD CLK |
| 9 | 16 | PA6 | MSP13 T_DO/SD MISO |
| 10 | 17 | PA7 | MSP6 SDI/MSP12 T_DIN/SD MOSI |
| 11 | 18 | PB0 ADC1_IN8 | Centro do divisor SW_BAT+ ->10k ->PB0 ->10k ->GND, 100nF em PB0/GND |
| 12 | 19 | PB1 | MSP14 T_IRQ |

## P9 completo

| Posição | Serigrafia | Rede | Destino |
| --- | --- | --- | --- |
| 1 | U/VDD | 3,3V | SD3V3 |
| 2 | G | GND | SD GND/retorno digital |
| 3 | 29 | PA8 | SD CS |
| 4 | 32 | PA11 | Reserva |
| 5 | 33 | PA12 | Reserva |
| 6 | 41 | PB5 | Reserva |
| 7 | 42 | PB6 | Reserva |
| 8 | 43 | PB7 | Reserva |
| 9 | 45 | PB8 | Reserva |
| 10 | 46 | PB9 | Reserva |
| 11 | 2 | PC13 | Reserva |
| 12 | 1 | VBAT do STM32 | Sem uso; não é GND nem monitor da célula |

## P5 completo

| Sinal | Pino | Retorno AGND | Uso |
| --- | --- | --- | --- |
| IN0 | 1 | 2 | Filtro CH0 H |
| IN1 | 3 | 4 | Filtro CH1 H |
| IN2 | 5 | 6 | Filtro CH2 H |
| IN3 | 7 | 8 | Filtro CH3 Q |
| IN4 | 9 | 10 | Filtro CH4 Q |
| IN5 | 11 | 12 | Filtro CH5 Q |
| IN6 | 13 | 14 | Reserva, não oferecido como gauge |
| IN7 | 15 | 16 | Buffer monitor VEXC |

Entradas onboard:100ohms série,10k para AGND,1nF para AGND e10nF entre pares0/1,2/3,4/5,6/7. Transferência DC nominal10000/10100; o firmware desfaz uma vez essa atenuação nos seis sinais. Não ligar ADC ao nó Y do Sallen-Key, mas à saída do OPA. SYNC/PDWN do ADS é ligado a VDD no hardware; não existe fio externo de SYNC nesta montagem.

O ADS usa BUFEN. O fundo de escala teórico5V em PGA1 não é sua janela de entrada bufferizada: limite superior físico AVDD-2V. O firmware exige VEXC estimado entre4,75 e5,25V e Vadc bruto atéVEXC-2,00-0,05V, margem provisória a validar. Em VEXC5V, isso dá2,95V no ADC ou2,9795V corrigidos antes do100/10k, limite mais estreito que a janela INA provisória0,15..3,15V. Conferir ambos os limites e a alimentação real CJ; zener3,3V não é proteção automaticamente adequada para essa entrada.

## SPI2 interno do ADS1256

| STM32 | Físico STM32 | ADS1256 | Rede |
| --- | --- | --- | --- |
| PB10 | 21 | 21 | DRDY |
| PB11 | 22 | 15 | RESET |
| PB12 | 25 | 20 | CS |
| PB13 | 26 | 24 | SCLK |
| PB14 | 27 | 22 | DOUT/MISO |
| PB15 | 28 | 23 | DIN/MOSI |

Não usar esses GPIOs para LCD/SD. SPI2 modo1, APB1=36MHz/prescaler32 ->1,125MHz. SPI1 PA5/6/7 compartilhado: LCD/SD9MHz, touch~1,125MHz, inicialização SD~281,25kHz. Confirmar liberação de MISO na bancada; não declarar compartilhamento garantido para qualquer adaptador.

## MSP2807 completo (header14)

| Pino | Rótulo | Liga em |
| --- | --- | --- |
| 1 | VCC | 5V bruto; J1 da tela aberto |
| 2 | GND | Retorno digital |
| 3 | CS | PA4/P8-7 |
| 4 | RESET | PA2/P8-5 |
| 5 | DC/RS | PA3/P8-6 |
| 6 | SDI/MOSI | PA7/P8-10 |
| 7 | SCK | PA5/P8-8 |
| 8 | LED | PA1/P8-4, controla transistor onboard |
| 9 | SDO/MISO | Aberto; LCD só recebe escrita |
| 10 | T_CLK | PA5/P8-8 |
| 11 | T_CS | PA0/P8-3 |
| 12 | T_DIN | PA7/P8-10 |
| 13 | T_DO | PA6/P8-9 |
| 14 | T_IRQ | PB1/P8-12 |

IO é3,3V mesmo com VCC5V. O slot SD da tela não é usado; J4 tem linhas separadas. R3 da tela é SD_CLK, não SD_MISO. Não remover R3 para supostamente isolar MISO. QD-TFT2803 é painel/FPC, não desenho da PCB MSP2807. Esquema/dimensões oficiais: LCDWIKI MSP2807, consultados em13/10/2026.

## SD externo3,3V, CJMCU e OPA

| Módulo/pino rotulado | Destino | Observação |
| --- | --- | --- |
| SD3V3 | P9-1 | Nunca5V |
| SD GND | P9-2 | Nunca P9-12 |
| SD CLK/MOSI/MISO | PA5/PA7/PA6 | Compartilha SPI1 |
| SD CS | PA8/P9-3 | Pull-up10k para3,3V |
| CJ VCC/GND | VEXC/AGND | Um por canal |
| CJ VIN+ | W3 sense do sensor | Sem junção local a W1 |
| CJ VIN- | S- do par1k/1k | Referência própria de cada canal |
| CJ Vout | R1 do filtro | Não diretamente P5 |
| CJ Vref | Aberto | Saída monitor nominal1,65V; não injetar tensão |
| CJ3.3V | Aberto | Saída auxiliar; não unir módulos |

Os rótulos CJ e SD prevalecem sobre ordem física inferida de anúncio. Confirmar revisão/orientação real.

| OPA2364 SOIC-8 | Função |
| --- | --- |
| 1 | OUT_A |
| 2 | -IN_A |
| 3 | +IN_A |
| 4 | V- = AGND |
| 5 | +IN_B |
| 6 | -IN_B |
| 7 | OUT_B |
| 8 | V+ = VEXC |

OPA1A/B filtrosCH0/1; OPA2A/B CH2/3; OPA3A/B CH4/5; OPA4A monitor. OPA4B livre: +IN_B em MEX, -IN_B em OUT_B, saída sem carga, sem unir OUT_A/B.100nF+10uF próximos a cada CI.

## Conector de sensor por canal

Numeração funcional; copiar para a vista do fabricante somente após escolher a família. Seis conjuntos de cinco contatos (painel + cabo), 12 metades, com trava/alívio e terminação para 0,50 mm².

| Contato | Quarto de ponte Q | Meia ponte H |
| --- | --- | --- |
| 1 W1 | RA350 -> terminal A | VEXC -> topo do gauge superior |
| 2 W2 | Terminal B -> AGND | Base do gauge inferior -> AGND |
| 3 W3 | Terminal A remoto -> VIN+ | Meio M dos gauges -> VIN+ |
| 4 | Reserva isolada nas duas pontas | Reserva isolada nas duas pontas |
| 5 | Dreno/SHIELD só no equipamento | Dreno/SHIELD só no equipamento |

Cabo instalado: 1×4×0,50 mm² com blindagem total + dreno, seis trechos até 1 m. Três vias ativas em ambas as topologias; dreno isolado no sensor, sem ligação ao corpo de prova. Duas vias + dreno não atendem. JP_QH em Q alimenta W1 por RA350; em H bypassa RA. W1 e W3 só se unem remotamente em Q; nunca junto ao INA.

## P4 e ST-Link comprado

| Porta | Liga em | Regra |
| --- | --- | --- |
| P4-1 TX/PA9 | RX USB-UART3,3V | 9216008N1 |
| P4-2 RX/PA10 | Aberto | Também recebe MAX232 R1OUT |
| P4-3 GND | GND USB-UART | TX/VCC adaptador sem ligação |
| ST1 RST | JTAG1-15 NRST | Conforme modo de programação |
| ST2 SWCLK | JTAG1-9 PA14 | SWD clock |
| ST3 SWIM | Aberto | STM8, não usado |
| ST4 SWDIO | JTAG1-7 PA13 | SWD dados |
| ST5/6 GND | JTAG1 par4..20 | Um retorno |
| ST7/8 3.3V | Aberto | Saídas do clone; não presumir VTref |
| ST9/10 5.0V | Aberto | Placa alimentada por J2 |

JTAG1-2 é VDD no original e não se conecta à saída do clone nesta montagem. JTAG1-4/6/8/10/12/14/16/18/20 são GND; pinos1/3/5/11/13/17/19 não são necessários ao SWD. Para programador diferente, conferir seu manual em vez de copiar a tabela do clone. MAX232/DB9 não usados; não ligar USB-TTL direto ao DB9.
