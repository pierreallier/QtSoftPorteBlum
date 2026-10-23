# Sauvegarde et ouverture des mesures

`GraphiqueWidget` exporte les points disponibles dans son historique local. `VisualisationPage` ouvre chaque fichier dans une fenêtre indépendante, sans commande série ni bouton Enregistrer.

## Contenu sauvegardé

L'export inclut toutes les courbes configurées, même décochées. Les valeurs sont converties dans leurs unités physiques, sans normalisation verticale. Les capacités en acquisition sont de 2000 trames moteur et 400 trames inertielles par graphique.

Les colonnes inertielles sont présentes si la centrale est cochée ou si des points inertiels restent en mémoire. Désactiver la centrale ne supprime donc pas les données déjà reçues. Une seule colonne de temps est utilis?e. Chaque trame inertielle est associ?e ? la mesure moteur strictement ant?rieure la plus proche. Seule la premi?re trame associ?e ? cette mesure est conserv?e pour le graphique et le CSV. Les cellules sans mesure restent vides.

## Format CSV

La première ligne contient la version exacte du logiciel et une date ISO 8601 :

```text
# Blum Mesures - 0.2.0 - 2026-10-18T14:30:00
```

La version provient de `BLUM_VERSION`, définie par CMake. La date est celle mémorisée par le graphique pour l'enregistrement.

Un enregistrement issu du pilotage ajoute exactement deux lignes de métadonnées : une description du mode avec ses gains utiles, puis la description de consigne préfixée par `CONSIGNE `. Par exemple :

```text
# Boucle Fermée - Vitesse (Kp=1,00, Ki=2,00, Kd=3,00)
# CONSIGNE Echelon : Amplitude 5,00, Durée 10,00 [s], Délai 0,00 [s]
```

Les gains figurent dans la ligne du mode ; aucune ligne `# PID ...` séparée n'est produite.

La ligne suivante contient les noms des colonnes entre guillemets. L'ordre suit les courbes configurées, d'abord les mesures moteur puis les courbes inertielles :

| Groupe | Colonnes avec la configuration actuelle |
| --- | --- |
| Mesures | `Temps [s]`, `Consigne []`, `Angle porte [°]`, `Tension moteur [V]`, `Courant moteur [A]`, `Angle moteur [°]`, `Vitesse moteur [rad/s]`. |
| Centrale facultative | Accélération X/Y/Z, Vitesse X/Y/Z et Angle X/Y/Z avec leurs unités. |

Dans Pilotage, l'unité de Consigne est adaptée au mode : PWM, rad/s ou °. Les noms et unités du fichier doivent correspondre à ceux attendus par le graphique lecteur.

Le séparateur est `;` lorsque la région utilise la virgule décimale, sinon `,`. Les nombres suivent la région sans séparateur de milliers, avec jusqu'à 17 chiffres significatifs. Les intitulés contenant des guillemets les échappent par doublement. L'écriture utilise `QSaveFile` pour remplacer le fichier après réussite de la sauvegarde.

## Ouverture et validation

Ouvrir utilise le dossier mémorisé dans `files/openFolder`, puis enregistre le dossier choisi. Le lecteur vérifie :

- le préfixe `# Blum Mesures`, la version exacte et la validité de la date ISO ;
- l'absence de métadonnées ou la présence des deux lignes du format actuel, avec les gains pour un mode en boucle fermée ;
- les noms, unités et l'ordre exact des colonnes par rapport à la configuration actuelle ;
- le nombre de champs et la validité des nombres finis de chaque ligne.

Le lecteur accepte les séparateurs virgule et point-virgule, les titres entre guillemets et un éventuel BOM initial. Une incompatibilité affiche une erreur. Les anciens en-têtes avec des lignes PID distinctes sont refusés. Une même version ne garantit pas la compatibilité si les noms ou unités de courbes ont changé.

La fenêtre affiche le nom du fichier, sa date et, lorsqu'ils existent, le mode, les gains et la consigne. La centrale est activée dans la vue si ses colonnes sont présentes. Le zoom et la sélection des courbes restent disponibles ; aucun rafraîchissement série ne recadre cette fenêtre. La croix ferme et détruit la fenêtre.

Voir [Config.md](Config.md) pour les conversions et [Interface.md](Interface.md) pour la manipulation des graphiques.

Les fichiers d?une version ant?rieure ? la version actuelle, notamment v0.1, sont refus?s avant la lecture des donn?es avec un message pr?cisant les deux versions. Les versions futures sont ?galement refus?es comme incompatibles.
