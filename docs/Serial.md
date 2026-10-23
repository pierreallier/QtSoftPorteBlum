# Protocole et liaison série

La liaison utilise 8 bits de données, sans parité, un bit de stop et aucun contrôle de flux. Le débit intégré est de 115200 bauds ; le mode manuel permet de le changer. Les commandes et codes nommés proviennent de `resources/protocol.json`. Voir [Config.md](Config.md).

## Découverte et initialisation

`SerialController` recherche les ports toutes les 1,5 s lorsqu'aucun worker n'existe. Les noms ou fabricants Arduino et les VID `2341`/`2A03` sont prioritaires ; cette heuristique ne garantit pas l'identité de la carte. Au démarrage, aucun port n'est ouvert. Connexion autorise les tentatives et les reconnexions en mode automatique ; Déconnexion les annule. Le mode manuel utilise le port choisi sans reconnexion automatique.

Après ouverture, `SerialWorker` tente un reset matériel : DTR est désactivé puis réactivé après 100 ms ; RTS suit la même séquence. Les données reçues pendant cette phase sont écartées et le décodeur est réinitialisé. L'indisponibilité du DTR génère un avertissement et laisse fonctionner le secours logiciel.

Le worker écoute ensuite le démarrage. Sans message `I` de code 61 ou 70 sous deux secondes, il envoie une fois `DO RESET`. La fin d'initialisation doit être confirmée par `I`, code 71. Un délai de surveillance de sept secondes est lancé au départ et lors des notifications de reset/début d'initialisation ; le secours logiciel le règle à cinq secondes après son envoi. En cas d'expiration, le port est fermé, l'intention de reconnexion est annulée et une erreur est présentée.

Les commandes utilisateur sont autorisées uniquement après le code 71. Un reset remet à zéro les indicateurs d'initialisation et d'acquisition et annule les attentes de réponse et arrêts différés.

## Réception Arduino vers PC

Chaque trame binaire commence par `0x01`, suivi du type ASCII. Les nombres sont little-endian, sans padding. Les tailles ci-dessous comprennent le marqueur et le type.

| Type | Format après le marqueur, type inclus | Taille totale |
| --- | --- | --- |
| `M` | `<BIhhiiHi>` | 24 octets. |
| `A` | `<BIhhhiiiiii>` | 36 octets. |
| Autre type binaire | `<BBi>` | 7 octets. |

La notation indique : `B` entier non signé sur 8 bits, `I` non signé sur 32 bits, `h` signé sur 16 bits, `H` non signé sur 16 bits et `i` signé sur 32 bits.

### Trame M : mesures moteur

| Champ après le type | Type | Interprétation de l'affichage courant |
| --- | --- | --- |
| Temps | `uint32` | Millisecondes, facteur `0.001` vers les secondes. |
| Tension moteur | `int16` | Centivolts signés, facteur `0.01` vers V. |
| Courant | `int16` | Facteur graphique `-0.01` vers A. |
| Angle moteur | `int32` | Centièmes de degré, facteur `0.01`. |
| Vitesse moteur | `int32` | Centièmes de rad/s, facteur `0.01`. |
| Angle porte | `uint16` | Centièmes de degré, facteur `0.01`. |
| Consigne | `int32` | Conversion selon la configuration et le contexte Pilotage. |

Aucun champ PWM distinct ne figure dans la trame M. Le décodeur conserve les entiers bruts et n'impose pas de bornes physiques. Les facteurs du graphique et du journal sont indépendants.

### Trame A : centrale inertielle

| Champ après le type | Type | Affichage courant |
| --- | --- | --- |
| Temps | `uint32` | Millisecondes. |
| Accélération X, Y, Z | 3 × `int16` | Valeurs brutes, facteur `1`. |
| Vitesse angulaire X, Y, Z | 3 × `int32` | Facteur `0.01` vers °/s. |
| Angles X, Y, Z | 3 × `int32` | Facteur `0.01` vers °. |

Les trois axes sont toujours transmis dans l'ordre X, Y, Z. L'accélération reste libellée « brut » dans la configuration ; aucune unité physique supplémentaire n'est déduite par le décodeur.

### Autres événements binaires

Après le type viennent un code `uint8` et une valeur `int32` toujours présente, même si le texte du message ne l'utilise pas.

| Type | Signification et traitement |
| --- | --- |
| `E` | Erreur : `ErrorEvent`, notification et boîte critique dans le GUI. |
| `W` | Avertissement : `WarningEvent` et notification. |
| `I` | Information, notamment les notifications d'initialisation. |
| `S` | État de machine : `StateEvent`. |
| `O` | Réponse positive ; valide la commande suivie en cours. |
| `N` | Réponse négative ; refuse la commande suivie en cours. |
| `R` | Réponse simple, sans validation de l'attente O/N. |
| `T` | Message binaire traité par le format générique ; présentation texte dans Debug. |

Un type binaire inconnu est également décodé sur sept octets. Hors préfixe binaire, les octets forment un `TextEvent`, interprété en UTF-8 jusqu'au prochain LF ou marqueur `0x01`. Un bloc sans séparateur est découpé à 4096 octets.

