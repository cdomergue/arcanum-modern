# Commandes de l’interface moderne

## Raccourcis

Dans **Options → Raccourcis**, cliquez sur une affectation puis appuyez sur une
lettre ou Espace. Une touche déjà utilisée est signalée sans remplacer son action.
Échap annule la saisie ; Retour arrière ou Suppr retire l’affectation.
Les boutons AZERTY et QWERTY rétablissent les commandes et placent le déplacement
de caméra sur ZQSD ou WASD. Les flèches restent utilisables.

Le survol des boutons affiche leur raccourci actuel. Les chiffres de la barre
rapide, les touches de fonction et les touches de navigation restent réservés.
La saisie d’une recherche ou d’une note suspend les raccourcis de jeu.
Dans l’inventaire, survolez un objet puis appuyez sur une touche de la rangée
**1–0** pour l’affecter à la barre : cela quitte la saisie sans effacer le filtre.
Un clic hors du champ quitte également la saisie. Les chiffres restent saisissables
quand aucun objet n’est survolé.

## Inventaire

- Recherchez un nom d’objet, sans tenir compte de la casse ni des accents latins.
  Les catégories se combinent avec la recherche. Les objets gardent leur place
  dans la grille ; effacez la recherche et choisissez **Tous** pour tout afficher.
- **Comparer l’équipement** affiche l’objet survolé et l’équipement correspondant
  du personnage. Chaque description défile indépendamment avec la molette.
- **Maj + clic** transfère une pile entière dans un échange ou un butin. Les prix,
  restrictions et vérifications du jeu s’appliquent. Le clic droit conserve son
  fonctionnement original, notamment la sélection d’une quantité.
- Vol à la tire et Discrétion restent disponibles dans les compétences et par
  leurs raccourcis ; les boutons permanents correspondants ont été retirés.

## Journal et carte

Les quêtes peuvent être filtrées par **En cours**, **Terminée**, **Interrompue**
ou **Mentionnée**, avec les libellés de la langue installée. Un objectif atteint
mais pas encore validé reste en cours ; les quêtes effectuées par un rival sont
classées avec les quêtes interrompues. Recherche et favoris se combinent avec
ces filtres.

L’étoile d’une entrée ajoute ou retire un favori. Le marqueur d’une quête
active permet de la suivre hors combat ; cliquez sur son rappel pour retrouver
l’entrée complète. Le suivi n’indique aucune destination inconnue.

Sur la carte, choisissez **Ajouter une note**, cliquez à l’endroit voulu, saisissez
le texte puis validez. Cliquez sur son marqueur pour la modifier ou la supprimer.
**Notes visibles** masque ou affiche les marqueurs. Limites : 64 notes et 128
favoris par personnage.

Les annotations sont conservées par personnage dans `modern-ui/notebook-*.txt`,
indépendamment des sauvegardes : recharger une sauvegarde antérieure ne les efface
pas. Les touches sont conservées dans `modern-ui/keys.ini`.

## Combat et lecture

L’aperçu indique le coût et les PA restants. Un `~` signale une estimation de
trajet ; l’animation et les règles originales déterminent le coût effectivement
consommé. Les règles de fatigue restent celles d’Arcanum.

Livres, lettres, inscriptions, journaux imprimés et télégrammes utilisent les
textes de l’installation. Les boutons de page, la molette et Page précédente /
Page suivante permettent de parcourir les documents longs ; Échap les ferme.

Les **sorts maintenus** apparaissent sous la barre supérieure, indépendamment du
zoom. Survolez une icône pour lire son nom ; cliquez pour arrêter ce sort lorsque
le personnage est disponible. En combat, cette zone suit le personnage contrôlé ;
pendant le tour adverse, elle montre les sorts du héros sans permettre de les arrêter.
