# Firmware ativo SG6 - STM32F103C8 da placa integrada

PlatformIO/HAL,72MHz/HSE8MHz. Firmware entregue com testes em host e build; funcionamento físico exige protocolo de bancada. `bluepill_f103c8` identifica o mesmo chip da placa integrada, não uma segunda controladora. `../firmware/ads1256_6ch.c` é histórico, não gravar. Testes da raiz: `python -m unittest discover -s tests -v`, no ambiente preparado.

## Compilar e programar

Da raiz workspace, no ambiente preparado:

```powershell
$env:PLATFORMIO_CORE_DIR="$PWD/.tools/platformio"
.tools/venv/Scripts/python.exe -m platformio run -d Projeto_Modulo_Portatil_StrainGauge/fw_pio
.tools/venv/Scripts/python.exe -m platformio run -d Projeto_Modulo_Portatil_StrainGauge/fw_pio -t upload
```

Upload é instrução, não ação executada nesta revisão. ST-Link clone4SWDIO ->JTAG1-7,2SWCLK ->9,1RST ->15 se necessário, GND5/6 ->um par4..20. Saídas3,3V/5V do clone abertas; placa por J2-2+5V/J2-1GND; BOOT0/1=0. Tabela completa em `../PINOUT_PLACA_INTEGRADA.md`; não copiar5/7/13 do guia histórico.

## Operação entregue

MAIN imediata/fonte6x10. Calibrar toque; ganho físico final -> BIAS com W3 desconectado/VIN+ em VIN- -> SAVEFLASH e conferir FLASH/CONCLUIDO -> OFF/restaurar W3/religar -> conferir canal/tipo/GF/ganho, BIAS OK e bias_v_f32 restaurados -> ZERO explícito -> informar R SHUNT medida -> shunt manual -> retirada/retorno -> SAVEFLASH final. SAVE com erro ou configuração/bias não restaurados impedem continuar para ZERO; resolver e repetir BIAS/SAVE. Presets não mudam G100 físico inicial, GF/tipo/calibração. CH0..2H/CH3..5Q, nove gauges/seis entradas. REC bloqueia configuração; somente arquivo válido permite estado ativo. Procedimento completo no manual de montagem.

CALIB mostra R SHUNT aplicada: editor por dígito/0,1ohm,1..999999,9ohms, padrão100k incluindo330k medido. REC/shunt pendente bloqueiam edição. R é transitória e reinicia em100k; escala resultante permanece no SAVE existente, sem nova versão flash. Registrar Rreal/aplicada na ficha e reentrar após boot antes de calibrar. EST por canal usa as guardas reais ADC/INA e bias/zero/VEXC/GF/ganho/escala; sem validade fica indisponível. Estimativa exige bancada. MAIN REC mostra arquivo e minutos:segundos; erros têm prioridade.

SPI2 ADC modo1, PB12CS/PB13SCK/PB14DOUT/PB15DIN/PB10DRDY/PB11RESET,1,125MHz. SPI1 PA5/6/7 com CS PA4LCD/PA0touch/PA8SD; LCD/SD9MHz, toque~1,125MHz, initSD~281,25kHz. LCD5V/J1 aberto/IO3,3V, SDO aberto; SD3,3V emP9-1/P9-2. UART9216008N1 só PA9/P4-1 ->RX USB-UART3,3V eGND/P4-3; TX/VCC adaptador abertos. P4-2 recebe MAX232, não ligar outra saída.

ADS PGA1/VREF ADR4525=2,5V/AINCOM AGND/J3 aberto. Atenuação DC100/10k corrigida uma vez. Sallen-Key2,15k/2,15k/220n/100n, f0=499,08Hz/Q=0,74162/−3dB=522,26Hz, não Butterworth; monitor2,2k/2,2k buffer OPA emIN7, fator0,4950495, leitura a cada8varreduras e retenção entre leituras para correção de deriva lenta, sem garantia para ripple/transientes. VEXC sai deC1+ apósL1; não bypassar com boost bruto. Janela CJ/buffer e ganho/banda dependem de bancada, não de24bits nominais.

## Arquivos, memória e evidência

