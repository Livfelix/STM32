# Manual de fabricação, calibração e operação SG6

Revisão atual15/10/2026. Ler conexões/pinagem antes de soldar. Configuração inicial: CH0..2 H, CH3..5 Q, nove gauges350ohms, seis CJ G100, PGA1, quatro OPA, monitor bufferizado. As etapas abaixo são instruções; aprovação física depende de preencher `REGISTRO_BANCADA.md` com instrumentos e dados reais.

## 1. Separar materiais, identificar e medir

1. Recontar6 CJ,12 OPA (usar4/reservar8) e pelo menos9 gauges. Conferir marcação SOIC-8, pino1, lote, resistência, GF do lote e aparência. Fotografar revisões e etiquetar CH0..5 nos módulos/cabos. Adaptadores SOIC/PCB própria são necessários, ainda não confirmados.
2. Abrir a BOM. Separar12 resistores1k/0,1% de referência,6 RA350/0,1%,12x2,15k,6x220n,6x100n dos filtros, pares2,2k do monitor,10k da bateria/pull-ups, shunt100k e desacoplamentos. Compra antiga não comprova quantidades/tolerâncias dos novos valores. Medir e registrar pares de referência; seleção térmica/TCR é requisito, sem atribuir TCR ao lote não identificado.
3. Conferir proteção real Li-ion1S, célula/capacidade, suporte, carregador e OUT protegido. TP4056 sozinho é carregador, não prova proteção nem PowerPath. Separar USB-UART3,3V que sustente921600, cartãoSDHC FAT32, quatro adaptadores SOIC, seis conjuntos completos de conectores5contatos (painel + cabo, 12 metades), seletorJP_QH por canal, seis trechos até1m de cabo 1×4×0,50 mm² com blindagem total + dreno, adesivo/terminais/proteção adequados ao corpo de prova.
4. Com paquímetro, medir placa integrada, CJ, adaptadores, bateria/suporte, MT3608/altura do trimpot, conector, SD/furos e raio mínimo de curva do cabo escolhido. Esses dados ainda faltam; não furar caixa usando fotos sem escala.

## 2. Gabinete e fabricação mecânica

| Peça | Cota documentada ou proposta | Ação |
| --- | --- | --- |
| MSP2807 PCB | 50x86mm | Conferir lote |
| Furos MSP2807 | Diâmetro3,20mm, centros44x74,08mm | Usar desenho oficial; não centros50x86 |
| Área ativa da tela | 43,2x57,6mm | Projetar janela após medir moldura/toque |
| Altura MSP com header | 12,78mm | Reservar ainda espaço real do chicote |
| SD adquirido | 21x18x3,5mm, dois furos | Medir furos e acesso ao cartão |
| Gabinete externo | Envelope proposto180x130x65mm | Não é CAD liberado nem encaixe comprovado |
| Placa integrada/CJ/bateria/boost | Cotas completas pendentes | Medir antes do gabarito/furação |

1. Fazer primeiro um gabarito em papel/cartão com medidas reais, suporte da célula e conectores instaláveis. Ensaiar abertura da tampa sem puxar chicotes, acesso ao SD, carga, UART, chave e trimpots. Não inventar folgas, paredes ou posições de furos finais.
2. Marcar frente/verso e posição do pino1 de cada header. Fixar LCD sem comprimir filme de toque/FPC; eletrônica em espaçadores isolantes e suporte mecânico adequado. Verificar cabeças de parafusos contra trilhas e isolamento da bateria/contatos.
3. Posicionar entradas analógicas longe de boost/correntes LCD/SD. Manter pares de referência juntos termicamente; não colocar um resistor perto de fonte quente e o outro longe. Reservar alívio de tração dos seis cabos e fixação da célula. Sem promessa de vedação/IP.
4. Só após prova física produzir gabarito definitivo/caixa. Desbastar e limpar cavacos antes de instalar PCB. Etiquetar CH0..5, H/Q,5V J2, carga somenteOFF, LEDs TP4056 visíveis CARREGANDO/CARGA TERMINADA, RX/GND PC, cartão e STOP antesOFF. Não publicar footprint/CAD como final sem medir.

