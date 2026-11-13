# Pacote 1b: "Hi Unleashed p1b" (versão OTA 1.1.0.81)

- **Gerado em 12/11/2026 ~22h45**. Arquivo: `pacote/saida/CR4NU200360C20_ota_img_V1.1.0.81.img`, 129.303.040 bytes, SHA-256 `2c6dbd191a4d182c26d6660985256e7ac228ee82f4ca44df121dd3697103f28c`.
- **Para reconstruir:** `pacote/build.sh 1.1.0.81 "Hi Unleashed p1b"`.

## Tudo do 1a, mais as correções que apareceram na instalação do 1a

- **`hi-ajustes` e `hi-diag` esperam o UDISK em segundo plano.** A Creality monta o UDISK tarde, e no 1a eles gravavam sob o ponto de montagem. Agora o `hi-ajustes` reinicia o Klipper se mudar configs depois que ele já subiu.
- **printer.cfg de fábrica com o PID do bico de 10/11 no corpo** (25,866/2,395/69,836). A autoverificação pode não calibrar o bico, e o PID comentado deu o erro key335.

## Novidades

- **Moonraker 165d4ddd (22/08/2026, API 1.5.0)**, a última versão que roda no Python 3.9.
  - Fica em `/usr/share/moonraker-hi/`. As dependências puras estão em `vendor/`: tornado 6.5.6 sem as extensões em C, dbus-fast 2.21.3 puro, apprise, jinja2 etc.
  - As extensões compiladas vêm do sistema: lmdb (migração), Pillow, streaming-form-data e libsodium.
  - Usa `-d /mnt/UDISK/printer_data` e a config `/usr/share/moonraker-hi/moonraker-hi.conf`, com as mesmas opções da Creality mais a câmera.
  - **Banco:** migra sozinho do LMDB para SQLite (`moonraker-sql.db`). O `data.mdb` continua lá.
  - **Volta ao Moonraker da Creality:** `touch /mnt/UDISK/hi-moonraker-antigo && /etc/init.d/moonraker restart`. O antigo continua em `/usr/share/moonraker`.
  - **Testado na própria Hi em paralelo (porta 7129) antes de entrar no pacote:**
    - Klipper conectado e "ready", sem componentes com falha;
    - migração do banco real;
    - as consultas de um painel externo (`printer/objects/query`, histórico) e o WebSocket (101).
  - O um painel externo do autor usa o WebSocket do Moonraker da Hi. A API é compatível, e ele já roda com o Moonraker moderno da Neptune.
- **Fluidd v1.38.0** (antes v1.27.0), em `/usr/share/fluidd`.
- **Câmera no Fluidd:** o nginx encaminha `/camera/` → `127.0.0.1:8000/` e `/call/` → `127.0.0.1:8000/call/`, a página WebRTC da Creality.
  - No Moonraker: `[webcam Creality Hi] service: iframe, stream_url: /camera/`.
  - O vídeo vai direto da impressora por WebRTC. Funciona por qualquer endereço da Hi.

## Validação

- Estrutura igual à oficial; o rootfs ficou com 123 MB, cabendo nos 300 MB da partição.
- No ARM emulado (chroot no rootfs gerado):
  - `nginx -t` aprovado;
  - `sh -n` em `moonraker`, `hi-ajustes` e `hi-diag`;
  - Moonraker subindo com 22 componentes, inclusive a `webcam`.

## Fica para depois (precisa de teste com a impressora mexendo e o usuário presente)

- **KAMP (malha e purga adaptativas):** o `BED_MESH_CALIBRATE` da Hi é substituído pelo `prtouch_v3` (fechado). É preciso testar se ele respeita MESH_MIN/MAX/PROBE_COUNT antes de colocar.
- **Timelapse:** a Creality já tem o dela (`delay_image`). O moonraker-timelapse precisa de um endpoint de foto (snapshot), que a câmera WebRTC não oferece.
- **Mainsail:** opcional, não incluído.
- Limpeza da camada gravável: depois do reset `wipe_all` ela ficou limpa. Fica só a regra para o futuro.

## Como instalar

Pendrive FAT32 com o arquivo na raiz e sem renomear; confirmar na tela. A instalação vai para a metade A, porque a atual é a B. Depois, conferir:
- `http://<IP-DA-HI>:4408` (Fluidd novo, com a câmera);
- `logs/hi-ajustes.log`;
- a versão 1.1.0.81.
