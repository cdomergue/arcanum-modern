# Cartes locales détaillées

Le cache est rendu par le moteur à partir des décors installés, à un pixel par pixel du monde. Il contient les 115 cartes locales disponibles d’Arcanum et les 11 de Vormantown. Les cartes sans image locale d’origine ne sont pas inventées.

Chaque morceau couvre 1024 × 1024 pixels du monde. Quatre PNG sans perte conservent cette même couverture aux résolutions 1, 1/2, 1/4 et 1/8. Les fichiers sont nommés `niveau_colonne_ligne.png`. Les morceaux du bord sont plus petits. `manifest.json` conserve les identifiants, dimensions, origines et associations aux cartes sources.

Les images contiennent les décors statiques avec leurs toits opaques, sans personnages, objets transportables, annotations ni noms de lieux ajoutés. Les bâtiments couverts restent masqués ; les cartes intérieures conservent leur géométrie propre. Les portes font partie du décor initial ; leurs changements d’état en cours de partie ne sont pas régénérés dans cet atlas. Les inscriptions appartenant aux sprites du décor restent celles du jeu.

L’interface affiche les morceaux directement à sa résolution native. Le brouillard d’exploration reste calculé par `townmap_tile_blit_info`. Le jeu dessine les repères du groupe, les trajets, les notes et les textes localisés séparément. Une carte manquante ou incomplète utilise automatiquement le rendu d’origine. Le cache de textures est limité à 32 morceaux.

## Génération sur macOS

Depuis la racine du projet, après le build du moteur :

```sh
python3 tools/build-map-exporter.py
python3 tools/generate-map-cache.py
python3 tools/verify-map-cache.py
```

L’exporteur fonctionne hors écran. `build/map-export` contient une installation isolée : liens vers les archives DAT, copies des données libres, aucun lien vers les sauvegardes. Le moteur y écrit ses fichiers temporaires. Les archives et les fichiers du jeu servent de sources en lecture seule. Les patchs du module sont appliqués dans leur ordre normal.

Une interruption est récupérable : la génération suivante reprend les morceaux dont les quatre niveaux ne sont pas terminés. Un fichier `complete` n’est créé qu’à la fin d’une carte. Si les sources ou la politique de rendu des toits changent, utiliser un nouveau dossier avec `--output` et un nouveau dossier temporaire avec `--work`. Les manifestes portant `roofs: false` sont refusés pour éviter de réutiliser des images sans toits.

Le passage de Gorgoth possède un ancien identifiant de carte locale qui n’est plus référencé par les secteurs installés. Son image est générée depuis ses coordonnées dans le monde principal, et cette association est indiquée dans le manifeste.

## Installation locale

Le dossier `game/modern-ui/local-maps` doit contenir les sous-dossiers des modules (`Arcanum`, etc.). Pour cette installation, il pointe vers `dist/local-map-cache`, ce qui évite de dupliquer plusieurs gigaoctets. Conserver ce dossier lors d’un nettoyage de `dist`.

Les PNG dérivent des données du jeu et restent locaux. Ils ne sont pas inclus dans le ZIP distribuable de l’application. Supprimer uniquement le lien `game/modern-ui/local-maps` rétablit les cartes d’origine au prochain lancement.

## Validation

`verify-map-cache.py` contrôle tous les morceaux attendus, leurs dimensions, chaque CRC PNG et l’absence de métadonnées textuelles. `tests/test_map_archive.py` vérifie la lecture des DAT compressés et des identifiants de carte dans les secteurs.

`tools/check-map-cache.c` charge une copie de sauvegarde dans une installation de test, capture la carte native, vérifie le retour au rendu d’origine sans cache, puis vérifie qu’une carte inexplorée est noire et qu’elle devient visible une fois explorée. Il ne doit jamais être lancé dans le dossier du jeu ; le marqueur `.map-export-sandbox` est obligatoire.

```sh
python3 tools/build-map-exporter.py --source tools/check-map-cache.c --name check-map-cache --exclude-object src/ui/modern_ui.c.o
build/macos-arm64/check-map-cache "$PWD/build/map-preview" Slot0001 "$PWD/build/map-preview.png"
```
