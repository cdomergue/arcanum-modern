ARCANUM MODERN — première version jouable de l'interface

Direction : rester fidèle au bois, au laiton, au livre et aux icônes d'Arcanum,
avec une présentation indépendante de la résolution du monde.

LANCEMENT
Depuis le dossier Arcanum : ./lancer-arcanum-modern.sh
Depuis ce dossier : ./play.sh
Mode fenêtre : ./play.sh -window -geometry=1280x720
Par défaut, le plein écran utilise la définition du moniteur ; le monde est rendu en 1280x720.
Notre interface et ses polices sont dessinées séparément à la définition de sortie.
Vulkan est sélectionné pour éviter le plantage EGL NVIDIA observé sur cette machine.

COMMANDES
J : ouvrir/fermer le nouveau journal.
Échap : quitter la recherche, puis fermer le journal ; menu en jeu.
Échap > Options > Graphismes : mode d'affichage et résolution.
F9 : basculer entre notre interface et la présentation classique hors combat.
Molette sur le monde : rapprocher / éloigner la vue autour du pointeur.
Les bandeaux natifs gardent leur taille. Avec le rendu par défaut, zoom de 50 à 120 %.
Les menus et panneaux conservent leur défilement ; le zoom revient à leur fermeture.
Flèches gauche/droite, Page précédente/suivante ou molette : tourner les pages.
A- / A+ : texte du journal de 16 à 26, mémorisé dans game/modern-ui/settings.ini.
La recherche fonctionne dans la rubrique choisie et ignore casse et accents.
Les quêtes disposent des filtres Toutes / En cours / Terminées.

COMBAT TACTIQUE
Le combat utilise le tour par tour d'Arcanum : initiative, vitesse, PA, fatigue,
dégâts, compétences et règles magiques restent calculés par le moteur original.
Chaque compagnon contrôlable reçoit ses ordres pendant son propre tour.
Cliquer sur un portrait déplace la caméra sans changer de tour ni rendre de PA.
E / Espace ou Fin du tour : terminer le tour du personnage actif.
M : sorts ; K : compétences ; Inventaire : équipement du personnage actif.
Les sorts inconnus sont masqués en combat ; les sorts sans ressources suffisantes
sont grisés. Le sélecteur indique les coûts et permet d'arrêter les sorts maintenus.
Le survol prévisualise le trajet, les PA, la portée et la chance de toucher depuis
la position actuelle. Les coûts de déplacement marqués ≈ sont estimatifs : le
moteur facture aussi les transitions entre animations et les cases partielles.
Le journal de combat conserve les messages du moteur.
La peur, la perte de contrôle et l'incapacité continuent d'obéir aux règles du jeu.
Les tours ne sont pas partagés entre alliés. Les nouveaux libellés tactiques sont
disponibles en français et en anglais ; les noms et descriptions viennent du jeu.

PÉRIMÈTRE DE CETTE VERSION
Nouveaux bandeaux de navigation et d'actions : commandes reliées au jeu,
vitalité/fatigue, raccourcis existants, or, expérience, heure, destin et munitions.
Nouveau journal : les sept rubriques utilisent les vraies données de la partie.
Le texte est réparti sur des doubles pages sans troncature. Les origines sont
réassemblées à partir du texte complet, indépendamment de l'ancienne pagination.
Les ornements, icônes et cadres de livre sont lus dans les archives du jeu.

