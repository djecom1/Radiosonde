#include "FS.h"
#include <SPIFFS.h>
#include <WiFi.h>
//Write data web

//Fonction 
void writedataweb(){


String updateHost = "xavier.debert.free.fr";
String updateDataWeb = "/RS/dataweb/index.html.txt";
String *updateData = &updateDataWeb;
  String dispHost = updateHost.substring(0, 14);
  //disp.rdis->drawString(2, 0, dispHost.c_str());

  Serial.println("Connecting to: " + updateHost);
  // Connect to Update host
  if (client.connect(updateHost.c_str(), updatePort)) {
    // Connection succeeded, fecthing the bin
    Serial.println("Fetching index.html: " + String(*updateData));
    

    // Get the contents of the bin file
    client.print(String("GET ") + *updateData + " HTTP/1.1\r\n" +
                 "Host: " + updateHost + "\r\n" +
                 "Cache-Control: no-cache\r\n" +
                 "Connection: close\r\n\r\n");

    // Check what is being sent
    //    Serial.print(String("GET ") + bin + " HTTP/1.1\r\n" +
    //                 "Host: " + host + "\r\n" +
    //                 "Cache-Control: no-cache\r\n" +
    //                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Client Timeout !");
        client.stop();
        return;
      }
    }
    // Once the response is available,
    // check stuff

    /*
       Response Structure
        HTTP/1.1 200 OK
        x-amz-id-2: NVKxnU1aIQMmpGKhSwpCBh8y2JPbak18QLIfE+OiUDOos+7UftZKjtCFqrwsGOZRN5Zee0jpTd0=
        x-amz-request-id: 2D56B47560B764EC
        Date: Wed, 14 Jun 2017 03:33:59 GMT
        Last-Modified: Fri, 02 Jun 2017 14:50:11 GMT
        ETag: "d2afebbaaebc38cd669ce36727152af9"
        Accept-Ranges: bytes
        Content-Type: application/octet-stream
        Content-Length: 357280
        Server: AmazonS3

        {{BIN FILE CONTENTS}}

    */
    while (client.available()) {
      // read line till /n
      String line = client.readStringUntil('\n');
      // remove space, to check if the line is end of headers
      line.trim();

      // if the the line is empty,
      // this is end of headers
      // break the while and feed the
      // remaining `client` to the
      // Update.writeStream();
      if (!line.length()) {
        //headers ended
        break; // and get the OTA started
      }

      // Check if the HTTP Response is 200
      // else break and Exit Update
      if (line.startsWith("HTTP/1.1")) {
        if (line.indexOf("200") < 0) {
          Serial.println("Got a non 200 status code from server. error.");
          break;
        }
      }

      // extract headers here
      // Start with content length
      if (line.startsWith("Content-Length: ")) {
        contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
        Serial.println("Got " + String(contentLength) + " bytes from server");
      }

      // Next, the content type
      if (line.startsWith("Content-Type: ")) {
        String contentType = getHeaderValue(line, "Content-Type: ");
        Serial.println("Got " + contentType + " payload.");
        if (contentType == "application/text") {
          isValidContentType = true;
        }
      }
    }
  } else {
    // Connect to updateHost failed
    // May be try?
    // Probably a choppy network?
    Serial.println("Connection to " + String(updateHost) + " failed. Please check your setup");
    // retry??
    // execOTA();
  }

/*
    Serial.printf("\nDataWeb On\n");
    //open file for appending new blank line to EOF.
    File f = SPIFFS.open("/data.html", "w");

    f.println("<html><body>TEST de DATAWeb1</body></html>");

    f.close();
    */
}



