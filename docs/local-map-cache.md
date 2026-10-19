# Cartes locales détaillées

Le cache est rendu par le moteur à partir des décors installés, à un pixel par pixel du monde. Il contient les 115 cartes locales disponibles d’Arcanum et les 11 de Vormantown. Les cartes sans image locale d’origine ne sont pas inventées.

Chaque morceau couvre 1024 × 1024 pixels du monde. Quatre PNG sans perte conservent cette même couverture aux résolutions 1, 1/2, 1/4 et 1/8. Les fichiers sont nommés `niveau_colonne_ligne.png`. Les morceaux du bord sont plus petits. `manifest.json` conserve les identifiants et dimensions ; les exports Python historiques contiennent aussi les origines et associations aux cartes sources.

Les images contiennent les décors statiques avec leurs toits opaques, sans personnages, objets transportables, annotations ni noms de lieux ajoutés. Les bâtiments couverts restent masqués ; les cartes intérieures conservent leur géométrie propre. Les portes font partie du décor initial ; leurs changements d’état en cours de partie ne sont pas régénérés dans cet atlas. Les inscriptions appartenant aux sprites du décor restent celles du jeu.

L’interface affiche les morceaux directement à sa résolution native. Le brouillard d’exploration reste calculé par `townmap_tile_blit_info`. Le jeu dessine les repères du groupe, les trajets, les notes et les textes localisés séparément. Une carte manquante ou incomplète utilise automatiquement le rendu d’origine. Le cache de textures est limité à 32 morceaux.

## Génération automatique : Windows, macOS et Linux

Au premier lancement, le moteur génère les cartes manquantes et affiche une barre de progression. Aucun Python, compilateur ni téléchargement supplémentaire n’est nécessaire. « Jouer maintenant » (ou Échap) interrompt la préparation et ouvre le jeu ; le prochain lancement reprend les morceaux terminés. Fermer la fenêtre arrête aussi le processus de génération.

Les quatre niveaux de chaque morceau sont vérifiés avant réutilisation, avec leurs dimensions et CRC PNG. Une carte n’est activée qu’une fois son fichier `complete` écrit. Le cache complet est réutilisé ; les fichiers manquants ou modifiés déclenchent une nouvelle vérification. Les modifications des sources déclenchent une régénération. Les dates des fichiers libres réécrits à l’identique par le moteur ne suffisent pas à invalider le cache.

Le processus séparé travaille dans `modern-ui/.map-work/` : archives liées physiquement lorsque possible (sinon copiées), fichiers libres copiés, sauvegardes exclues. Un verrou empêche deux générateurs d’écrire simultanément. Ce dossier de travail peut être supprimé lorsque le jeu est fermé. Le cache actuel d’Arcanum et Vormantown occupe environ **8,18 Go** ; prévoir aussi de la place temporaire. En cas de manque de place ou d’erreur, le joueur peut continuer avec les cartes originales.

Un export sans interface est également possible :

```sh
./arcanum-modern --modern-generate-maps /chemin/absolu/vers/Arcanum
```

Sous Windows, utiliser `arcanum-modern.exe`. Sous macOS, utiliser l’exécutable dans `Arcanum Modern.app/Contents/MacOS/`. L’argument optionnel suivant limite le nombre de morceaux nouvellement rendus pour les tests (code de sortie 75). Le fichier `modern-ui/.map-work/progress` contient `morceaux_validés total étape` : 0 préparation, 1 rendu/vérification, 2 terminé, 3 interrompu sur erreur ou limite de test.

Les caches de l’ancien exporteur sont adoptés après vérification s’ils déclarent des toits opaques. Les images ne contiennent aucun texte ajouté, quelle que soit la plateforme.

## Exporteur historique pour développeurs sur macOS

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

Les PNG dérivent des données du jeu et restent locaux. Ils ne sont pas inclus dans le ZIP distribuable de l’application. Supprimer ce cache provoque sa reconstruction au prochain lancement ; choisir « Jouer maintenant » pour utiliser les cartes d’origine.

## Validation

`verify-map-cache.py` contrôle tous les morceaux attendus, leurs dimensions, chaque CRC PNG et l’absence de métadonnées textuelles. `tests/test_map_archive.py` vérifie la lecture des DAT compressés et des identifiants de carte dans les secteurs.

`tools/check-map-cache.c` charge une copie de sauvegarde dans une installation de test, capture la carte native, vérifie le retour au rendu d’origine sans cache, puis vérifie qu’une carte inexplorée est noire et qu’elle devient visible une fois explorée. Il ne doit jamais être lancé dans le dossier du jeu ; le marqueur `.map-export-sandbox` est obligatoire.

```sh
python3 tools/build-map-exporter.py --source tools/check-map-cache.c --name check-map-cache --exclude-object src/ui/modern_ui.c.o
build/macos-arm64/check-map-cache "$PWD/build/map-preview" Slot0001 "$PWD/build/map-preview.png"
```

Le test natif de la génération vérifie les secteurs tronqués, les PNG et la reprise :

```sh
python3 tools/build-map-exporter.py --source tools/check-map-generation.c --name check-map-generation --exclude-object src/ui/modern_map_generator.c.o
build/macos-arm64/check-map-generation /tmp/arcanum-map-generation-check
```
