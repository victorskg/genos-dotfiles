# Wallpaper

Imagens paradas ficam em `~/.local/share/genos/wallpapers`. A imagem ativa é o link `~/.local/state/genos/wallpaper`. O `swaybg` desenha esse arquivo no modo `fill`: a imagem cobre cada monitor e o excesso é cortado. Os arquivos de imagem não entram neste repositório.

Sem arquivos nesse diretório, o desktop permanece na cor do abismo da paleta. A tela de login não desenha wallpaper. O `hyprlock` continua com o screenshot escurecido.

## Trocar a imagem

| Teclas | Ação |
| --- | --- |
| Super+W | Próxima imagem, em ordem de nome |
| Super+Shift+W | Imagem anterior |

Os mesmos passos no terminal:

```bash
wallpaper next
wallpaper prev
```

`wallpaper` sem argumentos equivale a `next`. Uma notificação mostra o nome do arquivo que passou a ser exibido.

`wallpaper list` lista a coleção. O arquivo ativo fica marcado com `*`.

## Acrescentar uma imagem

Os tipos aceitos são `png`, `jpg`, `jpeg` e `webp`.

```bash
wallpaper set ~/Downloads/foto.png
```

`set` copia o arquivo para a coleção e passa a exibi-lo. Um arquivo que já está na coleção é só selecionado.

```bash
wallpaper add ~/Downloads/foto.png
```

`add` apenas copia. A imagem atual permanece.

Copiar um arquivo direto para `~/.local/share/genos/wallpapers` também serve. O próximo `wallpaper next` passa a incluí-lo.

## Início da sessão

No login, o Hyprland executa `wallpaper show`. Esse comando restaura o link. Se o link não existir e o diretório tiver imagens, o primeiro nome é selecionado. Recarregar a config do Hyprland não reinicia o `swaybg`.

O `install.sh` instala o `swaybg` e cria o link `~/.local/bin/wallpaper` para `~/.config/hypr/wallpaper`.
