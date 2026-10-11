# Piste 1.05 : un protocole aux limites relevées

> Document de conception. Seule l'étape 1 est implémentée (§8), sans rien changer au comportement. Les 1.02, 1.03 et 1.04 restent intactes : 1.05 est un mode **en plus**, choisi par le serveur.

## 1. Pourquoi

Les phases 0 à 4 gardent la compatibilité totale avec les clients et les mods existants. Certaines limites sont pourtant gravées dans le protocole et dans les modules de jeu (QVM). Aucune optimisation ne peut les dépasser :

| Limite | Valeur | Où elle est figée |
|---|---|---|
| Joueurs | `MAX_CLIENTS` 32 | q_shared.h:1324, tableaux des QVM, configstrings des joueurs |
| Entités | `MAX_GENTITIES` 1024 (`GENTITYNUM_BITS` 10) | numéros d'entité codés sur 10 bits dans les snapshots |
| Modèles et sons | `MAX_MODELS` et `MAX_SOUNDS` 256 | indices envoyés sur 8 bits (q_shared.h:1338) |
| Configstrings | `MAX_CONFIGSTRINGS` 1400, `MAX_GAMESTATE_CHARS` 16000 | gamestate, positions des CS figées dans les QVM |
| Entités par snapshot | `MAX_ENTITIES_IN_SNAPSHOT` 256 | cg_public.h:12, structures des QVM |
| Taille de message | `MAX_MSGLEN` 16384 | netchan, fragmentation |

Une version 1.05 permettrait :
- des serveurs à 64 joueurs ;
- des cartes plus riches en entités, modèles et sons ;
- de nouveaux contenus qui débordent aujourd'hui les 256 modèles ou sons.

## 2. Principe : un mode négocié, pas un remplacement

- **Le serveur choisit.** Un serveur lance un mod compilé pour 1.05 (fork de mvsdk aux constantes relevées). Il annonce `protocol 17` et `mvversion 1.05` dans son serverinfo. Sans mod 1.05, rien ne change.
- **Le client suit.** Un client JK2MV assez récent reconnaît le protocole 17 et passe en mode 1.05, comme il bascule déjà entre 1.02, 1.03 et 1.04 (`MV_SetCurrentGameversion`). Un client 1.04 d'origine reçoit un refus clair : « ce serveur demande JK2MV 1.05 ».
- **Le moteur porte les deux jeux de limites.** Il est compilé aux valeurs maximales (64 joueurs, 2048 entités…) et applique à l'exécution les limites du protocole en cours.
  - Pour les QVM 1.02-1.04, la conversion de structures existe déjà : `G_MVAPI_DISABLE_STRUCT_CONVERSION` montre que le moteur traduit entre plusieurs dispositions de `playerState_t` et `entityState_t`.
  - On ajoute une disposition « 1.05 ».
- **La négociation passe par l'API MV.** Un niveau `MV_APILEVEL 5` ajoute un syscall du type `trap_MVAPI_SetLimits( const mvlimits_t * )`. Le mod 1.05 l'appelle à l'init pour déclarer les limites qu'il a été compilé avec. Le moteur refuse un mod qui déclare plus que ce qu'il sait gérer.

## 3. Ce qui change, couche par couche

1. **q_shared, qcommon, serveur.** Les constantes deviennent des maximums de compilation, avec des limites courantes à l'exécution :
   - le nombre d'emplacements est le cvar verrouillé `sv_maxclients` (il n'existe pas de `sv.maxClients`) ; l'étape 1 ajoute les limites du protocole courant, `MV_MaxClients()` et `MV_MaxGEntities()` (`qcommon/mv_limits.h`) ;
   - il faut ajouter le nombre de bits d'entité, les bits de modèle et de son, et la table des configstrings.
2. **Encodage réseau** (msg.cpp) :
   - les numéros d'entité, les indices de modèle et de son et les champs de delta lisent leur nombre de bits dans le protocole courant ;
   - les tables de champs (`entityStateFields`, `playerStateFields`) reçoivent une variante 17 ;
   - Huffman et le netchan ne changent pas.
