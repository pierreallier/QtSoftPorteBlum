# Réglage et acquisition dans Pilotage

`PilotagePage` prépare un mode, deux réglages PID et une consigne. Le fonctionnement actuel transmet ces paramètres puis réalise une acquisition locale de dix secondes. La commande de lancement `DO PILOTER`, définie dans le protocole, n'est pas envoyée par cette séquence.

## Asservissement et correcteurs

| Sélection | Commande de mode | Bornes d'amplitude de l'interface |
| --- | --- | --- |
| Asservissement désactivé | `SET MODE PWM` | -255 à 255. |
| Vitesse seule | `SET MODE VITESSE` | -10 à 10 rad/s. |
| Position seule | `SET MODE POSITION` | -150 à 150°. |
| Vitesse et position | `SET MODE POSITION_VITESSE` | -150 à 150°. |

L'asservissement est désactivé au départ. Son activation conserve au moins un correcteur sélectionné. Chaque `PIDWidget` propose P, PI ou PID et des gains de 0 à 100. Les coefficients inutilisés sont envoyés à zéro.

L'ordre des coefficients est toujours Kp, Ki, Kd :

```text
SET PID VITESSE 1 2 3
SET PID POSITION 1 0 0
```

Dans le graphique Pilotage, la présentation de la consigne remplace le facteur JSON par `1` en PWM et `0.001` en boucle fermée, avec l'unité du mode. Les autres grandeurs conservent leurs facteurs configurés.

## Consigne

`ConsigneWidget` propose un échelon, une rampe, un trapèze et une sinusoïde, avec aperçu. L'inversion change le signe de l'amplitude envoyée. L'interface exprime les durées en secondes ; les commandes utilisent des millisecondes entières et des nombres avec point décimal.

La durée totale est comprise entre 0,1 et 10 s. Le champ Délai représente un délai de départ, borné par cette durée. La durée utile vaut la durée totale moins ce délai. Pour le sinus, ce champ est masqué et remplacé par un offset vertical dans l'unité d'amplitude ; la fréquence va de 0,01 à 100 Hz. Les deux crêtes `offset ± |amplitude|` restent dans les limites d'amplitude. Une augmentation d'amplitude ajuste l'offset si nécessaire. Le délai des autres profils est conservé lors des changements de type.

| Profil | Commande produite après `SET CONSIGNE ` |
| --- | --- |
| Échelon | `ECHELON amplitude durée_utile_ms délai_ms` |
| Rampe | `RAMPE 0.0 valeur_finale durée_utile_ms délai_ms` |
| Trapèze | `TRAPEZE amplitude montée_ms plateau_ms descente_ms délai_ms` |
| Sinusoïde | `SINUS amplitude période_ms durée_ms offset` (offset dans l'unité d'amplitude) |

La rampe part de zéro. Pour le trapèze, le plateau vaut `durée totale - délai - montée - descente` ; les durées de montée et descente sont ajustées pour que le plateau reste positif ou nul. Pour le sinus, la période vaut `1 / fréquence` et le dernier paramètre transmet le décalage vertical au firmware.

Exemple d'échelon d'amplitude 100, durée totale 5 s et délai 1 s :

```text
SET CONSIGNE ECHELON 100 4000 1000
```

## Séquence du bouton Envoyer

La carte doit être connectée et initialisée. Envoyer ouvre une progression annulable et transmet, dans cet ordre :

1. `SET MODE ...` ;
2. `SET PID VITESSE ...` ;
3. `SET PID POSITION ...` ;
4. `SET CONSIGNE ...` ;
5. `SET MESURES ON`.

Chaque commande attend une réponse `O` avant de poursuivre. Une réponse `N`, une expiration après deux secondes ou une interruption arrête la séquence. L'ancien graphique est conservé tant que toute la configuration n'a pas été acceptée.

Après acceptation, le graphique est réinitialisé et les métadonnées du mode et de la consigne sont mémorisées. L'avant-dernière trame M de l'historique partagé est conservée seule comme référence à t=0 ; avec une seule trame M, celle-ci est utilisée. Les trames A ne participent pas au choix. Sans trame M, la première nouvelle trame reçue fixe l'origine.

Le curseur est ensuite placé à la fin du lot partagé. Les nouvelles trames sont copiées avec leur temps relatif, sans modifier les événements bruts de Debug. Un timer local de dix secondes démarre, indépendamment de la durée du profil et sans attendre un état matériel PILOTAGE.

## Arrêt et changement de page

Le bouton devient Stopper et les réglages sont désactivés pendant la séquence. Stopper, Annuler et l'expiration des dix secondes demandent `DO STOP`, terminent la séquence locale et arrêtent les mesures si l'acquisition locale avait commencé. Les arrêts peuvent être différés pendant l'attente d'une réponse. Un reset ou une déconnexion annule la séquence locale.

Quitter Pilotage vers Mesures suit une procédure distincte : réponse positive à `SET MESURES OFF`, puis mise en tampon de `DO STOP` si une séquence était en cours, avant de changer la destination des mesures. Debug conserve l'activité courante. Voir [Interface.md](Interface.md).

Calibration utilise la même confirmation que Mesures et envoie `DO CALIBRATION`. L'historique Pilotage est indépendant de celui de Mesures et peut être sauvegardé selon [Mesures.md](Mesures.md). Les constantes des commandes sont détaillées dans [Config.md](Config.md).
