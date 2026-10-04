# JK2MV : feuille de route « remaster »

> Établie le 2026-10-04 à partir d'une lecture du code (aucune mesure n'a pu être faite : pas de GPU ni d'assets dans l'environnement d'analyse). Les identifiants (`VQ-4`, `FE-2`, …) renvoient à [roadmap-opportunities.md](roadmap-opportunities.md), qui détaille chaque piste avec ses références `fichier:ligne`.

## 1. En bref

L'objectif : que JK2MV devienne le build de JK2 qui **impressionne dès la première capture d'écran**, et qui **tient un 32 joueurs au sabre** sans casser un seul serveur, client ou mod 1.02/1.03/1.04. Chaque nouvel effet est financé par un gain de perf trouvé dans le même bout de code, et tout reste opt-in : l'apparence classique reste le défaut.

Les trois gains phares :
1. **`preset enhanced`** : une commande, ou un clic dans MV Options, active anisotrope 16x, MSAA 4x, anticrénelage du feuillage, glow adapté à la résolution de l'écran et étalonnage couleur avec écran partagé avant/après. Le tout aussi rapide qu'aujourd'hui, et plus rapide avec le glow allumé.
2. **Director's cut** : n'importe quelle démo de 2003 rejouée avec caméra libre, trajectoires de caméra, ralenti et export MP4 en une commande, sous tous les mods.
3. **Lumière nouvelle génération sans perte de fps** : bloom HDR, lumière de sabre calculée par pixel, supersampling. Ensuite, travail CPU (Ghoul2, VBO) et serveur pour que les grosses bastons restent fluides.

*Comment j'ai arbitré entre les trois brouillons.* Le brouillon « wow d'abord » donne la meilleure structure : il montre quelque chose dès la semaine 2-3, sans nouvelle infrastructure GL. J'y ai greffé la phase 0 du brouillon « sûr », qui pose CI, baseline et images de référence avant tout code. Du brouillon « perf d'abord », j'ai gardé les seuils go/no-go mesurés et la règle « pas de chiffres, pas de merge ». J'ai écarté le serveur en phase 7 du brouillon « sûr » : trop loin pour l'objectif « plus de charge ».

## 2. Où on part

- **Renderer GL 1.x en pipeline fixe.** Tout passe par le buffer CPU `tess` (tr_local.h:1513), plafonné à 1000 sommets (qfiles.h:10), puis par `glDrawElements` sur tableaux clients (tr_shade.cpp:151-187). Il n'y a ni VBO, ni FBO, ni GLSL. Seuls le glow et le gamma post-process utilisent des programmes ARB.
- **Ce qui existe déjà, à ne pas réinventer :**
  - LUT 3D 64³ appliquée à chaque frame par le gamma post-process (`r_gammamethod 2` par défaut : sdl_window.cpp:1019, tr_image.cpp:2975-2995, tr_backend.cpp:1305-1357) ;
  - `r_DynamicGlow` et ses 6 réglages (tr_init.cpp:1075-1081), `.dynGlow`, `r_saberGlow` ;
  - `r_ext_multisample`, anisotrope (défaut 2), `r_highdpi`, `r_mode -2` ;
  - `.menu_patch` depuis assetsmv.pk3, qui est exempté de sv_pure (files.cpp:347-349, 1336-1341) ;
  - `timedemo`, `activeAction`, `com_speeds`, `r_speeds` 1-7, `cl_autoDemo`, `video` (AVI MJPEG).
- **Le preset retail « High Quality » met déjà `picmip 0` et le trilinéaire** (mvsdk ui_main.c:4316-4341). La valeur ajoutée d'un preset tient donc à l'anisotrope, au MSAA, à l'alpha-to-coverage, au glow, au LOD et à l'étalonnage. Il ne faut pas vendre « textures pleine résolution » comme une nouveauté.
- **Bugs et goulots vérifiés dans le code :**
  - `qglFinish()` inconditionnel à chaque frame dès que le glow est actif (tr_backend.cpp:1059).
  - Le glow tourne aussi dans les vues miroir et portail : la condition à tr_backend.cpp:1040 ne teste pas `isPortal`.
  - Le gamma post-process re-spécifie sa texture à chaque frame avec `qglCopyTexImage2D` (tr_backend.cpp:1326). Pire, `sceneImage` est allouée en `GL_RGBA16` flottant (tr_image.cpp:2816-2823), puis ré-allouée en RGBA à chaque frame.
  - `glFinish` à chaque swap sur les frames sans vue 3D, donc dans les menus (tr_backend.cpp:1268).
  - Avec exactement 32 dlights, `1U<<32` éteint toute la lumière dynamique du monde. La branche `>` ne peut jamais s'exécuter (tr_world.cpp:752-757), car `RE_AddLightToScene` plafonne à 32 (tr_scene.cpp:291).
  - `cl_avi.cpp:433` teste `s_backend`, une cvar qui n'existe pas dans jk2mv (le moteur utilise `s_UseOpenAL`).
  - `sv_fps` n'a aucune borne haute : au-delà de 1000, `frameMsec` vaut 0 et la boucle à sv_main_frame.h:79/150 ne se termine jamais.
