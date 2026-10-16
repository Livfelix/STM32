# Protocolo de bancada SG6 - preencher com medições reais

Revisão15/10/2026. **Nenhum ensaio físico foi executado nesta revisão de software/documentação.** Não liberar hardware por testes host/build. Esperado não conta como medido. Cada etapa exige condição, instrumento, dados, responsável e resultado. Definir limites de ruído/erro/banda adequados ao ensaio antes de executá-lo.

Unidade/revisão____; data/hora externa____; operador____; temperatura____; firmware/build____; instrumentos ID/resolução/calibração____; cartão____; célula/proteção____; cabos____; corpo de prova/geometria____.

## Execução e critérios

| Etapa | Procedimento / critério | Dados reais | Resultado |
| --- | --- | --- | --- |
| Continuidade OFF | Sem bateria: J2, P9-2GND/P9-12VBAT, P5AGND, R16AINCOM, JTAG1 7/9/15, J3 aberto | ____ | Pendente |
| PB0/VDD em OFF | Divisor10k/10k após chave em SW_BAT+, antes boost; medir PB0/VDD na partida/desligamento/carga USB OFF e descarga100nF | ____ | Pendente |
| Cabo Q/H | Seis cabos4vias+dreno0,50mm² até1m; três ativas, quarta isolada, dreno só equipamento; seis pares5contatos/12 metades, vista fabricante/trava/alívio | ____ | Pendente |
| Módulos/revisões | Vref CJ saída/aux aberta, SOIC pino1, LCD J1 aberto, SD3,3V, proteção real1S | ____ | Pendente |
| Rails/partida | Fonte limitada: J2/C1+/VDD/TP1, sob carga LCD+SD, sem bypassL1 | ____ | Pendente |
| Ripple/temperatura | Osciloscópio com ponta curta C1+/VDD durante writeSD/UI, temperatura LDO/boost/gauges | ____ | Pendente |
| Janela CJ/G100 | Alimentação efetiva CI, modo comum~2,5V, excursões ambos sentidos, ganho/headroom/banda | ____ | Pendente |
| ADC tensão/MUX | Referência segura PGA1, escala/código, ordem/skew e acoplamento entre pares | ____ | Pendente |
| Bias/SAVE/ZERO | W3 fora/VIN+ em VIN-; BIAS, SAVE FLASH/CONCLUIDO antes de OFF; restaurar/religar, conferir configuração/BIAS OK/bias_v_f32; só então ZERO | ____ | Pendente |
| Shunt/retorno | Rreal100k/330k informada e conferida na UI, Q ou inferior H, escala/SAT, retirar sem novoZERO e confirmar retorno | ____ | Pendente |
| Mecânica axial/flexão | Carga/geometria conhecidas, Q local/H flexão, adesão/alinhamento/repetibilidade/histerese/drift | ____ | Pendente |
| Frequência/alias/skew | Sweep por modo, amplitude/fase, >Nyquist e média8; comparar filtro ideal f0=499,08Hz/Q=0,74162/−3dB=522,26Hz, sem confundir com banda final | ____ | Pendente |
| VEXC IN7 | Calibrar fator0,4950495; leitura a cada8varreduras, deriva lenta; medir ripple/transientes, não inferir correção simultânea | ____ | Pendente |
| SD+PC+touch | SD integral/UART subset, trigger entre publicações normais/prioridade/reserva e trigger_uart_lost sob saturação; CRC, toque/cadência/máscaras | ____ | Pendente |
| Latência cartão/fila | REC RÁPIDO/BIN3 com LCD/toque por duração máxima; anotar SDmax (ms), #STATUS sd_max_io_us (µs) e busy separado sd_max_busy_ms(); orçamento70ms busy/op, fila128~99ms; medir pausa total e rejeitar cartão com INCOMPLETE | ____ | Pendente |
| Falhas/close | SD ausente/cheio/write parcial/close, preservação do arquivo, confirmação STOP | ____ | Pendente |
| Powercut | Cortes em header/frame/boundary/footer/sync; BIN3 estrito recusa ausência de rodapé, recovery só CRC válido/clean_close=0; conferir FAT e erro final sync/close mesmo com rodapé | ____ | Pendente |
| Baixa bateria | Calibrar PB0/VDDA, aviso <3,50V500ms com ícone/~% piscante/CARREGAR; <3,35V500ms fecha status2; falha ADC500ms fecha status3/BAT MONITOR FAIL; margem antes proteção | ____ | Pendente |
| Persistência/stack | SAVE foraREC, reset/restore semZERO, interrupção flash, high-water real de stack | ____ | Pendente |
| Autonomia/carga OFF | Modelo/capacidade/corrente/picos/temperatura/energia útil/eficiência; comparar cenários2000mAh3,15–4,57h/2600mAh4,09–5,95h/3000mAh4,72–6,86h/3500mAh5,50–8,01h; carga sóOFF com LEDs visíveis/PROG compatível | ____ | Pendente |
| Mecânica final | Cotas/furos, isolamento, alívios, acessoSD, célula presa e etiquetas | ____ | Pendente |