SD BIN3 frames AA56/46B/CRC e rodapé AA57/21B, nomes até3letras+5dígitos `.SGB` ou `.CSV`, criação exclusiva. Headers SD completos #SG6BIN3/#SG6CSV2; UART continua #SG6BIN2 +6CH +TRIGGER enabled +#ENDHDR; t_us64 de boot, tipo/GF/ganho/bias/zero/escala/validade, filtros eUARTmax1000. FATdate placeholder, sem RTC real. CSV nativo difere do CSV PC; layouts exatos no guia UI. SD contínuo/trigger marca subida/PC extrai janela, sem longo pré-buffer RAM. SD registra todas as varreduras aceitas; UART normal até1000quadros/s é subconjunto com sequência própria. Trigger ignora espaçamento1ms e usa reserva46B; falha de enfileiramento incrementa trigger_uart_lost, publicado no próximo #STATUS possível. Sequência só avança nos frames enfileirados. Fila overflow para e informa INCOMPLETE.

RÁPIDO exige BIN3 (`BIN REQUIRED / RAPIDO` para CSV); lote16×46B, fila128~99ms na previsão1289SPS/canal. Driver limita busy acumulado por escrita/sync a70ms durante REC RÁPIDO; outros prazos ficam próprios. `SDmax` na GUI mostra em ms o máximo por gravação do tempo decorrido de cada `f_write`/`f_sync` foreground (cabeçalho e fechamento incluídos). `app_sd_max_io_us()` e `#STATUS ... sd_max_io_us=N` expõem µs, saturados em UINT32_MAX; reset antes de abrir cada REC. Status inteiro é repetido até caber sem consumir a reserva46B do trigger. Operações com falha também contam, sem alterar sua classificação. `sd_max_busy_ms()` mantém o máximo somente de busy, arredondado para cima, separado do latch de timeout. Timeout dá `SD SLOW / INCOMPLETE`, interrompe aquisição e tenta sync/close. Validar cartão com LCD/toque durante duração máxima pretendida; não é garantia de latência total FATFS nem aquisição contínua universal.

Sync a cada1s/STOP/close verificados; partialwrite/timeout/cartão/nome falham explicitamente. Fechamento controlado drena fila/lote e escreve rodapé com contagem, último seq/tempo e status0STOP/1DURACAO/2BATERIA_BAIXA/3BAT_MONITOR_FAIL antes de sync/close. Conversor exige rodapé válido/coerente para BIN3; `--recover-incomplete` salva apenas registros completos válidos com aviso/clean_close=0. BIN1/BIN2 antigos permanecem aceitos sem comprovação de fechamento. Erro descoberto no sync/close final pode ocorrer depois do rodapé; conferir mensagem do aparelho e persistência em bancada. STOP/close antes OFF/SD.

OUT+ protegido -> chave -> SW_BAT+ -> boost; divisor10k/10k+100nF para PB0 sai de SW_BAT+ antes do boost. Ícone4segmentos/~% aproximado e tensão BAT; <3,50V500ms pisca/CARREGAR, <3,35V500ms em REC para/fecha. Monitor inválido500ms em REC fecha com BAT MONITOR FAIL; não auto-desliga chave. Carga sóOFF, LEDs TP4056 visíveis, sem CHRG/STDBY no MCU/estado CARREGANDO na tela. Sem PowerPath ouCAN. Filtro analógico fixo não assegura alias; MÉDIO LP50 atenua100Hz.

Linker `ld/STM32F103C8_config.ld`: código62KiB(63488B), RAM20KiB. Duas páginas config1KiB em0x0800F800/0x0800FC00, cópias versionadas CRC/readback/commit. Build limpo de integração de 16/10/2026: Flash 55320/63488 B (8168 B livres), RAM estática 15372/20480 B; fim alinhado _ebss em 15376 B (0x20003c10). Linker reserva heap mínimo 512 B e stack mínimo 2048 B; após alinhamento de 4 B, restam 2544 B além dessas reservas. Suíte host: 133 testes PASS; build sem warnings. Compiladores executados fora do sandbox após falha de inicialização no sandbox. A reserva do linker não mede pico de stack; high-water físico pendente. Evidência por tarefa em `.superpowers/sdd/sg6-integridade-bateria/`; nenhum flash/hardware executado. Timestamp CSV64 usa formatação decimal própria porque newlib-nano não suporta %llu; teste real C cobre0,4294967297 eUINT64_MAX.



