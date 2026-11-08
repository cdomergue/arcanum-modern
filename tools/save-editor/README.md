# Éditeur de sauvegardes Arcanum

Ouvrir `index.html` dans un navigateur récent. Aucun serveur, compte,
installation de dépendances ou accès réseau n’est nécessaire. Conserver les
fichiers JS et CSS dans le même dossier que le HTML.

Pour obtenir un **fichier HTML autonome**, depuis la racine du dépôt :

```sh
python3 tools/build-save-editor.py
```

Ouvrir ensuite `dist/arcanum-save-editor.html`. Les polices du mod sont incluses
si elles sont présentes lors de la génération.

Pour inclure les noms localisés d’une installation personnelle, ajouter
`--mes-directory /chemin/vers/mes` à cette commande. Les trois fichiers
`stat.mes`, `skill.mes`, `spell.mes` sont requis. Seuls les noms utiles sont
inclus dans le HTML généré sous `dist/`, qui reste exclu de Git. Ne pas ajouter
les textes extraits du jeu au dépôt.

## Modifier le personnage

Dans **Personnage**, saisissez un nom ou choisissez une race et un sexe. Le nom accepte jusqu’à 23 caractères du jeu, accents compris. Importez le fichier `.gsi` avec la paire `.tfai` / `.tfaf` pour mettre à jour le nom affiché dans le menu **Charger**. Sans ce fichier, le nouveau nom est enregistré dans la partie et le menu se met à jour lors de la prochaine sauvegarde en jeu.

Cochez **Mon jeu utilise le Race Mod de l’UAP** si ce composant est installé. Il fournit les races supplémentaires et change le modèle des demi-elfes. L’éditeur vérifie les combinaisons de race et de sexe disponibles dans le jeu. Il adapte le corps, la voix et les effets de race et de sexe. Le portrait et l’historique restent ceux du personnage.

## Utilisation

1. Copier les fichiers de l’emplacement à conserver dans un dossier de secours.
2. Sélectionner ensemble `Slot….tfai` et `Slot….tfaf` dans
   `modules/Arcanum/Save/`. Les noms doivent correspondre. Ajouter éventuellement
   le `.gsi` et le `.bmp` du même emplacement.
3. Modifier les valeurs et vérifier le récapitulatif.
4. Exporter le ZIP, fermer le jeu et décompresser les fichiers dans le dossier
   `Save` en conservant leurs noms. Si le `.gsi` et le `.bmp` n’ont pas été importés,
   conserver ceux de l’emplacement d’origine. Le ZIP seul n’est pas une sauvegarde
   que le jeu peut ouvrir.

Les fichiers d’origine ne sont jamais écrits par l’éditeur. Importation limitée
à 128 Mo au total. Les modifications ne persistent pas après fermeture de la page.

## Champs et compatibilité

- Attributs de base, expérience, alignement, points de destin et points disponibles.
- Solde bancaire s’il est enregistré ; or et munitions si leur pile est présente
  et référencée par le personnage.
- Les **16 compétences**, avec rang de 0 à 5 et entraînement de 0 à 3.
- Les **80 sorts**, répartis en 16 collèges : choisir un sort apprend aussi les
  rangs précédents ; le décocher retire les rangs suivants. Un bouton apprend
  les 80 sorts. Les compétences et collèges absents peuvent être ajoutés.
- Sélection du personnage lorsque plusieurs objets joueurs sont présents.
- Import facultatif de **`stat.mes`, `skill.mes`, `spell.mes` provenant du dossier
  `mes` de votre jeu** pour les noms des caractéristiques, compétences et sorts.
  Sinon, les identifiants du moteur sont affichés. Aucun texte extrait du jeu
  n’est suivi dans le dépôt. Les ressources gardent leurs identifiants.

Les attributs et ressources existants sont modifiés en place. Pour les compétences
et sorts, l’éditeur peut compléter les tableaux creux, ajouter les champs absents
et mettre à jour la taille des fichiers dans l’index. Les autres octets sont
conservés. Les archives TIG `.tfai`/`.tfaf` contenant des objets mobiles version 119
(`.mob`, `.mdy`) sont prises en charge. Un format inconnu ou incohérent est refusé.
Le fichier BMP reste identique. Dans le GSI, seul le nom du personnage peut changer. Le format TIG ne contient
pas d’empreinte permettant de prouver que deux fichiers de même nom et de même
taille viennent réellement de la même sauvegarde : importer une paire intacte.

Les quêtes, compagnons, objets complets et niveau ne sont pas édités. Les valeurs sont celles enregistrées avant les
modificateurs d’équipement, de race, d’historique et de magie. L’outil n’exécute
pas toutes les règles du moteur : il ne recalcule pas les effets dérivés, ne déclenche
pas de passage de niveau à partir de l’expérience et n’applique pas les bonus
liés au franchissement d’un seuil d’attribut. Les bornes d’édition sont des
limites techniques ; elles ne garantissent pas un personnage conforme aux règles
de chaque mod. Les piles de ressources non résolues ne sont pas inventées.

Les sorts et compétences sont ajoutés sans dépenser les points disponibles.
Les compteurs d’aptitude magique et technologique sont ajustés par différence,
comme pour les points ajoutés dans le moteur. Les disciplines technologiques,
la maîtrise d’un collège et les bits non concernés des compétences sont conservés.
Les prérequis du jeu restent applicables et les quêtes des maîtres ne sont pas
accomplies par un changement du degré d’entraînement.

## Vérification et maintenance

```sh
node tests/test_save_editor.cjs
ARCANUM_SAVE_TEST_DIR=game/modules/Arcanum/Save node tests/test_save_editor.cjs
```

La seconde commande lit les sauvegardes existantes et vérifie les modifications
sur des copies en mémoire. Elle n’écrit aucun fichier de sauvegarde. Les tests
couvrent aussi les archives tronquées, les tableaux creux, les bornes numériques,
les caractères accentués, l’ajout des sorts et compétences, les compteurs
d’aptitude, la conservation des bits d’entraînement, la résolution des piles et l’intégrité du ZIP (avec
le lecteur indépendant `zipfile` de Python).

`tools/check-save-editor.c` est un vérificateur optionnel à lier aux objets du
moteur. Il charge une sauvegarde préparée avec les 80 sorts et les 16 compétences
au rang 5 / entraînement 3, puis vérifie ces valeurs avec les accesseurs du jeu.
Il exige une installation de test isolée et le marqueur `.save-editor-sandbox`.

`schema.js` contient uniquement des métadonnées de format générées par
`python3 tools/generate-save-schema.py` depuis la révision épinglée du moteur.
Références : `source/first_party/tig/src/file.c`,
`source/src/game/{obj,obj_private,sa,stat,item}.c` et leurs en-têtes, dans
[Arcanum CE](https://github.com/alexbatalov/arcanum-ce).
