# Augustus — cartes agrandies 512 à 1024

Ce dossier contient la version complète des adaptations réalisées pour rendre Augustus compatible
avec des cartes agrandies. Les cartes ne sont pas générées automatiquement : elles peuvent être
créées manuellement avec l'éditeur de scénarios.

## Contenu

```text
src/             57 fichiers source modifiés, avec leur arborescence d'origine
CMakeLists.txt   fichier CMake complet du projet, fourni pour compiler le dossier
PATCH.diff       patch des 57 fichiers source modifiés
FICHIERS.txt     liste des fichiers source concernés
README.md        ce tutoriel
```

`CMakeLists.txt` est fourni en entier pour que le dossier soit identifiable et compilable avec un
clone Augustus. Il n'a pas de modification fonctionnelle dans ce travail ; `PATCH.diff` contient
uniquement les changements sous `src/`.

---

## 1. Prérequis Linux Mint

Ouvrir un terminal et installer les outils de compilation :

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  git \
  pkg-config \
  libsdl2-dev \
  libsdl2-mixer-dev \
  libgl1-mesa-dev \
  libx11-dev \
  libxext-dev \
  libxrandr-dev \
  libxcursor-dev \
  libxfixes-dev \
  libxi-dev \
  libxss-dev \
  libxtst-dev \
  libxkbcommon-dev \
  libpulse-dev
```

Les extensions `spng`, `miniz`, `sxml`, `zip`, `pl_mpeg` et `easyav1_dummy` sont déjà présentes
dans le dépôt. La compilation standard utilise `easyav1_dummy` et ne nécessite donc pas d'installer
une bibliothèque AV1 supplémentaire.

Vérifier les outils :

```bash
gcc --version
cmake --version
pkg-config --modversion sdl2
pkg-config --modversion SDL2_mixer
```

Si `pkg-config` ne trouve pas SDL2, réinstaller `libsdl2-dev` et `libsdl2-mixer-dev`, puis ouvrir
un nouveau terminal.

---

## 2. Préparer un clone Augustus

Le dossier de livraison est prévu pour un clone correspondant au même état de base Augustus que
celui utilisé pour créer le patch.

### Méthode recommandée : copier les fichiers

Depuis la racine du clone Augustus :

```bash
cp -a /chemin/vers/augustus-grandes-cartes/src/. ./src/
cp /chemin/vers/augustus-grandes-cartes/CMakeLists.txt ./CMakeLists.txt
```

Remplacer `/chemin/vers/augustus-grandes-cartes` par le chemin réel du dossier.

### Méthode patch

Depuis la racine du clone Augustus :

```bash
git apply --check /chemin/vers/augustus-grandes-cartes/PATCH.diff
git apply /chemin/vers/augustus-grandes-cartes/PATCH.diff
```

Si `git apply --check` signale des conflits, le clone ne correspond probablement pas au commit de
base attendu. Ne pas mélanger les deux méthodes : soit copier `src/`, soit appliquer le patch.

---

## 3. Compiler sous Linux Mint

Depuis la racine du clone Augustus modifié :

```bash
cmake -S . -B build-linux \
  -DCMAKE_BUILD_TYPE=Release \
  -DSDL_VERSION=2 \
  -DAV1_VIDEO_SUPPORT=OFF
cmake --build build-linux --parallel "$(nproc)"
```

Le programme compilé se trouve normalement ici :

```text
build-linux/augustus
```

Pour une compilation de diagnostic :

```bash
cmake -S . -B build-linux-debug \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSDL_VERSION=2 \
  -DAV1_VIDEO_SUPPORT=OFF