3. **Snapshots.** L'anneau d'entités (SV-3) et le budget par snapshot suivent les nouvelles limites. L'`areamask` est par zone de carte, pas par joueur : il ne change pas. Les masques de clients du module (`broadcastClients[2]`, les quatre masques de 16 bits du mind trick) couvrent déjà 64 joueurs. `snapshotIgnore[32]` et `snapshotEnforce[32]` de l'API MV ne grandissent pas sur place : le `mvsharedEntity_t` des niveaux 1 à 4 est partagé avec les modules existants et ne change jamais. Les modules 1.05 (`MV_APILEVEL 5`) ont leur propre structure, avec des tableaux de 64.
4. **QVM et mods.** Il faut un fork de mvsdk recompilé, avec :
   - les constantes relevées ;
   - les tableaux par joueur à 64 ;
   - les indices de configstrings recalculés.

   C'est le gros du travail côté contenu, et c'est aussi là que naissent les mods 1.05.
5. **Démos.** Une nouvelle extension `.dm_17`. Le lecteur JK2MV lit toutes les versions, les outils démo des phases 2-3 compris.
6. **Navigateur de serveurs.** Les serveurs 1.05 s'affichent, marqués comme tels. Les clients trop anciens les voient grisés.

## 4. Ce qui ne change pas

- Les cartes et assets existants se chargent tels quels. Les nouvelles cartes peuvent exploiter les limites relevées.
- Les serveurs et mods 1.02-1.04, et tout ce qui a été fait en phases 0-4 : leur physique (pmove), leurs armes et leur Force restent celles d'aujourd'hui.

Le gameplay du mode 1.05, lui, peut changer (§7, point 4) : `sv_fps 40` par défaut, correctifs jamais faits par compatibilité. La liste des changements reste à arrêter avec le propriétaire avant l'étape 3.

## 5. Étapes proposées

1. **Limites à l'exécution sans changer le protocole.**
   - Remplacer dans le moteur les usages directs de `MAX_CLIENTS` et `MAX_GENTITIES` par les limites courantes, en gardant 32 et 1024 partout.
   - Comportement identique, vérifié par les tests de fumée existants (31 bots, rendu), par `tests/limits/verify_fold.py` (objets identiques à la base) et par des tests qui figent les numéros d'entité et les octets des démos 1.02-1.04.
   - **Avancement :** faite, sans changer une valeur (32 joueurs, 1024 entités). Mise en œuvre et liste pour l'étape 2 au §8.
2. **Protocole 17 côté moteur :**
   - tables de champs, bits d'entité, de modèle et de son ;
   - négociation (`trap_MVAPI_SetLimits`) ;
   - un mod de test minimal qui déclare 64 joueurs et 2048 entités.
   - Tests unitaires d'encodage aller-retour 15, 16 et 17, et smoke test à 63 bots.
3. **Fork mvsdk 1.05** :
   - constantes relevées ;
   - recompilation des QVM ;
   - CI qui construit le mod et lance le serveur à 63 bots sur ci_box.
4. **Client :**
   - bascule en 1.05 ;
   - démos `.dm_17` ;
   - navigateur ;
   - message de refus pour les anciens clients.
5. **Contenu :** une première carte ou un pack qui profite des nouvelles limites (plus de modèles et de sons, combats à 64).

Effort estimé :
- étapes 1-2 : L ;
- étape 3 : M à L selon l'ampleur du fork ;
- étapes 4-5 : M.

## 6. Questions posées au propriétaire (réponses au §7)

1. **Quelles limites comptent le plus ?** 64 joueurs ? Plus d'entités (cartes) ? Plus de modèles et de sons (contenu) ? On peut tout relever, mais chaque limite a un coût en bande passante et en mémoire. 64 joueurs à sv_fps 40, c'est environ 4 fois le trafic d'aujourd'hui.
2. **Serveurs 1.05 seulement, ou passerelle ?** Les serveurs 1.05 peuvent refuser les clients 1.02-1.04 (simple). Ils peuvent aussi les accepter en les limitant à 32 joueurs visibles et 1024 entités : beaucoup plus difficile, à n'envisager que si la communauté en a besoin.
3. **Où vit le fork du mod ?** Un fork de mvsdk sur ton GitHub, en sous-module comme aujourd'hui, ou un dossier dans ce dépôt ?
4. **Gameplay :** reste-t-il strictement identique (seules les limites changent), ou 1.05 est-il l'occasion de changements (`sv_fps 40` par défaut, correctifs de gameplay jamais faits par compatibilité) ?
5. **Calendrier :** l'étape 1 est sans risque et prépare tout. Faut-il la lancer dès maintenant, avant que les phases 3-4 soient vérifiées sur ta machine ?