## 3. Continuidade e energia, ainda sem sensores

1. Sem bateria, confirmar J2-1=GND/J2-2=5V, P9-2=GND/P9-12=VBAT, P5 pares=AGND, R16/AINCOM paraAGND, JTAG1-7/9/15 atéPA13/PA14/NRST. Não usar pinagem do PDF português. Remover J3 do potenciômetro, BOOT0/1=0. Confirmar J1 da tela aberto.
2. Soldar/adaptar quatro OPA conferindo pino1 e orientação. Alimentação pino8=VEXC/pino4=AGND. Para cada CI eCJ, conferir100n+10u próximos, polaridade/tensão. Não unir saídas, Vref ou3.3V dos CJ.
3. Fora da caixa, ajustar MT3608 a5,00V sem carga com fonte limitada em corrente; desligar antes de ligar módulos. OUT+ protegido ->chave ->SW_BAT+ ->boostIN+; OUT- protegido ->boostIN-. OUT boost sóJ2-2/LCD1; OUT- sóJ2-1/LCD2. Nunca ligar OUT+ diretamente aC1+.
4. Usar fonte de bancada com limitação apropriada e adicionar cargas gradualmente. MedirJ2/C1+/VDD/TP1, queda sob carga, ripple com osciloscópio/ponta curta e aquecimento. Registrar valores reais e condições, não marcar "OK" porque um número esperado consta no guia.
5. RetirarVEXC deC1+ pós-L1, AGND deP5 para cluster analógico. Conectar monitor2,2k/2,2k+100n+bufferOPA4A emIN7 e bateria SW_BAT+ ->10k ->PB0 ->10k ->GND, com100nF em PB0/GND. O topo sai após a chave, antes do boost. Medir PB0 e VDD ao desligar e em carga OFF; conferir descarga dos100nF. Não instalar zener de3,3V como remendo. Conferir escala dos monitores com multímetro antes de depender da parada por bateria.

## 4. Digital e firmware

1. Conectar LCD/touch/SD pelos rótulos e tabela completa da pinagem. SD3,3V emP9-1/P9-2; slot SD da tela sem uso. SDO9 do LCD aberto. IO3,3V. Pull-upsCS10k. Confirmar liberaçãoMISO pelo SD. Não removerR3 da tela como solução de MISO.
2. Compilar `fw_pio` e programar porST-Link: SWDIOclone4 ->JTAG1-7, SWCLK2 ->9, RST1 ->15 se necessário, GND5/6 ->par4..20. Saídas3,3/5V clone abertas; placa porJ2. A gravação no hardware não foi executada nesta revisão.
3. O boot abreMAIN diretamente, sem splash. Calibrar toque emMENU, depois salvarFLASH explicitamente foraREC. Verificar leitura/configuraçãoADC, estadosSD, texto, botões e erro. Se carga/configuração não é válida, resolver aviso; não ocultar com tara automática.
4. Testar ADC com tensões conhecidas dentro da janela segura do buffer/PGA1, depois alternância de canais e acoplamento entre pares. Comparar código/tensão ao instrumento e observar varredura sequencial. Não aplicar tensão acima do limite com base apenas no fundo de escala teórico5V.
5. Antes de sensores reais, montar um canalQ com resistor350 no lugar do gauge e umH com dois350, conferir meio~VEXC/2, ganho físico e excursões nos dois sentidos. G100 é informado no firmware e ajustado no trimpot: selecionar preset não ajusta hardware. A janela software provisória0,15..3,15V não prova a janela real CJ/ADS.

## 5. Fabricação do front-end de cada canal

