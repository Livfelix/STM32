# UI, presets e formatos entregues

Tela240x320, fonte6x10, MAIN imediata; quatro botões2x2, seis valores H/Q, minigráfico selecionado, VEXC/bateria, SD/erro/arquivo/SAT. Em REC, arquivo e minutos:segundos; erro tem prioridade nessa linha. `uE` é µε. P indica cadência prevista, M medida e R atualmente repete M em SPS/CH com frações; R não é uma medição independente de persistência no SD. DRATE não é taxa por canal garantida. Sem splash, RTC calendário ou CAN.

Bateria: símbolo de quatro segmentos e percentual aproximado `~` no topo, tensão BAT junto de VEXC/SD. Estimativa linear por trechos: 3,35/3,50/3,65/3,75/3,85/3,95/4,05/4,15V correspondem a0/10/20/40/60/75/90/100%, saturada nas pontas. Não mede coulombs. Abaixo de3,50V por500ms, ícone/percentual piscam a cada500ms e aparece CARREGAR. Abaixo de3,35V por500ms em REC fecha com BATERIA BAIXA. Monitor inválido mostra `~--%`/BAT FALHA e, persistindo500ms em REC, fecha com `BAT MONITOR FAIL`. Carga só OFF por LEDs TP4056 visíveis; a tela não tem estado CARREGANDO.

## Presets

| Modo | Máscara/canais | DRATE | Digital/média | Trigger |
| --- | --- | --- | --- | --- |
| QUASE-ESTÁTICO | 0x3F/CH0..5 | 500SPS/0x92 | 5Hz/8 | OFF |
| MÉDIO | 0x3F/CH0..5 | 7500SPS/0xD0 | 50Hz/1 | OFF |
| RÁPIDO | 0x03/CH0,1 | 15000SPS/0xE0 | 200Hz/1 | SubidaCH0+500µε |
| CUSTOM inicial | 0x3F/CH0..5 | 7500SPS | 50Hz/1 | OFF, configurável |

PGA1 e ganho físico100 inicial. Presets preservam tipo/GF/ganho/bias/zero/escala/validade. CH0..2 H=HALF_BENDING, CH3..5 Q=QUARTER, nove gauges em seis entradas. Troca H/Q exige fiação/sensor/seletor físico correspondentes, confirmação e calibração; ponte completa indisponível.

CUSTOM:16DRATE (2,5/5/10/15/25/30/50/60/100/500/1000/2000/3750/7500/15000/30000SPS), máscara não vazia dos seis canais; cortes OFF/1/5/10/20/50/100/200/500Hz; médias1(OFF)/2/4/8/16/32/64/128. Corte efetivo limita-se pela cadência/Nyquist, exibido separado do pedido. Apply inválido mantém anterior; erro de hardware deixa estado inválido visível.

Sallen-Key fixo: f0=499,08 Hz, Q=0,74162, ponto de −3 dB=522,26 Hz; não Butterworth e não acompanha presets. IN7 monitora VEXC uma vez a cada oito varreduras, fator nominal0,4950495 com buffer; corrige deriva lenta, sem garantia sobre ripple/transientes. Digital não elimina alias já adquirido. Média móvel não decima: resultado por amostra, registro não cai por8, mas amplitude/fase/atraso mudam. MÉDIO LP50 atenua100Hz; para análise100Hz, CUSTOM com taxa/canais/corte apropriados e validação de amplitude/fase/alias em frequência conhecida. Ganho físico também altera banda/headroom INA; não há banda útil garantida apenas pelo preset.

## Menus e calibração

CALIB seleciona CH0..5, tipo H/Q, GF (padrão2,08, usar lote), ganho físico2..1000 como ajuste manual, BIAS/ZERO/SHUNT medido/REMOVE-RETURN e SAVEFLASH. BIAS exige W3 desconectado e VIN+ temporariamente em VIN-. Após cada BIAS, SALVAR FLASH e confirmar FLASH/CONCLUIDO antes de OFF para restaurar o sensor. Religado, conferir canal/tipo/GF/ganho, BIAS OK e bias_v_f32 restaurados antes do ZERO; erro de SAVE ou restauração exige interromper e corrigir, não usar bias perdido/antigo. Conservar SAVEFLASH final após shunt/retorno. ZERO é confirm/cancel; shunt só confirma após retorno. Ganho invalida bias/zero/shunt; tipo invalida zero/shunt; GF invalida shunt. Modelo linear Q4x/GF, H2x/GF; H não é deformação total da face. Procedimento completo no manual de montagem.