- **Mesure et CI quasi inexistantes.**
  - Timers à la milliseconde, et `timedemo` plafonné à 1000 fps (common.cpp:2904).
  - `tests.yml:46` compile avec `-DBuildMVMP=OFF -DBuildMVDED=OFF`, donc aucun test n'exécute le moteur.
  - `build.yml:179` (`windows-2019`) et `build.yml:263` (`macos-12`) visent des runners GitHub retirés.
- **Limites dures, non négociables :** 32 clients, 1024 entités, 256 entités par snapshot côté cgame, `MAX_MSGLEN`, `PACKET_BACKUP`, et mvapi.h, qu'un fork n'a pas le droit de modifier (mvapi.h:18-22).

## 3. Feuille de route

Légende de compatibilité : **T** = transparent (image et protocole identiques) · **O** = opt-in client, classique par défaut · **S** = serveur seul, transparent pour les clients.

### Phase 0 : un socle mesurable (2-3 week-ends)
**Objectif.** Figer l'apparence classique et les chiffres actuels avant de toucher quoi que ce soit, remettre la CI au vert, et livrer les correctifs gratuits.
**Ce que les joueurs verront.** Dans un FFA chargé, la lumière des sabres sur les murs ne clignote plus. Les vidéos enregistrées sous OpenAL ne sortent plus avec une piste audio cassée.

| id | titre | effort | compat |
|---|---|---|---|
| BM-17 | runners CI : windows-2022, macos-13/14 (garder ou abandonner le build XP est ta décision) | S | T |
| BM-3 | baseline client sans code : 3 démos × 3 presets × 5 runs + PresentMon | S | – |
| BM-4 | images de référence (golden frames) de l'apparence classique : `activeAction` + `screenshot_tga` sous timedemo | S | – |
| BM-1 | `Sys_Microseconds` (QPC / `CLOCK_MONOTONIC`) dans `com_speeds`, `SV_Frame` et les timers du renderer | S | T |
| FE-2 | correctif des 32 dlights (`>=`, masque complet `0xFFFFFFFF`) : commit isolé, à proposer en amont | S | T |
| PX-11 | `cl_avi.cpp:433` → tester `s_UseOpenAL` et supprimer la piste audio avec un avertissement | S | T |
| SV-14a | borner `sv_fps` à 1000 | S | S |
| BM-18/VQ-18 | CVARS.rst : `sv_hibernateTime` fantôme, `sv_hibernateFps` vaut 4 et non 5, `con_timestamps`/`r_printMissingModels` valent 0, `r_dynamicGlow 2` et ses réglages | S | T |

**Critère de fin.** Un fichier `docs/benchmarks/baseline.md` contient, pour chaque couple (démo, preset) : médiane des fps, p99 du temps de frame, écart entre runs inférieur à 3 %, SHA-256 de chaque démo, GPU et pilote. 10 à 20 TGA de référence par démo sont archivées. `com_speeds 1` sur jk2mvded avec 8 bots n'affiche plus `all:0`. Tous les jobs CI démarrent et passent.

### Phase 1 : le remaster en une commande (6-8 semaines, environ 10 petites PR)
**Objectif.** Le premier « wow », construit uniquement sur ce qui existe déjà (LUT, glow, MSAA, menu_patch). Chaque effet est payé par un correctif de perf dans le même chemin de code.
**Ce que les joueurs verront.** `preset enhanced` donne un anisotrope 16x, un MSAA 4x avec herbe et rambardes anticrénelées, et des halos de sabre lisses en 1440p et 4K au lieu d'un 320×240 agrandi. Des ambiances couleur optionnelles (`cinematic`, `vivid`, `cold`, `warm`, `noir`) s'accompagnent d'un écran partagé avant/après. Un overlay non-cheat affiche les fps, le 1 % low et la courbe des temps de frame, pour prouver que tout ça ne coûte rien. `preset classic` restaure exactement l'apparence d'origine.

