# Tela cheia de vídeo no Zen

O botão de tela cheia de um vídeo no X (`x.com`) expande o vídeo e ele volta ao tamanho original em menos de um segundo. A janela do Zen permanece em 1892x1008, `fullscreen` 0. Super+F continua sendo outro caminho: ele alterna a tela cheia da janela no Hyprland e esse caminho funciona.

Ambiente em que isso foi visto: Hyprland 0.56.2, Zen 1.23.2b (Firefox 157, build 20261009073723), Wayland nativo (`MOZ_ENABLE_WAYLAND=1`, classe `zen`). Monitores `eDP-1` e `HDMI-A-1`, os dois em 1920x1080 com escala 1. O perfil é `~/.config/zen/3mpivwxq.Default (release)/`.

## O que foi verificado

Um perfil Zen vazio, na mesma sessão, fica em tela cheia de verdade. O clique em `requestFullscreen` envia `xdg_toplevel.set_fullscreen`. O Hyprland responde com `configure(1920, 1080)` e a janela permanece em modo 2. A página reporta `fullscreenElement` com `outerWidth` 1920. O compositor não recusa o pedido.

No perfil de uso, o player do X desmonta o vídeo e o remonta no tamanho original. Com `full-screen-api.ignore-widgets` ligado, o Zen não envia `set_fullscreen`. Sem essa preferência, a janela também não passou a 1920x1080.

`zen-themes.json` está vazio e `chrome/zen-themes.css` não tem regra. Não há mod nem CSS de usuário nessa falha.

O `xulstore.json` grava `main-window` como 948x1012, `sizemode` `maximized`. O Hyprland mostra a mesma janela em 1892x1008. A regra `open-alone` move a janela para um workspace vazio depois do map, então o primeiro `configure` pode ser metade do tile (948x1012) antes da janela ir para o workspace vazio. Corrigir esse tamanho gravado não foi testado como correção.

## Soluções tentadas

`user_pref("full-screen-api.ignore-widgets", true)` no `user.js` do perfil. O Firefox copiou o valor para `prefs.js`. O Zen foi reiniciado com a preferência ativa. O vídeo do X continuou voltando. Essa é a correção publicada para Firefox e Zen em compositor de tiling: o vídeo preenche a janela em vez do monitor. Aqui ela não segura o player.

O mesmo `user_pref` foi acrescentado ao `install.sh` para perfis já existentes. Foi removido. Numa instalação nova o perfil ainda não existe, então o laço não faria efeito mesmo.

Busca no GitHub do Zen, no Bugzilla e no fórum do Hyprland. Os outros casos publicados não descrevem esta máquina: mod ou CSS que corta o vídeo, escala fracionária no segundo monitor, ou um compositor que descarta o `set_fullscreen`. Nenhum desses está presente. Forçar X11 com `MOZ_ENABLE_WAYLAND=0` aparece como contorno em outros bugs. Não foi aplicado: um perfil vazio já fica em tela cheia no Wayland nativo.

Não ficou nenhum ajuste de perfil, regra ou instalador por causa desta falha.