R SHUNT abre um rascunho de oito dígitos: DIGITO +1 cicla0..9; PROXIMO DIGITO avança; último dígito=0,1ohm. APLICAR valida1,0..999999,9ohms, CANCELAR preserva o aplicado. Padrão100000,0ohms; informar o valor medido, incluindo330000,0 ou330123,4. CALIB e confirmação mostram a resistência usada pela API de calibração. Edição bloqueada em REC e durante shunt pendente de qualquer canal; retirar/confirmar retorno antes de alterar. Entrada transitória, volta a100000,0 no boot; SAVE mantém a escala resultante no campo existente. Registrar Rreal e valor aplicado na ficha; reentrar antes de nova calibração após boot.

EST apresenta limites do canal selecionado em µε, arredondados para dentro da janela. Usa bias/zero medidos, VEXC atual, ganho/GF/tipo/escala e os mesmos códigos aceitos pelas guardas de medição: INA0,15..3,15V estritos e entrada ADC<=VEXC-2,05V, com correção1,01. Indisponível sem BIAS/ZERO válidos ou parâmetros/excitação válidos; faixas além de±999999µε pedem reverGF/ganho. É estimativa provisória, bancada pendente; não comprova modo comum, exatidão ou faixa mecânica. Escala positiva conforme validação existente.

MAIN ZERO atua na máscara ativa com confirmação. REC bloqueia toda configuração mutável, inclusive calibração/flash/touch/trigger/formato/duração; navegação/seleção do gráfico continuam. MENU inclui BIN/CSV, brilho20..100, toque calibrado, prefixo e duração. Prefixo até3 A-Z/0-9, sequência5dígitos (ENS00001.SGB); duração UI0..86400s,0manual. SAVE explícito fora de REC em duas páginas flash0x0800F800/0x0800FC00 com CRC/versão/readback/commit. Carga inválida pede calibração; nenhuma tara automática nem conversão de volts legados para razão.

Trigger de subida, fonte0..5, nível±1000000µε (UI passo100), pré0..10s, pós0,1..60s (passo0,1s); defaults0,5/2s. Marca um cruzamento, não repete em patamar. SD contínuo; PC extrai janela, sem longo pré-buffer C8. #TRIGGER enabled=0/1 distingue OFF de ON sem cruzamento.

RÁPIDO tem previsão de1289SPS/canal e só inicia REC em BIN3; CSV é recusado com `BIN REQUIRED / RAPIDO`. Lote binário16×46B; fila128 cobre cerca de99ms. Durante REC RÁPIDO o orçamento acumulado de busy SD é70ms por operação de escrita/sync, incluindo esperas repetidas. Não é prazo máximo de toda chamada FATFS. Fora desse modo os prazos próprios permanecem. Estouro interrompe aquisição, tenta sync/close e informa `SD SLOW / INCOMPLETE` (SD lento/ensaio incompleto). `SDmax` na linha de status mostra em ms o maior tempo decorrido de uma chamada foreground `f_write` ou `f_sync`, incluindo cabeçalho e fechamento, desde a tentativa de REC atual. A API `app_sd_max_io_us()` e o campo `sd_max_io_us` em `#STATUS` retêm esse máximo em µs (saturação UINT32_MAX); reiniciam a cada REC. O status é enfileirado inteiro e repetido até caber preservando46B para trigger. O máximo inclui operações com falha e não classifica sua causa. `sd_max_busy_ms()` continua medindo somente busy acumulado arredondado para cima, separado do tempo total e do latch de timeout. Testar cartão com LCD/toque pela duração máxima pretendida.

## Registro SD BIN3 e UART V2

SGB novo: header texto #SG6BIN3 + seis #CHn + #TRIGGER + #ENDHDR, seguido de frames AA56/46bytes little-endian e rodapé AA57/21B. UART permanece #SG6BIN2/AA56, com sequências próprias e headers completos a cada2s/em commit; publicações normais até1000quadros/s de subconjunto temporal. Trigger ignora o espaçamento1ms e pode usar uma reserva de46B que mensagens normais não consomem. Falha em enfileirar trigger incrementa `trigger_uart_lost`, emitido por `#STATUS` quando houver espaço. A sequência UART conta só frames enfileirados, preservando a ordem. SD registra todas as varreduras aceitas. SD/UI fora da ISR; fila esgotada para e informa INCOMPLETE.

| Bytes | Campo | Significado |
| --- | --- | --- |
| 0..1 | AA56 | SyncV2 |
| 2..5 | seq_u32 | Contador por transporte |
| 6..13 | t_us_u64 | Tempo de boot, varredura sequencial |
| 14..37 | 6i32 | 0,1µε, inativo INT32_MAX |
| 38..39 | VEXC_mV_u16 | Excitação medida |
| 40..41 | bateria_mV_u16 | Tensão medida |
| 42 | mask_u8 | Ativos bits0..5 |
| 43 | flags_u8 | SAT0..5, trigger6, incompleto7 |
| 44..45 | CRC16 | CCITT-FALSE dos44bytes anteriores |

