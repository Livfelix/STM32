# SG6 — módulo portátil para extensometria

Projeto de um módulo portátil de aquisição para seis canais de strain gauges, baseado na placa integrada ADS1256 + STM32F103C8T6. A montagem usa três canais de meia ponte, três canais de quarto de ponte, condicionamento com INA333/OPA2364, tela MSP2807, cartão microSD externo e alimentação por célula Li-ion 18650 protegida.

## Comece aqui

- [Visão geral e limites do projeto](Projeto_Modulo_Portatil_StrainGauge/README.md)
- [Manual final de montagem](output/pdf/SG6_Manual_Montagem_Revisado_2026-10-13.pdf)
- [Montagem passo a passo](Projeto_Modulo_Portatil_StrainGauge/MONTAGEM_PASSO_A_PASSO.md)
- [Conexões do sistema](Projeto_Modulo_Portatil_StrainGauge/CONEXOES_SISTEMA.md)
- [Pinagem da placa integrada](Projeto_Modulo_Portatil_StrainGauge/PINOUT_PLACA_INTEGRADA.md)
- [Lista de materiais](Projeto_Modulo_Portatil_StrainGauge/BOM_FINAL_ADS1256_MSP2807.xlsx)
- [Firmware PlatformIO](Projeto_Modulo_Portatil_StrainGauge/fw_pio/README.md)
- [Visualizador para PC](Projeto_Modulo_Portatil_StrainGauge/app/visualizador_tempo_real.py)
- [Checklist de validação física](Projeto_Modulo_Portatil_StrainGauge/REGISTRO_BANCADA.md)

## Compilar e testar

O firmware usa PlatformIO. A configuração está em `Projeto_Modulo_Portatil_StrainGauge/fw_pio/platformio.ini`.

```powershell
pio run -d Projeto_Modulo_Portatil_StrainGauge/fw_pio
python -m unittest discover -s tests -v
```

Na revisão publicada, a suíte possui 133 testes aprovados. Isso valida software e formatos; a montagem ainda precisa cumprir o checklist de bancada antes de uso experimental.

## Estrutura

- `Projeto_Modulo_Portatil_StrainGauge/fw_pio/`: firmware ativo.
- `Projeto_Modulo_Portatil_StrainGauge/app/`: visualização e conversão dos dados no computador.
- `Projeto_Modulo_Portatil_StrainGauge/esquema/`: diagramas elétricos revisados.
- `tests/`: testes host e arquivos de compatibilidade SGB/BIN.
- `uploads/`: datasheets e documentação dos módulos e componentes efetivamente usados.
- `docs/research/`: memória técnica das decisões e cálculos.
- `output/pdf/`: manual consolidado.

## Estado do hardware

Não energize a montagem apenas com base nos diagramas. Confirme continuidade, ausência de curto, pinagem da revisão física da placa, tensão de 5,00 V, VEXC, corrente da célula e os ensaios descritos em `REGISTRO_BANCADA.md`.
