# Fichiers de configuration

Ce document décrit les fichiers de `resources/` et leur interprétation par l'application Qt SoftPorteBlum.

## Chargement et modification

Les ressources sont intégrées à l'exécutable par `resources/resources.qrc`, avec des chemins comme `:/debug.json`. Modifier les fichiers sources nécessite donc de reconstruire l'application. Les fichiers JSON utilisent l'UTF-8 et la syntaxe JSON stricte (sans commentaires ni virgules finales). `protocol.json` contient un tableau à la racine ; `machines.json`, `courbe.json` et `debug.json` contiennent un objet.

`protocol.json` et `etats.csv` servent aussi à générer les identifiants C++ dans `generated/serial/CommandIds.h` (commandes) et `generated/serial/Protocol.h` (états) pendant la configuration CMake. Les textes des commandes sont chargés depuis le JSON au démarrage, sans être copiés dans cet en-tête. `config.ini` fournit des valeurs par défaut intégrées, surchargées clé par clé par un fichier `config.ini` situé à côté de l'exécutable. Copier les autres JSON à côté de l'exécutable ne surcharge pas leurs ressources intégrées.


`AppSettings::initialize()` charge les ressources une seule fois et les partage en lecture seule. Le constructeur de `Resources` appelle, dans cet ordre, `readProtocolFile()`, `readStateFile()`, `readCodesFile()`, `readCourbeFile()` et `readDebugFile()`. Le chargement s'arrête à la première erreur ; `initialize(&error)` renvoie le diagnostic. `readStateFile()` charge à la fois `etats.csv` et le diagramme `machines.json`.

## `codes.csv` : dictionnaire des messages

Format UTF-8, une entrée par ligne, avec le séparateur `;` :

```text
NOM;CODE;TEXT
DATA_TENSION;153;"tension d'alimentation $.2V"
```

Hors d'un champ entre guillemets, le caractère `#` commence un commentaire jusqu'à la fin de la ligne. Une ligne commençant par `#` (éventuellement après des espaces) est entièrement ignorée. Les commentaires sont retirés avant de découper les colonnes ; les lignes vides restantes sont ignorées. L'en-tête est facultatif et peut lui aussi être commenté : le lecteur n'utilise pas les noms de colonnes.

```text
# NOM;CODE;TEXT
# Mesures électriques
DATA_TENSION;153;"tension d'alimentation $.2V" # Valeur reçue en centivolts
DATA_COURANT;154;"courant consommé $.2A"
```

Dans cet exemple, le texte du code 153 reste `tension d'alimentation $.2V`. Un `#` à l'intérieur d'un texte entre guillemets est conservé.

| Colonne | Signification |
| --- | --- |
| `NOM` | Nom symbolique Arduino, utilisé par `AppSettings::codeId(name)` pour retrouver CODE. |
| `CODE` | Identifiant numérique du message reçu. Sur la liaison, le code est un entier non signé sur 8 bits : 0 à 255. |
| `TEXT` | Modèle du texte affiché, éventuellement paramétré par la valeur entière signée sur 32 bits accompagnant le message. |

L'en-tête est ignoré car sa deuxième colonne n'est pas un entier. Les lignes de moins de trois colonnes ou dont le code n'est pas dans 0 à 255 sont ignorées. Pour un code ou nom dupliqué, la dernière entrée gagne. Les espaces autour du champ TEXT sont supprimés. Les guillemets extérieurs sont retirés ; les espaces à l'intérieur sont conservés. Les `;` supplémentaires sont conservés dans le texte.

`AppSettings::instance().codeId("INFO_INIT_FIN")` renvoie le CODE associé au NOM ; un nom inconnu renvoie `-1`. Les guillemets extérieurs de TEXT sont retirés avant affichage et les paires `""` à l'intérieur deviennent un guillemet littéral.

Le préfixe du nom (`ERR_`, `INFO_`, `DATA_`, `TEST_`, `ETAT_`) est une convention ; le type reçu (`E`, `I`, etc.) détermine le style d'affichage. Un code absent produit `Code inconnu <code> ; valeur=<valeur>`.

### Substitutions communes à `codes.csv` et `debug.json`