CRC: poly0x1021, init0xFFFF, sem reflexão/XOR final;123456789=0x29B1. Header traz ch, pga1, drate_x10, spsch_x10 previsto, fc_x10 pedido, avg, vexc_mv, bias_valid/zero_valid/cal_valid, time=boot_relative_us, scan=sequential, uart_max_sps=1000, fat_date=placeholder, unit=0.1ue, inactive=INT32_MAX, encoding=IEEE754_HEX_F32. Cada CHn traz type e palavras float gf_f32/gain_f32/bias_v_f32/zero_ratio_f32/scale_f32. TRIGGER traz enabled/channel/level_x10/pre_ms/post_ms/effective_fc_x10/analog_hz499/analog_q_x1000=742/alias_protection=unverified. Dados já convertidos/filtrados, não ADC bruto.

CSV nativo SD: #SG6CSV2 (unit=ue), metadados e colunas `seq;t_us;ch0_ue;ch1_ue;ch2_ue;ch3_ue;ch4_ue;ch5_ue;vexc_mv;battery_mv;mask;flags`. Inativo2147483647, sem CRC por linha. Difere do CSV PC com t_ms/trigger_mark/epoch/inativos vazios. Conversor SGB não lê esse CSV. BIN3 recomendado para integridade e obrigatório no RÁPIDO; CSV nativo não tem rodapé BIN3.

Rodapé BIN3 (21B, little-endian): AA57 em0..1, record_count_u32 em2..5, last_seq_u32 em6..9, last_time_us_u64 em10..17, close_status_u8 em18 e CRC16 CCITT-FALSE dos19bytes anteriores em19..20. Vazio: count0, last_seq=UINT32_MAX e last_time_us=0. Status0=STOP_NORMAL,1=DURACAO,2=BATERIA_BAIXA,3=BAT_MONITOR_FAIL. Fechamento controlado drena fila/lote, escreve rodapé, f_sync e f_close. Erro prévio SD/ADC/fila impede rodapé limpo; falha descoberta só no sync/close final pode deixar rodapé válido sem persistência confirmada.

Conversão normal BIN3 exige rodapé/CRC e contagem/últimos seq/tempo coerentes, sem bytes excedentes; EOF alinhado sem rodapé falha. `--recover-incomplete` exporta só registros completos válidos, avisa e marca `clean_close=0`; não reconstrói fragmentos nem ressincroniza inserção/remoção arbitrária de bytes. Não confundir fechamento controlado por bateria/status2 ou3 com duração pretendida cumprida. Registros com flags incompletos/SAT e janelas pré/pós parciais exigem interpretação própria.

Nomes exclusivos; erro de numeração/SD cheio/write parcial/close/ADC/fila é visível. Sync1s/STOP e close conferidos; sem auto-OFF da chave. STOP/close antes OFF/cartão; margem, persistência FAT e powercut pendentes de bancada. Sem segmentação automática FAT32.

## PC e legado

Ao vivo V2 CRC é padrão; UART V1 exige `--legacy-v1` ou header antigo válido. Offline aceita BIN1/BIN2 legados sem rodapé e BIN3 com validação de fechamento. Autodetecta V1 e avisa sem CRC, sparse-mask/identidade, timewrap/ambiguidade de marcador; não inventa recuperação. V2 exige seis CH/TRIGGER/#ENDHDR; header parcial não substitui anterior. Metadados desconhecidos antes do primeiro header completo. Cada header completo mantém seu snapshot bruto; VEXC medida pode variar sem mudar epoch/histórico. Configuração/calibração estável diferente ou BOOT confirmado/reset de sequência/tempo regressivo inicia nova epoch, com metadados antes do primeiro registro. Wrap uint32 normal permanece contínuo. Histórico e seleção de evento ao vivo ficam em uma epoch; ao combinar registros na API extract_event_window, informar epoch, pois janelas que contêm épocas diferentes são recusadas.

CSV PC UTF-8 com separador ponto e vírgula: `seq;t_us;t_ms;ch0_ue;ch1_ue;ch2_ue;ch3_ue;ch4_ue;ch5_ue;vexc_mv;battery_mv;mask;flags;trigger_mark;epoch`. Unidadeµε, inativos vazios, metadados brutos em comentários e #PC_STATUS. CLI:--port/--baud921600/--log, --conv/-o/--out, --event-time-us/--pre/--post, --legacy-v1 e --recover-incomplete (somente recuperação explícita de BIN3). Log exclusivo; leitor em thread grava antes do gráfico; histórico72000, plot_drops visíveis. Conversão valida input inteiro, inclusive fora da janela, salva atomicamente só após sucesso. Limites inclusivos com janelas aplicadas ou override CLI. Sem teste físico de COM; operação completa no manual de montagem.
