# Anotações técnicas do laboratório (português)

Estas são as anotações originais, feitas durante o desenvolvimento, com dados pessoais removidos. Ficam aqui para quem quiser entender os detalhes.
O passo a passo atualizado para compilar está no README principal; alguns comandos antigos citados aqui (`pacote/build.sh ...`) foram substituídos pelo `compose.yaml`.

- `raio-x-completo.md`: hardware, MCUs, partições, serial/JTAG, serviços, SWUpdate.
- `placa-mae.md`: fotos, botões RES/LOAD (FEL), UART0, SWD.
- `recuperacao-usb.md`: reset `wipe_all` e gravação por cabo com o PhoenixSuit.
- `repositorio-oficial-creality.md`: o que a Creality publicou (Hi_Klipper) e o que é fechado.
- `kernel.md`: como o código do kernel foi achado e validado, e como foi o porte do Wi-Fi.
- `pacote-1a.md`, `pacote-1b.md`, `pacote-2.md`: a evolução dos pacotes, com os defeitos achados e corrigidos.
- `versao-completa-1.2.0.0.md`: a versão completa (alfa).
- `hi-painel.md`: o Hi Painel (tela própria): investigação, ajustes de 13/11 (ventoinha, G29, Imprimir), da tela da Creality, proporção, rotação, touch, dados ao vivo, calibrações, Wi-Fi, queda de energia, modo boot e as decisões com o usuário.
- `calibracoes.md`: varredura das calibrações e nivelamento da Hi, otimizações aplicadas (z_tilt, malha 7x7, limpeza do bico, shaper mzv, G29 com temperatura, imprimir com malha salva), desempenho, serviços, backup/como voltar atrás, o que falta testar e fontes.