| Syntaxe | Interprétation | Exemple |
| --- | --- | --- |
| `$` | Entier brut, sans conversion. | `1234` → `1234` |
| `$.d` | Division par 10 puissance `d`, puis affichage avec `d` décimales ; `d` va de 0 à 6. | `$.2` avec `1234` → `12.34` |
| `$w.d` | Même conversion, avec alignement à droite et espaces. Largeur minimale totale : `w + d + 1` si `d > 0`, sinon `w`. | `$4.2` avec `1234` → `  12.34` |
| `${texte0\|texte1}` | Premier texte si la valeur vaut zéro, second pour toute autre valeur. | `${KO\|OK}` : 0 → `KO`, 1 → `OK` |

Les unités (`V`, `A`, `°`, etc.) sont du texte ajouté après la substitution. Dans `codes.csv`, chaque substitution d'une ligne réutilise la même valeur reçue. Dans les modèles de mesures de `debug.json`, chaque substitution consomme le champ suivant. Une substitution sans valeur disponible reste inchangée. Il n'existe pas de syntaxe dédiée pour échapper un `$` littéral.

## `courbe.json` : courbes et conversion des mesures

L'objet racine contient `time` et le tableau `courbes`.

### Objet `time`

| Item | Type | Signification |
| --- | --- | --- |
| `nom` | Chaîne | Libellé temporel, par défaut `Temps`. |
| `unite` | Chaîne | Unité affichée, par défaut `s`. |
| `facteur` | Nombre fini strictement positif | Multiplicateur du temps brut ; `0.001` convertit les millisecondes en secondes. Valeur absente ou invalide : conservation du facteur par défaut `0.001`. |
| `echelle` | `null` dans le fichier actuel | Champ présent mais non exploité pour le temps. |

### Entrées de `courbes`

| Item | Type | Signification |
| --- | --- | --- |
| `source` | Chaîne | `Mesures` pour les trames `M`, `Acceleration` pour les trames `A`. Respecter la casse. |
| `id` | Chaîne | Champ de la trame à tracer ; liste ci-dessous. |
| `nom` | Chaîne non vide | Libellé de la courbe ou du groupe de trois axes. |
| `unite` | Chaîne | Unité affichée, vide si aucune. Ne réalise aucune conversion. |
| `facteur` | Nombre fini | `valeur physique = valeur brute × facteur`. Par défaut `1` ; un facteur négatif inverse le signe. |
| `echelle` | Nombre positif ou `null` | Diviseur de normalisation verticale après conversion. `null`, zéro ou absence : maximum absolu des points disponibles, ou `1` sans amplitude. Une valeur négative invalide la courbe. |
| `couleur` | Chaîne | Couleur Qt valide, par exemple `#56B4E9`, pour une courbe simple. |
| `couleurs` | Tableau de trois couleurs | Pour les groupes `Acceleration`, couleurs des axes X, Y, Z, dans cet ordre. |
| `visible` | Booléen facultatif | Case de sélection cochée au départ si `true` ; valeur par défaut `true`. |

La coordonnée verticale normalisée est la valeur physique divisée par l'échelle. Cette normalisation permet de superposer des grandeurs différentes ; les valeurs affichées et exportées restent dans leurs unités physiques. Par exemple, `voltage` avec `facteur: 0.01` et `echelle: 24` transforme `2400` en `24 V`, placé à l'ordonnée normalisée `1`.

| Source | `id` du fichier | Grandeur | Facteur actuel | Échelle actuelle |
| --- | --- | --- | --- | --- |
| `Mesures` | `target` | Consigne (unité dépendante du mode) | `0.01` | Automatique |
| `Mesures` | `doorAngle` | Angle porte, ° | `0.01` | `360` |
| `Mesures` | `voltage` | Tension moteur, V | `0.01` | `24` |
| `Mesures` | `current` | Courant moteur, A | `-0.01` | `5` |
| `Mesures` | `motorAngle` | Angle moteur, ° | `0.01` | Automatique |
| `Mesures` | `motorSpeed` | Vitesse moteur, rad/s | `0.01` | Automatique |
| `Acceleration` | `acceleration` | Accélération X/Y/Z, brute | `1` | Automatique |
| `Acceleration` | `velocity` | Vitesse angulaire X/Y/Z, °/s | `0.01` | Automatique |
| `Acceleration` | `angles` | Angles X/Y/Z, ° | `0.01` | Automatique |

