"""Contas da BOM em Python; exportação XLSX pelo Artifact Tool do runtime Codex."""
import json
from pathlib import Path
import subprocess


def shortage(necessary, confirmed):
    return None if confirmed is None else max(0, necessary-confirmed)


def build_bom():
    old = 'Compra anterior declarada'
    # Confirmado = declaração específica do usuário, não inspeção física.
    items = [
        ('board','Placa integrada ADS1256+STM32F103C8',1,'Adquirida',1,'J2 5V; Blue Pill avulsa reserva.'),
        ('gauges','Gauge350ohms',9,'Vários, sem limitação declarada',None,'3 pares de flexão + 3 locais. GF/lote conferir. 12 para seis meias.'),
        ('ina','CJMCU-333',6,'6 confirmados pelo usuário',6,'G físico100; Vref e3.3V abertos. Janela real medir.'),
        ('opa','OPA2364AIDR SOIC-8',4,'12 confirmados pelo usuário',12,'3 CIs para filtros + 1 monitor; oito reserva.'),
        ('soic','Adaptador SOIC-8 ou PCB equivalente',4,'Não informado',None,'Conferir disponibilidade e orientação dos quatro OPA.'),
        ('ref1k','Referência1k /0.1%',12,'Não confirmado para esta revisão',None,'Um par por canal, TCR compatível e acoplamento térmico. Não usar5%.'),
        ('ra350','RA350 /0.1%',6,old,None,'6 montados nos seletores, 3 bypassados. Lista antiga18; recontar valor/tolerância.'),
        ('selector','Seletor físico JP_QH',6,'Não informado',None,'W1 em RA(Q) ou VEXC(H). Exige trocar sensor/fiação e recalibrar; não é J3.'),
        ('filter_r','Resistor filtro2.15k',12,old,None,'2 por canal; conferir valor/tolerância reais.'),
        ('filter_220n','Capacitor filtro220nF',6,old,None,'C1 entre X e OUT, não GND. Conferir dielétrico/tolerância.'),
        ('filter_100n','Capacitor filtro100nF',6,old,None,'C2 entre Y e AGND; não inclui desacoplamento.'),
        ('monitor_r','Monitor VEXC2.2k /0.1%',2,old,None,'Par casado -> buffer -> IN7; fator nominal ADC0.49505.'),
        ('battery_r','Monitor bateria 10 kΩ',2,old,None,'SW_BAT+ pós-chave -> 10k -> PB0 -> 10k -> GND; 100nF em PB0. Medir par/VDDA.'),
        ('pull','Pull-up CS 10 kΩ',3,old,None,'LCD/toque/SD para 3,3V; descontar existentes. Com bateria: cinco 10k funcionais.'),
        ('shunt','Shunt100k /0.1%',1,old,None,'Aplicador manual reutilizável no sensor. Medir valor real; UI100k nominal.'),
        ('decouple_100n','Desacoplamento100nF INA/OPA',10,old,None,'6 INA + 4 OPA; conferir caps onboard.'),
        ('decouple_10u','Desacoplamento10uF INA/OPA',10,old,None,'6 INA + 4 OPA; polaridade/tensão conforme rail5V.'),
        ('extra_100n','Capacitor100nF extra',5,old,None,'Monitor VEXC, bateria, cluster, boost, SD.'),
        ('analog_bulk','Cluster analógico47uF',1,old,None,'Pós-L1; tensão nominal compatível com5V.'),
        ('boost_bulk','Boost100-470uF /10V',1,old,None,'Valor final depende de partida/estabilidade.'),
        ('sd_bulk','SD47-100uF',1,old,None,'Próximo ao módulo3.3V; medir transientes.'),
        ('lcd','MSP2807 + XPT2046',1,'Adquirida',1,'VCC5V/J1 aberto; IO3.3V; slot SD tela sem uso.'),
        ('sd','Módulo SD SPI3.3V',1,'Adquirido',1,'P9-1 VDD/P9-2 GND; PDF5V é outra variante.'),
        ('card','Cartão SDHC FAT32',1,'Não informado',None,'8-32GB inicial; validar latência/cartão; sem promessa de horas.'),
        ('uart','USB-UART3.3V /921600',1,'Não informado',None,'PA9 -> RX e GND; TX/VCC abertos.'),
        ('stlink','ST-Link V2 Mini',1,'Adquirido',1,'JTAG1 SWDIO7/SWCLK9/NRST15; clone não alimenta a placa.'),
        ('boost','MT3608',1,'Adquirido',1,'Regular5.00V; datasheet1.2MHz; medir ripple/corrente.'),
        ('charger','TP4056 carregador 1S',1,'Adquirido',1,'Carga só OFF; LEDs visíveis. Revisão/proteção e corrente de carga a conferir; sem PowerPath.'),
        ('protection','Proteção real Li-ion1S',1,'Não comprovada no carregador',None,'Conferir descarga/curto e OUT protegido; não presumir PowerPath.'),
        ('cell','Célula 18650 protegida + suporte',1,'Suporte declarado; célula pendente',None,'Um conjunto. Modelo/capacidade/proteção/encaixe pendentes; medir autonomia e picos.'),
        ('connector','Conjunto conector sensor 5 contatos',6,old,None,'6 pares painel/cabo = 12 metades; 0,50 mm², trava/alívio. Vista/pinout e família após medir caixa.'),
        ('cable','Cabo 4 vias + dreno, 0,50 mm², até 1 m',6,'Cabo blindado existente; recontar trechos',None,'3 ativas W1/W2/W3, quarta isolada nas duas pontas; dreno só no equipamento. 2 vias não atende.'),
        ('front','PCB/perfboard front-end',1,old,None,'Dimensões/furação após medir módulos; sem CAD de fabricação.'),
        ('case','Gabinete e chave ON/OFF',1,old,None,'Envelope proposto180x130x65mm; medir encaixe; chave após OUT+.'),
        ('install','Adesivo, terminais, proteção gauges',1,'Não confirmado',None,'Conjunto conforme material/temperatura; alívio de tração.'),
        ('assembly','Fiação, headers, espaçadores, prensa-cabos',1,old,None,'Conjunto após levantamento físico; cotas finais pendentes.'),
        ('ldo','AMS1117 externo',0,'Adquirido',1,'Reserva/alternativa SD após ensaio; nunca paralelizar saídas.'),
        ('can','TJA1050',0,'Adquirido',1,'Reserva; CAN não implementado.'),
        ('rg_external','Rg externo 10 kΩ',0,old,None,'Não montar: CJMCU usa ajuste interno. Não confundir com os cinco 10k funcionais.'),
        ('zener','Zener de entrada 3,3 V',0,old,None,'Não montar: fuga e limite inadequado não substituem janela ADC ou sequência OFF.'),
        ('or_diode','Diodo OR antigo 1N5819',0,old,None,'Não montar OR: queda nos rails; carga somente OFF, sem PowerPath.'),
    ]
    return [dict(id=i,component=c,necessary=n,declared=d,confirmed=s,
                 missing=shortage(n,s),reserve=None if s is None else max(0,s-n),note=t)
            for i,c,n,d,s,t in items]


def main():
    product = Path(__file__).resolve().parent
    bundle = Path.home()/'.cache/codex-runtimes/codex-primary-runtime/dependencies/node'
    node = bundle/'bin/node.exe'
    if not node.exists() or not (bundle/'node_modules/@oai/artifact-tool').exists():
        raise SystemExit('Runtime Codex Node/Artifact Tool indisponível; BOM existente preservada.')
    qa = product.parent/'tmp/sg6-task5-artifacts'
    qa.mkdir(parents=True,exist_ok=True)
    junction = qa/'node_modules'
    if not junction.exists():
        subprocess.run(['powershell','-NoProfile','-Command',
                        f"New-Item -ItemType Junction -Path '{junction}' -Target '{bundle/'node_modules'}' | Out-Null"],check=True)
    data = qa/'bom-data.json'
    data.write_text(json.dumps(build_bom(),ensure_ascii=False,indent=2),encoding='utf-8')
    subprocess.run([str(node),str(product/'tools/bom_artifact.mjs'),str(data),
                    str(product/'BOM_FINAL_ADS1256_MSP2807.xlsx'),str(qa)],check=True)


if __name__ == '__main__':
    main()
