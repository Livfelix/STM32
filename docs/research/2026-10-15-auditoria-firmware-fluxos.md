# Auditoria do firmware ativo e dos fluxos de aquisição — 15/10/2026

## Escopo e evidência

O firmware ativo lê seis condicionadores de strain gauge com um único ADS1256 multiplexado, calcula deformação, mostra os valores no LCD e grava SD. A UART fornece uma seleção temporal dos registros para o visualizador Python. A carga considerada é uma unidade física, seis canais no modo MÉDIO e dois no RÁPIDO, gravação contínua enquanto a interface funciona.

Esta revisão leu `Projeto_Modulo_Portatil_StrainGauge/fw_pio`, o visualizador de `app` e os testes/fixtures pertinentes de `tests`. Não alterou firmware, testes, BOM ou esquemáticos. A indicação de firmware ativo vem de `fw_pio/README.md:1-3` e `platformio.ini:4-20`; `firmware/ads1256_6ch.c` é histórico.

Convenções abaixo: **FW** = `Projeto_Modulo_Portatil_StrainGauge/fw_pio`; **APP** = `Projeto_Modulo_Portatil_StrainGauge/app/visualizador_tempo_real.py`. “Confirmado” significa demonstrado pelo código ou por uma verificação local descrita; não significa verificado no equipamento. Números de taxa e de banda calculados são previsões, não medições de bancada.

## Aquisição e taxas

### Caminho confirmado

1. `main.c` inicia relógio de 72 MHz, contador de microssegundos de 64 bits com TIM3, SPI2 do ADC a 1,125 MHz/modo 1, ADC interno de bateria, LCD e aplicação. O loop chama `app_poll()` e `gui_tick()`; DRDY chama `app_drdy_irq()` por EXTI (`FW/src/main.c:19-42,104-120,175-198`).
2. O ADS1256 usa PGA=1, buffer de entrada habilitado e leitura single-ended contra AINCOM. Há validação/readback de configuração, SELFCAL ao trocar DRATE e sequência MUX → SYNC → WAKEUP → espera DRDY → RDATA de 24 bits com extensão de sinal (`FW/src/ads1256.c:25-79`). Não se aproveita um único DRATE nominal como taxa de cada canal.
3. Ao retomar, lê IN7 primeiro. Depois percorre apenas `act[]`, derivado da máscara dos seis canais. A cada oito varreduras completas insere outra leitura IN7 (`FW/src/app.c:81-108,339-342`). IN6 não participa desse caminho.
4. A interrupção publica uma `RawScan` com seis códigos, código de excitação retido, timestamp e estado REC. O timestamp representa o começo da varredura; os canais seguintes têm instantes físicos diferentes (`FW/src/app.c:64-108`).
5. O processamento de deformação, filtros, SD, bateria e UART ocorre no foreground (`FW/src/app.c:261-320`). A ISR executa transferências SPI e esperas de microssegundos; ela não é apenas uma cópia de dados, mas também não chama FatFs nem DSP.

### Taxas previstas pelo próprio firmware

`app_sps_per_ch()` usa `1e6 / ((settling_us + 115) × (n_act + 0,125))`; os 115 µs são uma estimativa explícita do custo de comandos. A tabela de settling e essa hipótese estão em `FW/src/app.c:35-37,116-120`; os presets, em `:29-34`.

| Preset | Canais | DRATE nominal | Settling por troca | SPS/canal previstos | Nyquist previsto | Fila de 128 varreduras | Intervalo médio entre leituras IN7 |
|---|---:|---:|---:|---:|---:|---:|---:|
| QUASE-ESTÁTICO | 6 | 500 | 2180 µs | 71,14 | 35,57 Hz | 1799 ms | 112,46 ms |
| MÉDIO | 6 | 7500 | 310 µs | 384,15 | 192,08 Hz | 333 ms | 20,83 ms |
| RÁPIDO | 2 | 15000 | 250 µs | 1289,28 | 644,64 Hz | 99 ms | 6,21 ms |
| CUSTOM inicial | 6 | 7500 | 310 µs | 384,15 | 192,08 Hz | 333 ms | 20,83 ms |