Chaque groupe inertiel est développé en trois courbes : suffixes internes `0`, `1`, `2` pour X, Y, Z. Un groupe avec un identifiant inconnu ou sans exactement trois couleurs est ignoré. Une courbe avec un identifiant non pris en charge, un nom vide, une couleur invalide, une échelle négative ou un facteur non fini est ignorée.

## `debug.json` : texte et présentation du journal

### Modèles `message` par type

Chaque entrée de `types` contient un modèle `message`. Pour les messages et commandes, `$` est remplacé par le contenu de l'événement (par exemple `Erreur : $` ou `Avertissement : $`). Pour les mesures, les marqueurs numériques consomment les entiers bruts dans cet ordre :

`AppSettings` charge et valide ce fichier via `DebugStyles`. `DebugPage` récupère les styles partagés et applique les substitutions : le contenu d'un événement à code fournit un élément ; les valeurs brutes d'une mesure fournissent un élément par champ. Les événements ne chargent pas les formats de diagnostic.

| Clé | Ordre des valeurs consommées |
| --- | --- |
| `M` | Temps, tension, courant, angle moteur, vitesse moteur, angle porte, consigne. |
| `A` | Temps, accélération X/Y/Z, vitesse angulaire X/Y/Z, angles X/Y/Z. |

Les libellés et unités sont libres, mais l'ordre des champs est fixé par le code. Le modèle actuel utilise `$.3` pour convertir le temps en secondes et des formats à deux décimales pour les grandeurs transmises en centièmes. Un modèle absent utilise `$` et affiche le contenu brut de l'événement.

Les modèles du journal n'utilisent pas les facteurs de `courbe.json` : notamment, le courant y est actuellement affiché avec son signe brut, tandis que la courbe applique `-0.01`.

### Tableau `types`

Chaque objet définit la présentation d'un type d'événement dans le journal.

| Item | Type / valeurs | Signification |
| --- | --- | --- |
| `type` | Un caractère ASCII imprimable, hors espace | Identifiant unique du type. |
| `foreground` | Couleur ou `null` | Couleur du texte. Accepte notamment `#RRGGBB`, `#RRGGBBAA` (alpha à la fin) et les noms de couleurs Qt. |
| `background` | Couleur ou `null` | Couleur du fond du texte, même syntaxe. |
| `justify` | `left`, `right`, `center`, `justify` ou `null` | Alignement du paragraphe ; gauche par défaut. |
| `underline` | Booléen ou `null` | Soulignement. |
| `overstrike` | Booléen ou `null` | Texte barré. |
| `prefix` | Chaîne ou `null` | Au plus un caractère Unicode imprimable, par exemple `◀` ou `▶`. Chaîne vide : aucun symbole. |
| `message` | Chaîne ou `null` | Modèle de texte, `$` par défaut. Inclut les libellés de catégorie ou les formats numériques des mesures. |

Les propriétés de style absentes ou `null` laissent le format par défaut. Un type sans définition reçoit le style de repli. Si un préfixe est défini pour au moins un type, le journal réserve une colonne de préfixes pour aligner les messages.

| Type actuel | Usage |
| --- | --- |
| `E` | Erreur, rouge, modèle `Erreur : $`. |
| `W` | Avertissement, orange, modèle `Avertissement : $`. |
| `S` | État d'une machine, vert, modèle `Etat : $`. |
| `I` | Information, bleu. |
| `O` | Réponse positive à une commande, bleu, préfixe `◀`. |
| `N` | Réponse négative à une commande, rouge, préfixe `◀`. |
| `R` | Réponse reçue, préfixe `◀`. |
| `Q` | Commande envoyée, soulignée, préfixe `▶`. |
| `M` | Trame de mesures. |
| `A` | Trame inertielle, préfixe `◀`. |
| `T` | Texte reçu hors trame binaire. |

Le tableau doit être non vide. Les types dupliqués, propriétés de mauvais type, couleurs invalides, alignements inconnus et préfixes non conformes provoquent une erreur de chargement.

