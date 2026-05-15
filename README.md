# DoomGeneric on STM32MP1 (Wayland SHM backend)

Ce fork cible **STM32MP1 / OpenSTLinux** avec un backend Wayland minimal en **SHM** (sans SDL, sans EGL, sans OpenGL).

## Objectif

Faire tourner Doom sur Weston/Wayland avec un écran 480x272 en envoyant un framebuffer partagé (`wl_shm`) au compositeur.

## Prérequis

### Machine de build (WSL/Linux)

- Toolchain cross ARM: `arm-linux-gnueabihf-gcc`
- Sysroot STM32 disponible (exemple: `$HOME/stm32-sysroot`)
- `make`, `scp`

### Cible STM32MP1

- OpenSTLinux
- Weston lancé en session utilisateur `weston`
- Socket Wayland active (souvent `wayland-1`)
- Un fichier WAD (Freedoom ou Doom)

## Build (cross-compilation)

Depuis le dossier `doomgeneric/` du projet:

```bash
make -f Makefile.wayland_shm \
  CC=arm-linux-gnueabihf-gcc \
  CFLAGS="-DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 -I$HOME/stm32-sysroot/usr/include -I$HOME/stm32-sysroot/usr/include/arm-linux-gnueabihf" \
  LDFLAGS="-L$HOME/stm32-sysroot/usr/lib/arm-linux-gnueabihf -Wl,-rpath-link,$HOME/stm32-sysroot/usr/lib/arm-linux-gnueabihf" \
  LIBS="-lwayland-client -lffi"
```

Le binaire généré est `doomgeneric`.

## Copie vers la carte

Exemple:

```bash
scp doomgeneric weston@172.19.165.127:/tmp/
```

## Installation de Freedoom (IWAD)

### Option A (recommandée): installer via paquets OpenSTLinux

Sur la carte:

```bash
su -
opkg update
opkg install freedoom
```

Ensuite, repérer les IWAD installés (chemin variable selon image):

```bash
find / -type f \( -name "freedoom*.wad" -o -name "doom*.wad" \) 2>/dev/null
```

### Option B: copier un WAD depuis le PC

Sur votre machine de dev, téléchargez un IWAD Freedoom (Phase 1, ex: `freedoom1.wad`) depuis le projet officiel:

- https://freedoom.github.io/

Puis copiez-le sur la carte:

```bash
scp freedoom1.wad weston@<IP_CARTE>:/home/weston/
```

## Lancement sur STM32MP1

Connectez-vous en utilisateur `weston` (ou basculez-y), puis:

```bash
su - weston
export XDG_RUNTIME_DIR=/run/user/1000
export WAYLAND_DISPLAY=wayland-1
cd /tmp
./doomgeneric -iwad /home/weston/freedoom1.wad
```

Si tout est OK, vous verrez des logs du type:

- `Scaling: Doom 320x200 -> 435x272, offset 22,0`
- `Wayland SHM init OK: 480x272`

## Vérifications utiles

Wayland actif:

```bash
find /run/user -name "wayland-*"
```

Weston actif:

```bash
ps aux | grep weston
```

Logs Weston:

```bash
tail -f /home/weston/weston.log
```

## Dépannage

- Écran noir au démarrage:
  - Vérifier `XDG_RUNTIME_DIR` et `WAYLAND_DISPLAY`
  - Vérifier que le socket existe bien (`wayland-1` vs `wayland-0`)
- Erreurs de link au build (`-lwayland-client`, `-lffi`):
  - Vérifier `LDFLAGS` et le sysroot
- Pas d'affichage via SDL/EGL sur certaines configs:
  - Ce backend SHM n'utilise ni EGL ni OpenGL

## Notes

- Résolution logique Doom recommandée: `320x200`
- Fenêtre cible: `480x272` avec upscaling nearest-neighbor
- Ce port est volontairement minimal et orienté robustesse sur Weston
