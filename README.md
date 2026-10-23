# SoftPorteBlum — étape 1, socle Qt6/C++

Projet Qt Widgets C++17 pour Windows. Aucun accès au matériel n'a été validé pendant la création. Voir VALIDATION.md pour les vérifications réellement effectuées.

## Compiler dans VS Code / Qt Creator

Installer Qt 6.4 ou supérieur avec **Widgets**, **Serial Port**, et **Qt Test** pour les tests, ainsi que le compilateur MinGW correspondant à cette installation Qt, CMake et Ninja. Ouvrir ce dossier comme projet CMake dans Qt Creator ou VS Code (extension CMake Tools). Choisir le kit Qt MinGW.

Exemple PowerShell, adapter les chemins à l'installation réelle :

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.12.0\mingw_64\bin;C:\Qt\Tools\Ninja;$env:PATH"
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build --output-on-failure
.\build\SoftPorteBlum.exe
```

Pour compiler sans Qt Test : ajouter `-DBUILD_TESTING=OFF` lors de la configuration. Ne pas mélanger un Qt MSVC avec MinGW. La cible conserve sa console pour qDebug. Pour distribuer : exécuter `windeployqt` issu de la même installation Qt sur l'exécutable compilé.

## Configuration utilisateur

Les valeurs par défaut sont embarquées depuis `resources/config.ini` via `resources.qrc`.
Un fichier `config.ini` placé à côté de l'exécutable surcharge uniquement les clés présentes :

```ini
[files]
openFolder=C:/Users/CCS/Desktop