1. Montar referência própria RC/RD1k iguais, sérieVEXC->RC->S-->RD->AGND. S- paraVIN-. InstalarRA350 eJP_QH: centroW1, Q=saídaRA, H=VEXC. Registrar resistências medidas/posição por canal. Não usar3x1k com gauge350.
2. CJ VCC=VEXC/GND=AGND/VIN+=W3/VIN-=S-/Vref e3.3V abertos. ConectarVout ao filtroR1, não diretamenteADC. NenhumRG externo.
3. MontarINAOUT->2,15k->X->2,15k->Y->OPA+IN;100nY/AGND;220nX/OUT; -IN=OUT; OUT->P5 INn. Verificar realimentação e continuidade antes de alimentar. MapearOPA1CH0/1, OPA2CH2/3, OPA3CH4/5, OPA4A monitor; metade4B seguidorMEX sem carga.
4. Conector5contatos:1W1,2W2,3W3,4isolado,5dreno. Confirmar numeração pela vista do fabricante, trava/alívio e terminação para0,50mm² após medir caixa. Conferir ausência de curto entre contatos e corpo de prova. No cabo, W1/W2 mesma bitola/comprimento, W3 separado para sense. Dreno sóAGND no equipamento, extremidade sensor isolada.
5. CH0..2H: W1 topo do gauge superior; W3 midpointM; W2 terminal inferior. JP_QH emH bypassaRA. CH3..5Q: W1/W3 ao terminalA do único gauge; W2 aoB; JP_QH emQ. Na unidade, VIN+ é somenteW3, sem ligação local ao nó apósRA.
6. Alimentar um canal por vez e medir sinais/temperatura, depois os seis. Nove gauges, seis entradas; IN6 aberto/reserva. Toda trocaQ/H exige trocar sensor/fiação real e posiçãoJP com aparelhoOFF, confirmar tipo noMENU e repetirZERO/shunt. Se ganho mudar, repetirbias também.

## 5.1 Guia rápido: cabo Q

1. Aparelho OFF; identificar três vias W1/W2/W3 do cabo instalado de quatro vias + dreno. Isolar a quarta via nas duas pontas; dreno só no SHIELD/AGND do equipamento, isolado no sensor.
2. Colocar JP_QH em Q: VEXC -> RA350 -> W1 -> terminal A do gauge; terminal B -> W2 -> AGND. Levar W3 separado de A remoto até VIN+; não unir W1/W3 junto ao INA.
3. Conferir contatos 1=W1, 2=W2, 3=W3, 4=reserva, 5=dreno pela vista do conector. Medir continuidade/isolamento; duas vias + dreno não atendem.
4. Configurar Q, conferir ganho/GF e executar BIAS/SAVE/restauração, ZERO e shunt/retorno conforme seção7. Shunt entre A/B no sensor.

## 5.2 Guia rápido: cabo H

1. Aparelho OFF; usar as mesmas três vias ativas do cabo de quatro vias + dreno. Isolar quarta via e dreno no sensor; ligar dreno só no equipamento.
2. Colocar JP_QH em H, bypassando RA: VEXC -> W1 -> topo do gauge superior; base do inferior -> W2 -> AGND; meio M dos dois gauges -> W3 -> VIN+.
3. Conferir os mesmos cinco contatos e isolamento. Gauges compatíveis em faces opostas da mesma seção, inferior na face tracionada para sinal positivo.
4. Configurar H, conferir ganho/GF e executar BIAS/SAVE/restauração, ZERO e shunt/retorno conforme seção7. Shunt entre M/base do gauge inferior no sensor. H mede flexão e reduz axial comum.

## 6. Posicionamento, colagem e proteção do gauge