## `protocol.json` : dictionnaire des commandes

Le fichier contient uniquement une liste non vide d'objets `key` / `cmd` :

```json
[
    {"key": "Reset", "cmd": "DO RESET"},
    {"key": "MesuresOn", "cmd": "SET MESURES ON"}
]
```

| Champ | Signification |
| --- | --- |
| `key` | Nom symbolique unique de forme `[A-Za-z_][A-Za-z0-9_]*`, par exemple `Reset`. |
| `cmd` | Texte série unique, non vide, par exemple `DO RESET`. Sans espaces aux extrémités, espaces multiples, retour à la ligne ni tabulation. |

`AppSettings::Resources::readProtocolFile()` charge le dictionnaire au démarrage. Les clés ou textes dupliqués et les entrées invalides provoquent une erreur de chargement.

```cpp
const auto &settings = AppSettings::instance();
settings.command(Protocol::CMD::Reset);                         // "DO RESET"
settings.command(Protocol::CMD::MesuresOn);     // "SET MESURES ON"
```

Un nom inconnu ou un identifiant typé invalide renvoie une chaîne vide. Les états et codes de messages proviennent respectivement de `etats.csv` et `codes.csv`.

### Commandes actuelles

| Clés | Textes et signification |
| --- | --- |
| `Reset`, `Calibration`, `Pilotage`, `Stop`, `Ouvrir`, `Fermer` | `DO RESET`, `DO CALIBRATION`, `DO PILOTER`, `DO STOP`, `DO OUVRIR`, `DO FERMER`. |
| `MesuresOn`, `MesuresOff` | `SET MESURES ON` / `SET MESURES OFF` : activer/désactiver les mesures. |
| `AccelerometreOn`, `AccelerometreOff` | `SET ACCEL ON` / `SET ACCEL OFF` : activer/désactiver l'accéléromètre. |
| `SetPidVitesse`, `SetPidPosition` | `SET PID VITESSE` / `SET PID POSITION`, suivies de Kp, Ki, Kd. |
| `ModePWM`, `ModePosition`, `ModeVitesse`, `ModePV` | `SET MODE PWM`, `SET MODE POSITION`, `SET MODE VITESSE`, `SET MODE POSITION_VITESSE`. |
| `ConsigneEchelon`, `ConsigneRampe`, `ConsigneTrapeze`, `ConsigneSinus` | `SET CONSIGNE ECHELON`, `SET CONSIGNE RAMPE`, `SET CONSIGNE TRAPEZE`, `SET CONSIGNE SINUS`, suivies des valeurs du profil. |

### Identifiants typés et paramètres

CMake génère `Protocol::CMD` à partir des champs `key` de `protocol.json`, ainsi que les conversions `Protocol::commandName()` et `Protocol::commandId()`. Les appels et signaux utilisent les identifiants typés, par exemple `Protocol::CMD::Reset`. Ne pas éditer les en-têtes générés dans le dossier de construction. Les textes restent chargés au démarrage dans le dictionnaire d'`AppSettings`.

`Protocol::CommandRequest` associe un identifiant à une liste de paramètres ; `text()` récupère le texte via `AppSettings::command()` et ajoute les paramètres séparés par un espace.

```cpp
serial->sendCommand(Protocol::CMD::SetPidVitesse, {"1", "0.5", "0"}, true);
// Envoie : SET PID VITESSE 1 0.5 0
```

`Protocol::parseCommand()` utilise ce même dictionnaire, recherche la commande correspondante la plus longue et sépare ses paramètres. Une commande inconnue renvoie `std::nullopt`.

## `etats.csv` : référence des états Arduino

Format UTF-8, séparateur `;`, avec quatre colonnes :

```text
# TYPE ; ETAT ; VALEUR ; NOM
CALI ; DEBUT ; 20 ; INIT
```

`TYPE` identifie la machine, `ETAT` son nom Arduino, `VALEUR` l'identifiant série et `NOM` le libellé de debug. Les espaces autour des champs, les lignes vides et les commentaires commençant par `#` sont ignorés. Les identifiants `TYPE_ETAT` et les valeurs doivent être uniques. VALEUR est comprise entre `0` et `2147483647`. TYPE et ETAT suivent la forme `[A-Z_][A-Z0-9_]*` et NOM doit être non vide.