## 7. Décisions (2026-10-11)

Réponses du propriétaire aux questions du §6 :

1. **Limites :** on relève tout (joueurs, entités, modèles et sons, configstrings), pas seulement les joueurs.
2. **Anciens clients :** un serveur 1.05 refuse les clients 1.02-1.04 avec un message clair. Pas de passerelle.
3. **Fork du mod :** un fork de mvsdk sur le GitHub du propriétaire, en sous-module comme aujourd'hui.
4. **Gameplay :** 1.05 peut changer le gameplay (`sv_fps 40` par défaut, correctifs jamais faits par compatibilité). La liste des changements reste à arrêter avec le propriétaire avant l'étape 3.
5. **Calendrier :** l'étape 1 (limites à l'exécution, comportement inchangé) démarre tout de suite.

## 8. Étape 1 : mise en œuvre

L'étape 1 prépare le moteur sans rien changer : mêmes octets réseau, mêmes démos, même ABI des QVM, mêmes valeurs. L'étape 2 n'aura plus qu'à changer les limites à l'exécution et les tables d'encodage.

### Les limites du protocole courant

- `qcommon/mv_limits.h` donne une table, `MV_LimitsForProtocol( protocole )`. Chaque ligne a trois valeurs : le nombre de joueurs, les bits d'un numéro d'entité, le nombre d'entités.
- Les protocoles 15 et 16, `PROTOCOL_UNDEF` et tout protocole inconnu ont la ligne historique : 32 joueurs, 10 bits, 1024 entités. Ces valeurs (`MV_LEGACY_*`) sont écrites en chiffres exprès : elles ne bougent pas quand les maximums montent.
- La table suit le protocole, pas la version : 1.02 et 1.03 partagent le protocole 15.
- Les accesseurs lisent la ligne de `MV_GetCurrentProtocol()` :
  - `MV_MaxClients()` et `MV_MaxGEntities()` ;
  - `MV_ENTITYNUM_NONE` (`MV_MaxGEntities() - 1`) et `MV_ENTITYNUM_WORLD` (`- 2`).
- `MV_CurrentLimits()` (common.cpp) n'a ni état ni initialisation. Avant le choix d'une version, c'est la ligne historique.
- `MAX_CLIENTS`, `MAX_GENTITIES` et `GENTITYNUM_BITS` deviennent des maximums de compilation : ils dimensionnent les tableaux.
- `MV_LIMITS_ROW_VALID` vérifie qu'une ligne tient dans les tableaux du moteur et dans les masques de 64 joueurs de l'ABI des modules (`broadcastClients[2]`, les masques du mind trick). Un `static_assert` fige l'étape 1 à 32 joueurs et 1024 entités.
- `snapshotIgnore[32]` et `snapshotEnforce[32]` de l'API MV (`mvsharedEntity_t`) ne tiennent que 32 joueurs. C'est la disposition des modules des niveaux 1 à 4, qui ne change jamais ; le niveau 5 aura sa propre structure. `MV_LIMITS_ROW_VALID` ne les regarde pas, une garde marque l'endroit qui les indexe (ci-dessous).

### La règle

- Un numéro de joueur ou d'entité qui vient du réseau, d'un module, de botlib ou d'un cvar se compare à la limite courante (`MV_MaxClients()`, `MV_MaxGEntities()`).
  - Sauf avant le choix de la version : la ligne courante est alors celle de la carte précédente, ou de `PROTOCOL_UNDEF` sur un serveur qui démarre. `SV_BoundMaxClients`, appelé par `SV_Startup` et `SV_ChangeMaxClients` avant ce choix, borne donc `sv_maxclients` par `MAX_CLIENTS`, la capacité du moteur.
- Une sentinelle échangée avec un module, botlib ou `SV_Trace` vient de `MV_ENTITYNUM_NONE` ou `MV_ENTITYNUM_WORLD`.
- Les boucles qui écrivent des baselines sur le réseau (gamestate, démos) s'arrêtent à `MV_MaxGEntities()`.
- Une taille de tableau, une remise à zéro ou un parcours de tout un tableau et les pools de botlib gardent `MAX_*`.
  - Un parcours de tout un tableau ne passe pas par le contrôle de la limite courante : au-delà de cette limite, il afficherait une erreur par emplacement et sauterait ceux-ci. `S_ClearLoopingSounds` borne donc ses emplacements par la taille de `loopSounds`, et seul `S_StopLoopingSound`, appelé par cgame, par `MV_MaxGEntities()`.
