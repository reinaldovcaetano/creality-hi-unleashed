# pacote/local/ (dados pessoais, fora do git)

Copie esta pasta para `pacote/local/` e coloque, se quiser:

- `authorized_keys`: sua chave **pública** SSH (ex.: `cat ~/.ssh/id_ed25519.pub > pacote/local/authorized_keys`).
  Permite entrar com `ssh -p 2222 root@<ip-da-impressora>` sem senha.
- `save_config.txt`: o bloco `#*# <--- SAVE_CONFIG --->` do fim do `printer.cfg` da **sua** impressora
  (PID, input shaper, malha). Ele só é usado se, depois de um reset, o `printer.cfg` estiver sem calibração.

A pasta `pacote/local/` está no `.gitignore` e nunca é publicada.
