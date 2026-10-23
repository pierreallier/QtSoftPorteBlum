# Interface de l'application

La fenêtre `MainWindow` propose les onglets Mesures et Pilotage, un bouton de connexion et un bouton Ouvrir pour les fichiers CSV. Le pied de fenêtre affiche la liaison série, le dernier état d'erreur ou d'avertissement et le bouton de reset `DO RESET`.

## Connexion et état de la carte

Au démarrage, les ports sont recherchés sans ouverture automatique. Connexion autorise l'ouverture et, en mode automatique, les reconnexions ultérieures. Déconnexion annule cette intention. Les commandes restent désactivées jusqu'à la confirmation d'initialisation par le message `I`, code 71.

Trois clics rapides sur le logo affichent ou masquent Debug. Lorsque Debug est visible, cliquer sur le statut série ouvre les réglages : recherche automatique, port manuel et débit. Ils sont enregistrés dans le `config.ini` placé à côté de l'exécutable et s'appliquent à la prochaine connexion. Le débit automatique vient toujours des valeurs intégrées.

Le voyant de sécurité est gris lorsque l'état est inconnu, rouge en cas d'erreur et orange en cas d'avertissement. Les états Production Ouverture, Fermeture, Pilotage et Calibration le remettent au vert. Chaque erreur carte ouvre une boîte critique non bloquante pour la réception série. Il n'existe pas encore d'inventaire complet des erreurs actives et acquittées.

## Mesures

À gauche, Acquérir envoie `SET MESURES ON` et réinitialise le graphique local ainsi que son curseur de lecture. Arrêter envoie `SET MESURES OFF`. Le bouton reflète l'état d'acquisition confirmé par la carte. Calibration demande confirmation, puis envoie `DO CALIBRATION`.

`StateGraphWidget` affiche Production (250) par défaut et Calibration (251) lorsque Production est à l'état 6. Le dernier état reçu est surligné. Les états cachés gardent leur nom dans les informations d'état, sans nœud ni flèche. La propriété `cmd` d'un état contient une clé de `protocol.json` (par exemple `Ouvrir`) ; le clic émet le `Protocol::CMD` correspondant lorsque la carte est prête. Les noms viennent de `etats.csv` ; les références, coordonnées, dimensions et transitions viennent de `machines.json` ; leur format est décrit dans [Config.md](Config.md).

À droite, `GraphiqueWidget` regroupe la légende, le tracé, les valeurs et le bouton Enregistrer. Les mesures moteur et la centrale inertielle ont des sélections séparées. La case de la centrale envoie `SET ACCEL ON/OFF` avec attente de réponse, puis reprend l'état confirmé. Les axes inertiels sont sélectionnés par X, Y et Z.

## Utilisation du graphique

Les valeurs et couleurs viennent de `courbe.json`. Chaque grandeur est convertie par son facteur, puis normalisée uniquement pour le tracé. Le panneau affiche les valeurs converties au point survolé ; en quittant le tracé, il reprend les dernières valeurs. Les périodes moyennes sont calculées séparément pour les deux sources.

- Molette : zoom ; Ctrl limite le zoom au temps, Maj à l'amplitude.
- Glisser : déplacement de la vue.
- Double-clic : cadrage des courbes visibles et retour à l'amplitude normalisée de -1,1 à 1,1.
- En acquisition, chaque rafraîchissement recadre la fenêtre sur les dix dernières secondes avec le facteur temporel actuel.

Les tracés sont interrompus lors d'un retour en arrière du temps. Le graphique conserve au maximum 2000 trames moteur et 400 trames inertielles. Le temps affiché utilise deux décimales ; l'export conserve davantage de précision. Voir [Mesures.md](Mesures.md) pour le CSV et sa relecture.

## Pilotage

Le panneau gauche contient les correcteurs vitesse et position, la consigne avec aperçu et les boutons Envoyer/Stopper et Calibration. Le graphique possède son propre historique. Les modes, bornes, commandes et étapes sont détaillés dans [Pilotage.md](Pilotage.md).

Le fonctionnement actuel envoie les réglages avec acquittement, puis acquiert pendant dix secondes. Il n'envoie pas la commande de lancement `DO PILOTER` à la fin de cette séquence : ce démarrage reste simulé localement.

## Changements d'onglet

Passer de Mesures à Pilotage demande `SET MESURES OFF` si l'acquisition est confirmée active, puis change la destination des nouvelles trames après lecture du lot restant.

Pour quitter Pilotage vers Mesures, la séquence locale est interrompue et l'interface attend une réponse positive à `SET MESURES OFF`. Si un pilotage était en cours, elle envoie ensuite `DO STOP` et attend sa mise en tampon par le port avant de basculer. Refus, expiration ou perte de connexion pendant cette procédure annulent le changement. Si la carte est déjà déconnectée ou non initialisée au moment de la demande, le changement est immédiat.

Un passage par Debug conserve l'activité et la dernière destination Mesures/Pilotage. Revenir ensuite vers l'autre page applique les mêmes règles. Une commande d'arrêt peut être différée si une autre commande attend sa réponse ; un reset annule les demandes différées.

## Debug

Debug propose une saisie de commande, l'envoi, l'effacement visuel et le défilement automatique. Deux vues séparent les événements/commandes des trames M/A. Ouvrir Debug rejoue les deux historiques partagés de 200 entrées ; l'effacement visuel ne les vide pas.

Les modèles de texte et formats par type viennent de `debug.json` : erreurs rouges, avertissements orange, informations bleues, états verts, commandes soulignées avec `▶`, réponses avec `◀`. Le contenu est inséré en texte brut. Les formats sont préparés au chargement et un retrait suspendu aligne les lignes après la colonne de préfixe. Voir [Config.md](Config.md) pour les champs et les substitutions.

Le mode Debug affiche aussi les informations de mémoire des graphiques et les états des machines sous le diagramme.

## Thème et configuration

`resources/styles.qss` règle les contrôles communs, panneaux, commandes colorées, indicateurs, graphiques, PID, consignes et machines à états. La palette utilise notamment l'orange `#F75A1B`, le rouge `#DC2626`, le vert `#16A34A` et le bleu `#2563EB`.

Les noms d'objets, propriétés dynamiques et propriétés `qproperty-*` configurent les dessins QPainter. Cela inclut les couleurs de grille, axes, curseur, aperçu de consigne et états normaux/actifs/survolés. Les cases colorées utilisent leurs couleurs de courbe ; les axes inertiels conservent leur lettre lorsqu'ils sont décochés. Les champs natifs utilisent aussi la palette Qt définie par `Application` et les aides de style.

Les calculs de mise en page restent en C++. Les ressources sont intégrées : reconstruire après modification du QSS ou des JSON. La géométrie de fenêtre est mémorisée par `QSettings` ; les préférences de fichiers et de liaison passent par `AppSettings`. Voir [Architecture.md](Architecture.md) pour le partage des données.