As durações de fila são `128/fs` com fila inicialmente vazia; não são garantia de tempo útil sob carga. O intervalo IN7 é uma média de oito varreduras: há ciclos extras de monitoramento, e a aquisição não é perfeitamente uniforme. O pior envelhecimento entre a leitura IN7 e um canal posterior precisa incluir sua posição dentro da oitava varredura.

O firmware também estima a taxa efetivamente observada pelos timestamps ao longo de pelo menos 500 ms. Se diferir mais de 10% de `dsp_sps`, troca a frequência usada no filtro e reinicia seus estados (`FW/src/app.c:282-287`). A UI mostra previsão P e medição M; o campo R repete M (`FW/src/gui.c:84`). O campo `spsch_x10` do header é a previsão de `app_sps_per_ch()`, não essa medição (`FW/src/app.c:181-183`). Os testes de aquisição injetam tempos/códigos e substituem o driver ADS; não demonstram a taxa física (`tests/host/acquisition_harness.c:33-49,73-77`).

Os canais não são simultâneos. No modelo de custo, CH0 e CH5 no MÉDIO ficam separados aproximadamente por `5 × 425 µs = 2,125 ms`; isso equivale a cerca de 61° a 80 Hz. Esse valor é uma estimativa, e o formato não registra timestamp individual de cada canal. Análise de fase modal ou combinação instantânea de canais precisa levar esse escalonamento em conta.

## IN7, VEXC e cálculo ratiométrico

- `CH_VEXC=7`, `VEXC_DIV=0,5/1,01=0,4950495` (`FW/include/app_cfg.h:39-43`). O modelo é divisor externo 2,2 kΩ/2,2 kΩ com buffer, seguido da atenuação de entrada da placa 100 Ω/10 kΩ. O fator completo é usado uma única vez para reconstruir VEXC (`FW/src/app.c:263,353`). A presença e o valor físico dessa rede dependem da montagem.
- `sg_adc_voltage(code) = code × 5/8388608`, assumindo VREF=2,5 V e PGA=1 (`FW/src/sg_measure.c:3`; configuração PGA em `FW/src/ads1256.c:51`). O define `VREF_ADC` não parametriza essa função: uma mudança física de referência exigiria mudar a conversão também.
- Para o canal de medida, `Vout = Vadc × 1,01`, `ratio = (Vout − bias)/ganho/VEXC`. Depois subtrai o zero medido, aplica `2/GF` em meia ponte de flexão ou `4/GF` em quarto de ponte, multiplica por `1e6` e pela escala de shunt (`FW/src/sg_measure.c:9-30`; `FW/src/app.c:148-152`). A escala linear é a aproximação de pequenas deformações.
- O bias deve ser medido com entradas INA iguais antes da tara; ZERO registra um desbalanço normalizado, não apenas uma tensão fixa. Os testes variam VEXC de 5 para 4,8 V e mantêm o resultado com bias constante (`tests/host/measure_harness.c:29-40`; `tests/host/app_measure_harness.c:70-75`).
- O código rejeita VEXC fora de 4,75…5,25 V, códigos negativos/full-scale, entrada do buffer acima de `AVDD−2−0,05 V` e janela INA fora de 0,15…3,15 V. A guarda do buffer usa VEXC como estimativa de AVDD (`FW/src/sg_measure.c:4-7,22-30`; `FW/src/app.c:263-270`). Isso pressupõe a relação elétrica entre excitação e alimentação analógica; o firmware não mede AVDD independentemente.
- O monitor IN7 é retido por oito varreduras. Portanto a compensação é ratiométrica para variações suficientemente lentas, não simultânea à leitura dos seis canais. Uma mudança de excitação entre monitoramentos entra temporariamente no numerador com denominador antigo. A magnitude no equipamento depende de ripple, resposta do filtro analógico e referência de bias; não foi medida nesta auditoria.
- Uma excitação inválida gera registros inválidos/INCOMPLETE, mas o LCD continua com o último `vexc_v` válido (`FW/src/app.c:263-269`; `FW/src/gui.c:82`). Esse último valor não constitui prova de alimentação válida naquele instante.

## Filtros e banda útil

