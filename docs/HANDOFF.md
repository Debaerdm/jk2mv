# Passation : l'état après l'intégration du 2026-10-05

Ce fichier dit où en est la branche locale `integ5` et comment continuer sur la machine Windows du propriétaire. Le détail par phase est dans docs/ROADMAP.md (une liste « Avancement » par phase), les cvars et commandes dans CVARS.rst.

## Où en est la branche

- `integ5` part de `claude/explore-codebase-A8IMd` (0d90b42) et n'a pas été poussée. L'historique est linéaire, sans commit de fusion. Les branches d'origine sont intactes.
- Les 17 branches du deuxième tour de revue y sont, aucune n'a été écartée :
  - correctifs de revue : `fix5-testscene`, `fix5-render`, `fix5-dlights`, `fix5-fov`, `fix5-demotools`, `fix5-capture`, `fix5-tools` ;
  - piste serveur : `sv2-huffman-final` (SV-2), `sv3-snapring-final` (SV-3), `sv7-world` (SV-7), `bm7-serverstats` (BM-7), `sv13-usercmd` (SV-13), `sv11-server-demos` (SV-11) ;
  - `spawn-windows-handles` (ffmpeg n'hérite plus que de ses handles), `vq7-renderscale` (VQ-7), `ui-presets-menu` (preset dans Setup > Vidéo) et `fe16-ghoul2-tests` (FE-16).
- Commits ajoutés à l'intégration :
  - les démos serveur utilisent le test de sortie de l'anneau de SV-3 (`SV_SnapshotEntitiesRolledOff`) ;
  - la sonde « menu principal » de `smoke_render` suit la taille des captures de `r_renderScale` ;
  - `test_cl_presets` déclare `r_dlightPriority` et `r_ext_alphaToCoverage` (latched) comme le renderer ;
  - CVARS.rst et deux commentaires reprennent les notes basses des relectures (formulations seulement).
- Choix d'intégration : `preset` redémarre le rendu une seule fois, juste avant que la frame suivante soit dessinée (mécanisme de `ui-presets-menu`). Il remplace l'insertion immédiate de `vid_restart` de `fix5-demotools` et garde ce qu'elle visait : ce qui suit un `wait` tourne avec le preset, et une chaîne de presets ne redémarre qu'une fois. En plus, une cvar latched réglée juste après le preset est appliquée avec lui, et Apply Changes dans les menus ne fait qu'un `vid_restart`.
- Messages de commit retouchés à l'intégration, comme les relectures le demandaient : SV-3 (les deltas trop anciens pour l'anneau du client, ce que 64 veut dire) et FE-16 (quels scénarios ont un condensé `world`).

## Ce qui a été vérifié