Le décodeur accepte les lectures fragmentées et plusieurs trames dans une seule lecture. Les octets LF et `0x01` dans une charge binaire ne sont pas des séparateurs. Le protocole n'a ni checksum ni longueur explicite : après perte d'un octet, une resynchronisation fiable ne peut pas être garantie.

## États des machines

Les identifiants actuels de `machines.json` et `codes.csv` sont **Production = 250** et **Calibration = 251**. Pour une trame S, le code identifie la machine et la valeur identifie l'état.

| Machine | Valeurs et noms configurés |
| --- | --- |
| Production | 0 INIT ; 1 REPOS ; 2 DEMANDE ; 3 OUVERTURE ; 4 FERMETURE ; 5 PILOTAGE ; 6 CALIBRATION ; 7 CALIBRATION REQUISEE ; 8 DEBRAYAGE ; 9 ERREUR ; 10 PAS DE PUISSANCE. |
| Calibration | 20 INIT ; 21 OUVERTURE ; 22 ATTENTE HAUTE ; 23 RECHERCHE BUTEE BASSE ; 24 ATTENTE BASSE ; 25 RECHERCHE BUTEE HAUTE ; 26 ENREGISTREMENT ; 27 ERREUR ; 28 NONE. |

Les noms et états cachés proviennent du JSON. Le signal `stateReceived(int)` transmet uniquement la valeur ; la page Mesures retrouve la machine si cet état appartient à une seule définition. Pour le texte d'un `StateEvent`, un état ambigu utilise le code de machine reçu. Une valeur non définie est affichée comme état inconnu. Les états S ne déclenchent pas d'alerte E/W.

## Envoi PC vers Arduino

Les commandes sont des chaînes UTF-8 terminées par LF. Les espaces aux extrémités sont supprimés ; une commande vide ou contenant un retour à la ligne interne est refusée. Les espaces finaux des préfixes dans `protocol.json` restent nécessaires lors de la construction des commandes à paramètres.

| Usage | Commandes utilisées |
| --- | --- |
| Réinitialisation | `DO RESET` |
| Calibration | `DO CALIBRATION` |
| Mouvement manuel | `DO OUVRIR`, `DO FERMER`, `DO STOP` |
| Lancement prévu par le protocole | `DO PILOTER` ; non envoyé par la séquence temporisée actuelle de Pilotage. |
| Acquisition | `SET MESURES ON`, `SET MESURES OFF` |
| Centrale inertielle | `SET ACCEL ON`, `SET ACCEL OFF` |
| Réglages | `SET MODE ...`, `SET PID ...`, `SET CONSIGNE ...` ; voir [Pilotage.md](Pilotage.md). |

Les commandes manuelles de Debug utilisent le même chemin d'envoi et les mêmes restrictions d'initialisation. Une commande mise intégralement en tampon par `QSerialPort::write` produit un `RequestEvent` de type Q dans l'historique et le signal `commandSent`. Cela ne prouve pas sa réception par la carte.

## Réponses et commandes différées

`SerialController::send(command, true)` attend le premier O/N reçu, avec un délai de deux secondes. `commandFinished(command, accepted)` décrit le résultat ; `commandResult` fournit le texte de réponse ou d'erreur. Une seule commande suivie est autorisée à la fois. Sans identifiant de corrélation, une réponse tardive peut être attribuée à une commande ultérieure.

Pendant une attente, les autres envois sont refusés, sauf les demandes d'arrêt prises en charge : `DO STOP` sans attente et `SET MESURES OFF` peuvent être différés jusqu'à la réponse ou l'expiration. Le worker transmet alors STOP avant OFF ; OFF conserve sa demande éventuelle d'acquittement. Un reset annule ces demandes.

La configuration de Pilotage, les commandes de centrale inertielle et l'arrêt OFF du changement d'onglet utilisent l'attente de réponse. Les commandes simples d'acquisition, de mouvement et de calibration ne l'utilisent pas.

## État confirmé et historiques

`SystemState` contient `port`, `baud`, `connected`, `initialized`, `measuring` et `accelerometer`. Les notifications O/I/R 150 et 151 actualisent l'acquisition ; 44 et 53 actualisent la centrale. Le code 77 de type E/N la marque inactive. Après initialisation, la première trame M/A observée confirme aussi l'acquisition correspondante.

Les trames M/A partagent un historique de 200 événements ; les autres événements et commandes ont un second historique de 200 entrées. Les graphiques disposent de stockages locaux plus grands. Les objets conservent leurs octets bruts et un horodatage de réception UTC. Voir [Architecture.md](Architecture.md).

La table complète des noms, codes et textes est `resources/codes.csv` ; elle n'est pas recopiée ici pour éviter des versions divergentes. Les substitutions et codes structurants sont documentés dans [Config.md](Config.md).

## Fermeture et perte de liaison

Les erreurs de ressource, périphérique absent, lecture ou écriture arrêtent le worker. À l'arrêt, si la carte est initialisée, il tente `SET MESURES OFF`, attend au maximum 300 ms pour les écritures puis ferme le port. Avant initialisation, cette commande n'est pas envoyée. Le contrôleur attend la fin du thread sans arrêt forcé. Une déconnexion physique ne permet pas de garantir la livraison de OFF.