CMake génère les constantes `Protocol::Etat::PROD_OUVERTURE`, `Protocol::Etat::CALI_ATTENTE_HAUT`, etc. Le CSV embarqué est aussi chargé au démarrage pour résoudre les références de `machines.json` et les noms du debug. Reconstruire le logiciel après modification du CSV.

Dans le code, `AppSettings::instance().stateId("CALI_DEBUT")` renvoie `20`, ou `-1` si le nom `TYPE_ETAT` est inconnu. `stateName(20)` renvoie le NOM du CSV (`INIT` ici) ; un numéro inconnu donne `État inconnu`.

## `machines.json` : machines et diagrammes d'états

Ce fichier décrit l'affichage des états référencés dans `etats.csv` ; les transitions sont dessinées, elles n'exécutent pas la logique du firmware.

| Item racine | Signification |
| --- | --- |
| `default_machine` | Code de la machine affichée par défaut (`250`). À défaut, code de la première machine. |
| `machines` | Tableau non vide des définitions de machines. |

### Définition d'une machine

| Item | Type | Signification |
| --- | --- | --- |
| `name` | Chaîne | Nom de la machine : `Production` ou `Calibration` actuellement. |
| `code` | Entier | Identifiant de machine (`250` / `251`), également présent dans les messages d'état du protocole. |
| `type` | Chaîne | TYPE du CSV : `PROD` ou `CALI`. |
| `default_state` | Chaîne facultative | ETAT Arduino après remise à zéro : `NONE` pour Calibration. |
| `display_when` | Objet facultatif | Condition d'affichage : `machine` désigne une autre machine et `state` son état requis sous la forme `TYPE_ETAT`. La première condition satisfaite dans le tableau gagne ; sinon affichage de `default_machine`. |
| `etat_diameter` | Nombre | Diamètre des états circulaires, rapporté à la hauteur du diagramme ; défaut du code `0.05`, valeur configurée `0.03`. |
| `etat_height` | Nombre | Hauteur maximale des rectangles, rapportée à la hauteur du diagramme ; défaut `0.1`. |
| `etat_width` | Nombre | Largeur maximale des rectangles, rapportée à la largeur du diagramme ; défaut `0.8`. |
| `etat_font_max_px` | Entier | Taille maximale de la police des états en pixels ; absente ou ≤ `0`, aucune limite supplémentaire. La police reste réduite si nécessaire pour tenir dans les rectangles. |
| `etats` | Tableau | Définitions des états. |
| `transitions` | Tableau | Flèches entre états. |

Les rapports de dimensions sont bornés entre `0.001` et `1`. Le texte et l'espace disponible peuvent réduire la taille réelle des rectangles. Avec `display_when: {"machine": 250, "state": "PROD_CALIBRATION"}`, le diagramme Calibration apparaît lorsque Production est en calibration.

### Entrées de `etats`

| Item | Type | Signification |
| --- | --- | --- |
| `id` | Entier | Identifiant local unique dans la machine, utilisé par les transitions. |
| `etat` | Chaîne | ETAT Arduino du CSV, combiné au `type` de la machine. |
| `alt` | Chaîne facultative | Libellé du diagramme ; sinon NOM du CSV. Le debug utilise toujours NOM. |
| `x`, `y` | Nombres | Centre de l'état en coordonnées relatives : origine en haut à gauche ; `x=0.5` à mi-largeur, `y=0.5` à mi-hauteur. |
| `type` | Chaîne | `start` : cercle initial ; `end` : cercle final ; `normal` : rectangle arrondi ; `hidden` : état non dessiné, conservé dans les définitions. |
| `cmd` | Chaîne facultative | Clé `key` de `protocol.json`, par exemple `Stop`, `Ouvrir` ou `Fermer`. Le clic émet l'identifiant `Protocol::CMD` correspondant lorsque les commandes sont autorisées ; une clé inconnue est refusée au chargement. |

Au chargement, `AppSettings` résout les références en valeurs série pour les vues. Les champs numériques `code` et les noms `nom` sont ajoutés en mémoire ; ils ne sont pas à renseigner dans le fichier source.