O processamento aplica um passa-baixas Butterworth de segunda ordem por transformação bilinear, depois média móvel de N pontos, depois arredondamento para 0,1 µε (`FW/src/app.c:126-145,267-269`). A média móvel não reduz o número de registros. Filtro OFF é passagem direta; média=1 elimina a média. Se o cutoff pedido alcançar Nyquist, limita a `0,45 × fs` (`FW/src/app.c:122-142`).

O filtro analógico é uma hipótese fixa de montagem: Sallen-Key aproximadamente 499 Hz/Q 0,742, descrito no README e impresso no header. O firmware não muda nem mede esse filtro (`FW/README.md:25`; `FW/src/app.c:193`; `FW/src/gui.c:86`). O próprio header declara `alias_protection=unverified`.

Consequências confirmadas pelo modelo configurado:

- MÉDIO/CUSTOM inicial, 6 canais: Nyquist previsto ≈192 Hz. Um sinal de 260 Hz já excede esse limite; o LP digital de 50 Hz ocorre depois da amostragem e não desfaz alias. Para um sinal de 80 Hz, a equação real do LP50, usando fs previsto, fornece ganho ≈−10,34 dB. Esse preset não representa uma banda plana de 80…260 Hz.
- RÁPIDO: apenas CH0 e CH1 ficam ativos; não é uma opção de seis canais rápidos. LP200 também atenua a extremidade de 260 Hz. A taxa nominal 15000 SPS do ADC não é a taxa de cada canal.
- QUASE-ESTÁTICO: LP5 e média de oito pontos reforçam a redução de banda e o atraso; a média contribui aproximadamente `3,5/fs ≈49 ms` de atraso de grupo, além do LP e do restante da cadeia.
- O filtro analógico de 499 Hz não oferece, por si só, forte rejeição acima de Nyquist nos modos de seis canais. Magnitude real de alias, ruído e banda dependem de bancada, não dos 24 bits nominais.

## SD, UART, CSV e completude

| Caminho | O que contém | Proteções e limites confirmados |
|---|---|---|
| SD `.SGB` | Header `#SG6BIN2`, seis linhas de calibração, trigger e `#ENDHDR`; registros binários de 46 bytes | CRC16 por registro, sequência SD iniciada em zero, tempo de boot em µs64, seis inteiros em 0,1 µε; não contém códigos ADC brutos nem footer de conclusão |
| SD `.CSV` | Header `#SG6CSV2`; `seq;t_us;ch0_ue;…;ch5_ue;vexc_mv;battery_mv;mask;flags` | Valores em µε com uma casa; inválido/inativo é literal `2147483647`; não possui CRC por linha |
| UART | Headers binários completos periódicos e registros BIN2 | Limite temporal ≤1000 publicações/s, sequência própria; pode omitir varreduras SD sem indicar gap |
| CSV produzido pelo PC | `seq,t_us,t_ms`, seis canais, excitação, bateria, máscara, flags, marca trigger e epoch | Separador `;`, inválido/inativo vira campo vazio; conversão SGB valida CRC e escreve por arquivo temporário antes de substituir destino |

Fontes: `FW/src/sg_record.c:3-19`; `FW/src/app.c:179-246,275-280,318-319`; `APP:32-55,123-152,173-211`. O layout CSV nativo e o CSV PC diferem; o conversor `--conv` aceita SGB BIN1/BIN2, não CSV nativo (`APP:123-151`). Headers carregam calibração em representação hexadecimal IEEE754, tipos de ponte, validade e configuração aplicada. Os registros armazenam resultados já calibrados/filtrados, de modo que uma alteração posterior de GF/bias/filtro não permite refazer o processamento a partir do SGB.

Os arquivos SD são criados com nomes exclusivos e prefixo de até três caracteres; o tempo de busca tem limite de dois segundos. Há checagem de escrita parcial, f_sync a cada segundo, f_sync/f_close no STOP e propagação de erro (`FW/src/app.c:173-177,205-228,300-318`). Data FAT é placeholder e `rtc_set()` apenas troca campos estáticos; não existe um relógio calendário avançando nesse driver (`FW/src/sd_diskio.c:15-23`).