- L'encodage 15/16 (tables de champs, numéros d'entité sur `GENTITYNUM_BITS`, indices de modèle et de son sur 8 bits, marqueur de fin, numéro d'une entité retirée) et les configstrings des modules 1.02-1.04 (disposition de bg_public.h, `gameState_t` copié dans cgame) restent tels quels. Des `static_assert` (mv_limits.h) arrêtent la compilation à ces endroits dès qu'un maximum change :
  - `MV_ASSERT_LEGACY_ENTITY_ENCODING()` (`GENTITYNUM_BITS`, `MAX_GENTITIES`) : msg.cpp (tables de `entityState_t` et de `playerState_t`), cl_parse.cpp, sv_snapshot_delta.h ;
  - `MV_ASSERT_LEGACY_MODEL_ENCODING()` (`MAX_MODELS`, `MAX_SOUNDS`) : msg.cpp, devant les tables de `entityState_t` et de `playerState_t`. Les indices sur 8 bits ne sont pas seulement `modelindex`, `modelindex2` et `loopSound` : les paramètres d'événement portent aussi des indices de son (`G_Sound`, `G_AddEvent( ent, EV_GENERAL_SOUND, … )`), dans `eventParm` de `entityState_t`, `externalEventParm` et `eventParms[1]` de `playerState_t`, et `weapon` pour `EV_DEBRIS` ;
  - `MV_ASSERT_LEGACY_CONFIGSTRINGS()` (`MAX_CLIENTS`, `MAX_MODELS`, `MAX_SOUNDS`, `CS_PLAYERS`, `CS_MAX`, `MAX_CONFIGSTRINGS`, `MAX_GAMESTATE_CHARS`) : strip.cpp, cl_cgame.cpp (`CL_GetGameState`, `CL_ConfigstringModified`), cl_demos_auto.cpp, sv_client_usercmd.h, sv_client_userinfo.h ;
  - `MV_ASSERT_LEGACY_MVAPI_CLIENTS()` (`MAX_CLIENTS` à 32, et toute la disposition de `mvsharedEntity_t` des niveaux 1 à 4 : `snapshotIgnore[32]` à 0, `snapshotEnforce[32]` à 32, `mvFlags` à 64, 68 octets) : sv_snapshot_entities.h. Agrandir ces tableaux ne passe donc pas la garde ;
  - `G2_LoadGhoul2Model` (G2_misc.cpp) cherche aussi une place dans `CS_MODELS`, mais rien ne l'appelle.
  - Le code moteur qui parle des configstrings des joueurs utilise `CS_PLAYERS + MV_MaxClients()`, pas `CS_CHARSKINS`.
- Le moteur de rendu n'utilise jamais les accesseurs. Son numéro de monde est un numéro de refEntity dans la clé de tri : `REFENTITYNUM_WORLD` (tr_local.h), 1022, tiré de la disposition de la clé de tri.
- botlib n'a plus sa copie de `MAX_CLIENTS`, `MAX_MODELS` et `MAX_SOUNDS` (be_aas_def.h) : elle aurait gardé 32 après l'étape 2.

### Les vérifications

- **Recensement** (test ctest `limits_census`) : `tests/limits/limits_census.py` relève chaque usage restant des symboles de limite dans `src/` et le compare à `tests/limits/allowlist.txt`.
  - Classes : `def` (définition), `capacity` (taille de tableau, pool), `wire` (encodage 15/16), `cs-layout` (configstrings 1.02-1.04), `abi` (structure partagée avec les modules), `dead` (code jamais compilé ni appelé).
  - 114 usages classés. Un usage non classé fait échouer le test : on le convertit, ou on le classe avec sa raison.
  - Les accesseurs sont interdits dans le moteur de rendu, et `MV_CurrentLimits()` hors de common.cpp, où qu'ils soient sur la ligne.
  - `tests/limits/test_limits_census.py` (test ctest `limits_census_selftest`) vérifie ces règles sur de petits arbres écrits pour chaque cas.
