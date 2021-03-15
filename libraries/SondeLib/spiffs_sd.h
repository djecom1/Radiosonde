#include <FS.h>
#include <SPIFFS.h>
#include <SD_MMC.h> // (or SD_MMC.h)


void transfert_sd() {

  if (!SD_MMC.begin()) {
    Serial.println("Carte SD introuvable");
    
  }
  else{
    Serial.println("Carte SD détectée");
    sonde.clearDisplay();
    disp.rdis->drawString(0, 2, "Carte SD On");
    disp.rdis->drawString(0, 4, "Transfert Data");
    File sourceFile = SPIFFS.open("/data.csv", "r");
    File destFile = SD_MMC.open("/data.csv","w");

    static uint8_t tanpon[512];
    while( sourceFile.read( tanpon, 512) ) {
        destFile.write( tanpon, 512 );
    }
    destFile.close();
    sourceFile.close();
    delay(1000);
    //sonde.updateDisplay();
    sonde.clearDisplay();
    disp.rdis->drawString(0, 2, "Sortir SD");
    disp.rdis->drawString(0, 4, "Reboot 5s");
    delay(5000);
    ESP.restart();
  }

    
}