A fila de aquisição tem 128 × 40 = 5120 bytes; há mais 3072 bytes para médias e 2048 bytes UART (`FW/src/app.c:44-66,109-111`; assert de layout em `tests/host/acquisition_harness.c:175-177`). A fila não é pré-trigger. Ao encher, interrompe aquisição, drena o que foi aceito, marca INCOMPLETE e fecha a gravação (`FW/src/app.c:99-104,262,300-318`). Falha de mídia impede continuar o restante da fila. Nenhuma dessas proteções garante recuperar dados ainda não persistidos em um corte de energia.

O SPI1 é compartilhado em foreground com LCD/toque e restaura a velocidade após cada operação SD (`FW/src/sd_diskio.c:43-61`). O driver aguarda cartão pronto por até 500 ms e tokens por até 200 ms; essas são esperas reais enquanto o ADC continua produzindo interrupções (`:11-13,63-78,152-166`). A GUI cede quando a fila passa de 32 varreduras (`FW/src/app.c:68`; `FW/src/gui.c:261-263`). Isso ajuda na carga média, mas não remove o bloqueio de um cartão ocupado.

## Trigger, pré e pós

O trigger é a subida de `valor_filtrado >= limiar`, no canal configurado; não usa módulo do sinal nem histerese. Marca bit `0x40` apenas naquele registro. `pre_s` e `post_s` ficam nos metadados e não comandam uma janela de RAM ou encerramento automático após o evento (`FW/src/app.c:273-280`; `FW/include/app.h:14-18`). O SD grava continuamente durante REC até STOP, duração configurada, bateria baixa ou falha.

No PC, `--event-time-us`, `--pre` e `--post` recortam por tempo; os limites são inclusivos. Na visualização ao vivo, uma marca recebida move a janela para o evento. O histórico é um deque de até 72000 registros; não captura dados anteriores à conexão/REC e não cria dados que a UART omitiu (`APP:155-170,179-209,359-403`). Há comentário explícito para usar SD/CLI se a janela antiga exceder o histórico live (`APP:398`). A janela PC não equivale a uma garantia física de pré/pós completo.

## Bateria e carregamento

O PB0/ADC1_IN8 lê um divisor 10 kΩ/10 kΩ. O ADC interno é calibrado na inicialização, mas o cálculo `raw × 6600/4095` presume VDDA de 3,3 V e resistores nominais; não consulta VREFINT nem medidor de carga (`FW/include/app_cfg.h:35-37`; `FW/src/main.c:157-172`; `FW/src/app.c:251-259`).

A aplicação consulta bateria a cada 50 ms no foreground. Menos de 3,50 V ativa aviso; menos de 3,35 V por 500 ms de leituras válidas solicita STOP/fechamento. Não controla uma chave de alimentação. A UI mostra `BATx.xx` junto de VEXC/SD e usa vermelho sob aviso (`FW/src/app.c:251-259,313-318`; `FW/src/gui.c:82`). A escrita em SD pode atrasar a consulta; a rotina de bateria não é uma supervisão independente de tempo real.

Não há pinagem, leitura de CHRG/STDBY, VBUS, corrente, estado de carga ou detecção de carregador no firmware ativo. A busca por bateria/carregamento nos includes e fontes encontra apenas tensão PB0, limiares, campos de log e UI. Portanto o display não sabe diferenciar “carregando” de “alimentado por bateria”, nem pode estimar carga percentual confiável apenas com esse código. A orientação existente é carregar OFF (`FW/README.md:31`); detecção de carga exigiria também um sinal físico disponível, não apenas uma nova legenda.

## Falhas e correções mínimas prováveis

### 1. Marca de trigger pode desaparecer antes de chegar ao PC — confirmado

**Local:** `FW/src/app.c:273-280`; consumidor `APP:390-395`.

**Caso concreto:** há uma publicação UART em t=1000 µs. Uma subida de limiar ocorre numa varredura processada em t=1700 µs: SD recebe `0x40`, mas o teste dos 1000 µs impede publicar esse registro. Em t=2400 µs o sinal segue acima, então a nova varredura não carrega `0x40` e é publicada. A sequência UART continua consecutiva; o PC não vê marca nem gap. Uma fila SD drenada em lote também pode deixar passar somente uma das varreduras do lote.