- **Repliement** : `tests/limits/verify_fold.py <build> <base>` compile chaque unité de jk2mvmp et jk2mvded deux fois : les sources de la base, et les sources courantes avec `-DMV_LIMITS_VERIFY_FOLD`, où les accesseurs redeviennent `MAX_CLIENTS` et `MAX_GENTITIES`.
  - La base est le dernier commit avant l'étape 1 dans `src/`, le parent de « refactor(qcommon): limits of the protocol in use (mv_limits.h) ». Son empreinte change à chaque rebase : `"$(git log -1 --format=%H --grep='^refactor(qcommon): limits of the protocol in use')^"` la retrouve.
  - Les deux objets ont les mêmes sections et les mêmes tailles (`.bss` comprise), les mêmes instructions, les mêmes données initialisées et les mêmes relocations : mêmes fonctions appelées, mêmes globales lues ou écrites, mêmes cibles des pointeurs. La table des symboles et les informations de débogage ne sont pas comparées.
  - Seules exceptions : be_aas_main.cpp et be_interface.cpp, où la valeur par défaut de `maxentities` vient de `va()`, toujours « 1024 ».
  - Linux et binutils seulement, pas dans ctest.
- **Tests figés**, en chiffres : 1022 et 1023 dans `SV_Trace`, les numéros d'entité sur 10 bits, l'empreinte d'une démo .dm_15 et d'une .dm_16, `sv_maxclients 64` ramené à 32 sur une carte 1.04 (test de fumée ; aujourd'hui par le plafond `MAX_CLIENTS` de `SV_BoundMaxClients`, avant le choix de la version).
- **Sondes** : avec une ligne plus petite (16 joueurs, 512 entités), `SV_Trace`, les baselines des démos et `MSG_*DeltaEntity` suivent la limite courante ; `SV_BoundMaxClients`, `SV_Startup` et `SV_ChangeMaxClients` ne la suivent pas (`SvBoundMaxClients.*`). `MvLimits.LegacyConfigstringLayout` échoue exprès dès que `MAX_CLIENTS`, `MAX_MODELS` ou `MAX_SOUNDS` montent sans table de configstrings par protocole.

### Pour l'étape 2

1. Relever les maximums dans q_shared.h (joueurs, entités, modèles, sons, configstrings) et retirer le verrou « step 1 keeps 32 clients and 1024 entities ».
   - La compilation s'arrête alors à chaque garde `MV_ASSERT_LEGACY_*` (liste dans « La règle »).
   - Rendre chacun de ces endroits dépendant du protocole : tables de champs, bits d'un numéro d'entité (ajouter `MV_GEntityBits()`), bits des indices de modèle et de son (paramètres d'événement compris : `eventParm`, `externalEventParm`, `eventParms[1]`, `weapon` d'`EV_DEBRIS`), marqueur de fin, numéro d'une entité retirée, affichage `cl_shownet`, `CS_PLAYERS` et `gameState_t` des modules.
   - La liste des endroits : ces gardes, et pour les joueurs et les entités les entrées `wire`, `cs-layout` et `abi` du recensement. Le recensement ne compte pas les symboles des modèles, des sons ni des configstrings (`MAX_MODELS`, `CS_PLAYERS`…) : seules les gardes marquent leurs endroits.
2. Ajouter `PROTOCOL17` et sa ligne dans `MV_LimitsForProtocol` (`case PROTOCOL17: return &v105;`). Les autres protocoles gardent la ligne historique. `MV_LIMITS_ROW_VALID` impose au plus 64 joueurs.
3. Borner `sv_maxclients` par `MV_MaxClients()` après le choix de la version, à chaque carte.
   - Aujourd'hui, `SV_SpawnServer` choisit la version (`MV_SetCurrentGameversion`) après `SV_Startup` et `SV_ChangeMaxClients`. Ceux-ci ne bornent `sv_maxclients` qu'au démarrage ou quand le cvar a changé, par `MAX_CLIENTS` et non par `MV_MaxClients()`, et dans `SV_BoundMaxClients` le minimum (le plus haut emplacement occupé + 1) passe avant le plafond.
   - Ce plafond doit rester `MAX_CLIENTS` : avant le choix, la ligne courante est celle de la carte précédente ou de `PROTOCOL_UNDEF`, à 32 joueurs. Avec `MV_MaxClients()` à sa place, `+set sv_maxclients 64 +map x` sur un serveur 1.05 serait ramené à 32, et `svs.clients` alloué pour 32, avant que le protocole 17 soit choisi.
   - Choisir la version plus tôt ne suffit donc pas : d'une carte en protocole 17 à 64 joueurs à une carte 1.02-1.04, `sv_maxclients` resterait à 64 avec un module de 32 joueurs. Le moteur parcourt les emplacements jusqu'à `sv_maxclients` (`MV_FixSaberStealing` écrit dans chaque `playerState_t` à chaque frame) alors que `G_LOCATE_GAME_DATA` ne valide le tableau des clients du module que pour `MV_MaxClients()` entrées.
   - Vers le bas : ramener `sv_maxclients` au plafond à chaque carte, même s'il n'a pas changé, le plafond passant avant les emplacements occupés : déconnecter les clients au-delà, ou refuser le changement de protocole sur un serveur lancé.
   - Vers le haut : une carte 1.05 après une carte 1.02-1.04 retrouve les emplacements demandés. Garder la valeur demandée à part et prendre `min( demandée, MV_MaxClients() )` à chaque carte, pas un cvar qu'une ligne précédente a abaissé. Agrandir `svs.clients` passe par `SV_ChangeMaxClients`, qui le réalloue.
   - Contrôler le plafond aussi en Release. L'assert qui suit le choix de la version (sv_init_mapload.h) n'existe qu'en Debug, et ne tient à l'étape 1 que parce que toutes les lignes ont 32 joueurs.
4. Faire de la disposition des configstrings une table par protocole (la chaîne de bg_public.h), avec la taille du `gameState_t` des modules. Les endroits de `MV_ASSERT_LEGACY_CONFIGSTRINGS()` y lisent `CS_PLAYERS`, et `CL_GetGameState` le `gameState_t` à copier. `MvLimits.LegacyConfigstringLayout` échoue jusque-là.
5. API MV : les modules 1.05 (`MV_APILEVEL 5`, `trap_MVAPI_SetLimits( const mvlimits_t * )`) partagent une structure à part, avec `snapshotIgnore[64]` et `snapshotEnforce[64]`. L'endroit de `MV_ASSERT_LEGACY_MVAPI_CLIENTS()` lit la structure du niveau d'API du module.
   - Le `mvsharedEntity_t` des niveaux 1 à 4 (api/mvapi.h) ne change jamais. Les modules existants le gardent : 68 octets, `snapshotEnforce` à 32, `mvFlags` à 64. Agrandir ses tableaux ferait lire au moteur `snapshotEnforce` et `mvFlags` au mauvais endroit pour tous ces modules, et le `mvFlags` de la dernière entité au-delà de la zone que `VM_ArgArray` a validée.
6. `MV_GetCurrentProtocol()` est commun au client et au serveur. Vérifier qu'aucun chemin ne lit un protocole côté client pendant qu'un serveur local en utilise un autre (lecture de démo, connexion ailleurs).
7. Démos : ajouter `.dm_17`. `SvDemo.LegacyDemoBytesNeverChange` garde ses deux empreintes.
8. Relancer le recensement et reclasser : les entrées `capacity` suivent les maximums d'elles-mêmes ; revoir les entrées `abi` et `dead`.

À corriger avant qu'une ligne grandisse, dans une branche à part (ces correctifs changent le comportement) :
- `SV_LocateGameData` et `MVAPI_LocateGameData` (sv_game_data.h) : `numGEntities` n'est pas borné par `MV_MaxGEntities()`.
- `ValidEntityNumber` (be_interface.cpp) : `num > maxentities` devrait être `>=`.
- be_ai_chat.cpp : la boucle d'arrêt parcourt 0 à `MAX_CLIENTS - 1` alors que les handles vont de 1 à `MAX_CLIENTS`.
- sv_snapshot_entities.h : `snapshotIgnore`, `snapshotEnforce` et `broadcastClients[ps.clientNum/32]` sont indexés par `ps.clientNum`, que seul `MV_MaxGEntities()` borne (sv_snapshot_builder.h). Le comparer à `MV_MaxClients()`.
- be_ea.cpp : les `EA_*` ne contrôlent pas le numéro de client.
