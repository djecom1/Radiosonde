# Près-requis
<img src="http://xavier.debert.free.fr/RS/TTGO2.jpg" width="50%">


## Arduino IDE

Avoir une version à jour avec les librairies à jour également
exemple 1.8.13  à télécharger [https://www.arduino.cc/en/software]

## ESP32 support

Fichier -> Préférences

en bas dans la case "URL de gestionnaire de cartes supplémentaires"

Ajouter *https://dl.espressif.com/dl/package_esp32_index.json* et appuier sur oK

Puis dans Outils
Outils -> type de cartes -> Gestionnaire de cartes

dans la case de recherche taper "esp32"

Installer "esp32 by Espressif Systems"

Puis après

## ESP32 Flash Filesystem Upload support

Télécharger le fichier zip de la dernière version 
https://github.com/me-no-dev/arduino-esp32fs-plugin/releases/

Décompresser l'archive dans le répértoire tools de votre Arduino IDE

## Ajouter les bibliothèques

Séléctionner Outils -> Gestionnaire de bibliothèques

Installer "U8g2"

Installer "MicroNMEA"

Installer "TFT_22_ILI9225" nécessaire pas pour l'écran car j'ai tout supprimé 
mais pour les fonts, car le TTGO fonctionne avec OLED SSD1306 par défaut!

Puis pour APRS nouvelle librairies:
APRS-Decoder-Lib et APRS-IS-Lib

## Ajouter les bibliothèques, parties 2

Depuis https://github.com/me-no-dev/ESPAsyncWebServer télécharger le ZIP , l'extraire dans "libraries"
, renommer le répertoire en  ESPAsyncWebServer (supprimer juste "-master")

Depuis https://github.com/me-no-dev/AsyncTCP télécharger le ZIP, l'extraire dans "libraries", et renommer le répertoire en AsyncTCP

de même pour https://github.com/lewisxhe/AXP202X_Library télécharger le ZIP, l'extraire dans "libraries", et renommer le répertoire en AXP202X_Library


## Ajouter les bibliothèques, parties 3

Copier libraries/SX1278FSK 
et libraries/SondeLib et dans SondeLib le répertoire fonts 

fourni dans le dépôt libraries


```
ou sous Linux un lien symbolique est aussi possible mais pas obligatoire!
cd ~/Arduino/libraries
ln -s <whereyouclonedthegit>/radiosonde/libraries/SondeLib/ .
ln -s <whereyouclonedthegit>/radiosonde/libraries/SX1278FSK/ .
```
Redémarrer Arduino IDE

## Ajout carte esp32

Allez dans Outils-> type de cartes -> gestionnaire de cartes

puis dans la case taper esp32 et Installer.

## Dernière parties

Dans Outils -> Esp32 arduino: ->
Séléctionner "TTGO LoRa32-OLED v1"

Puis il vous faut ouvrir le fichier
RadioSonde_FSK.ino

Compiler et Téléverser le dans votre TTGO 

puis il faut téléverser maintenant les DATA!

Dans Outils
cliquer sur ESP32 Sketch Data Upload

voila le TTGO est prêt!

Pour les futur mise à jour,
j'ai prévue une mise à jour directe via OTA depuis le TTGO donc
s'il est connecté à internet depuis votre Smartphone ou votre Box.
Cela se fera depuis le menu de la page Web.
73
Xavier