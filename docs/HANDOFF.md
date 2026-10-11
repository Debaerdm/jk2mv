# Passation : l'état après l'intégration du 2026-10-11

Ce fichier dit où en est `master` et comment continuer sur la machine Windows du propriétaire. Le détail par phase est dans docs/ROADMAP.md (une liste « Avancement » par phase), les cvars et commandes dans CVARS.rst.

## Où en est la branche

- Le travail vit maintenant sur `master`, la branche par défaut : le propriétaire travaille seul et pousse directement sur `master`. `claude/explore-codebase-A8IMd` reste à 2de7198 (la fin de l'intégration du 2026-10-05) comme historique ; on n'y ajoute plus rien. L'historique reste linéaire, sans commit de fusion.
- Ce tour (2026-10-11) a repris trois branches locales par cherry-pick, dans cet ordre :
  - `claude/fe3-ghoul2-cache` : les deux premiers caches de FE-3 (liste des os utilisés par instance, shader de chaque surface pour le dernier skin et le dernier modèle) et `bench_ghoul2` sorti des dénormaux ; restent les caches (c), (d) et (e) de ROADMAP.md.
  - `claude/review-notes-server` : les notes basses de SV-11 (noms de démo refusés, fichier fini net après une erreur d'écriture, chemins longs, `svstoprecord all`, client non pur), SV-3 (snapshot complet quand la base du delta est sortie de l'anneau du client), SV-13, BM-7 et SV-2 (`bench_huffman`) ; restent celles listées plus bas, surtout le démarrage d'une démo dans la frame serveur.
  - `claude/review-notes-client` : les notes basses côté client (capture sans mémoire, `vid_restart` pendant que le renderer est arrêté, page Advanced, dlights, FOV, outils démo, jk2bench.ps1, spawn, testscene) et deux scènes FE-16 (`OneInstanceTwoViews`, `SharedSkin`) ; restent celles listées plus bas, surtout ce qui demande les assets retail, un GPU ou Windows.
- Seul conflit : la liste des notes basses de ce fichier, que les deux branches de notes raccourcissaient chacune de son côté ; la liste ci-dessous garde ce que l'une et l'autre laissaient. ROADMAP.md et tests/unit/CMakeLists.txt se sont fusionnés sans conflit.
- Correctifs de CI poussés juste après (ebe7f15..599603d) : `Huff_Compress` envoyait un octet de pile comme octet de bourrage du paquet de connexion, et `Huff_Decompress` laissait des octets de pile après un message coupé ; les deux mettent maintenant des zéros (les octets de code et la taille ne changent pas, les clients et serveurs 1.02-1.04 ne lisent pas ces octets). Le build macOS de tr_gputimer.cpp prend ses propres types de pointeurs GL, et le test `macos_gl_types` (label portability) reproduit sous Linux les en-têtes OpenGL d'Apple.
- Après ce tour, l'étape 1 de la piste 1.05 est arrivée sur `master` par rebase de `claude/protocol-105-step1`, sans commit de fusion : les décisions du propriétaire (docs/PROTOCOL-1.05.md §7), puis les limites à l'exécution (§8), voir « Ce qui reste ». Au rebase, `tests/smoke/server_smoke.py` a gardé les deux côtés : `run_server()` lance aussi la passe `sv_maxclients 64` et rend le PID de chaque serveur, et seuls les journaux de crash de ces deux serveurs comptent ; tests/unit/CMakeLists.txt garde `bench_huffman` et `test_mv_limits`.
- Avant, l'intégration du 2026-10-05 (`integ5`, de 0d90b42 à 2de7198) :
  - les 17 branches du deuxième tour de revue : correctifs de revue (`fix5-testscene`, `fix5-render`, `fix5-dlights`, `fix5-fov`, `fix5-demotools`, `fix5-capture`, `fix5-tools`), piste serveur (`sv2-huffman-final`, `sv3-snapring-final`, `sv7-world`, `bm7-serverstats`, `sv13-usercmd`, `sv11-server-demos`), `spawn-windows-handles`, `vq7-renderscale`, `ui-presets-menu` et `fe16-ghoul2-tests` ;
  - commits ajoutés à l'intégration : les démos serveur utilisent le test de sortie de l'anneau de SV-3 (`SV_SnapshotEntitiesRolledOff`), la sonde « menu principal » de `smoke_render` suit la taille des captures de `r_renderScale`, `test_cl_presets` déclare `r_dlightPriority` et `r_ext_alphaToCoverage` (latched) comme le renderer, et CVARS.rst et deux commentaires reprennent des notes basses des relectures ;
  - choix d'intégration : `preset` redémarre le rendu une seule fois, juste avant que la frame suivante soit dessinée (mécanisme de `ui-presets-menu`). Ce qui suit un `wait` tourne avec le preset, une chaîne de presets ne redémarre qu'une fois, une cvar latched réglée juste après le preset est appliquée avec lui, et Apply Changes dans les menus ne fait qu'un `vid_restart`.

## Ce qui a été vérifié

- **Ce tour, sous Linux** (Ubuntu 24.04, GCC 13.3, Ninja Release, `-DBuildTests=ON -DSmokeTests=ON -DUseInternalMiniZip=ON`, Mesa llvmpipe et SDL offscreen) :
  - le build complet passe ; aucun avertissement de GCC ne tombe sur une ligne changée par ce tour (ceux qui restent sont dans du code plus ancien et dans mvsdk) ;
  - ctest : 816 tests, 813 passent et 3 sont sautés (les tests Kyle, sans `assets0.pk3`). Le compte comprend les trois tests de fumée (`smoke_server`, `smoke_client`, `smoke_render`), `test_ghoul2_cache` (8), les passes `--quick` de `bench_ghoul2` et `bench_huffman`, les 3 tests des correctifs de CI (deux tests Huffman, `macos_gl_types`) et les 18 tests de l'étape 1 de la piste 1.05 (ci-dessous) ;
  - l'étape 1 de la piste 1.05, rebasée sur l'intégration : chaque commit se construit et passe les tests unitaires (`-LE smoke`). Ses 18 tests : `test_mv_limits` (5), `test_sv_init_clients` (5), `limits_census` et `limits_census_selftest`, et dans `test_sv_world_clip`, `test_qcommon_huffman` et `test_sv_demo` trois tests qui figent les valeurs 1.02-1.04 et trois sondes. `tests/limits/verify_fold.py` contre sa base (PROTOCOL-1.05.md §8) : 228 objets identiques et les 4 de botlib attendus ; à chaque commit de l'étape qui touche src/, aucune autre différence ;
  - les 21 tests FE-16 sur les modèles synthétiques, dont `OneInstanceTwoViews` et `SharedSkin`, passent avec les caches de FE-3 sans changer une valeur ;
  - `tools_jk2bench` n'est pas dans ce compte : CMake ne le crée que là où il trouve `pwsh`, absent de cette machine.
- **CI GitHub :** verte sur 599603d, avant l'étape 1 de la piste 1.05 (tous les jobs de `build.yml` et `tests.yml` : Windows x64, Win32 et v141_xp, Linux, macOS arm64, Debug et Release). L'étape 1 n'avait pas encore tourné en CI quand ce fichier a été écrit.
- **Pas vérifié pour ce tour :** rien n'a tourné en jeu ni sur les assets retail (les 3 tests Kyle compris). À faire d'abord sur la machine Windows : build.ps1, ctest avec `JK2MV_TEST_BASE` (les tests Kyle doivent passer avec les caches de FE-3), puis un tour en jeu.
- **Sur `integ5`** (2026-10-05, Windows, avant ce tour) :
  - **Build propre** (Ninja, MSVC 2022 x64 Release) : aucun nouvel avertissement. Restent les C4311/C4312 de src/mvsdk/tools/rcc et l'avertissement de q3lcc sur src/mvsdk/code/ui/ui_main.c:7793, tous deux dans mvsdk.
  - **Tests unitaires** : 778, tous verts. Les 3 tests Ghoul2 sur Kyle sont sautés sans assets ; avec `JK2MV_TEST_BASE` qui pointe sur un base\ retail, `test_ghoul2` passait 22/22 (24 tests depuis ce tour).
  - **Tests de fumée** `smoke_render` et `smoke_client` : verts contre le build Windows (vraies fenêtres et OpenGL du GPU, voir plus bas).
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
  - le reste de FE-3 (caches (c), (d) et (e)), FE-4, FE-5, RB-4/RB-3 et RB-5 ;
  - BM-8/9, SV-5, SV-14b et BM-13/14.
- **Protocole 1.05 :** le propriétaire a tranché les 5 questions ouvertes (docs/PROTOCOL-1.05.md §7) : on relève toutes les limites, un serveur 1.05 refuse les clients 1.02-1.04, et le gameplay du mode 1.05 peut changer (liste à arrêter avant l'étape 3). L'étape 1 est faite sur `master` (§8) : le moteur lit les limites du protocole courant à l'exécution (`MV_MaxClients()`, `MV_MaxGEntities()` et `MV_ENTITYNUM_*` de `qcommon/mv_limits.h`), toujours 32 joueurs et 1024 entités pour tous les protocoles, sans changer le comportement.
  - Ce qui le vérifie :
    - le recensement (ctest `limits_census`, et `limits_census_selftest` pour ses propres règles) : chaque usage restant de `MAX_CLIENTS`, `MAX_GENTITIES`, `GENTITYNUM_BITS`, des `ENTITYNUM_*` et des configstrings qui suivent `MAX_CLIENTS` est classé, avec sa raison, dans `tests/limits/allowlist.txt` ;
    - le repliement (`tests/limits/verify_fold.py`, hors ctest, Linux et binutils seulement ; sa base est donnée au §8) : avec les accesseurs ramenés aux constantes, chaque objet de jk2mvmp et jk2mvded est celui de la base, relocations comprises, sauf les deux de botlib attendus (be_aas_main.cpp, be_interface.cpp) ;
    - les tests qui figent les valeurs 1.02-1.04 en chiffres : sentinelles de `SV_Trace`, numéros d'entité sur 10 bits, empreintes d'une démo .dm_15 et d'une .dm_16, `sv_maxclients 64` ramené à 32 (`smoke_server`), et des sondes sous une ligne plus petite ;
    - les gardes `MV_ASSERT_LEGACY_*` (mv_limits.h), qui arrêtent la compilation à chaque endroit encore figé sur 1.02-1.04 dès qu'un maximum change.
  - Avant qu'une limite grandisse, dans une branche à part car ils changent le comportement : les correctifs de la fin du §8.
  - Puis l'étape 2, le protocole 17 côté moteur (§8, « Pour l'étape 2 ») : relever les maximums et retirer le verrou de l'étape 1, rendre par protocole chaque endroit des gardes (tables de champs, bits des numéros d'entité, des indices de modèle et de son, paramètres d'événement compris), borner `sv_maxclients` par `MV_MaxClients()` après le choix de la version à chaque carte, une table des configstrings et du `gameState_t` par protocole, une structure de l'API MV niveau 5 à côté de celle des niveaux 1 à 4, qui ne change pas, et les démos `.dm_17`.
- **CI et autres plateformes :** `build.yml` et `tests.yml` partent seuls à chaque push sur `master` (et à la main par workflow_dispatch). Ils étaient rouges depuis avril : le build macOS (types GL 1.5 absents de l'OpenGL d'Apple, dans tr_gputimer.cpp) et `HuffmanAdaptive.CompressAndDecompressMatchOldCode` (voir plus haut). Depuis 599603d, tout est vert. Les valeurs de référence de FE-16 tiennent donc aussi sous MSVC x64 et Win32 et sous macOS arm64 dans la CI ; les sept valeurs des deux nouvelles scènes (`views/…`, `sharedskin/…`) ont été relevées sous Linux GCC. L'étape 1 de la piste 1.05 n'a été construite et testée que sous Linux (GCC) avant son push ; la CI de ce push dira le reste.
- **Notes basses des relectures, non traitées** (tout le reste est intégré) :
  - SV-11 :
    - le démarrage d'une démo (ouverture du fichier, gamestate, messages console) se fait encore dans la frame serveur, et `svrecord all` ouvre tous les fichiers dans la même commande ; seule l'écriture passe par le thread. Le sortir de la frame demande de copier le gamestate et les commandes fiables du client pour le thread : pas fait dans les correctifs de relecture ;
    - deux mutants survivaient aux tests (M9, W3) : leur liste n'est pas dans le dépôt, on ne sait pas lesquels c'étaient. Les tests ajoutés depuis (noms de périphériques, fichier coupé après une erreur, chemins longs, `svstoprecord all`, client non pur) les ont peut-être tués ; le départ d'un bloc à la seconde (`DEMOFILE_DELAY`) reste sans test.
  - SV-3 : les tests ne compilent pas la glu moteur qui appelle `sv_snapshot_ring.h` (`SV_WriteSnapshotToClient`) ; `SV_DeltaBaseBeyondClientRing` est testé contre un modèle de l'anneau du client.
  - SV-13 :
    - le message développeur des réponses `disconnect` perdues (même forme que ceux des paquets hors connexion) ;
    - pas de seau global contre une inondation à sources usurpées : la réponse (14 octets de données) n'amplifie guère le paquet qui la provoque et chaque adresse reste à 10 par seconde, et un seau global laisserait une inondation priver de leur `disconnect` les vrais clients d'un serveur qui redémarre ;
    - la glu moteur n'a pas de test.
  - SV-2 : SV-8 devra traiter les globales de msg.cpp.
  - Menus :
    - après un réglage latched fait à la console, les copies des pages retail montrent la valeur courante (comme en retail) : laissé tel quel ;
    - le crochet de clic des relectures n'est pas dans l'arbre : pas ajouté, il mettrait dans le jeu livré une commande qui clique à la place du joueur ; à réserver à un build de test si on le veut ;
    - « FX system out of effects » après un `vid_restart` au menu principal puis une partie locale mise en pause (sans doute ancien) : pas reproduit, il faut le cgame et les assets retail ;
    - les lignes Render Scale et Light Priority de la page Advanced (`assets/ui/jk2mp/setup.menu_patch`, chaînes dans mv.sp) n'ont pas été vues dans le jeu : sans le setup.menu retail, seule leur lecture sans erreur dans un menu de test a été vérifiée.
  - Rendu :
    - le flou vidéo sans cible RGBA16 rendable abandonne au premier échec (un message, et l'enregistrement continue sans flou) ; pas de repli 8 bits, et ce chemin n'a pas pu tourner (llvmpipe et les GPU essayés rendent en 16 bits) ;
    - l'alpha-to-coverage d'un mod avec un alpha de sommet constant inférieur à 1 laisse passer une partie du fond : pas changé, aucun shader retail ne le fait et choisir les shaders qui gardent l'A2C demande un mod qui le fait ;
    - la glu (table du grade, image liée après `GL_State`) n'a pas de test : il faudrait un contexte GL dans les tests unitaires ;
    - `GL_INVALID_VALUE` au premier rechargement du renderer avec MSAA 8x, HDR et bloom sous `r_ignoreGLErrors 0` (défaut de la base) : pas reproduit sous llvmpipe (`preset ultra`, deux `vid_restart`), à reprendre sur le GPU qui le montre.
  - Dlights : le choix de l'historique de chaque scène (`R_DlightHistoryForScene`) est testé, pas son appel dans `R_SelectDlights` (tr_scene.cpp), qui lui donne `tr.frameCount` pour la frame : il faudrait deux scènes monde par frame dessinées par le moteur (un cgame ; testscene n'en dessine qu'une).
  - FOV : la glu cgame (`CL_CgameCvarUpdate`, `CL_CgameCvarStringBuffer`) n'a pas de test, il faudrait une VM cgame ; la valeur qu'elles donnent (`CL_FovAdjustValue`) en a un.
  - Outils démo : la bande où la squad passe au slerp (-0,8 à -0,9) pourrait commencer plus près de 180° ; laissé, c'est un réglage sans défaut vu et le changer modifie les chemins existants.
  - Capture :
    - l'attente de ffmpeg ne peut pas être annulée ; il faudrait tuer ffmpeg (un appel par plateforme) et lire les touches pendant l'attente ;
    - quand la liste de commandes de la frame est pleine (console pleine de texte en haute résolution), `screenshot_png` et `screenshot_tga` écrivent un tampon qui n'a pas été lu et disent « Wrote » (défaut de la base) ; `screenshot` (JPEG) dit qu'elle n'est pas écrite, et `video` y perd ses images (des blocs vides, comme dans la base) ; `video_mp4` n'envoie alors rien à ffmpeg mais compte l'image, pas vérifié.
  - testscene : Échap pendant la scène n'a pas de test automatique (il faudrait injecter une touche, comme le crochet de clic).
  - Correctifs de `claude/review-notes-client` dont le cas n'a pas pu être produit ici : `R_DlightProgram` qui laisse signaler les erreurs GL antérieures (aucune sous llvmpipe), `cam_play` et `photomode` dans la même frame (il faut une démo, donc les modèles retail), `tests/video/av_sync.py` (assets retail) et `sys_spawn.cpp` (Windows : ni compilé ni lancé). jk2bench.ps1 a tourné sous PowerShell 7.4 sur Linux avec un faux jeu (`tools_jk2bench`), pas sous Windows PowerShell 5.1.

## Construire et tester sur cette machine

- **Arbres de travail :** un par chantier, sous D:\jk2mv-wt, avec des chemins sans espaces (q3lcc et q3asm échouent sur les espaces). Le dépôt D:\Mathieu\Disque D\LucasArts\jk2mv a des espaces : on n'y construit pas.
  - `git -C "D:\Mathieu\Disque D\LucasArts\jk2mv" fetch`, puis `git -C "D:\Mathieu\Disque D\LucasArts\jk2mv" worktree add D:\jk2mv-wt\<nom> -b <branche> origin/master`
  - `git -C D:\jk2mv-wt\<nom> submodule update --init` ; ne jamais modifier src/mvsdk (sous-module amont).
  - Un chantier fini revient sur `master` sans commit de fusion (rebase sur `origin/master` puis avance rapide, ou cherry-pick), et `master` est poussée telle quelle.
- **Scripts :** build.ps1 et run.ps1 sont dans D:\jk2mv-wt\tools (ci-dessous `<rt>`), hors du dépôt car propres à cette machine. run.ps1 lance par défaut les binaires de D:\jk2mv-wt\integ\build-nj\out\Release et crée au besoin, dans le base\ du dossier `-BasePath` (par défaut celui des binaires), des liens durs vers les quatre pk3 retail (lecture seule). Les dossiers D:\jk2mv-wt\integ-logs (scripts des relectures et de l'intégration) et le scratchpad de la session d'origine sous %TEMP% sont temporaires : ce qui compte est recopié ici ou dans D:\jk2mv-wt\tools.
- **Build :** `powershell -NoProfile -File <rt>\build.ps1 -Src D:\jk2mv-wt\<nom> [-Targets jk2mvded,jk2mvmp] [-Jobs 4] [-Clean]`. Ninja, MSVC 2022 x64 Release, CRT statique, tests compris. Les binaires sortent dans `build-nj\out\Release`, assetsmv*.pk3 dans son base\. Un build complet prend environ 90 s à -Jobs 4.
- **Tests unitaires :** `ctest --test-dir D:\jk2mv-wt\<nom>\build-nj\tests -j 4` (le ctest de VS 2022 BuildTools). Pour les tests Kyle : `JK2MV_TEST_BASE=<dossier base\ retail>`. `bench_ghoul2` et `bench_huffman` se lancent à la main pour les mesures ; ctest ne les fait tourner que brièvement (`--quick`).
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
- **Ailleurs :** sous Windows sans ces scripts, CMake + Visual Studio 2022 (`cmake -B build -A x64`, `-DBuildTests=ON`) et les pk3 retail dans `base\` à côté de `jk2mvmp.exe`. Sous Linux (vu avec Ubuntu 24.04 et GCC 13) : `cmake -S . -B <build> -G Ninja -DCMAKE_BUILD_TYPE=Release -DBuildTests=ON -DSmokeTests=ON -DUseInternalMiniZip=ON`, `cmake --build <build>`, puis `ctest --test-dir <build>/tests -j3 --output-on-failure` (environ 40 s, fumée comprise ; `-LE smoke` pour les seuls tests unitaires, `-L smoke` pour la fumée). La fumée demande Python 3, Mesa llvmpipe et SDL offscreen ; WSL convient. La CI GitHub part à chaque push sur `master`, ou à la main.

## À essayer en jeu

- `preset enhanced`, `preset ultra`, `preset movie`, puis `preset classic` ; ou Setup > Vidéo > Graphics Preset, et la page Advanced.
- `r_fbo 1` et `r_renderScale 2` (supersampling) ou `0.75` (plus léger), puis `vid_restart` : les deux sont latched.
- `cl_perfOverlay 2` et `r_gpuTimers 1` pour voir les fps, le temps GPU, le glow et le post.
- Démo : `demo_freecam`, `cam_add` / `cam_play`, `photomode`, `video_mp4` (ffmpeg dans le PATH ou à côté du jeu).
- Serveur : `serverstats`, `clientstats`, `svrecord all`, `sv_autoRecord 1`.
- `testscene ci_box -256 0 96` marche aussi sans assets, pour essayer le rendu.