Les fiches de personnage, l’inventaire, les échanges et achats, les cartes,
les sorts, les compétences, les schémas et le repos disposent de panneaux natifs.
Les jauges sont dessinées à la définition de sortie. Les dialogues utilisent les
nouvelles polices avec un fond sombre semi-transparent ; les choix restent
accessibles à la souris et avec les touches 1 à 5. La molette fait défiler les
longues répliques ou réponses sous le pointeur.
L’inventaire conserve les règles de placement, d’équipement, de prix et de quantité
du jeu. Glisser un objet entre les grilles permet de le transférer ou de l’acheter.
Les cartes conservent les images et annotations du jeu ; leurs commandes et leur
cadre utilisent la nouvelle interface. Les images originales restent à leur
résolution d’origine.
L’enregistrement des parties, les options de jeu et les portraits des compagnons
conservent leur présentation classique.
Les emplacements rapides activent les affectations existantes ; leur édition
par glisser-déposer reste celle des écrans classiques (F9).
L’accueil, Un joueur, Nouvelle partie, la pause et les confirmations partagent
la nouvelle présentation. Le fond game/modern-ui/menu-background.png conserve
ses proportions et reste entier.
Le choix des personnages prédéfinis, l’identité du nouveau personnage et la
répartition des points disposent d’écrans natifs. Les descriptions défilent avec
la molette ou les flèches. Cliquer sur le nom permet de le saisir ; Entrée termine
la saisie. Les noms respectent la limite de 23 caractères du format du jeu.
Le chargement présente les sauvegardes par pages, avec aperçu, lieu, niveau et date.
Les touches haut/bas sélectionnent une partie ; Entrée la charge.
Ces écrans lisent les libellés et descriptions dans les ressources originales de
l’installation, sans embarquer de copie des traductions du jeu. Le rendu utilise
actuellement l’encodage Windows-1252 des éditions occidentales.
Les options Graphismes permettent Fenêtre, Plein écran sans bordure (définition du
bureau) et Plein écran exclusif (modes annoncés par l'écran). Appliquer ouvre une
confirmation de 15 secondes. Conserver mémorise le choix ; Échap, Rétablir ou
l'expiration du délai rétablissent le mode précédent. Jeu et audio ouvre les options
classiques. La résolution concerne l'affichage ; le monde conserve son rendu
1280x720 et l'interface est dessinée à la définition de sortie.
Les réglages sont dans game/modern-ui/graphics.ini. Pour ignorer ce fichier :
ARCANUM_IGNORE_DISPLAY_SETTINGS=1 ./play.sh -window -geometry=1280x720
Le lanceur ignore aussi les réglages d'affichage si -window est donné explicitement.
Les modes disponibles dépendent de l'écran et du compositeur Linux.
Le rendu du monde et les ressources graphiques du monde ne sont pas remastérisés.
Il s'agit d'une première tranche fonctionnelle, pas encore d'une refonte complète.

BASE TECHNIQUE
Moteur : Arcanum Community Edition, commit a7ff41b300ef712f0e7d088183a3d08957110cdd.
Source : https://github.com/alexbatalov/arcanum-ce
Notre code principal : source/src/ui/modern_ui.c, modern_ui.h et modern_panels.inc.
Adaptateurs de données/actions : logbook_ui.c et intgame.c.
Points d'intégration : main.c, TIG video.c et message.c (rendu et événements).
Les règles de jeu et les formats de sauvegarde ne sont pas modifiés.
Les fichiers de game/ sont des copies indépendantes de CommunityEdition/.

COMPILATION LINUX
./build.sh
prepare-source.sh prépare le moteur à la révision indiquée ci-dessus et applique
patches/engine.patch. ARCANUM_ENGINE_SOURCE permet de réutiliser un clone local ;
sinon les sources amont sont téléchargées avec Git. Les données du jeu restent
à installer séparément dans game/.
Le script réutilise le SDK temporaire et les dépendances téléchargées s'ils
existent. Sinon il utilise CMake du système et les dépendances officielles de
l'amont via FetchContent. Prérequis Fedora/Nobara : gcc, gcc-c++, make, cmake,
ninja-build, libX11-devel, libXext-devel, libXrandr-devel, libXi-devel,
libXinerama-devel, libXcursor-devel, alsa-lib-devel, pulseaudio-libs-devel,
libXScrnSaver-devel et libXtst-devel. SDL est compilé avec X11/XWayland et Vulkan.
ARCANUM_BUILD_DIR, ARCANUM_BUILD_SDK et ARCANUM_DEPENDENCY_CACHE permettent de
choisir les dossiers. L'exécutable est remplacé atomiquement après compilation.

GIT
Le dépôt contient uniquement nos modules modern_ui / modern_graphics, les scripts,
tests, ressources d’interface et patches/engine.patch. Les autres fichiers du
moteur, données du jeu, exécutables, sauvegardes et préférences sont exclus.
Après modification d’un fichier amont : python3 tools/update-engine-patch.py
Ce script compare les fichiers à la révision de référence d’ARCANUM_ENGINE_SOURCE.

VÉRIFICATION
ARCANUM_UI_SELFTEST=1 ./play.sh
Les contrôles d'encodage CP1252, de recherche accentuée et de retour à la ligne
sans perte de caractères produisent MODERN UI TEST PASS dans modern-launch.log.
Pour générer les captures de rendu, créer un dossier puis lancer avec
ARCANUM_UI_CAPTURE_DIR=/chemin/absolu ./play.sh et ouvrir le journal.
Résolutions couvertes : 1920x1080, 2560x1440, 3840x2160 et 1280x1024.
Les captures hors écran vérifient le rendu ; elles ne remplacent pas des parties
prolongées sur chaque configuration matérielle.

python3 tests/test_keyboard_capture.py
python3 tests/test_creation_controls.py
python3 tests/test_hud_transitions.py
Tests du routage clavier, des commandes de création et du défilement du texte
(Python 3 et compilateur C nécessaires).

LICENCES ET FICHIERS
Licence du moteur : source/LICENSE.md.
Polices Noto : game/modern-ui/FONT-LICENSE.txt (SIL Open Font License).
Rasteriseur : source/third_party/stb/stb_truetype.h, licence incluse dans le fichier.
.gitignore exclut données du jeu, sauvegardes, binaires, réglages personnels et captures.
