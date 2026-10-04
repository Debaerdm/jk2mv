# Piste 1.05 : un protocole aux limites relevées

> Document de conception, rien n'est implémenté. Les 1.02, 1.03 et 1.04 restent intactes : 1.05 est un mode **en plus**, choisi par le serveur.

## 1. Pourquoi

Les phases 0 à 4 gardent la compatibilité totale avec les clients et les mods existants. Certaines limites sont pourtant gravées dans le protocole et dans les modules de jeu (QVM). Aucune optimisation ne peut les dépasser :

| Limite | Valeur | Où elle est figée |
|---|---|---|
| Joueurs | `MAX_CLIENTS` 32 | q_shared.h:1321, tableaux des QVM, configstrings des joueurs |
| Entités | `MAX_GENTITIES` 1024 (`GENTITYNUM_BITS` 10) | numéros d'entité codés sur 10 bits dans les snapshots |
| Modèles et sons | `MAX_MODELS` et `MAX_SOUNDS` 256 | indices envoyés sur 8 bits (q_shared.h:1336) |
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
   - `sv.maxClients` existe déjà ;
   - il faut ajouter le nombre de bits d'entité, les bits de modèle et de son, et la table des configstrings.
2. **Encodage réseau** (msg.cpp) :
   - les numéros d'entité, les indices de modèle et de son et les champs de delta lisent leur nombre de bits dans le protocole courant ;
   - les tables de champs (`entityStateFields`, `playerStateFields`) reçoivent une variante 17 ;
   - Huffman et le netchan ne changent pas.
3. **Snapshots.** L'anneau d'entités (SV-3) et le budget par snapshot suivent les nouvelles limites. Les `areamask` et les bits de visibilité par client passent à 64 joueurs.
4. **QVM et mods.** Il faut un fork de mvsdk recompilé, avec :
   - les constantes relevées ;
   - les tableaux par joueur à 64 ;
   - les indices de configstrings recalculés.

   C'est le gros du travail côté contenu, et c'est aussi là que naissent les mods 1.05.
5. **Démos.** Une nouvelle extension `.dm_17`. Le lecteur JK2MV lit toutes les versions, les outils démo des phases 2-3 compris.
6. **Navigateur de serveurs.** Les serveurs 1.05 s'affichent, marqués comme tels. Les clients trop anciens les voient grisés.

## 4. Ce qui ne change pas

- La physique (pmove), les armes et la Force : un mod 1.05 reste du JK2.
- Les cartes et assets existants se chargent tels quels. Les nouvelles cartes peuvent exploiter les limites relevées.
- Les serveurs et mods 1.02-1.04, et tout ce qui a été fait en phases 0-4.

## 5. Étapes proposées

1. **Limites à l'exécution sans changer le protocole.**
   - Remplacer dans le moteur les usages directs de `MAX_CLIENTS` et `MAX_GENTITIES` par les limites courantes, en gardant 32 et 1024 partout.
   - Comportement identique, vérifié par les tests de fumée existants (31 bots, rendu).
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

## 6. Questions ouvertes (à trancher par toi)

1. **Quelles limites comptent le plus ?** 64 joueurs ? Plus d'entités (cartes) ? Plus de modèles et de sons (contenu) ? On peut tout relever, mais chaque limite a un coût en bande passante et en mémoire. 64 joueurs à sv_fps 40, c'est environ 4 fois le trafic d'aujourd'hui.
2. **Serveurs 1.05 seulement, ou passerelle ?** Les serveurs 1.05 peuvent refuser les clients 1.02-1.04 (simple). Ils peuvent aussi les accepter en les limitant à 32 joueurs visibles et 1024 entités : beaucoup plus difficile, à n'envisager que si la communauté en a besoin.
3. **Où vit le fork du mod ?** Un fork de mvsdk sur ton GitHub, en sous-module comme aujourd'hui, ou un dossier dans ce dépôt ?
4. **Gameplay :** reste-t-il strictement identique (seules les limites changent), ou 1.05 est-il l'occasion de changements (`sv_fps 40` par défaut, correctifs de gameplay jamais faits par compatibilité) ?
5. **Calendrier :** l'étape 1 est sans risque et prépare tout. Faut-il la lancer dès maintenant, avant que les phases 3-4 soient vérifiées sur ta machine ?