1. Definir antes a grandeza: flexãoH ou deformação localQ; marcar seção, eixo longitudinal, faces, distância/posição e orientação. ParaH, dois gauges compatíveis em faces opostas da mesma seção e alinhados ao eixo de deformação. Inferior na face tracionada para sinal positivo na convenção adotada. Acesso só a uma face pedeQ, nãoH improvisado.
2. Preparar superfície conforme instrução do fabricante do gauge/adesivo e material: desengordurar, preparar acabamento, remover resíduos e manter área seca. Não inferir tempo de cura, temperatura, solvente ou pressão sem ficha do adesivo escolhido. Registrar produto/lote e condições.
3. Medir resistência do gauge antes e após instalação e isolamento elétrico da peça com instrumento/procedimento compatíveis; inspecionar grade e terminais. Colar sem dobra, manter alinhamento/posicionamento e cumprir cura real. Não aplicar força de solda/chicote diretamente à grade.
4. Soldar fios em terminais apropriados, proteger conexão e dar alívio de tração antes da área sensível. ParaH, juntar os terminais que formamM conforme identificação do par; verificar W1/M/W2 pela resistência. ParaQ, W1/W3 chegam aA separadamente. Isolar dreno e quarto fio no sensor.
5. Aplicar proteção ambiental compatível com material/temperatura sem criar esforço parasita. Manter parH em temperaturas semelhantes; gradiente, GF/resistência diferentes e colagem desigual reduzem compensação. Autoaquecimento~17,86mW por gauge em5V permanece; referência1k economiza no divisor, não no sensor.
6. Estabilizar térmica e mecanicamente antes da calibração. ApostilaSG páginasPDF24-27 discute ambiente/temperatura/excitação,29-32 pontes; blindagem não elimina erro resistivo ou deriva dos fios. Nenhuma recomendação genérica substitui ficha de instalação do gauge adquirido.

## 6.1 Ajustar G100 com multímetro e shunt

É possível ajustar o ganho físico sem gerador de milivolts. Com a ponte real descarregada, medir VEXC e CJ Vout diretamente com multímetro. Aplicar shunt100k medido ao gauge inferior H (M para terminal inferior remoto/W2), ou ao único gauge Q (A/B). Medir a nova saída e calcular a diferença em relação à leitura sem shunt. Para dois braços350ohms ideais, o alvo G100 é deltaVout = -VEXC*100*350/[2*(350+2*100000)]; em5V, -0,436736V. Ajustar cuidadosamente o trimpot até essa diferença, mantendo ambas as leituras sem clipping. Medir no CJ Vout: não aplicar o fator1,01 da atenuação ADC ao multímetro. GF e casamento da referência não alteram essa diferença elétrica ideal; valores reais dos braços/shunt/cabos, temperatura e erro do instrumento alteram a realização prática.

Remover shunt, conferir retorno, informar o ganho físico final no menu e repetir: BIAS -> SAVEFLASH confirmado -> OFF/restaurar W3/religar -> conferir configuração e bias restaurados -> ZERO -> SHUNT -> retirar/confirmar retorno -> SAVEFLASH final. Não conservar bias/calibração anteriores após ajustar o trimpot. Se houver saturação ou impossibilidade de ajustar, investigar a janela real CJ antes de usar os dados. A expectativa é nominal, não prova de exatidão mecânica.

## 7. Calibração completa por canal

Fazer fora de REC, na condição de referência definida. Registrar GF do lote, resistência, ganho físico, topologia, cabos, temperatura e shunt. O padrão GF2,08 é apenas um valor inicial. Trocar ganho invalida bias/zero/shunt; tipo invalida zero/shunt; GF invalida shunt. Falha de medição preserva a calibração anterior.

