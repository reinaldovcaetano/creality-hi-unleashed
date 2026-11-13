# Recuperação da Hi (USB e gravação pela placa)

Tudo o que é usado aqui está em `creality-oficial/recuperacao-usb/` e `creality-oficial/firmware-V1.1.0.50/`, baixado em 12/11/2026 do wiki e do GitHub oficiais da Creality.

## Nível 1: reset de fábrica por pendrive (sem cabo)

Fonte: wiki.creality.com/en/cr-series/creality-hi/factory-reset-usb. Os prints estão em `imagens-guia-hi-wipe_all/`.

- **Quando usar:** a impressora não chega à tela inicial, ou os serviços ficam reiniciando.
- **Atenção:** **apaga todos os dados** (logs, histórico e arquivos) e volta para o estado de fábrica. Isso desfaz todos os nossos ajustes.

Passos:
1. Criar no pendrive (raiz) um arquivo vazio chamado exatamente `wipe_all`, **sem extensão**. No Mac: `touch /Volumes/<PENDRIVE>/wipe_all`.
2. Colocar o pendrive na impressora ligada. O sistema apaga o arquivo e faz o reset.
3. Em uns 20 segundos ela reinicia e entra na autoverificação.

## Nível 2: gravação pela placa com PhoenixSuit ("linha de gravação", FEL)

A Creality não tem uma página só da Hi. O procedimento oficial usado aqui é o da **K2 Plus** (wiki.creality.com/en/k2-flagship-series/k2-plus/firmware-flashing), que tem o mesmo chip Allwinner **T113** e o mesmo tipo de imagem (`t113_i_linux_<placa>_uart0_<versão>.img`). Os prints estão em `imagens-guia-k2plus/`.

### Material

- **Computador com Windows 10.** O PhoenixSuit só roda no Windows. No Mac, só com uma máquina virtual que repasse a USB, ou com ferramentas livres como `sunxi-fel` ou `xfel`, que não foram testadas.
- `PhoenixSuit_EN.msi` (Allwinner, 15 MB). SHA-256 `18037a73…6ffd`.
- `UsbDriver_v1.0.0.3.zip`: rodar o `InstallUSBDrv.bat` ou o `InstallUSBDrv.exe`. SHA-256 `b7d173da…2e66`.
- **Cabo Micro USB** (de dados), ligado na porta USB da placa-mãe.
- **Imagem da Hi:** `firmware-V1.1.0.50/t113_i_linux_CR4NU200360C20_uart0_V1.1.0.50.img`.
  - O código da placa tem que ser **CR4NU200360C20**. A imagem da K2 (`cr0cn240110c10`) **não** serve.
  - É a única versão de imagem uart0 da Hi que a Creality publicou, na release do GitHub.

### Passos (adaptados da K2 Plus)

1. Instalar o driver USB.
2. **Desligar a impressora** e abrir a tampa da placa-mãe.
   - Se o conector Micro USB ficar apertado, soltar os parafusos da placa. Não forçar.
   - Se tirar o cabo flat (FPC), recolocar bem alinhado.
3. Entrar em modo de gravação: **segurar o botão FEL, apertar o RST por 1 segundo e soltar, depois soltar o FEL.**
   - Ainda falta confirmar onde ficam os botões FEL e RST na placa da Hi. Fotografar a placa quando abrir.
4. No Gerenciador de Dispositivos deve aparecer **"USB Device(VID_1f3a_PID_efe8)"**. Se aparecer "Dispositivo desconhecido", o driver não instalou.
5. Abrir o PhoenixSuit, carregar o `.img` da Hi e começar.
   - **YES:** apaga os dados do usuário (recomendado para recuperação).
   - **NO:** só grava o firmware e mantém os dados.
6. Não desligar, não desconectar e não apertar o RST durante a gravação, que leva menos de 30 segundos.

### Depois de gravar

A impressora volta para a **1.1.0.50 de fábrica**. Depois dá para atualizar pela tela ou pendrive para a .60 ou a .74, e reaplicar os nossos ajustes (o futuro "pacote de correções").

## Por que isso importa para o firmware próprio

Com o nível 2 confirmado (botões FEL e RST localizados, driver funcionando), um firmware próprio que falhe deixa de ser o risco de perder a impressora. Antes de testar firmware próprio:
1. Localizar os botões FEL e RST na placa da Hi.
2. Ter um PC com Windows (ou uma máquina virtual) com o driver instalado.
3. Testar que a placa aparece como `1f3a:efe8`, mesmo sem gravar nada.