| id | titre | effort | compat |
|---|---|---|---|
| RB-2a | supprimer le `qglFinish` du glow (tr_backend.cpp:1059), avec `r_dynamicGlowFinish 1` comme porte de secours | S | T |
| VQ-4 | `sceneImage` allouée en `GL_RGBA8` et `qglCopyTexSubImage2D` à la place de `CopyTexImage` (tr_backend.cpp:1326) | S | T |
| RB-11b | pas de `glFinish` au swap sur les frames de menu (tr_backend.cpp:1268) | S | T |
| VQ-3 | `r_DynamicGlowWidth/Height 0` = taille auto (environ écran/4, rayon constant à l'écran) ; glow sauté si `viewParms.isPortal` | S | O |
| VQ-2 | `r_colorGrade`, `r_saturation`, `r_contrast`, `r_vibrance`, `r_colorLUT <image>` intégrés à la LUT 64³ ; `r_colorGradeSplit 1` (deux passes sous `glScissor`) | S | O |
| VQ-8 | `r_ext_alphaToCoverage` pour les étapes `GLS_ATEST_*` quand le MSAA est actif | S | O |
| VQ-1 | commande `preset classic\|enhanced\|ultra\|competitive\|movie` (nouveau `cl_presets.cpp`) + une ligne dans `assets/ui/jk2mp/setup.menu_patch` | S | O |
| BM-16 | `cl_perfOverlay 1/2` dessiné après `CL_CGameRendering` (cl_scrn.cpp:~489), police console, non-cheat | S | O |
| PX-6 | `cl_demoHideHud` : ignore les appels 2D du cgame, **uniquement si `clc.demoplaying`** | S | O |
| PX-16 | `r_fullscreen 2` (`SDL_WINDOW_FULLSCREEN_DESKTOP`) + Alt+Entrée | S | O |

Contenu des presets. La commande affiche chaque changement et ne fait qu'un seul `vid_restart`.

| preset | réglages |
|---|---|
| `enhanced` | `r_picmip 0`, `r_textureMode GL_LINEAR_MIPMAP_LINEAR`, aniso 16 (borné par `_avail`), `r_ext_multisample 4`, `r_ext_alphaToCoverage 1`, `r_DynamicGlow 1` en taille auto, `r_subdivisions 2`, `r_lodCurveError` plus élevé |
| `ultra` | `enhanced` + MSAA 8 + `cl_autolodscale 0` (coûteux en CPU tant que la phase 4 n'est pas faite, l'overlay le montrera) |
| `competitive` | glow coupé, `r_swapInterval 0` |
| `movie` | `enhanced` + `cl_aviFrameRate 60` + `cl_aviMotionJpegQuality 95` |
| `classic` | `Cvar_Reset` sur exactement la même liste |

Jamais touchés par un preset : `com_maxfps` (lié à la physique de saut), `snaps`, `rate`, `cl_timeNudge`, `r_flares` (code mort, tr_backend.cpp:712), `cg_shadows 2` (ramené à 1 hors mode developer, tr_backend.cpp:438).

**Critère de fin.**
1. Avec toutes les nouvelles cvars à leur défaut, les images de référence sont identiques à celles de la baseline, à la tolérance près. Seule exception admise : les frames à 32 dlights (FE-2). Un test unitaire vérifie que `r_colorGrade ""` produit une LUT identique octet par octet.
2. `preset enhanced` puis `preset classic` ne laissent aucune différence dans `cvarlist` par rapport à un profil neuf.
3. Avec `r_DynamicGlow 1` sur bench_duel : médiane des fps au moins égale à la baseline, p99 pas pire. Gain attendu : +5 à 25 % quand CPU et GPU sont équilibrés, confiance moyenne.
4. L'étalonnage coûte moins que le bruit de mesure.
5. Un clip avant/après de 60-90 s est tourné avec `video` + `cl_demoHideHud`, puis publié.

### Phase 2 : Director's cut et vrais instruments (3-4 mois)
**Objectif.** Des outils de cinéma intégrés au moteur qui marchent sur toute démo et sous tout mod, sans toucher aux VM. Les points d'accroche existent : refdef à cl_cgame.cpp:855, listener son à :795, vue FX à :1067, usercmds générés en démo à cl_input.cpp:1002. En parallèle, mettre en place les instruments qui permettront de juger la phase 3.
**Ce que les joueurs verront.** Voler librement dans un vieux duel et tourner autour d'un blocage de sabres. Poser une trajectoire de caméra avec rampe de ralenti jusqu'au kill, puis exporter un MP4 en une commande. Un mode photo pour faire des fonds d'écran. Les serveurs peuvent enregistrer chaque match.

| id | titre | effort | compat |
|---|---|---|---|
| PX-7 | `demo_pause/speed/step`, timeline à l'écran, report de la fraction de ms pour un ralenti juste (common.cpp:2774-2790) | S | O |
| PX-4 | caméra libre `demo_freecam`, limitée à `clc.demoplaying`, scènes plein écran sans `RDF_NOWORLDMODEL` | M | O |
| PX-5 | images clés `cam_add/cam_play/cam_save` (Catmull-Rom + slerp + rampes de timescale), maths testées sous gtest | M | O |
| PX-12 | `screenshot_png`, readback asynchrone par PBO, libjpeg-turbo | S | O |
| PX-13 | `photomode` (gel + caméra libre + HUD masqué + PNG) | S | O |
| PX-9 | `video-pipe` vers ffmpeg ; `cl_aviPipeFormat` en `CVAR_VM_NOWRITE`, traité comme liste d'arguments, jamais passé à un shell | M | O |
| PX-10 | flou de mouvement à la capture par accumulation de sous-frames (version CPU) | M | O |
| BM-2 + BM-5 | commande `benchmark <demo> [runs]` (p50/p99/1 % low, CSV, sans plafond à 1000 fps) + compteurs `r_speeds 8` (draws, binds, états, raison de chaque flush) | M | T |
| BM-6 | temps GPU par `GL_ARB_timer_query` (`r_gpuTimers`) | M | O |
| BM-11/12 | CI : serveur dédié sans assets (BSP de 856 octets, 31 bots) + client headless (SDL offscreen + llvmpipe), les deux vérifiés par la recherche | M | – |
| SV-11 | `svrecord` / `sv_autoDemo` au format dm_15/dm_16 standard | M | S |

**Critère de fin.**
1. Une démo 1.02 (.dm_15) et une démo 1.04 (.dm_16), sous base et sous 2 mods populaires, sont rejouées en caméra libre avec un chemin d'au moins 5 images clés, une rampe de ralenti et le HUD masqué, puis exportées en MP4 en une commande. Dérive audio inférieure à une frame sur 2 minutes.
2. Un test scripté prouve que chaque commande caméra, HUD ou step refuse d'agir hors `clc.demoplaying`.
3. `benchmark` donne un écart entre runs inférieur à 3 %, et les compteurs `r_speeds 8` sont identiques sur deux runs de la même démo.
4. Une démo issue de `svrecord` se lit sur un client 1.04 stock.

### Phase 3 : la lumière nouvelle génération (4-6 mois)
**Objectif.** Une seule nouvelle brique GL : une cible FBO et un gestionnaire de programmes intégrés (ARB d'abord, GLSL 1.20 en option, chaînes C embarquées donc compatibles sv_pure). Elle se rembourse en supprimant des copies plein écran : 1 par frame par défaut, 3 en pleine résolution + 5 en résolution de flou avec le glow (tr_backend.cpp:1046, 1065, 1083, 1614, 1633, 1326). Ce budget dégagé finance ensuite les effets.
**Ce que les joueurs verront** (preset `ultra`). Des sabres qui brûlent et débordent de lumière, plus de bandes de couleur dans le brouillard et les couloirs sombres, des taches de lumière rondes et lisses au lieu de triangles baveux, toutes les lumières visibles allumées même dans le chaos, et une image supersamplée sans scintillement. En réglage par défaut : la même image, un peu plus vite.

| id | titre | effort | compat |
|---|---|---|---|
| VQ-5 | FBO scène + gestionnaire de programmes, `r_fbo 0` par défaut, le chemin par copies reste le repli | M (prévoir 1 mois) | O |
| VQ-6 | scène RGBA16F, tonemapping ACES/Hable, bloom à seuil qui partage le flou du glow | M | O |
| VQ-7 | `r_renderScale` / SSAA, captures et AVI supersamplés (le découplage VM/drawable existe déjà, cl_main.cpp:4096-4100) | S | O |
| RB-9 | `r_dlightMode 1` : dlights par pixel en ARB fp, sans FBO (**peut démarrer dès la phase 2**) | M | O |
| FE-9 | `r_dlightPriority` : tri, frustum et PVS pour garder les 32 dlights qui comptent | M | O |
| VQ-11 | flou de mouvement accumulé dans un FBO flottant | M | O |
| VQ-2b | étalonnage appliqué avant le HUD (la v1 teinte aussi le HUD, cl_scrn.cpp:503) | S | O |

**Avancement.**
- **VQ-5 et VQ-6 sont livrés :** `r_fbo`, `r_hdr`, `r_bloom`, `r_exposure` (src/renderer/tr_postfx.cpp), dans les presets `ultra` et `movie`.
- **`r_fbo 1` :** il supprime la copie plein écran de la passe gamma et toutes les copies du glow.
  - Les objets lumineux se dessinent dans leur propre cible, qui partage la profondeur de la scène.
  - Le flou alterne entre deux petites cibles 16 bits.
  - La scène n'est plus redessinée.
  - Sous llvmpipe en 640x480 : glow à 4,5 ms au lieu de 9,9 ms, temps GPU de la frame à 6,9 ms au lieu de 13,4 ms (`r_gpuTimers`). Le critère 2 reste à mesurer sur une vraie carte.
- **RB-9 est livré :** `r_dlightMode 1` (src/renderer/tr_shade.cpp), activé par le preset `enhanced`. Le fragment program reproduit la forme du halo classique, mais rond et sans la traînée verticale. `smoke_render` le vérifie.
- **PX-3 est livré :** `cl_fovAspectFix 1`, la correction Hor+ du champ de vision en écran large, dans le preset `enhanced`.
- **Phase 4, piste client :**
  - FE-17 : `-ffp-contract=off` explicite pour GCC et Clang, plus l'option CMake `UseLTO`.
  - RB-11a : `r_maxFrameLatency` (fences ARB_sync), à 1 dans le preset `competitive`.
- **PX-10 (phase 2) est livré :** `cl_aviMotionBlur N` fait N frames de jeu par image vidéo et les moyenne dans une cible flottante, d'où un vrai flou de mouvement dans `video` et `video_mp4`. Le preset `movie` le met à 4.
- **BM-6 (phase 2) est livré :** `r_gpuTimers 1` mesure le temps GPU de la frame, du glow et des passes post. Le résultat s'affiche dans `cl_perfOverlay` et entre dans les percentiles de `benchmark`. C'est l'outil pour vérifier les critères 2 et 3 sur une vraie carte.
- **FE-9 est livré :** `r_dlightPriority 1` (par défaut). Au-delà de 32 lumières, chaque scène garde les 32 qui comptent le plus, au lieu des 32 premières ajoutées. `testscene ... dlights` crée ce cas et `smoke_render` le vérifie.
- **VQ-2b est livré :** avec `r_fbo 1`, l'étalonnage passe dans une table 3D appliquée à la fin de la vue 3D, avant le HUD. La passe gamma garde la table classique.
- **Tonemapping :** c'est une épaule exponentielle au-dessus de 0,8 × blanc, appliquée à la fin de la vue 3D, avant le HUD. Je l'ai préférée à ACES pour que l'image d'origine reste intacte sous le genou.
- **Banc d'essai sans assets :** la commande `testscene` et la salle de test de ci_box. Le test CI `smoke_render` vérifie :
  - `r_fbo 1` identique au pixel près, avec et sans glow ;
  - le halo du bloom ;
  - le halo resté rouge en HDR ;
  - l'absence d'erreur GL avec tous les effets et le MSAA.
- **`testscene` avec les menus retail :** la scène prend bien la place du menu principal plein écran, qui la cachait jusqu'ici ; Échap ou `testscene off` ramène le menu. Le stub de `smoke_render` a maintenant un menu principal plein écran, pour attraper ce cas.

**Critère de fin.**
1. `r_fbo 0` reste identique bit à bit aux références de la phase 1. `r_fbo 1` sans effet est identique à la tolérance près, captures et AVI compris.
2. Glow actif en 1440p : le temps GPU sur bench_duel est plus bas avec `r_fbo 1`.
3. HDR + bloom coûtent au plus 1 ms GPU en 1440p sur une carte dédiée milieu de gamme, au plus 2,5 ms sur un iGPU.
4. Sans extension, repli propre avec un avertissement dans `gfxinfo`. Testé sous llvmpipe en CI.
5. `r_dlightMode 1` : `c_dlightVertexes` proche de 0, CPU backend −5 % ou mieux dans une grosse baston.

### Phase 4 : les grosses bastons à pleine qualité (5-8 mois, deux pistes parallèles)
**Objectif.** Garder `ultra` fluide à 32 joueurs et rendre le serveur mesurable et robuste, sans toucher aux constantes de protocole. La piste serveur peut démarrer dès la fin de la phase 0 si l'envie t'en prend.
**Ce que les joueurs verront.** Des 1 % low nettement meilleurs en FFA/CTF chargés, surtout sur portable, iGPU, écran 240 Hz et ARM. Des modèles en LOD0 à toute distance. Un tick serveur à 40 Hz en opt-in, des connexions plus rapides, et plus d'expulsions « Server command overflow ».

**Piste client.** Tout ce qui touche Ghoul2 doit rester bit-exact, car tr_ghoul2.cpp est aussi compilé dans le dédié et les points d'attache du sabre font le gameplay.

| id | titre | effort | compat |
|---|---|---|---|
| FE-16 | tests de référence Ghoul2 (GLA/GLM synthétiques) + micro-benchmarks : **porte d'entrée** de tout ce qui suit | M | T |
| FE-17 | `-ffp-contract=off` explicite (GCC est en `fast` en gnu++11) + option LTO | S | T |
| FE-1 | arène par frame pour `CRenderableSurface` (corrige aussi la fuite glow + `RDF_NOWORLDMODEL`) | S | T |
| FE-3, FE-4, FE-5 | caches bit-exacts, SIMD des matrices d'os + port NEON du skinning, skinning une seule fois par frame | M×3 | T |
| RB-4 → RB-3 | alléger les appels GL par batch ; agrandir `tess` **seulement si** `r_speeds 8` montre des flushes par débordement | S | T |
| RB-5 | `r_vbo 1` : monde statique en VBO (GL 1.5 fixe, repli par batch, patches LOD restés sur CPU) | L | O |
| RB-11a | `r_maxFrameLatency` via ARB_sync | S | O |

**Piste serveur.**

| id | titre | effort | compat |
|---|---|---|---|
| BM-7 | `serverstats` + `sv_statsLog` (µs par étape, frames de rattrapage) | M | S |
| BM-8/9 | rampe de bots (voir §5) + `sv_benchBotSnapshots` | S | S |
| SV-3 | anneau d'entités de snapshot dimensionné par cvar, en puissance de deux | S | S |
| SV-2 | Huffman par tables (sortie identique bit à bit) | S | S |
| SV-5 | envoyer les fragments dès que le rate le permet | M | S |
| SV-13 | comptage des usercmds par client, `sv_maxUsercmdRate` opt-in | S | S |
| SV-7 (1)(2) | boîte de trace coupée au point d'impact monde, unlink en O(1) | S | S |
| SV-14b | preset `competitive` à 40 Hz, documenté avec son coût | S | S |
| BM-13/14 | soak ASan/UBSan + nombre d'instructions Callgrind par `SV_Frame` en CI | M | – |

**Critère de fin.**
1. Les références Ghoul2 sont bit-exactes sur x86-64 et ARM64 après chaque PR.
2. Sur ta démo 32 joueurs, preset CPU-bound : −30 % de µs Ghoul2 par joueur visible, p99 −15 %.
3. `r_vbo 1` : au moins 60 % des batches monde servis par VBO, CPU backend −25 %. Sinon, `r_vbo` reste à 0 et on arrête cette piste.
4. 31 bots, `sv_fps 40` : zéro frame de rattrapage, zéro snapshot complet de repli, p99 sous 25 % du budget.
5. Gamestate livré en moins de 250 ms à rate 90000 (contre au moins 650 ms aujourd'hui).
6. Quand tout ça est tenu, `cl_autolodscale 0` passe dans `enhanced`.

**Après la phase 4, à la carte** selon les réactions de la communauté : météo modernisée (FE-13), flares par occlusion queries (FE-12), recherche dans les démos (PX-8, puis PX-20 et PX-22), manettes et gyro avec mise à jour de SDL (PX-15), liens `jk2mv://` (PX-18), particules douces (VQ-12), packs HD (VQ-14).

### Piste 1.05 (à part, au choix du serveur)
Un protocole 17 négocié, aux limites relevées (64 joueurs, plus d'entités, de modèles et de sons). Il demande un fork de mvsdk et laisse les 1.02-1.04 intactes. La conception, les étapes et les questions ouvertes sont dans [PROTOCOL-1.05.md](PROTOCOL-1.05.md).

## 4. Premier jalon concret

Avant tout code (toi, un week-end) : BM-3 et BM-4, décrits au §5.

1. **PR 0, BM-17 (une soirée).** `build.yml:179` passe en `windows-2022`, `:263` en `macos-13` ou `macos-14`. Ajouter à `tests.yml` un job qui compile vraiment `jk2mvded`.
2. **PR 1, trois correctifs pour l'amont (une soirée, trois commits).**
   - tr_world.cpp:752 : `if ( tr.refdef.num_dlights >= MAX_DLIGHTS ) { ...; dlightBits = 0xFFFFFFFF; }`.
   - cl_avi.cpp:433 : tester `s_UseOpenAL` et retirer la piste audio avec un avertissement.
   - sv_main_frame.h:76 : borner `sv_fps` à 1000, ce qui ne change rien pour qui reste en dessous.
3. **PR 2, cadence du glow.**
   - Supprimer le `qglFinish()` de tr_backend.cpp:1059, derrière `r_dynamicGlowFinish` (défaut 0). Le `g_bTextureRectangleHack` des vieux pilotes ATI en est probablement la raison historique.
   - Ajouter `&& !backEnd.viewParms.isPortal` à la condition de la ligne 1040. Cela change les miroirs quand le glow est actif : valider avec une image de référence sur une map à miroir.
4. **PR 3, passe gamma.**
   - Dans `R_BindGlowImages` (tr_image.cpp:2816-2823), allouer `sceneImage` en `GL_RGBA8`.
   - Remplacer `qglCopyTexImage2D` par `qglCopyTexSubImage2D` à tr_backend.cpp:1326. Le redimensionnement passe déjà par `R_UpdateImages` (tr_image.cpp:2905).
   - Mesurer sur iGPU en 4K.
5. **PR 4, `r_colorGrade`.**
   - Dans la branche post-process de `R_SetColorMappings` (tr_image.cpp:2975-2995), remplacer `gammaCorrected[x/y/z]` par `grade(x,y,z)` calculé avant gamma et overbright.
   - Étendre le test de reconstruction de tr_cmds.cpp:386-390 aux nouvelles cvars.
   - Test unitaire : un grade vide produit une LUT identique octet par octet.
   - Ensuite : `r_colorGradeSplit`, VQ-3a, VQ-8, `preset`, l'overlay.

## 5. Ce qu'il faut mesurer et comment

**Client (Windows, assets retail)**

- **Installation.**
  - Un build portable RelWithDebInfo par version testée, dans `C:\jk2bench\builds\<tag>\`, avec assets0/1/2/5.pk3 copiés dans `base\`.
  - Un profil commun : `+set fs_homepath C:\jk2bench\home`.
  - Vsync coupée, overlays (Steam, Discord) coupés, plan d'alimentation « Performances élevées ».
- **Démos de référence, 120 s chacune, figées avec leur SHA-256 pour toute la durée du projet.**
  - `bench_ffa` : ffa_bespin, environ 12 bots ajoutés avec `addbot <nom> 4` espacés, en spectateur avec `follow`.
  - `bench_ctf` : ctf_yavin.
  - `bench_duel` : sabres en gros plan.
  - En plus : une démo d'un vrai serveur public chargé et une .dm_15 en 1.02.
- **Presets de mesure.** Les cvars latched passent en `+set`, car un `+exec` n'est appliqué qu'après `R_Init`.
  - CPU-bound : `+set r_mode -1 +set r_customwidth 640 +set r_customheight 480`.
  - Classique : 1080p, réglages par défaut.
  - GPU-bound : 4K, `r_ext_multisample 4`, aniso 16, `r_DynamicGlow 1`.
- **Lancement.** Un processus par run, 1 run de chauffe + 5 runs mesurés :
  `Start-Process -Wait jk2mvmp.exe -ArgumentList '+set fs_homepath C:\jk2bench\home +set logfile 2 <preset> +set timedemo 1 +set nextdemo quit +demo bench_ffa'`
  - `nextdemo quit` fonctionne : `CL_NextDemo` exécute la commande avant le longjmp (cl_main.cpp:675-689). En revanche, enchaîner `demo x` via `nextdemo` serait tué par `CL_Disconnect_f`.
  - Après chaque run, copier `qconsole.log`, qui est tronqué à chaque démarrage, et y lire la ligne `(\d+) frames, ([\d.]+) seconds: ([\d.]+) fps`.
- **Percentiles.** PresentMon ou CapFrameX en attendant `benchmark` (phase 2). Vérifier que le CSV n'est pas vide sur un jeu OpenGL.
- **Règle de décision.** Comparer les médianes de fps et de p99. Un écart ne compte que s'il dépasse max(3 %, 2 × l'écart entre runs). Le plafond de 1000 fps peut saturer le preset 640×480 jusqu'à BM-2.
- **Images de référence.**
  `+set activeAction "wait 100; screenshot_tga g100; wait 100; screenshot_tga g200; ..." +set timedemo 1 +demo bench_ffa`
  - Le timedemo avance de exactement 50 ms par frame (cl_cgame.cpp:1664-1673) et `wait` compte des frames, donc les captures tombent toujours au même instant de jeu.
  - Comparer avec `magick compare -metric AE -fuzz 3% a.tga b.tga diff.png`. Les références dépendent de la machine.
- **Chaque PR de perf** joint son tableau avant/après et ses compteurs `r_speeds 8`. Sans chiffres, pas de merge.

**Serveur**

- **Commande de base :**
  `jk2mvded +set dedicated 1 +set sv_maxclients 32 +set sv_fps 20 +set sv_hibernateFps 0 +set g_gametype 0 +set fraglimit 0 +set timelimit 0 +set logfile 2 +map ffa_bespin +exec load.cfg`
- **load.cfg :** paliers de 8, 16, 24 puis 31 bots, `addbot <nom> 4` + `wait 40` entre chaque bot, puis `wait 1200` et `serverstats` (après BM-7). Refaire la série à `sv_fps 40`.
- **Pièges, tous vérifiés :**
  - les bots ne comptent pas comme joueurs, donc sans `sv_hibernateFps 0` le serveur hiberne à 4 Hz ;
  - sans `dedicated 1`, le serveur envoie des heartbeats aux masters publics ;
  - au-delà de 32 arguments `+`, les suivants sont ignorés silencieusement : passer par `+exec` ;
  - ajouter les bots sans `wait` les fait expulser pour « overflow » ;
  - une ERR_DROP sort avec le code 0, il faut donc lire le log.
- **CPU avant BM-7 :** `typeperf "\Process(jk2mvded)\% Processor Time" -si 1`, puis ms par frame = CPU% × 10 / sv_fps.
- **Les bots ne testent ni l'encodage ni le netchan.** Ajouter `sv_benchBotSnapshots 1` (BM-9), puis des clients headless (BM-10) avec une IP de loopback par client.
- **En CI :** soak sans assets sur BSP de 856 octets (BM-11), puis Callgrind en instructions par `SV_Frame` (BM-14), stable à environ 1 % près contre 10 à 30 % pour le temps mural.

## 6. Ce que toi seul peux faire

- Enregistrer les démos de référence avec les assets retail et lancer la baseline BM-3 **avant la première PR**, idéalement sur un portable iGPU et sur une machine avec carte dédiée.
- Générer et tenir à jour les images de référence sur ton GPU, et ne les rebaser qu'exprès. Les images llvmpipe de la CI ne jugent pas le vrai rendu.
- Trancher les questions de goût, en jouant en duel et en FFA :
  - taille et flou du glow, ambiances couleur et leurs bornes, contenu des presets ;
  - plus tard : seuil du bloom, tonemapper, intensité des dlights par pixel ;
  - vérifier que `classic` « sent » toujours le retail.
- Vérifier la compatibilité à la main à chaque phase : serveurs 1.02/1.03/1.04 réels, cgame de base, MVSDK, mods populaires. Les outils démo doivent refuser d'agir en partie live.
- Tester sur une matrice de pilotes (NVIDIA, AMD, Intel sous Windows, Mesa, macOS GL 2.1 legacy) avant que `r_fbo`, `r_vbo` ou `r_dlightMode` entrent dans un preset.
- Lancer les rampes de bots sur de vraies maps (fichiers `.wnt`) et organiser une soirée test à `sv_fps 40` avec de vrais joueurs avant de publier le preset `competitive`.
- Profiler avec Tracy (BM-15) ou WPR/WPA avant RB-5 et FE-3, pour que l'ordre suive les données plutôt que les estimations (±2x).
- Tourner et diffuser les vidéos : avant/après après la phase 1, « tes démos 2003 rejouées en cinématique » après les phases 2-3.
- Prendre les décisions produit :
  - garder ou abandonner le build XP ;
  - ce qui part en amont vers mvdevs ;
  - dépendances ffmpeg (processus externe) et libjpeg-turbo ;
  - plafond côté serveur des ambiances couleur ou non.

## 7. Risques et garde-fous

- **Rien n'est mesuré ici.** Tous les gains sont des estimations tirées du code et de forks comparables (Quake3e, rend2, ioq3), à ±2-3x. Garde-fous : la baseline d'abord, un seuil de bruit, et retirer toute PR qui n'atteint pas sa métrique.
- **À `com_maxfps 125`, les gains de débit sont invisibles pour beaucoup de joueurs.** Exprimer les objectifs en p99 et 1 % low, sur iGPU, en 240 Hz et à 32 joueurs. Le limiteur à ms entières par défaut reste intact, parce qu'il conditionne la physique.
- **Retirer le `glFinish` du glow** peut réveiller un vieux bug de pilote ATI. La porte `r_dynamicGlowFinish` est là pour ça.
- **Sauter la passe gamma quand la LUT est l'identité** (idée des trois brouillons) n'est pas bit-exact. Le shader échantillonne la LUT sans décalage d'un demi-texel, donc l'« identité » actuelle décale les valeurs jusqu'à environ 2 LSB aux extrêmes. Et avec `r_overBrightBits 1` par défaut, la LUT n'est presque jamais l'identité. Je ne garde donc que `CopyTexSubImage`.
- **Étalonnage et visibilité.** Il faut `r_gammamethod 2` et ARB fp : le signaler dans `gfxinfo`. Une ambiance extrême peut devenir une aide à la visibilité, donc bornes serrées et presets sobres. Un éventuel plafond serveur passerait par une configstring propre au fork, jamais par mvapi.h.
- **Les outils démo sont des wallhacks potentiels.** Chaque commande est conditionnée à `clc.demoplaying`, avec un test. `video-pipe` : cvar en `CVAR_VM_NOWRITE`, traitée comme arguments uniquement.
- **Ghoul2 est partagé avec le serveur.** Il faut les tests FE-16 sur x86 et ARM et `-ffp-contract=off` avant tout refactor. Ne pas « corriger » la garde morte `if (!boneUsedList)` (tr_ghoul2.cpp:1859).
- **VQ-5 (FBO) est l'étape la plus risquée :** MSAA (résolution par blit), stéréo, `GL_FRONT`, ordre captures/AVI, macOS legacy. Le chemin par copies reste le repli `r_fbo 0`.
- **RB-4, cache d'état GL :** il peut devenir faux près du code qui contourne les wrappers (flou du glow, gamma, tr_backend.cpp:1551-1569, 1669). Invalider le cache à ces frontières.
- **Mémoire :** SV-3 fait passer l'anneau de 19,4 à 77,6 Mo à 32 slots, plus FBO, VBO et SSAA. C'est serré sur les builds 32 bits.
- **Mods et tick à 40 Hz :** certains mods supposent des frames de 50 ms. Tester mod par mod, en opt-in.
- **Épuisement :** les phases 1 à 3 représentent environ un an à temps partiel. Chaque phase doit se suffire à elle-même. PX-4/6/7 peuvent être avancés pour garder la motivation. Les gros paris ne démarrent que sur preuve mesurée.

## 8. Idées écartées et pourquoi

- **Thread de rendu (RB-13)** : XL, risque de courses avec Ghoul2 et les requêtes cgame. ioq3 a abandonné le SMP. Mieux vaut le threading des pilotes (glthread) une fois RB-5 fait.
- **Matériaux type rend2 dans le renderer GL1 (VQ-17)** : il n'y a ni normal maps ni deluxemaps dans JK2. Si un jour c'est voulu, porter rd-rend2 d'OpenJK comme renderer séparé.
- **FOV Hor+ côté moteur (PX-3)** : le FOV appartient au cgame, avec un risque d'avantage compétitif. MVSDK a déjà `cg_fovAspectAdjust`.
- **Relever `MAX_CLIENTS`, `MAX_GENTITIES` ou la limite de 256 entités par snapshot** : c'est du protocole ou de l'ABI des VM, donc interdit par la contrainte de compatibilité.
- **Paralléliser la simulation du jeu** : syscalls VM synchrones et CM global. Seule la phase snapshots (SV-8) est parallélisable, et son gain reste inconnu tant que BM-7 n'a pas mesuré.
- **JIT QVM Quake3e (SV-10), relais MVTV (SV-12), skinning GPU (FE-8), pool de threads Ghoul2 (FE-6), évaluation paresseuse des points d'attache (FE-7)** : chacun L ou XL. Ce sont des spikes go/no-go à lancer seulement si un profil montre que leur cible pèse au moins 20 % du temps de frame.
- **SSAO (VQ-13), shadow maps joueurs (VQ-16)** : coût CPU (double skinning), équité (ombres visibles derrière les coins), peu de « wow » par heure investie.
- **Fusion d'étapes sur 4 TMU (RB-10)** : écart d'environ 1 LSB, gain incertain. On regarde seulement si `r_speeds 8` montre beaucoup de shaders à 3 étapes ou plus.
- **API temps sub-ms (PX-23) et navigateur 2.0 (PX-24)** : demandent des changements dans mvsdk, coordination amont nécessaire.
- **Discord Rich Presence (PX-19)** : dépendance tierce et question de vie privée, plus tard et opt-in.
- **Skip de la passe gamma quand la LUT est l'identité** : pas bit-exact et rarement applicable (voir §7).