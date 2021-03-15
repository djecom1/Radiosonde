  RadioSonde Version 0.9.1
============================
<img src="http://xavier.debert.free.fr/RS/TTGO.jpg" width="50%"><img src="http://xavier.debert.free.fr/RS/TTGO2.jpg" width="50%">
<img src="http://xavier.debert.free.fr/RS/TTGO3.jpg" width="50%"><img src="http://xavier.debert.free.fr/RS/TTGO4.jpg" width="50%"><img src="http://xavier.debert.free.fr/RS/TTGO5.jpg" width="50%"><img src="http://xavier.debert.free.fr/RS/TTGO6.jpg" width="50%">
<img src="http://xavier.debert.free.fr/RS/TTGO7.jpg" width="50%"><img src="http://xavier.debert.free.fr/RS/TTGO8.jpg" width="50%">
<img src="http://xavier.debert.free.fr/RS/Web4.png" width="20%"><img src="http://xavier.debert.free.fr/RS/Web.png" width="20%">
<img src="http://xavier.debert.free.fr/RS/Web3.png" width="20%"><img src="http://xavier.debert.free.fr/RS/Web2.png" width="20%">
<img src="http://xavier.debert.free.fr/RS/Web5.png" width="20%"><img src="http://xavier.debert.free.fr/RS/Web6.png" width="20%">
<img src="http://xavier.debert.free.fr/RS/Web7.png" width="20%">


Projet basé sur le travail de DL9RDZ
====================================

Pour TTGO et LilyGo LORA 32 <br>
Décodage de RadioSonde RS41/92 and DFM06/09/17 et M10+/20

Attention à la version de votre TTGO! <br>
vous devez modifier dans config.txt, le port de l'écran OLED <br>
-  TTGO v1:  SDA=4  SCL=15, RST=16 <br>
-  TTGO v2:  SDA=21 SCL=22, RST=16

## Version en production 0.9.1 devel 0.9.2

## 0.9.1
Corection RS41 <br>
Correction DFM 06/09 <br>
Add DFM17 <br>
Correction for all trame recived for M10 and M20 1000ms to 1512ms,<br>
Correction formulaire QRG, and end RS no save
Correction google-maps
Correction Vbat for axp92

## 0.9.0
Add M20

## 0.8.8

Add M10+ <br>
Add Temps restant avant impacte au sol si 99: 0. 0 soit le balon de la sonde n a pas encore éclaté, <br>
ou les informations ne sont pas disponible actuellement <br>
Add Test Buzzer au démarrage "Arche Perdu" Lol pour des chasseurs de sonde!<br>
Compatible Lilygo esp32 GPS inboard pin 34 Rx, 12 Tx

## 0.8.7

correction bug Buzzer Off->On->Off <br>
Add GainLNA RX SX1278FSK on Web config paramètre <br>
Add update OTA Os + DataWeb <br>
correction bugs sondmap.html <br>
correction text upgrade Os et DataWeb <br>
correction texte boussole S et N <br>
correction bugs distance 4928Km si lat et lon =0 erroné <br>
correction bugs fonction Vbat <br>
Add Telemetry width export data.csv <br>
Suppression µSD incompatible avec pin SX1278FSK et SPI <br>
Add transfert Telemetry To µSD on put SD automatic

## 0.8.5 

Evolution majeur du système <br>
affichage du pourcentage de la batterie en mode scanning <br>
création d'une fenetre Batterie, Boussole <br>
suppresion lib et code TFT <br>
création Azimute, elevation correction de Bugs majeur , mineur <br>
Ajout fonction Smetre, Buzzer, QTH,  Gps on off ... <br>
mise à jour OTA <br>
trop de modification pour toutes les expliciter!

## 0.8.1 

modification de la partie Web

## 0.8.0

travail de refonte et réécriture du code

-------------------------------------------------------------------------------

## Les Boutons optionnel à ajouter(souder)
sur les GPIO 1002 et 1004 [pour les TTGO sans GPS]<br>
attention:

+3.3V--[ SW ]---GPIO----[  R1  ]---/   R1=10 ou 12KOhms

- appuie court	        <1.5 seconds <br>
- appuie double court   0.5 seconds <br>
- appuie moyen          2-4 seconds <br>
- appuie long           >5 seconds

## Buzzer optionnel à ajouter(souder)
sur les GPIO 2 (avec GPS) ou 12 ou perso suivant le modèl

GPIO --[ BUZZER ]---/ 

## Wifi configuration

Au démarrage, si aucune connexion possible au wifi paramètré, il monte un Wifi AP<br>
le SSID et mot de passe par défaut est: <b>Radiosonde</b> <br>
en mode AP, il doit être en 192.168.4.1, <br>
mais vous avez aussi la possibilité de mettre http://radiosonde.local dans n'importe quel Wifi 
connecté.



## Mode Scanne

Le fichier qrg.txt contient la liste par défaut des cannaux.
pour y configurer les noms, fréquences et mode [M=M10, 6=DFM06, 9=DFM09 et 4=RS41]


## Mode Réception

En réception, une seul fréquence  est décodé, les infos de la sonde (ID, GPS, RSSI, ...)
seront affichées dans plusieur fenetre à choisir ( 0 à 6) à configurer dans la page Web
rubrique config.
In receiving mode, a single frequency will be decoded, and sonde info (ID, GPS
coordinates, RSSI) will be displayed. The bar above the IP address indicates,
for the last 18 frames, if reception was successfull (|) or failed (.) 
A DOUBLE press will switch to scanning mode.
A SHORT press will switch to the next channel in channels.txt

## Mode Analyseur

Le mode analyseur de spectre (400..406 MHz) est affiché (chaque ligne == 50 kHz)
Pour les cartes TTGO sans bouton configurable, il y a un nouveau paramètre dans config.txt:
- spectrum=10       // 0=off / 1-99 nombre de seconds pour afficher l'analyseur
- timer=1           // 0=off / 1= afficher le compte à rebours du spectre dans l'affichage du spectre
- marker=1          // 0=off / 1= afficher la fréquence dans l'affichage du spectre

## Setup

voir Setup.md pour l'installation!

## OLED

<img src="http://xavier.debert.free.fr/RS/oled_lilygo.jpeg" width="20%">

Si votre écran du lilygo deviens ainsi plusieurs raisons:

- Votre alimentation est instable
- Votre batterie est trop faible donc instable
- Vous avez un conflit de configuration avec les pins (voir dans le menu configuration) accèssible même si l'écran est dans cette état.

73
Xavier



