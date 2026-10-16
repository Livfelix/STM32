# SG6 portátil - referência atual de construção

Revisão de 15/10/2026: integridade BIN3, bateria e cabeamento. Placa integrada ADS1256 + STM32F103C8T6, seis CJMCU-333, quatro OPA2364, MSP2807 com toque, SD externo3,3V e uma célula Li-ion protegida. Firmware ativo em `fw_pio/`; programa PC em `app/visualizador_tempo_real.py`.

São **seis entradas e nove gauges**: CH0..2 meia ponte de flexão (H, dois gauges de faces opostas); CH3..5 quarto de ponte (Q, deformação local). H reduz a resposta axial comum; Q preserva axial e flexão local. Para comparar ambas no mesmo local, posicionar Q perto e alinhado ao gauge inferior de H e registrar a geometria. Meia ponte de Poisson e ponte completa não fazem parte desta montagem.

## Por onde começar

- `MONTAGEM_PASSO_A_PASSO.md`: fabricação, colagem, calibração, operação em campo e PC.
- `PINOUT_PLACA_INTEGRADA.md`: tabelas completas, numeradas pelo símbolo original.
- `CONEXOES_SISTEMA.md`: todas as ligações, incluindo filtro/monitor/cabos.
- `ESPECIFICACAO_UI_PRESETS.md`: telas efetivamente entregues, presets e formatos.
- `REGISTRO_BANCADA.md`: protocolo preenchível e pendências físicas.
- `BOM_FINAL_ADS1256_MSP2807.xlsx`: necessário, compra declarada, confirmado, falta e reserva. Quantidade pendente não significa zero.
- `../output/pdf/SG6_Manual_Montagem_Revisado_2026-10-13.pdf`: manual consolidado em 15/10/2026, gerado dos guias-fonte e SVGs atuais; mantém o nome histórico do arquivo.

O esquema original `../uploads/ADS1256+STM32F103数据采集板原理图.pdf` prevalece sobre o PDF explicativo em português. Confirmar a revisão real por continuidade antes de energizar. Os uploads permanecem originais; auditoria e cinco comparações antigas estão marcadas como históricas.

Base desta revisão: [potência, autonomia, IN7 e filtro](../docs/research/2026-10-15-datasheets-potencia-analogico.md), [auditoria dos fluxos antes das correções](../docs/research/2026-10-15-auditoria-firmware-fluxos.md) e [BOM/ligações](../docs/research/2026-10-15-auditoria-bom-ligacoes-documentos.md). Os comportamentos corrigidos de bateria, SD, trigger e BIN3 estão descritos nos guias atuais e implementados em `fw_pio`/`app`.

## Arquitetura e decisões fixas

Célula 18650 1S -> proteção real -> OUT+ -> chave -> **SW_BAT+** -> MT3608 regulado5,00V -> J2 e LCD. Divisor 10k/10k+100nF de PB0 sai de SW_BAT+, após a chave e antes do boost; medir PB0/VDD em OFF. **VEXC sai de C1+ após L1** para pontes/INA/OPA; não ligar esse nó diretamente ao boost bruto por outro fio. Retorno analógico em P5/AGND, digital/potência em J2/P9-2; R1 onboard une os retornos. Carregar somente OFF; LEDs TP4056 visíveis e rotulados CARREGANDO/CARGA TERMINADA, sem CHRG/STDBY no MCU nem estado de carga na tela. Identificar revisão/proteção/célula e corrente de carga. Não há PowerPath nem desligamento automático por latch.

Cada ponte -> CJMCU G físico100 inicial -> Sallen-Key fixo (f0=499,08 Hz, Q=0,74162, −3 dB=522,26 Hz; não Butterworth) -> P5 IN0..5 -> ADS1256/SPI2 -> STM32. Monitor VEXC usa divisor2,2k/2,2k com buffer OPA -> IN7, fator nominal ADC0,4950495. IN7 é lido a cada oito varreduras: correção de deriva lenta da excitação; ripple/transientes exigem ensaio. IN6 é reserva. Referência ADR4525 fixa2,5V, PGA1. LCD/toque/SD externo compartilham SPI1, CS próprios PA4/PA0/PA8; slot SD da tela sem uso; UART PA9 envia para RX USB-UART3,3V; TX/VCC do adaptador abertos. CAN não implementado.

Cada Q e H usa W1/W2/W3 em cabo instalado 1×4×0,50 mm² + blindagem/dreno, até 1 m: quarta via isolada nas duas pontas, dreno apenas no equipamento. São seis conjuntos completos de conectores5contatos, 12 metades; família/vista de pinout dependem das medidas da caixa. Cabo de duas vias + dreno não atende.