Les états cachés peuvent omettre les coordonnées. Les valeurs série et les noms proviennent du CSV ; les ids locaux du diagramme restent indépendants de ces valeurs. Les références inconnues, les ids dupliqués et les transitions invalides sont signalés au démarrage.

### Entrées de `transitions`

| Item | Type | Signification |
| --- | --- | --- |
| `start` | Entier | `id` local de l'état source. |
| `end` | Entier | `id` local de l'état destination. |
| `points` | Tableau d'objets `{ "x": nombre, "y": nombre }` | Déplacements intermédiaires successifs, relatifs à la largeur/hauteur du diagramme. |

Le premier déplacement part du centre de l'état source ; chaque suivant part du point précédent. Les valeurs négatives sont permises. `points: []` donne une liaison directe. Le tracé est coupé aux contours des états ; une transition dont une extrémité n'est pas dessinée est ignorée.

## `config.ini` : préférences utilisateur

Format INI lu et écrit par `QSettings`. Les commentaires commencent notamment par `;`.

```ini
[files]
; Vide : dossier Documents de l'utilisateur.
openFolder=

[serial]
automatic=true
port=COM1
baud=115200
```

| Section / clé | Type et défaut intégré | Signification |
| --- | --- | --- |
| `files/openFolder` | Chemin, vide | Dossier initial des dialogues de fichiers. Vide : Documents de l'utilisateur. Un chemin relatif est résolu depuis le dossier du `config.ini` externe, donc normalement celui de l'exécutable. |
| `serial/automatic` | Booléen, `true` | Recherche et reconnexion automatiques après une demande de connexion. Au démarrage, seul le scan est effectué. Accepte `true`/`false` sans distinction de casse, ou `1`/`0` ; valeur invalide : défaut intégré. |
| `serial/port` | Chaîne, `COM1` | Port utilisé en mode manuel. |
| `serial/baud` | Entier strictement positif, `115200` | Débit série en bauds en mode manuel. Valeur invalide : défaut intégré. En mode automatique, le débit intégré est utilisé même si le fichier externe en définit un autre. |

Une clé absente du fichier externe reprend sa valeur intégrée. L'application enregistre les réglages série dans ce fichier externe ; le dossier doit donc être accessible en écriture pour sauvegarder les changements.

## Autres ressources

| Ressource | Rôle |
| --- | --- |
| `styles.qss` | Feuille de style Qt : sélecteurs, couleurs, polices, bordures et présentation des widgets. Ce n'est pas un fichier de paramètres métier. |
| `resources.qrc` | Manifeste XML des fichiers embarqués, sous le préfixe `/`. Ajouter une ressource à embarquer nécessite de l'y déclarer. |
| `qtbase_fr.qm` | Traductions françaises compilées de Qt ; fichier binaire. |
| `icons/` | Images et icônes de l'interface, référencées par les ressources. |

## Points de cohérence lors d'une modification

- Conserver les CODE de `codes.csv` et les VALEUR de `etats.csv` en accord avec le firmware.
- Utiliser les noms `TYPE_ETAT` dans le code et les conditions d'affichage ; dans les états du diagramme, utiliser ETAT avec le TYPE de la machine.
- Utiliser les ids locaux du diagramme dans les transitions, indépendamment des valeurs série.
- Garder les textes de commandes sans espace final ; les paramètres sont ajoutés par le code.
- Respecter l'ordre des substitutions des modèles de mesures. Les facteurs des courbes et les conversions du journal sont indépendants.
- Reconstruire après modification des ressources. CMake se reconfigure lorsque `etats.csv` ou `protocol.json` change. Le JSON est aussi chargé comme ressource au démarrage. Les préférences du `config.ini` externe sont relues sans reconstruction.

Références d'implémentation : `src/core/AppSettings.cpp`, `src/core/Event.cpp`, `src/core/DebugStyles.cpp`, `src/gui/widgets/DebugPage.cpp`, `src/gui/widgets/GraphWidget.cpp`, `src/gui/widgets/StateGraphWidget.cpp`, `src/serial/SerialController.cpp`, `src/serial/SerialWorker.cpp` et `cmake/GenerateProtocol.cmake`.