**Impacto:** o autoenquadramento live pode nunca selecionar o evento, mesmo com SGB contendo o trigger.

**Menor correção provável:** reter um único registro de trigger pendente com seu timestamp e publicá-lo no próximo slot UART disponível, antes de descartar essa informação; definir explicitamente o comportamento se houver múltiplas subidas nesse intervalo. Incluir um teste com a subida na varredura omitida, mantendo o limite de taxa. Se o produto só garantir triggers via SD, declarar esse limite expressamente no live.

### 2. EOF alinhado a registro não prova término íntegro — limitação confirmada já documentada

**Local:** `FW/src/app.c:209-214,300-306`; `APP:143-151,195-211`; reconhecimento atual em `FW/README.md:31`.

**Caso concreto:** arquivo termina depois de um registro completo seq=0 sem INCOMPLETE. O parser retorna normalmente; não existe footer/contagem final que distinga STOP de corte de energia antes do próximo registro. `sequence_gaps=0` e `incomplete_records=0` podem coexistir com fim perdido.

**Impacto:** CRC assegura integridade dos registros presentes, não duração/finalização integral do ensaio. Isso não é um novo defeito oculto: o README já declara o limite, mas o CSV de saída não o evidencia em seu status final.

**Menor correção provável:** imediatamente escrever no resultado PC `completion=unknown` para BIN2. Se for obrigatório certificar STOP, adicionar marcador final versionado/contagem de registros e validação no consumidor; não inferir conclusão a partir de EOF limpo.

### 3. Janela extraída pode faltar pré ou pós sem aviso — confirmado

**Local:** `APP:155-170,179-211`; origem dos limites em `FW/src/app.c:193,273-276`.

**Caso concreto:** trigger ocorre 0,1 s após iniciar REC com pré=0,5 s; faltam 0,4 s anteriores. Ou STOP ocorre antes de completar pós=2 s. O recorte só seleciona as linhas existentes e finaliza normalmente, sem informar o trecho indisponível.

**Impacto:** um arquivo legível pode ser confundido com janela completa, mesmo sem corrupção ou gap interno.

**Menor correção provável:** calcular primeiro/último timestamp disponível no recorte e registrar aviso/estado de janela parcial no CSV/live. Testar evento perto do início/fim e evento inteiramente fora dos dados. Isso não exige buffer MCU novo.

### 4. Cartão dentro do timeout do driver pode estourar a fila — risco calculado, ocorrência física não demonstrada

**Local:** `FW/src/app.c:64-68,99-104,230-244,290`; `FW/src/sd_diskio.c:11-13,63-78,152-166`.

**Caso concreto calculado:** no RÁPIDO previsto, uma espera SD de 150 ms aceita pelo timeout de 500 ms produz cerca de 193 varreduras enquanto cabem somente 128, mesmo partindo de fila vazia. No MÉDIO, 400 ms produz cerca de 154. O código para e sinaliza o overflow, mas perde a continuidade do ensaio.

**Impacto:** throughput médio suficiente e build aprovado não bastam para garantir gravação contínua em qualquer cartão.

**Menor ação corretiva:** medir latência máxima de escrita/sync no cartão e combinações permitidas, com LCD/toque ativos; documentar o limite de taxa/cartão validado. Se exceder a fila, reduzir carga/taxa ou redesenhar o armazenamento com base nessa medida. Aumentar 128 indiscriminadamente não cabe como conclusão sem orçamento de RAM/stack. Adicionar ao harness um f_write/f_sync que avance tempo e dispare aquisições verificaria o caso em host; hoje o mock não simula esse bloqueio.

### 5. Falha de leitura de bateria mantém tensão antiga sem indicar inválido — confirmado em caminho de erro

**Local:** `FW/src/app.c:251-259`; `FW/src/gui.c:82`; teste da reinicialização do temporizador em `tests/host/acquisition_harness.c:160-163`.

**Caso concreto:** após mostrar 3,7 V, HAL_ADC_Start/Poll/Stop falha repetidamente. `batt_mv` e `battery_warn` continuam com seus valores anteriores; `battery_low=false` cancela o período de 500 ms. A gravação pode continuar com supervisão de bateria indisponível e display/log aparentemente normais.

