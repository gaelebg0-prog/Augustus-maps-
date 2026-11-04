# Augustus — support des cartes agrandies (512 → 1024)

Ce dossier contient **uniquement les fichiers source modifiés** par rapport au dépôt Augustus d'origine
(commit `1b4b029`), avec toutes les modifications déjà appliquées. Vous pouvez les copier dans votre
propre arbre de sources, modifier, puis compiler vous-même.

Les cartes elles-mêmes ne sont **pas** incluses : la carte d'origine est simplement centrée dans la
grille agrandie. Remplir le terrain autour (ou fabriquer de vraies cartes 512/1024) reste à faire de
votre côté, avec l'éditeur de scénarios.

## Contenu

```
src/…            tous les fichiers source modifiés (arborescence identique à augustus/src/)
CMakeLists.txt   le seul fichier de build modifié
PATCH.diff       le diff complet (git diff) — alternative à la copie de fichiers
FICHIERS.txt     la liste des fichiers concernés
README.md        ce fichier
```

## Installation

Deux méthodes, au choix.

**1. Copier les fichiers** (la plus simple) : copiez `src/` et `CMakeLists.txt` par-dessus ceux de
votre clone d'Augustus, en écrasant les fichiers existants.

**2. Appliquer le patch** depuis la racine de votre clone :

```bash
git apply --check /chemin/vers/PATCH.diff   # vérifie que ça passe
git apply         /chemin/vers/PATCH.diff
```

## Compilation

Comme Augustus normalement (CMake + compilateur C++) :

```bash
cmake -S . -B build
cmake --build build -j 8
```

Sous Windows avec MinGW (chemin utilisé pour tester) :

```bash
export PATH=/c/msys64/mingw64/bin:$PATH
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j 8
```

Le binaire et les données du jeu doivent rester à côté du dossier `C3/` comme d'habitude.

## Choisir la taille de carte

Dans `C3/augustus.ini`, section `[game]`, la clé `map_expanded_size` accepte les index suivants :

| index | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|---|
| taille | 162 | 192 | 256 | 320 | 384 | **512** | 640 | 768 | 1024 |

`5` (512×512) est la valeur par défaut. Le jeu réécrit ce fichier en quittant : vérifiez la valeur
après un test si vous changez de taille.

## Ce qui a été corrigé pour les grandes cartes

### 1. Tables de voisinage figées sur 162 tuiles

Les tables historiques (`MAP_GRID_STATIC_STRIDE`, tables 2×2 / 3×3 / 4×4) sont compilées pour une
grille de 162 : sur une carte 512 leurs valeurs tombaient sur la mauvaise ligne. Elles restent
déclarées telles quelles mais sont désormais converties à l'usage par
`map_grid_convert_legacy_offset()` (`src/map/grid.c`), appliqué dans :

- `building/house.c` — fusion et agrandissement des maisons (empreintes 2×2/3×3/4×4 + 3 directions)
- `map/road_network.c` — propagation du réseau routier
- `map/water_supply.c` — connecteurs de réservoirs et d'aqueducs
- `map/water.c` — canaux, quais, berges
- `map/tiles.c` — images de highway et d'accès rampe
- `widget/city/draw.c` — bâtiments voisins à redessiner
- `widget/city/highway.c` — direction des barrières
- `widget/city/building_ghost.c` — aperçu de construction (réservoir, fort)
- `window/building_info.c` — figures voisines dans la fenêtre d'information
- `editor/tool_restriction.c` — restrictions d'outils

### 2. Débordements mémoire

- `widget/map_editor.c` : `event_tiles[26244]` et le cache d'images de séisme, indexés par offset de
  grille, débordaient dès 512 → tableaux alloués à la taille réelle de la carte + garde de bornes.
- `scenario/earthquake.c` : `candidates[26244]` sur la pile (262 144 tuiles en 512) → allocation
  dimensionnée à la carte, libérée sur tous les chemins de sortie.
- `widget/city/building_ghost.c` : écriture dans la liste de portée du réservoir bornée.
- `window/message_dialog.c` : limite `26244` codée en dur → `map_grid_is_valid_offset()`.

### 3. Files d'attente à capacité fixe

- `map/road_network.c` et `map/water_supply.c` : files de 1000 entrées → dimensionnées à
  `map_grid_tiles + 1`, réutilisées entre les passes (l'ancien `memset` de la structure détruisait le
  pointeur, ce qui provoquait un plantage au chargement d'une sauvegarde).

### 4. Grille, sauvegardes et scénarios

- Grille allouée dynamiquement au pas courant (`map/grid.c`), registre `MAP_GRID_REGISTER`, tous les
  tableaux dérivés (terrain, building, sprite, routing, edge, aqueduc, élévation, désirabilité…)
  suivent la taille de la carte.
- Sauvegardes et scénarios : les pièces de grille de taille fixe ne servent plus qu'aux versions
  anciennes ; les versions courantes utilisent des pièces dynamiques, donc 512/1024 s'enregistrent et
  se relisent à leur taille réelle.
- Chargement d'une sauvegarde 162 dans une carte agrandie : la carte est centrée et les coordonnées
  des objets décalées (`map_shift_world_coordinates`), avec un rapport
  `Map expansion: N buildings misaligned` dans le journal.
- Éditeur de scénarios, minimap, écran de config et traductions adaptés à la taille choisie.

## État de test

- Compilation Release et RelWithDebInfo : 0 erreur.
- Scénario Caesarea en 512 et en 1024 : chargement sans plantage, `0 buildings misaligned`.
- Campagne originale (sauvegarde avec bâtiments et villageois) : chargement sans plantage.
- Non testé : partie longue sur 1024 (comportement de tous les systèmes quartier par quartier).

## Notes

- Aucune génération procédurale de terrain n'est incluse : la zone ajoutée autour de la carte
  d'origine reste vide, comme avant.
- `C3/augustus.ini` n'est pas fourni (fichier généré par le jeu) ; seul le réglage
  `map_expanded_size` compte.