Janela software provisória0,15..3,15V, resposta shunt negativa de módulo≥50µε e retorno±50µε são critérios iniciais a validar. CJ/ADC buffer/modo comum podem limitar antes do fundo teórico5V. Não declarar ruído/faixa/banda/autonomia sem essas medições.

## Ficha por canal (copiar seis vezes)

CH____; H/Q____; posição/faces/alinhamento____; gauges marca/lote/R/GF____; RA/RC/RD medidos/TCR____; W1/W2/W3 comprimentos____; quarta via isolada nas duas pontas____; dreno só equipamento____; família/vista conector____; G trimpot/informado____; temperatura____.

Bias com sense isolado: VEXC____V; bias Vout corrigido____V; bias_v_f32____; dados____. SAVE após BIAS e antes de OFF: FLASH/CONCLUIDO____; canal/tipo/GF/ganho salvos____. OFF, ponte VIN+/VIN- removida/W3 restaurado____; religado: configuração conferida____; BIAS OK____; bias_v_f32 igual ao salvo____. Não continuar se SAVE/restauração falhar. ZERO: condição de carga____; tara_ratio____; estabilidade____. Shunt: Rreal____ohms; R aplicada na UI____ohms; posição no sensor____; esperado pelo tipo/GF____µε; medido____; escala____; retorno sem novoZERO____µε; validade/metadados____; SAVE final/reboot____; EST por canal____µε, excursões reais/limites ADC e CJ____. Reentrar Rreal após boot antes de nova calibração. Carga/geom./incerteza mecânica____; resultado/histerese/drift____.

## Declarações históricas preservadas

Texto anterior2026-10 declarava MT3608 sem carga5,00V, TP1=2,500V, P9U=3,30V, C1+~5V, seis CJ com entradas abertas Vout/Vref=1,66V e J3 removido. Sem ID de instrumento, dados brutos, carga, revisão ou rastreabilidade disponíveis; não foram reproduzidas. Constituem histórico, não liberação atual. Entradas abertas não comissionam bias em modo comum real; ZERO sozinho não separa offset e desequilíbrio.

## Evidência de software separada

Integração de 16/10/2026: 133 testes C/Python PASS; build limpo sem warnings, Flash 55320 B e RAM estática 15372 B (fim alinhado em 15376 B). Reservas heap512/stack2048 B e folga adicional2544 B; pico real de stack pendente. Evidências atuais em `.superpowers/sdd/sg6-integridade-bateria/final-fix-report.md`; histórico em `.superpowers/sdd/sg6-modulos-adquiridos/`. Mocks apenas nas bordas de hardware. Não preencher tabela física com resultados de software. Manual consolidado em 15/10/2026 é referência de construção; auditoria de 13/10/2026 é histórica.