- **Build propre** (Ninja, MSVC 2022 x64 Release) : aucun nouvel avertissement. Restent les C4311/C4312 de src/mvsdk/tools/rcc et l'avertissement de q3lcc sur src/mvsdk/code/ui/ui_main.c:7793, tous deux dans mvsdk.
- **Tests unitaires** : 778, tous verts. Les 3 tests Ghoul2 sur Kyle sont sautés sans assets ; avec `JK2MV_TEST_BASE` qui pointe sur un base\ retail, `test_ghoul2` passe 22/22.
- **Tests de fumée** `smoke_render` et `smoke_client` : verts contre le build Windows (vraies fenêtres et OpenGL du GPU, voir plus bas). Ils n'ont pas tourné sous Linux ni dans la CI.
- **En jeu** (assets retail 1.04, binaires de `integ5`) :
  - une démo rejouée en timedemo avec `preset classic` donne la même vue 3D au pixel près que les binaires de la base ; seules les jauges animées du HUD changent, autant qu'entre deux lancements du même binaire ;
  - `preset enhanced`, puis `preset ultra` avec `r_renderScale 1.5` (rendu en 960x720 avec MSAA 8x, HDR et bloom, affiché en 640x480) ;
  - un script en partie live : `preset ultra` suivi de `seta r_ext_multisample 2` donne un seul redémarrage en MSAA 2x, et `preset enhanced; preset ultra` un seul aussi ;
  - menus : Setup > Vidéo affiche Classic, on choisit Ultra, Apply Changes, et le popup Yes donne `cl_preset ultra` avec un seul `vid_restart` ; la page Advanced montre ensuite MSAA 8x, post-processing, HDR et bloom ;
  - outils démo : chemin de caméra de 5 clés (`cam_load`, `cam_play`, ralenti), `photomode`, `demo_freecam`, et `video` (150 images à 30 fps) ;
  - `tests/video/av_sync.py` : le son de chaque tir démarre 30,4 ms après le début de l'image du tir, à 125 comme à 20 fps (l'amorce du MP3 du Bryar) ;
  - serveur dédié, ffa_bespin à `sv_fps 40` avec 31 bots et un client : `serverstats`, `sv_statsLog 1`, `svrecord all`, `map_restart 0`, changement de carte, `clientstats`, environ 3 minutes. Moyenne 6,2 à 6,8 ms par frame, p99 de 29 à 32 % du budget, aucun snapshot de repli ;
  - deux de ces démos serveur (celle d'un bot, et celle du client, qui traverse le `map_restart`) se rejouent jusqu'au bout sur le client intégré et sur celui de la base, avec les mêmes nombres de frames ;
  - compatibilité : le client de la base sur le serveur intégré (connexion, changement de carte), et le client intégré sur le serveur de la base (entrée en jeu, `map_restart`, changement de carte).

## Ce qui reste

- **Feuille de route :** la ligne « Reste » de chaque phase dans docs/ROADMAP.md. En bref :
  - BM-3 et BM-4 (baseline et images de référence sur ta machine), dont dépendent les critères chiffrés ;
  - le glow dans les vues miroir et portail (VQ-3), le readback PBO et libjpeg-turbo (PX-12), un test scripté des refus hors démo (critère 2 de la phase 2) ;
  - VQ-11 ;
  - FE-3, FE-4, FE-5, RB-4/RB-3 et RB-5 ;
  - BM-8/9, SV-5, SV-14b et BM-13/14.
- **Protocole 1.05 :** la conception (docs/PROTOCOL-1.05.md) attend toujours les réponses du propriétaire à ses 5 questions ouvertes avant toute implémentation.
- **CI :** `build.yml` et `tests.yml` (workflow_dispatch) n'ont pas tourné sur ce code. Les valeurs de référence de FE-16 sous Linux GCC, macOS arm64 et v141_xp, et les seuils de `smoke_render` sous llvmpipe, n'ont été vus que sous Windows.
- **Notes basses des relectures, non traitées** (tout le reste est intégré) :
  - SV-11 :
    - `SV_ValidDemoName` accepte les noms de périphériques Windows (`nul`, `com1`…) et un point final ;
    - après une erreur d'écriture ou un bloc perdu à 64 Mo, le thread continue d'écrire puis ajoute la marque de fin (trou au milieu du fichier) ;
    - `qpath` en `MAX_QPATH` peut tronquer un nom automatique de 65 caractères ;
    - `svstoprecord all` ne marque pas les démos auto pas encore ouvertes ;
    - un client non pur rejeté peut laisser une démo d'un snapshot ;
    - le démarrage d'une démo (ouverture du fichier, gamestate, messages console) se fait encore dans la frame serveur, et `svrecord all` démarre tous les clients dans la même commande ; seule l'écriture des snapshots passe par le thread ;
    - deux mutants survivent aux tests (M9, W3).
  - SV-3 : envoyer un snapshot complet quand le delta dépasse ce que l'anneau du client garde ; les tests ne compilent pas le code moteur qui appelle `sv_snapshot_ring.h`.
  - SV-13 :
    - la borne inutile de sv_usercmd_rate.cpp:97 ;
    - le message développeur des réponses `disconnect` perdues ;
    - pas de seau global contre une inondation à sources usurpées ;
    - la glu moteur n'a pas de test.
  - BM-7 : les appels de `SVStats_SetPacing` dans `SV_FrameMsec` n'ont pas de test unitaire.
  - SV-2 : les tests `HuffmanBenchmark` tournent à chaque ctest (1,4 s) ; `Huff_offsetTransmitTable` et `Huff_offsetReceiveTable` ne servent qu'aux tests ; SV-8 devra traiter les globales de msg.cpp.
  - VQ-7 :
    - en 32 bits, une capture énorme qui ne trouve pas sa mémoire termine le jeu (`Z_Malloc`), et `video` alloue un tampon par image ;
    - aucun test automatique de la console ni des graphes à une échelle autre que 1 ;
    - après un `vid_restart` pendant que le renderer est arrêté, polices et icônes restent blanches (défaut de la base, correctif proposé dans la relecture).
  - Menus :
    - pas de test de la garde du cache contre `cvar_restart` ;
    - après un réglage latched fait à la console, les copies des pages retail montrent la valeur courante (comme en retail) ;
    - le crochet de clic des relectures n'est pas dans l'arbre ;
    - « FX system out of effects » après un `vid_restart` au menu principal puis une partie locale mise en pause (sans doute ancien) ;
    - `r_renderScale` et `r_dlightPriority` ne sont pas sur la page Advanced.
  - Rendu :
    - le flou vidéo sans RGBA16 rendable n'a plus de repli 8 bits et réessaie à chaque image ;
    - l'alpha-to-coverage d'un mod avec un alpha de sommet constant inférieur à 1 laisse passer une partie du fond ;
    - la glu (table du grade, image liée après `GL_State`) n'a pas de test ;
    - `GL_INVALID_VALUE` au premier rechargement du renderer avec MSAA 8x, HDR et bloom sous `r_ignoreGLErrors 0` (défaut de la base).
  - Dlights :
    - le rayon de 32 unités qui reconnaît une lumière n'est pas couvert par les tests ;
    - `R_DlightProgram` efface les erreurs GL en attente ;
    - un seul historique pour toutes les scènes monde d'une frame.
  - FOV : la glu cgame n'a pas de test ; un mod qui lit son `cg_fovAspectAdjust` par nom ne s'élargit pas.
  - Outils démo :
    - `cam_play` et `photomode` dans la même frame partent d'une vue périmée ;
    - la bande où la squad passe au slerp (-0,8 à -0,9) pourrait commencer plus près de 180° ;
    - le durcissement de `QuatSlerpAligned` n'est pas testé.
  - Capture : `tests/video/av_sync.py` ne voit pas un son en avance (comparer à l'amorce de 30,4 ms du Bryar) ; l'attente de ffmpeg ne peut pas être annulée.
  - Outils : `server_smoke.py` compte les journaux de crash des autres processus dans ~/.jk2mv ; jk2bench.ps1 bloque sur un « + » dans un chemin, accepte une liste de presets vide et lit les chemins en jokers.
  - FE-16 : aucune scène ne dessine une instance dans deux vues de LOD différents, ni un skin partagé par deux modèles.
  - `spawn-windows-handles` : repli silencieux si la liste d'attributs échoue ; un test laisse un dossier temporaire si `CopyFileA` échoue.
  - testscene : `smoke_client` ne vérifie pas ce qu'`ultra` hérite d'`enhanced` ; Échap pendant la scène n'a pas de test automatique.

## Construire et tester sur cette machine

- **Arbres de travail :** un par chantier, sous D:\jk2mv-wt, avec des chemins sans espaces (q3lcc et q3asm échouent sur les espaces). Le dépôt D:\Mathieu\Disque D\LucasArts\jk2mv a des espaces : on n'y construit pas.
  - `git -C "D:\Mathieu\Disque D\LucasArts\jk2mv" worktree add D:\jk2mv-wt\<nom> -b <branche> claude/explore-codebase-A8IMd`
  - `git -C D:\jk2mv-wt\<nom> submodule update --init` ; ne jamais modifier src/mvsdk (sous-module amont).
- **Scripts :** build.ps1 et run.ps1 sont dans D:\jk2mv-wt\tools (ci-dessous `<rt>`), hors du dépôt car propres à cette machine. run.ps1 lance par défaut les binaires de D:\jk2mv-wt\integ\build-nj\out\Release et crée au besoin, dans le base\ du dossier `-BasePath` (par défaut celui des binaires), des liens durs vers les quatre pk3 retail (lecture seule). Les dossiers D:\jk2mv-wt\integ-logs (scripts des relectures et de l'intégration) et le scratchpad de la session d'origine sous %TEMP% sont temporaires : ce qui compte est recopié ici ou dans D:\jk2mv-wt\tools.
- **Build :** `powershell -NoProfile -File <rt>\build.ps1 -Src D:\jk2mv-wt\<nom> [-Targets jk2mvded,jk2mvmp] [-Jobs 4] [-Clean]`. Ninja, MSVC 2022 x64 Release, CRT statique, tests compris. Les binaires sortent dans `build-nj\out\Release`, assetsmv*.pk3 dans son base\. Un build complet prend environ 90 s à -Jobs 4.
- **Tests unitaires :** `ctest --test-dir D:\jk2mv-wt\<nom>\build-nj\tests -j 4` (le ctest de VS 2022 BuildTools). Pour les tests Kyle : `JK2MV_TEST_BASE=<dossier base\ retail>`.
- **Lancer le jeu :** `<rt>\run.ps1 -Name <nom> [-Server] -Bin <build-nj\out\Release> [-BasePath <dossier>] -Commands '+cmd',...`. Il prend un fs_homepath neuf dans D:\jk2mv-wt\homes, des sockets sur 127.0.0.1, ne capture pas la souris et n'écrit jamais dans l'installation. Pièges vus pendant l'intégration :
  - Appeler run.ps1 avec `&` dans la session PowerShell quand une commande contient des guillemets (`+set activeAction "..."`) : `powershell -File` coupe ces arguments.
  - run.ps1 ajoute `wait 10; quit` au client sans `quit` : pour rejouer une démo jusqu'au bout avec `nextdemo quit`, il faut une copie qui ne l'ajoute pas (D:\jk2mv-wt\integ-logs\run2.ps1, option `-NoAutoQuit`).
  - Un dossier `-BasePath` à soi doit contenir, à côté de son base\ (liens durs vers assets0, 1, 2 et 5.pk3, plus ses assetsmv*.pk3), les modules natifs jk2mvmenu_x64.dll, ui_x64.dll, cgame_x64.dll et jk2mpgame_x64.dll. Sans eux : « Failed loading library file: jk2mvmenu » et une boîte d'erreur qui bloque jusqu'au timeout.
  - Un `devmap` demande environ 1000 `wait` avant `team free` ou `record`.
  - Noms de bots valides : Desann, Tavion, Shadowtrooper, Reborn, Galak, Reelo, Chiss, Krussk, Ak-Buz, Beedo, Lieutenant_Cabbel, IW-323, SP-597, SW-967, TK-421, Luke, Kyle, Jedi_Trainer, Morgan, Jedi, Jan, Mon_Mothma, Prisoner, Bespin_Officer, Rebel, Ree-Yees, Lando, Ugnaught. stormtrooper, rax et boba n'existent pas.
  - En FFA (`g_gametype 0`), un client qui arrive voit le menu de configuration de la Force ; `g_gametype 1` l'évite.
- **Comparer deux binaires à l'image près :** rejouer la même démo en timedemo avec `+set activeAction "wait 40; screenshot_png a; wait 80; screenshot_png b"` et `+set nextdemo quit` : le timedemo avance de 50 ms par frame, les captures tombent au même instant de jeu. Les jauges du HUD bougent d'un lancement à l'autre ; comparer la zone au-dessus.
- **Menus :** `+set developer 1` puis `ui_openmenu setup_menu` ouvre Setup. Pour cliquer sans souris, les relectures et l'intégration ont utilisé un build de test, jamais commité, avec une commande `uiclick <x> <y> [cfg]` (coordonnées 640x480). Elle met un mouvement de souris et un clic dans la file d'événements par `Sys_QueEvent`, et lance `cfg` deux frames plus tard, après les commandes que le menu a ajoutées (`preset pending`, `vid_restart`).
- **Tests de fumée sous Windows :** D:\jk2mv-wt\integ-logs\smoke\render_win.py et client_win.py (copies des scripts d'un relecteur) lancent `tests/smoke/render_smoke.py` et `client_smoke.py` avec de vraies fenêtres et l'OpenGL du GPU, et lisent qconsole.log. Arguments : `render_win.py <tests\smoke> <jk2mvmp.exe> <build-nj\out\Release> <dossier de travail>` et `client_win.py <jk2mvmp.exe> <build-nj\out\Release> <dossier de travail> <tests\smoke>`. Une vingtaine de petites fenêtres s'ouvrent pendant environ 30 s.
- **Serveur avec clients :** D:\jk2mv-wt\integ-logs\orch.ps1 (31 bots, client intégré puis client de la base) et orch3.ps1 (serveur de la base, client intégré) attendent les marqueurs `echo PHASE_...` du journal du serveur pour lancer les clients. Les `wait` du serveur comptent les frames, qui accélèrent quand un client envoie des paquets.
- **Ailleurs :** sous Windows sans ces scripts, CMake + Visual Studio 2022 (`cmake -B build -A x64`, `-DBuildTests=ON`) et les pk3 retail dans `base\` à côté de `jk2mvmp.exe`. Les tests de fumée Linux : `-DBuildTests=ON -DSmokeTests=ON`, puis `ctest -L smoke` (Python 3, Mesa llvmpipe, SDL offscreen ; WSL convient). La CI GitHub se lance à la main sur la branche.

## À essayer en jeu

- `preset enhanced`, `preset ultra`, `preset movie`, puis `preset classic` ; ou Setup > Vidéo > Graphics Preset, et la page Advanced.
- `r_fbo 1` et `r_renderScale 2` (supersampling) ou `0.75` (plus léger), puis `vid_restart` : les deux sont latched.
- `cl_perfOverlay 2` et `r_gpuTimers 1` pour voir les fps, le temps GPU, le glow et le post.
- Démo : `demo_freecam`, `cam_add` / `cam_play`, `photomode`, `video_mp4` (ffmpeg dans le PATH ou à côté du jeu).
- Serveur : `serverstats`, `clientstats`, `svrecord all`, `sv_autoRecord 1`.
- `testscene ci_box -256 0 96` marche aussi sans assets, pour essayer le rendu.