1. **Bias independente e SAVE obrigatório:** com aparelho OFF, desconectar W3 de VIN+ e isolar a ponta. Colocar ponte temporária VIN+ para VIN- no midpoint S- (~2,5V), sem curto entre rails ou ligação das entradas ao terra. Ligar, selecionar CHn em CALIB e executar BIAS/confirmar. Logo após cada BIAS, executar SALVAR FLASH fora de REC e verificar a mensagem **FLASH / CONCLUIDO**, antes de desligar. Se SAVE falhar, não prosseguir para OFF/restauração como se a medição estivesse salva: resolver a falha e salvar com sucesso. Anotar canal/tipo/GF/ganho, BIAS OK e o valor de bias (palavra bias_v_f32 no #CHn dos metadados). BIAS sozinho só altera RAM/metadados. A leitura nominal1,65V não separa desequilíbrio da ponte de offset do INA.
2. **Restaurar e verificar após religar:** somente após SAVE confirmado, desligar, remover a ponte VIN+/VIN-, reconectar apenas W3 a VIN+, conferir e religar. Antes do ZERO, selecionar o mesmo canal e conferir tipo/GF/ganho restaurados, **BIAS OK** e bias_v_f32 igual ao anotado antes de OFF. Se houver aviso de configuração inválida, falta de validade ou bias/configuração antigos, interromper e repetir BIAS/SAVE corretamente; não executar ZERO com bias perdido. Nunca executar ZERO/shunt com VIN+ ainda preso à referência. Estabilizar temperatura/carga.
3. **ZERO explícito:** confirmar a condição sem carga adicional e executar ZERO do canal selecionado, ou a ação MAIN para os ativos. Cancelar preserva o anterior. A tara fica em razão normalizada pela excitação, não em volts legados. Não rezerar ao trocar preset ou reiniciar uma série de creep.
4. **Shunt medido:** fora de REC, tocar R SHUNT e informar a resistência medida. Editor: DIGITO +1 cicla0..9, PROXIMO DIGITO avança; último=0,1ohm. APLICAR valida1,0..999999,9ohms; CANCELAR preserva anterior. Padrão100000,0, incluindo opção330000,0 e valor real330123,4. Registrar Rreal e valor aplicado mostrado em CALIB/confirmar. Instalar manualmente o mesmo resistor em paralelo ao gauge Q (A/B) ou inferior H (M/terminal inferior), no sensor. Selecionar SHUNT do canal correto; conferir sinal negativo, resposta suficiente e ausência de SAT. Entrada é transitória e volta a100k no boot; escala resultante é salva. Reentrar antes de nova calibração após religar. Edição bloqueada enquanto qualquer shunt aguarda retirada/retorno; não trocar resistor nessa etapa.
5. **Retirada/retorno:** remover o resistor, aguardar estabilidade e confirmar REMOVE/RETURN. A escala só é confirmada após retorno; critério inicial software±50µε, pendente validação. Não fazer novo ZERO para esconder retorno incorreto. Saturação, sinal insuficiente ou falha não substituem a escala anterior.
6. **Salvar:** repetir canais, executar SAVEFLASH fora de REC, reiniciar e conferir restauração sem tara automática. Conferir tipos e máscaras de validade nos metadados. REC exige bias/zero válidos nos canais ativos; concluir também o shunt para comissionamento.
7. **Validar mecanicamente:** usar carga e geometria conhecidas, registrar incerteza, repetibilidade, histerese, alinhamento, adesão e drift. Shunt elétrico verifica a cadeia naquele ponto, não exatidão mecânica completa.

Modelo linear: Rsh=R*Rcal/(R+Rcal), delta_x=Rsh/(R+Rsh)-1/2. Para350ohms/100k/GF2,1, referência Q=-1663,755µε e H=-831,878µε. Em G100/VEXC5V, deltaVout=-0,436736V em ambos. O equivalente exato de resistência (-1660,854µε) é outro modelo, não deve ser misturado com o conversor linear. Para GF2,08 a referência muda. Q usa4*delta_ratio/GF; H usa2*delta_ratio/GF; depois escala shunt. Não há garantia para grande deformação plástica, par desigual ou gradiente térmico.

Para ganho físico alto, considerar330k medido para reduzir excursão e evitar clipping, mantendo o procedimento físico/retorno. Exemplo ideal350ohms/VEXC5V/G1000:100k exige deltaVout=-4,367V;330k exige-1,325V. São expectativas elétricas, não garantia da janela CJ. Na fórmula de ajuste do ganho, substituir100000 pela Rreal.

CALIB apresenta EST do canal após bias/zero válidos: mesma cadeia de razão/strain e guardas INA0,15..3,15V/ADC buffer (raw<=VEXC-2,05V), ganho físico, VEXC atual, GF e escala. Registrar estimativa e validar excursões em bancada; indisponível sem validade. Não usar como faixa mecânica garantida.

## 8. Guia rápido: operação autônoma no SD externo

1. Conferir protocolo físico aceito, caixa/cabos, bateria sob carga e cartão FAT32 com espaço/nome livre. Inserir cartão com OFF. Ligar e checar H/Q, calibração, VEXC/bateria, SD, ausência de SAT/erro e taxas P(prevista)/M(medida)/R em SPS/CH. R atualmente repete M, sem medir persistência no SD de forma independente. Texto `uE` significa µε.
2. **QUASE-ESTÁTICO:** seis canais, DRATE500, digital5Hz/média8 para evolução lenta. Creep/axial em Q; H mede flexão e reduz axial comum. Média8 mantém um registro por amostra, mas atenua e atrasa. Não zerar silenciosamente durante a série.
3. **MÉDIO:** seis canais,7500, LP50/média1, para oscilação dentro da banda validada. LP50 atenua100Hz; não é modo de amplitude plana100Hz. Para esse objetivo, escolher CUSTOM com canais/taxa/corte apropriados e validar amplitude, fase e alias em frequência conhecida.
4. **RÁPIDO:** CH0/1,15000, LP200/média1, trigger de subida CH0+500µε, pré0,5/pós2s. Taxa prevista1289SPS/canal, BIN3 obrigatório; CSV direto no SD bloqueado (`BIN REQUIRED / RAPIDO`). Lotes16×46B reduzem escritas pequenas. Busy acumulado de escrita/sync limitado a70ms por operação; fila128 cobre cerca de99ms. `SD SLOW / INCOMPLETE` para aquisição e tenta sync/close: preservar arquivo e rejeitar o cartão para este modo até validação. Inicialização e operações fora de REC têm prazos próprios. Anotar `SDmax` da tela e `sd_max_io_us` do `#STATUS`/log PC: máximo por gravação de cada chamada foreground `f_write`/`f_sync`, incluindo cabeçalho e fechamento, em ms na tela e µs no log. Reinicia no próximo REC; capturar o log conectado para reter a medição. Não mede abertura, `f_close` ou pausa total entre consumidores. Busy via `sd_max_busy_ms()` e causa de timeout são métricas separadas. Validar latência e margem da fila com LCD/toque durante a duração máxima pretendida. Ponto inicial de transientes, não banda garantida. A varredura é sequencial; timestamp comum não prova simultaneidade ou fase entre canais.
5. **CUSTOM:** selecionar1..6 canais, uma das16DRATE, corte/média/trigger e APPLY. PGA1 fixo. Conferir corte efetivo e Nyquist por canal. O filtro analógico é fixo: f0=499,08Hz, Q=0,74162, −3dB=522,26Hz; não Butterworth e não prova banda útil; controlar conteúdo fora da banda na origem e validar alias. Ganho físico também afeta banda/headroom INA; não editar G sem ajustar trimpot/calibrar.
6. Definir BIN3 recomendado (obrigatório no RÁPIDO), prefixo até3 caracteres A-Z/0-9, duração UI0..86400s (0 contínua) e trigger. SAVEFLASH explícito se desejar persistir. Nome exemplo ENS00001.SGB; sem RTC real, FATdate é placeholder. Registrar identificação/horário real externamente.
7. Estabilizar, definir ZERO deliberadamente se necessário, pressionar GRAVAR e confirmar arquivo/REC/contador minutos:segundos em MAIN (erro tem prioridade). Configuração bloqueada em REC. Trigger marca um cruzamento; SD continua gravando e pré-evento permanece no arquivo, não em longo buffer RAM C8.
8. PARAR/STOP, aguardar confirmação de fechamento/ausência de REC, depois OFF e retirar cartão. Bateria abaixo de3,50V por500ms pisca o ícone/percentual aproximado e mostra CARREGAR; abaixo de3,35V por500ms durante REC fecha com BATERIA BAIXA. Monitor inválido por500ms também fecha (`BAT MONITOR FAIL`). Validar margem de fechamento; o firmware não desliga a chave. Falha ADC/SD/fila avisa INCOMPLETE: preservar arquivo e investigar.

## 9. Guia rápido: PC ao vivo e análise posterior

1. Aparelho na bateria; ligar somente P4-1 PA9 para RX USB-UART3,3V e P4-3 para GND. TX/VCC do adaptador abertos. Não carregar durante operação nesta versão. Identificar COM; exemplo com porta/nome a ajustar:

```powershell
python Projeto_Modulo_Portatil_StrainGauge/app/visualizador_tempo_real.py --port COM5 --baud 921600 --log pc_novo.csv
```

2. Ao vivo exige pyserial/matplotlib; conversão offline usa biblioteca padrão. `--log` cria arquivo exclusivo; existente é erro. Padrão V2 valida CRC; UART antiga exige `--legacy-v1`. Não foi executado teste físico de COM. Fechar janela/Ctrl+C encerra leitor/arquivo; falhas de serial/disco são visíveis.
3. Conferir header completo com seis CH, TRIGGER e #ENDHDR. Antes dele, metadados desconhecidos/epoch0. Header parcial não substitui anterior. Snapshots completos e texto bruto ficam preservados; variação só da VEXC medida mantém epoch/histórico. Mudança estável de configuração/calibração ou BOOT/reset/tempo regressivo separa épocas; wrap uint32 normal continua. CSV emite #PC_EPOCH/metadados antes do primeiro registro da fronteira. Eventos ao vivo não misturam boots; na API de seleção sobre registros combinados, informar epoch. Registros46B/0,1µε no protocolo; PC mostra µε. Sequências SD/UART independentes; UART normal até1000quadros/s é subconjunto temporal; frames de trigger ignoram o espaçamento1ms e usam reserva de46B. Se não enfileirados, o contador cumulativo `trigger_uart_lost` é emitido no próximo `#STATUS` possível. A sequência conta somente frames enfileirados; não deduzir ausência de perdas de trigger só por sequência contínua. CSV PC não representa aquisição integral SD.
4. Leitor grava antes de enfileirar gráfico; `plot_drops` conta descartes de visualização sobrecarregada. Histórico até72000 registros visuais; evento antigo pode sair da história, recuperável pelo SD. Conferir contadores CRC/gaps/resets/headers/incompletos, H/Q e SAT. Tempo é relativo ao boot.
5. Depois STOP/close/OFF, copiar e preservar SGB original. Converter ou extrair janela:

```powershell
python Projeto_Modulo_Portatil_StrainGauge/app/visualizador_tempo_real.py --conv ENS00001.SGB -o ensaio.csv
python Projeto_Modulo_Portatil_StrainGauge/app/visualizador_tempo_real.py --conv ENS00001.SGB --out evento.csv --event-time-us 10000000 --pre 0.5 --post 2
```

6. Evento em microssegundos de boot; pré/pós em segundos; limites inclusivos. Defaults vêm da janela aplicada nos metadados, fallback0,5/2 no legado; overrides CLI prevalecem. Conversor valida todos os registros, inclusive fora da janela, rejeita CRC/truncamento/header incompleto, e só substitui saída após sucesso. Mesmo input/output ou hardlink é recusado.
7. CSV PC: `seq;t_us;t_ms;ch0_ue;ch1_ue;ch2_ue;ch3_ue;ch4_ue;ch5_ue;vexc_mv;battery_mv;mask;flags;trigger_mark;epoch`. Inativos vazios, unidade µε, metadados brutos em comentários e #PC_STATUS. CSV nativo SD tem outras colunas/sentinel2147483647/sem CRC por linha; detalhes no guia UI. Não converter CSV SD como SGB.
8. V1 offline é autodetectado, com aviso de ausência de CRC, máscaras sparse/identidade, tempo wrap e ambiguidade da marca antiga. Recupera somente o efetivamente registrado; não inventa calibração, canais ou cronologia perdidos. BIN2 legado tem CRC por registro, mas não prova fechamento; novos ensaios usam BIN3 no SD e V2 na UART.
9. Identificar saturação/incompletos/inativos na análise; preservar metadados, período ZERO e registros de colagem/cabos/temperatura. Distinguir H flexão e Q local, atraso/filtro e varredura sequencial. Validar amplitude/fase/alias por ensaio, não só Nyquist. TCC Silva páginas PDF60/62/63 usa outra arquitetura ADC12bits/oversampling; PDF67/70 relata discrepâncias mecânicas. Não transferir sua taxa/exatidão ao SG6.

## 10. Integridade, duração e liberação

BIN3 exige header #SG6BIN3, registros AA56/46B com CRC e rodapé AA57/21B coerente com contagem/últimos seq e tempo. STOP/duração/baixa bateria/falha de monitor fecham com status0/1/2/3. O firmware drena a fila, escreve o lote final e rodapé, sincroniza e fecha. EOF alinhado sem rodapé continua incompleto. Falha SD/fila/ADC já detectada impede rodapé limpo; um erro descoberto só no sync/close final pode ocorrer após o rodapé. Confirmar também ausência de erro no aparelho. CRC/rodapé não garantem persistência física do cartão/FAT nem a janela pré/pós completa.

Conversão normal valida o arquivo inteiro antes de substituir a saída. Para arquivo BIN3 interrompido, preservar o original e pedir recuperação explicitamente:

```powershell
python Projeto_Modulo_Portatil_StrainGauge/app/visualizador_tempo_real.py --conv ENS00001.SGB -o recuperado.csv --recover-incomplete
```

Recuperação exporta somente registros completos válidos, com CRC válido; marca `clean_close=0` e aviso visível. Pode pular um frame completo inválido, mas não reconstrói registros parciais nem ressincroniza bytes inseridos/removidos. Não apresentar esse CSV como ensaio concluído. BIN1/BIN2 antigos continuam aceitos com suas limitações. Sync cada1s e STOP antes de OFF/cartão continuam obrigatórios; cortes podem perder dados e danificar FAT.

BIN3 volume aproximado=46*varreduras/s*3600bytes/h, mais header e rodapé21B; seis valores num frame, não multiplicar novamente por6. Usar taxa medida, não DRATE. Sem segmentação automática: planejar duração/espaço abaixo do limite de arquivo FAT32~4GiB, parando/abrindo novo arquivo quando necessário. Autonomia de planejamento usa `t_h=(3,7*capacidade_Ah*0,80*0,85)/(5*corrente_5V_A)`, com carga total média hipotética0,22–0,32A em5V, 80% da energia nominal aproveitável e boost85%:

| Capacidade | Faixa calculada |
| --- | --- |
| 2000 mAh | 3,15–4,57 h |
| 2600 mAh | 4,09–5,95 h |
| 3000 mAh | 4,72–6,86 h |
| 3500 mAh | 5,50–8,01 h |

Medir capacidade útil, corrente, picos, eficiência, temperatura e margem de close; as faixas não garantem horas de campo. O cenário chega a cerca de0,562A na célula em3,35V, antes dos picos. Identificar e dimensionar célula/proteção/chave/fiação para esse consumo e partida/SD.

Carga somente OFF. Usar terminais identificados do TP4056/proteção conforme revisão real; ajustar corrente de carga à célula e dissipação. Deixar LEDs visíveis e rotulados CARREGANDO/CARGA TERMINADA, sem CHRG/STDBY no STM32. A tela fica desligada durante carga; os LEDs não são medidor de capacidade.

Liberar campo após protocolo de rails/janela CJ, calibração e carga mecânica, banda/fase/alias/skew, SD+UART+touch e latência/overflow, baixa bateria/fechamento e autonomia. Instrumentos: multímetro adequado, fonte limitada, osciloscópio, referência de tensão/sinal, carga/geometria conhecidas, ST-Link/PC/USB-UART, paquímetro, lupa e ferramentas de instalação. Falta de equipamento fica pendente; número esperado nunca substitui medição.