**Impacto:** ausência de medição atual não fica distinguível de bateria saudável; a parada preventiva não é garantida nessa falha.

**Menor correção provável:** manter estado de validade/idade, mostrar bateria indisponível e dar erro explícito após falha persistente. A política de parar REC sem uma leitura válida deve ser definida conforme o requisito de proteção. Não substituir leitura inválida por zero nem tratá-la como uma leitura boa.

### 6. Preset MÉDIO não atende seis canais de 80…260 Hz — confirmado pelo modelo; depende desse requisito

**Local:** `FW/src/app.c:29-37,116-145`; `FW/README.md:31`.

**Caso concreto:** seis canais no MÉDIO dão previsão de ≈384 SPS/canal com LP50. 260 Hz ultrapassa Nyquist e 80 Hz é atenuado em ≈10,34 dB só pelo filtro digital. RÁPIDO resolve parte da taxa usando dois canais; não entrega seis.

**Impacto:** se o objetivo do equipamento exigir essa banda em todos os seis canais, a seleção desse preset não pode validar o requisito. Se for banda quase-estática, não é falha funcional.

**Menor correção provável:** fechar a banda/canais exigidos, medir taxa e resposta ponta a ponta e então escolher DRATE/cutoff compatíveis. Se o ADS1256 multiplexado não cumprir, a correção passa por arquitetura de aquisição. Mudar somente a etiqueta “MÉDIO” ou desligar o LP não corrige alias.

## Cobertura dos testes e verificações desta revisão

Os testes existentes cobrem a sequência SPI/DRDY e temporização simulada (`tests/test_adc.py:34-39`), fórmulas e guardas (`tests/test_measure.py:26-28`), calibração e transportes (`tests/test_app_measure.py:26-28`), filtros/fila/máscaras/watchdog (`tests/test_acquisition.py:24-39`), escrita parcial/sync/close/duração/bateria/CSV64 (`tests/test_logging.py:8-12`), SD e barramento compartilhado (`tests/test_sd.py:39-47`) e CRC/layout entre C e Python (`tests/test_record.py:27-37`).

O visualizador testa fragmentação, CRC, ressincronização, headers completos, resets/epochs, memória limitada, atomicidade da conversão, janela inclusiva, log em thread separado do gráfico e erros de disco (`tests/test_visualizador.py`). O teste de fila de plot confirma que o log PC é escrito antes de descartar eventos gráficos (`:393-411`). Os testes de truncamento rejeitam registros parciais, mas não podem detectar EOF integral sem marcador de conclusão (`:129-136`).

Foram executadas nesta revisão apenas verificações em memória com o Python preparado, sem produzir fixtures/logs novos: cálculo das quatro taxas/fila/IN7, resposta do LP50 a 80 Hz e execução do `read_sgb()` real sobre header BIN2 + um frame seq=0/CRC válido via BytesIO. O EOF foi aceito com um registro e `INCOMPLETE=False`. Também foi executado o `StreamParser` real com duas publicações consecutivas sem marca: `sequence_gaps=0`, `trigger_marks=0`; o apagamento da marca entre elas é demonstrado pelo ramo de limitação UART do firmware, não por esse teste do parser isolado.

Esta auditoria não reexecutou a suíte completa: vários testes geram arquivos em `tmp`, figuras e reescrevem `tests/fixtures/record_v2.bin`. Não alterou esses artefatos. Os logs preexistentes não foram tratados como prova de aprovação do estado atual. Não houve upload, teste em placa, osciloscópio/analisador lógico, cartão físico, teste de carga/descarga, validação da resposta analógica ou medição de stack máxima.

**Veredito:** os caminhos possuem validação e tratamento de erro úteis, mas não demonstram aquisição contínua sem perda, janela completa de evento nem banda de seis canais até 260 Hz. A marca de trigger omitida na UART é o primeiro defeito lógico a corrigir; completude, bateria inválida e limites de armazenamento devem ficar explícitos antes de usar os arquivos como evidência integral de um ensaio.