Os quatro presets mudam taxa, máscara, filtro, média e trigger. Não mudam ganho físico, GF, tipo, zero ou escala. A seleção de H/Q exige sensor/fiação e seletor físico correspondentes, aparelho OFF, configuração e recalibração. A tara nunca é automática. Alterações de configuração em REC são bloqueadas.

CALIB recebe R SHUNT medida (padrão100k, incluindo330k), mostra a faixa estimada por canal após BIAS/ZERO e exige retirada/retorno antes de confirmar a escala. Registrar Rreal/aplicada: a entrada é transitória e a escala resultante é persistida. MAIN mostra arquivo e minutos:segundos em REC; erros têm prioridade. No PC, VEXC medida mantém histórico; configuração/calibração ou BOOT/reset separa épocas e eventos.

## Estado de validação e limites

Há testes automatizados e build STM32 documentados no registro de execução da revisão. Isso não prova funcionamento de placa, cartão, touch, UART física, colagem, banda, autonomia ou calibração. O protocolo de bancada atual não registra ensaios fictícios. Valores de tensão de textos antigos permanecem como declarações históricas sem rastreabilidade experimental suficiente.

O modelo é linear de pequenas deformações: Q usa4*delta_ratio/GF; H usa2*delta_ratio/GF, depois escala shunt. Ganho/cabos/temperatura/adesão e cargas reais precisam de validação. Shunt comprova a cadeia elétrica naquele ponto, não exatidão mecânica. O filtro analógico fixo não assegura antialias quando Nyquist por canal é menor; filtros digitais não removem alias já adquirido. O modo MÉDIO LP50 atenua100Hz. Ganho físico também afeta banda/headroom do INA; G100 permanece ponto inicial, sem garantia de largura de banda do módulo.

SD BIN3 registra todas as varreduras aceitas, com frames AA56/46B/CRC e rodapé AA57/21B no fechamento controlado. Conversão normal exige rodapé válido e coerente; recuperação explícita `--recover-incomplete` preserva registros completos válidos e marca `clean_close=0`. BIN1/BIN2 antigos continuam aceitos sem prova de fechamento. UART mantém BIN2 e limite normal de 1000 quadros/s; trigger tem prioridade, reserva de um frame e contador `trigger_uart_lost` quando não enfileirado. SD é a fonte integral. RÁPIDO exige BIN3, lote de 16 registros e orçamento acumulado de busy SD de 70 ms por operação de escrita/sync; erro `SD SLOW / INCOMPLETE` exige preservar o arquivo e revalidar o cartão. Fila128 cobre cerca de99ms a1289SPS/canal. Fazer STOP e confirmar fechamento antes de OFF/cartão. Sync a cada1s; um rodapé não atesta persistência física se o sync/close final falhar. Sem segmentação automática ou calendário RTC real.

Durante uso, ícone de quatro segmentos e percentual aproximado `~` acompanham a tensão BAT; abaixo de3,50V por500ms piscam e aparece CARREGAR. Durante REC, abaixo de3,35V por500ms ou monitor inválido por500ms fecha e para (`BATERIA BAIXA`/`BAT MONITOR FAIL`). A estimativa não é medidor de coulombs.

Autonomia de planejamento: 2000mAh=3,15–4,57h; 2600mAh=4,09–5,95h; 3000mAh=4,72–6,86h; 3500mAh=5,50–8,01h. Usa `(3,7 * capacidade_Ah * 0,80 * 0,85)/(5 * corrente_5V_A)` com 0,22–0,32A em5V; confirmar capacidade útil, picos, eficiência e temperatura. Cálculos não garantem horas de campo.

## Materiais e regeneração

6 CJMCU e12 OPA confirmados por declaração específica; usam-se4 OPA e sobram8. Gauges disponíveis sem limitação declarada, mas contar nove e conferir lote/GF. Adaptadores SOIC, USB-UART, proteção real/célula,12 resistores1k de precisão e encaixe físico são pendências. Passivos da lista anterior foram comprados; quantidade, tolerância e adequação dos valores novos ainda precisam de conferência. Sem preços atualizados por suposição.

Regenerar a BOM a partir da raiz com `python Projeto_Modulo_Portatil_StrainGauge/gerar_bom.py`. Usa o Node/Artifact Tool do runtime Codex e `tools/bom_artifact.mjs`; não exige openpyxl para autoria. Na máquina sem esse runtime, o gerador informa indisponibilidade e preserva a planilha existente. Os testes das contas em `../tests/test_bom.py` usam somente biblioteca padrão.
