#!/bin/sh
# Abre o Hi Painel: pausa a tela da Creality (SIGSTOP) e garante que ela volta (SIGCONT) ao sair, mesmo em erro.
P=$(pidof display-server)
volta() { [ -n "$P" ] && kill -CONT $P; }
trap volta EXIT INT TERM HUP
[ -n "$P" ] && kill -STOP $P
/tmp/hi-painel >> /tmp/hi-painel.log 2>&1