cmake --build build-linux-debug --parallel "$(nproc)"
```

Pour repartir d'une configuration CMake propre :

```bash
rm -rf build-linux build-linux-debug
```

Ne supprimer que les dossiers de build, jamais `src/`, `C3/` ou vos sauvegardes.

---

## 4. Installer les fichiers de Caesar 3

Augustus nécessite les fichiers originaux de Caesar 3 version 1.0.1.0. Ils ne sont pas inclus dans
ce dossier.

Créer un dossier de jeu, par exemple :

```bash
mkdir -p "$HOME/Jeux/Augustus-Caesar3"
```

Copier dans ce dossier les fichiers originaux de Caesar 3, notamment `c3.eng`, les répertoires de
cartes, sons, graphismes et vidéos selon votre installation.

Pour une version GOG avec son installateur hors ligne, `innoextract` peut extraire les fichiers :

```bash
sudo apt install -y innoextract
innoextract -m setup_caesar3_2.0.0.9.exe
```

Utiliser ensuite le répertoire extrait comme dossier de données Caesar 3.

---

## 5. Premier lancement

Lancer Augustus en donnant le dossier contenant les données Caesar 3 :

```bash
./build-linux/augustus "$HOME/Jeux/Augustus-Caesar3"
```

Ou, depuis un dossier contenant l'exécutable et `C3/` :

```bash
./build-linux/augustus
```

Le programme doit avoir le droit d'écrire dans le dossier de données afin de créer les sauvegardes
et `augustus.ini` :

```bash
chmod u+rwX -R "$HOME/Jeux/Augustus-Caesar3"
```

Ne pas lancer le jeu avec `sudo`.

---

## 6. Choisir la taille de carte

Dans le fichier généré :

```text
C3/augustus.ini
```

modifier la valeur `map_expanded_size` :

| valeur | taille |
|---:|---:|
| 0 | 162 × 162 |
| 1 | 192 × 192 |
| 2 | 256 × 256 |
| 3 | 320 × 320 |
| 4 | 384 × 384 |
| 5 | 512 × 512 |
| 6 | 640 × 640 |
| 7 | 768 × 768 |
| 8 | 1024 × 1024 |

Exemple pour 1024 :

```ini
map_expanded_size=8
```

La valeur par défaut livrée est `5`, soit 512 × 512. Le jeu peut réécrire le fichier en quittant ;
contrôler sa valeur après un test.

La carte historique est centrée dans la grille agrandie. La zone supplémentaire n'est pas remplie
par génération automatique : elle peut être créée et modifiée manuellement avec l'éditeur de
scénarios.

---

## 7. Corrections incluses

### Grilles et voisinages

- Toutes les grilles de données utilisent `map_grid_tiles` et `map_grid_stride` réels.
- Les anciennes tables compilées avec le pas 162 sont converties pour le pas courant.
- `map_grid_add_legacy_offset()` vérifie les limites X/Y et empêche les voisinages de passer sur
  une mauvaise ligne ou de sortir de la grille.
- Les corrections couvrent maisons, routes, réseaux routiers, aqueducs, réservoirs, canaux, berges,
  highways, rampes, forts, bâtiments et outils de l'éditeur.

### Mémoire et capacités

- Les tableaux de l'éditeur et des séismes suivent la taille réelle de la carte.
- Les files des réseaux routiers et d'eau sont dimensionnées selon le nombre de tuiles.
- Les listes d'offsets de construction sont protégées contre les écritures hors limites.
- Les limites codées en dur à 162² ont été supprimées des systèmes concernés.

### Sauvegardes et chargement

- Les sauvegardes récentes utilisent des grilles dynamiques.
- Les anciens formats restent lus avec leurs dimensions historiques.
- Les coordonnées des bâtiments et figures sont recalculées lors de l'agrandissement.
- Le journal indique le résultat avec `Map expansion: N buildings misaligned`.

---

## 8. Contrôles effectués sur cette version

- 57 fichiers source présents dans `src/`.
- `PATCH.diff` contient les 57 fichiers source.
- `CMakeLists.txt` est présent en entier.
- Compilation Release : réussie.
- Compilation RelWithDebInfo : réussie.
- Caesarea en 512 : chargement sans `SIGSEGV`, `0 buildings misaligned`.
- Caesarea en 1024 : chargement sans `SIGSEGV`, `0 buildings misaligned`.
- `C3/augustus.ini` restauré sur `map_expanded_size=5` après les tests Windows.

Une partie longue avec une ville fortement construite en 1024 reste recommandée pour valider votre
carte personnalisée et vos réglages graphiques.

---

## 9. Dépannage Linux Mint

### CMake ne trouve pas SDL2

```bash
sudo apt install --reinstall libsdl2-dev libsdl2-mixer-dev pkg-config
rm -rf build-linux
```

Puis relancer la commande de configuration CMake.

### Erreur de bibliothèque au lancement

Vérifier les dépendances dynamiques :

```bash
ldd ./build-linux/augustus | grep 'not found'
```

Une ligne `not found` indique un paquet manquant. Installer le paquet de développement SDL2
correspondant et relancer CMake.

### La carte ne s'agrandit pas

1. Vérifier que `map_expanded_size` est dans l'intervalle `0..8`.
2. Vérifier que le fichier modifié est bien celui du dossier de données utilisé au lancement.
3. Fermer complètement le jeu avant de relire le fichier.
4. Vérifier le journal Augustus et rechercher `Map expansion`.

### Le jeu refuse les données Caesar 3

Vérifier que les fichiers originaux sont présents et que la version Caesar 3 est bien 1.0.1.0.
