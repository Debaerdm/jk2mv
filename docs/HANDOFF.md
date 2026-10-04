# Passation : reprendre le travail en local

Ce fichier note l'état de la branche `claude/explore-codebase-A8IMd` au moment où le travail quitte la session cloud. Une session Claude Code locale peut s'en servir comme point de départ, avec docs/ROADMAP.md (section « Avancement » de chaque phase) et CVARS.rst.

## Ce qui est fait

- **Phase 0 :**
  - le build compile à nouveau ;
  - tests unitaires (555) ;
  - tests de fumée sous Linux (serveur à 31 bots, client, rendu via `testscene`) ;
  - timers en microsecondes ;
  - CI verte, job WinXP compris.
- **Phase 1 :**
  - `preset classic|enhanced|ultra|competitive|movie` ;
  - color grading ;
  - overlay de perf ;
  - plein écran fenêtré, Alt+Entrée ;
  - alpha-to-coverage.
- **Phase 2 :**
  - outils démo (caméra libre, chemins de caméra squad, photomode) ;
  - captures PNG ;
  - `benchmark` ;
  - `video_mp4` ;
  - chronomètres GPU (`r_gpuTimers`) ;
  - flou de mouvement vidéo (`cl_aviMotionBlur`).
  - Reste SV-11 (démos côté serveur).
- **Phase 3 :**
  - `r_fbo` ;
  - `r_hdr`, `r_bloom` ;
  - glow sans copies d'écran ;
  - dlights par pixel (`r_dlightMode`) ;
  - priorité des dlights ;
  - grading avant le HUD ;
  - FOV écran large (`cl_fovAspectFix`).
  - Restent VQ-7 `r_renderScale` (voir plus bas) et VQ-11 (flou de mouvement temps réel).
- **Phase 4 :**
  - `r_maxFrameLatency` ;
  - `-ffp-contract=off` et option `UseLTO` ;
  - arène Ghoul2.
  - La piste serveur était en cours dans le cloud (voir plus bas).
- **Piste 1.05 :** conception dans docs/PROTOCOL-1.05.md. Les questions ouvertes attendent tes réponses.

## À essayer en jeu (assets retail)

- `preset enhanced`, `preset ultra`, `preset movie`, puis `preset classic` pour revenir à l'original.
- `cl_perfOverlay 2` et `r_gpuTimers 1` pour voir les fps, le temps GPU, le glow et le post.
- `r_fbo 1` + `r_DynamicGlow 1` : comparer le temps du glow avec et sans `r_fbo` (critère 2 de la phase 3).
- Démo : `demo_freecam`, `cam_add` / `cam_play`, `photomode`, `video_mp4` (ffmpeg dans le PATH).
- `testscene ci_box -256 0 96` existe aussi sans assets, pour essayer le rendu.

## En cours au moment de la passation (non terminé)

- **Revue adversariale phases 2-3 :**
  - capture, outils démo, video_mp4 et post-traitement : constats corrigés et poussés ;
  - restent `testscene` et « correctifs de la revue précédente » : leurs conclusions n'étaient pas arrivées.
- **Piste serveur phase 4 :** les branches suivantes n'existaient que dans le conteneur cloud.
  - `sv3-snapring` : terminé, non vérifié, anneau de snapshots `sv_snapshotEntityBudget`.
  - `sv2-huffman`, `bm7-serverstats`, `sv13-usercmd`, `sv7-world` : en cours.
  - S'ils n'ont pas été poussés, il faut les refaire. Les fiches détaillées sont dans docs/roadmap-opportunities.md (SV-2, SV-3, BM-7, SV-13, SV-7).
- **VQ-7 `r_renderScale`, conçu mais pas écrit :**
  - Dans tr_postfx.cpp, garder la taille de la fenêtre (`pfx.windowWidth/Height`) et appliquer l'échelle à `glConfig.vidWidth/Height` après chaque `WIN_UpdateGLConfig` (fonction `R_ApplyRenderScale`, appelée dans `RE_UpdateGLConfig` et dans `R_InitPostFX`).
  - Ajouter une cible `final` à la taille de rendu. La passe gamma y dessine (au lieu de FB 0) et la lecture de pixels (captures, vidéo) y lit.
  - Juste avant `WIN_Present`, un `glBlitFramebuffer` GL_LINEAR vers la fenêtre (attention au scissor, qui coupe les blits).
  - `testscene` doit utiliser `winWidth/winHeight` pour son refdef, pas `vidWidth`.

## Construire et tester

- **Windows :** CMake + Visual Studio 2022.
  - `cmake -B build -A x64` puis compiler `jk2mvmp` (Release).
  - Copier assets0.pk3 à assets5.pk3 de ton JK2 dans `base\` à côté de `jk2mvmp.exe` (version portable).
- **Tests unitaires :** `-DBuildTests=ON`, puis `ctest` dans `build/tests`.
- **Tests de fumée :** Linux seulement (Python 3, Mesa llvmpipe, SDL offscreen ; WSL convient). Activer `-DBuildTests=ON -DSmokeTests=ON`, puis `ctest -L smoke`.
- La CI GitHub (`build.yml`, `tests.yml`) se lance à la main (workflow_dispatch) sur la branche.