[serial]
automatic=true
port=COM1
baud=115200
```

Sans surcharge, le dossier d'ouverture est Documents, la recherche série est automatique,
le port manuel est COM1 et le débit est 115200 bauds. Un dossier relatif est résolu depuis
le répertoire de l'exécutable. `automatic=false` active le port manuel.
Un débit invalide ou un booléen invalide reprend la valeur embarquée.
En mode automatique, le débit est verrouillé et provient toujours du `config.ini`
embarqué dans les ressources ; la surcharge du débit ne s'applique qu'en mode manuel.

Le choix d'un fichier avec **Ouvrir** mémorise son dossier. Le bouton **Save / Enregistrer**
du dialogue série mémorise le mode, le port et le débit (accès par trois clics sur le logo,
puis clic sur le statut série). Ces opérations créent `config.ini` si nécessaire et conservent
les autres clés. Une erreur d'écriture est affichée ; le dossier de l'exécutable doit être
accessible en écriture. Les changements série s'appliquent à la prochaine connexion.
Les anciens réglages série du registre ne sont plus utilisés ; la géométrie de la fenêtre
continue à être mémorisée par le mécanisme Qt existant.

## Inclus

- MainWindow, une classe par onglet et un thème clair QSS dans resources.qrc.
- Debug masqué au démarrage ; trois clics sur B en moins d'une seconde l'affichent ou le cachent. Ouvrir l'onglet affiche l'historique conservé.
- Découverte des ports toutes les 1,5 s tant que le worker n'existe pas ; priorité par nom/fabricant Arduino ou VID 2341/2A03. Cette priorité est une heuristique, pas une identification certaine. Les autres ports restent sélectionnables.
- Aucun port ouvert au démarrage. Le bouton Connexion autorise la connexion et, en mode automatique, la reconnexion après retrait. Déconnexion annule cette intention. Le mode manuel ne relance pas la connexion tout seul.
- Réglages série par clic sur le statut lorsque Debug est affiché. Ils sont conservés dans `config.ini`. En mode automatique, le débit est celui des ressources ; passer en manuel permet de le modifier.
- À l'ouverture du port, l'application écoute. Si aucun événement de code 70 (début d'initialisation) n'arrive sous 2 secondes, elle envoie une seule commande DO RESET, également conservée dans l'historique. Si le code 71 n'arrive pas dans les 5 secondes suivant ce reset, le port est fermé, la reconnexion automatique est annulée et une erreur est affichée dans le GUI. Les envois et les boutons de commande sont autorisés après réception d'un événement de code 71 (fin d'initialisation). Cette autorisation est réinitialisée à chaque connexion. Avant ce code, même la fermeture n'envoie pas SET MESURES OFF.
- QSerialPort créé et utilisé uniquement dans le worker, déplacé dans QThread. Lectures via readyRead, commandes via connexions queued. Aucun accès direct du GUI au port.
- Deux historiques circulaires mutex de 200 objets avec numéros de séquence, lectures non destructives et comptage des pertes. Objets Event polymorphes et immuables après insertion. Les trames Mesure et Accéléromètre partagent l'historique des mesures.
- Rafraîchissement GUI toutes les 50 ms. Les signaux dédiés transmettent immédiatement les états et erreurs à la boucle GUI. Le statut de connexion vient des signaux d'ouverture/fermeture et des erreurs série.
- Le graphique possède deux historiques FIFO bornés : 2000 mesures moteur et 400 trames accéléromètre, en entiers bruts, dans le thread GUI. Les nouvelles entrées remplacent les plus anciennes. Le worker publie les objets dans un historique partagé de 200 trames M/A. Les sessions ne sont pas encore séparées.
- Acquérir/Stopper envoient SET MESURES ON/OFF. Le bouton indique la demande locale, pas un acquittement matériel. Commandes manuelles OUVRIR/FERMER/STOP, étalonnage avec confirmation, DO RESET.
- Terminal Debug séparant événements/commandes et mesures, coloration, défilement automatique et effacement visuel. L'effacement ne vide pas les historiques ; réouvrir Debug les recharge.
- À l'arrêt : tentative SET MESURES OFF, vidange bornée à 300 ms dans le worker, fermeture du port, sortie et attente du thread. Aucun terminate(). La livraison physique de OFF n'est pas garantie, notamment après débranchement.
- API facultative `SerialController::send(command, true)` : attente d'un O/N, délai de 2 secondes et signal commandResult. Sans identifiant dans le protocole, l'association reste au mieux : une réponse tardive peut être ambiguë. Une seule commande suivie à la fois, autres envois refusés pendant l'attente. Les boutons actuels utilisent l'envoi simple.

## Protocole JSON

Les commandes, modes, types de consigne, états de production et codes auparavant définis
dans le fichier source Protocol.h sont définis dans resources/protocol.json, également
embarqué dans resources.qrc. CMake génère automatiquement generated/serial/Protocol.h
dans le dossier de compilation. Modifier le JSON puis recompiler ; ne pas modifier le
fichier genere. Les commandes JSON sont completes et sans espace final ; CMake genere
aussi `Protocol::CMD` et `Protocol::CommandRequest`. Les composants et les pages
emettent des signaux `sendCommand(CMD, values, awaitReply)` et `sendRawCommand`.
MainWindow les relaie vers le controleur serie.
Les arguments sont ajoutes avec un espace par le controleur.

## Décodage et points à confirmer avec le firmware

Source : Architecture.md, Interface.md et Serial.md fournis. Le décodeur utilise le préfixe binaire suivi du payload little-endian :

| Préfixe | Payload | Total préfixe inclus |
|---|---|---|
| 0x01 | `<BBi>` (type E/W/I/S/R/O/N/T) | 7 octets |
| 0x01 | `<BIhhiiHi>` (type M inclus) | 24 octets |
| 0x01 | `<BIhhhiiiiii>` (type A inclus) | 36 octets |
| autre | texte terminé par LF ou le prochain 0x01 | variable |

Les lectures fragmentées et plusieurs trames dans une lecture sont gérées. Les octets LF et 0x01 à l'intérieur d'un payload binaire ne sont pas des séparateurs. L'ASCII sans LF est découpé à 4096 octets pour borner l'attente. Le protocole ne définit pas de checksum. Le marqueur 0x01 pouvant aussi apparaître dans les données binaires, une resynchronisation fiable après une perte d'octet reste impossible sans évolution du firmware. Les valeurs sont gardées brutes : les unités et facteurs de l'accéléromètre ne sont pas spécifiés.

**Contradictions conservées explicitement** :

- Les identifiants des machines sont définis dans `resources/machines.json` et doivent correspondre au firmware. `protocol.json` contient les commandes ; `etats.csv` définit les états Arduino et `codes.csv` les codes de notification.
- Interface.md utilise DO ETALONNAGE et DO PILOTAGE ; la table des commandes de Serial.md mentionne CALIBRATION et PILOTER. Le JSON retient les commandes de l'interface. Modifier les chaînes avant de les utiliser sur la carte. Le bouton de pilotage utilise DO PILOTER, conformément au firmware local.
- Les identifiants illustratifs du diagramme d'état d'Interface.md diffèrent des valeurs du protocole. L'affichage d'état utilise les valeurs explicites de Serial.md ; le diagramme interactif est différé.
- La table codes.csv est extraite du document fourni, y compris ses deux entrées d'état contradictoires et quelques caractères déjà mal encodés. Le numéro brut est toujours visible. La remplacer par le CSV exporté du firmware sera préférable.

Les ressources sont embarquées : après modification du JSON, du CSV ou du QSS, recompiler.

## Étapes suivantes

L'onglet `PilotagePage` affiche les mesures dans un `GraphiqueWidget` et gère directement à gauche l'asservissement avec deux `PIDWidget` (vitesse et position). L'asservissement est initialement désactivé ; son activation impose au moins un PID sélectionné. Chaque correcteur propose P, PI et PID, avec des gains de 0 à 100 et une aide. Sous les PID, `ConsigneWidget` propose un échelon, une rampe, un trapèze ou une sinusoïde avec aperçu, inversion et paramètres. L'amplitude s'adapte au mode : ±255 en PWM, ±10 rad/s en vitesse, ±150° en position. L'offset est borné par la durée ; les rampes du trapèze sont ajustées pour conserver un plateau non négatif. `getCommandes()` prépare les commandes SET MODE, SET PID et SET CONSIGNE ; les gains inutilisés sont transmis comme zéro et l'inversion est appliquée à l'amplitude de la commande.

Les listes reprennent le style du dialogue série en version compacte, sur des panneaux sans fond coloré. Sous les réglages, Envoyer transmet chaque commande avec acquittement, puis DO PILOTER ; Stopper annule la suite et transmet DO STOP. Calibration reprend le bouton et la confirmation de Mesures. Les durées sont converties en millisecondes ; le profil sinusoïdal utilise SINUS et un délai nul, conformément au firmware local.

Pas encore implémentée : gestion complète des erreurs actives/acquittées. Le voyant est gris tant que l'état est inconnu ; ensuite il reflète les états/erreurs reçus, sans inventaire complet des erreurs actives.

## Organisation

| Fichier | Responsabilité |
|---|---|
| src/gui/MainWindow.* | Onglets, statut et rafraîchissement |
| src/gui/widgets/MeasurePage.* | Page des mesures |
| src/gui/widgets/PilotagePage.* | Onglet de pilotage et graphique des mesures |
| src/gui/widgets/PIDWidget.* | Réglages des correcteurs |
| src/gui/widgets/ConsigneWidget.* | Paramètres et aperçu de la consigne |
| src/gui/widgets/DebugPage.* | Page de debug |
| src/serial/SerialController.* | Découverte, paramètres et cycle de vie du thread |
| src/serial/SerialWorker.* | Port, envois, réception et signaux critiques |
| src/serial/SerialDecoder.* | Reconstruction des trames et décodage explicite |
| src/core/Event.* | Événements spécialisés et table des textes |
| src/core/EventQueue.h | Historique borné et curseurs |
| src/core/MeasurementStore.h | Mémoire dédiée aux futures courbes |
| src/gui/components/SerialSettingsDialog.* | Boîte de configuration série |
| resources/ | Style, protocole configurable, table des codes |
| tests/CoreTests.cpp | Fragmentation, tailles, valeurs signées, historique concurrent |
| tests/IntegrationTests.cpp | Fenêtre, cycle de vie du worker et simulation série |

Documentation Qt utilisée : https://doc.qt.io/qt-6/qserialport.html et https://doc.qt.io/qt-6/qthread.html.

## Thème graphique

Le thème de référence PySide est adapté dans `resources/styles.qss` et chargé depuis les ressources Qt. Le logo est embarqué dans `resources/icons/logo.svg`. Les noms d'objets (`boutonRunStop`, `boutonCalibration`, `boutonOuvrir`, `boutonReset`) et les propriétés `running`, `connectionState` et `state` sélectionnent les styles des contrôles et voyants. Modifier le QSS puis recompiler pour mettre à jour l'apparence.

## Graphes des machines à états

Le widget `StateGraphWidget`, intégré à Mesures, utilise `resources/machines.json` embarqué dans les ressources Qt (recompiler après modification). Production (code série 250) est affichée par défaut ; son état 6 sélectionne Calibration (251), puis tout autre état Production rétablit le graphe Production. Le dernier état reçu de chaque machine est conservé et surligné. Les états cachés restent indiqués dans le texte d'état, sans nœud ni transition dans le graphe.

Les coordonnées des états sont normalisées. Les transitions sont des segments droits sans angles configurables ; leurs extrémités sont calculées sur les bordures selon le premier et le dernier segment, y compris avec des points intermédiaires. Les points intermédiaires sont interprétés comme des déplacements successifs depuis le centre de l'état source, dans le repère normalisé (y positif vers le bas). Les commandes `cmd` sont accessibles par clic lorsque la connexion série est prête. La surbrillance suit uniquement les états confirmés par la carte.
Les dimensions sont définies dans chaque machine : `etat_height` donne la hauteur maximale des rectangles, `etat_diameter` le diamètre des cercles initiaux et finaux (fractions de la hauteur disponible dans le widget), et `etat_width` la largeur maximale des rectangles (fraction de la largeur du widget). Par exemple, `0.05` signifie 5 %. La police s'adapte à la hauteur et à la largeur maximale ; les rectangles suivent la largeur et la hauteur du texte avec une marge intérieure, dans les limites définies. Le graphe utilise directement toute la zone fournie par le layout : x et y sont des fractions de sa largeur et de sa hauteur réelles, sans marge liée aux dimensions des états. La valeur `etat_height` à la racine n'est plus utilisée.
Les couleurs du graphe se règlent dans le bloc `StateGraphWidget` de `resources/styles.qss` : `qproperty-stateBorderColor` et `qproperty-stateBackgroundColor` pour tous les états, `qproperty-activeBorderColor` et `qproperty-activeBackgroundColor` pour l'état actif, `qproperty-hoverBorderColor` et `qproperty-hoverBackgroundColor` pour le survol d'un état cliquable lorsque la connexion série est prête. Le survol est prioritaire sur les couleurs actives ; l'épaisseur de bordure conserve l'indication de l'état actif. Les flèches et les points centraux des états finaux utilisent la couleur de bordure correspondante (bordure normale pour les flèches).
Les trames série `S` sont décodées en `StateEvent`, spécialisation de `MessageEvent`. Les noms sont chargés une seule fois depuis `:/etats.csv` au démarrage, avant le lancement du worker série. Le Debug affiche par exemple `S [Production] REPOS` ou `S [Calibration] FIN`. Les états cachés utilisent aussi leur nom ; les codes inconnus restent affichés numériquement. Les codes, valeurs et octets bruts restent disponibles dans les événements.

Le graphe ne comporte plus de titre et utilise toute la hauteur disponible, sans marge supplémentaire ni réduction liée aux dimensions des états. Le champ optionnel `alt` remplace `nom` uniquement pour le libellé et le dimensionnement dans le graphe ; le nom complet reste affiché en infobulle et dans les événements et informations d’état.

Le signal série `stateReceived(int state)` transmet uniquement le numéro d'état ; la page Mesures retrouve sa machine dans les définitions JSON. Les états ne génèrent pas d'alertes. Les trames `E` et `W` sont des `ErrorEvent` et `WarningEvent` et déclenchent respectivement les notifications d'erreur et d'avertissement. Chaque erreur carte ouvre une boîte de dialogue critique à acquitter avec OK ; la réception série continue pendant son affichage.
## Courbes de mesures

`GraphiqueWidget` remplace le panneau provisoire de l'onglet Mesures. Son tracé utilise QPainter, sans module Qt Charts supplémentaire. `resources/courbe.json` est embarqué : recompiler après modification.

Chaque entrée de `courbes` définit `source` (`Mesures` ou `Acceleration`), `id`, `nom`, `unite`, `couleur`, `echelle`, `facteur` et éventuellement `visible`. La valeur affichée/exportée vaut la valeur brute multipliée par `facteur`. Le tracé la divise ensuite par `echelle` ; `null` ou `0` utilise le maximum absolu de la série conservée. Les valeurs du panneau et du CSV ne sont pas normalisées.

Identifiants Mesures : `target`, `doorAngle`, `voltage`, `current`, `motorAngle`, `motorSpeed`. `voltage` est directement la tension moteur signée reçue en centivolts (int16, -2500 à 2500), convertie en volts par le facteur 0,01. Le champ PWM est supprimé de la trame. Les facteurs moteur 0,01 et les facteurs des vitesses/angles inertiels 0,01 suivent `ArduinoPorteBlum/src/Config/Types.h` et `Inputs/BN0055.cpp`. L'unité de la consigne dépend du mode de pilotage et reste vide. L'accélération linéaire reste en unités brutes : le commentaire du firmware indique une accélération angulaire alors que la lecture utilise les registres linéaires ; son unité doit être confirmée.

La source `Acceleration` contient trois entrées JSON : `acceleration`, `velocity` et `angles`. Chacune partage `nom`, `unite`, `echelle` et `facteur` entre les axes et définit `couleurs`, un tableau de trois couleurs X, Y, Z. Le widget développe ces entrées en neuf courbes (`acceleration0` à `acceleration2`, `velocity0` à `velocity2`, `angles0` à `angles2`) pour les données et le CSV. Les titres de légende affichent le nom et les unités du groupe. Les courbes sont cochées au démarrage ; la centrale inertielle reste désactivée avant son activation. Les sélections utilisent `ColorCheckBox` (dérivée de QCheckBox) : bordure toujours colorée et fond coloré lorsque la case est cochée. `AxisColorCheckBox` remplace la coche par X, Y ou Z ; la lettre reste visible en gris lorsque la case est décochée. Les indicateurs mesurent 18 × 18 pixels logiques, avec les couleurs définies dans le JSON. Ces composants sont dans `src/gui/components/ColorCheckBox.*`.

Le suivi adapte la plage temporelle aux premiers et derniers points disponibles, avec une fenêtre maximale de 10 s. Il ne force pas le début à zéro si les premières données commencent plus tard. Molette : zoom ; Ctrl : temps seul ; Maj : amplitude seule. Glisser déplace la vue. Chaque rafraîchissement rétablit automatiquement les dix dernières secondes, sans case de suivi. Un double-clic cadre l'historique visible et rétablit l'amplitude [-1,1 ; 1,1]. Le survol sélectionne l'échantillon visible le plus proche ; chaque série utilise son propre temps pour les valeurs. Quitter le tracé rétablit les dernières valeurs reçues. La période moyenne est affichée séparément pour chaque source, en ignorant les différences négatives lors d'une remise à zéro de l'horloge. Les tracés sont interrompus lors d'un retour en arrière du temps.

« Enregistrer » exporte toutes les courbes configurées, même décochées, dans un CSV au format long `source;id;temps_<unité>;valeur;unite`, avec les valeurs converties et les temps indépendants. L'export porte sur les 2000 mesures moteur et 400 trames accéléromètre maximum conservées, sans fusion des horodatages. `reset()` efface uniquement la vue : un nouveau rafraîchissement recharge l'historique existant.

L'objet `time` de `courbe.json` définit le nom, l'unité et le facteur du temps pour l'axe, les valeurs, les périodes et le CSV. Chaque valeur de courbe est multipliée par son `facteur` ; `echelle` règle uniquement la normalisation verticale du tracé.

Dans `resources/debug.json`, l'objet `messages` définit les textes `Mesures` et `Acceleration` affichés dans le débogage et le résumé. Chaque `$` consomme le champ entier suivant ; `$.n` le divise par 10 puissance n et affiche n décimales (0 à 6). `$x.y` réserve au minimum x caractères avant le point, signe moins compris, avec des espaces à gauche, sans zéros de remplissage. y conserve la division et le nombre de décimales ; `$x.0` affiche un entier. Une valeur trop large reste complète. Ordre M : `time, voltage, current, motorAngle, motorSpeed, doorAngle, target`. Ordre A : `time`, les trois `acceleration`, les trois `velocity`, les trois `angles` (X, Y, Z). Ces divisions de présentation ne modifient pas les données stockées. Dans `codes.csv`, tous les marqueurs utilisent la même valeur du message, y compris plusieurs `$.2`.

## Sauvegarde et ouverture des mesures

Le bouton Enregistrer exporte toutes les courbes en mémoire, y compris celles masquées, au format décrit dans `docs/Mesures.md`. Le séparateur et les décimales suivent la région du PC. Les mesures et la centrale inertielle gardent leurs temps indépendants ; les cellules de la série la plus courte restent vides. Les données inertielles déjà acquises sont conservées même si la centrale a ensuite été désactivée.

Ouvrir affiche chaque fichier dans une fenêtre indépendante avec la date, le mode, les gains PID et la consigne du pilotage. Cette fenêtre conserve le zoom et la sélection des courbes, sans bouton Enregistrer ni commande série. Les fichiers doivent porter la même version que le logiciel et des colonnes compatibles avec la configuration des courbes actuelle.

### Donn?es brutes index?es et temps commun

Les `id` de `courbe.json` sont des indices dans les tableaux sign?s `rawValues()` : 0 = temps ; M : 1 tension, 2 courant, 3 angle moteur, 4 vitesse moteur, 5 angle porte, 6 consigne ; A : 1?3 acc?l?ration, 4?6 vitesse, 7?9 angles. Les entiers 64 bits pr?servent aussi les horodatages et angles non sign?s du protocole. L?ordre JSON reste l?ordre des l?gendes ; les groupes inertiels donnent l?indice du premier axe. Les anciens identifiants textuels restent accept?s.

Le graphique convertit chaque trame une seule fois par rafra?chissement, puis applique les facteurs par indice. La recherche au survol est dichotomique lorsque les temps sont ordonn?s. Les lots vides ne reconstruisent plus les courbes. Les mesures inertielles utilisent le temps moteur strictement ant?rieur ; les ?v?nements s?rie restent immuables. Le CSV utilise d?sormais une seule colonne temporelle avec des cellules vides pour les valeurs absentes ; voir docs/Mesures.md. Cette description remplace les indications de temps ind?pendants et de CSV long ci-dessus.
