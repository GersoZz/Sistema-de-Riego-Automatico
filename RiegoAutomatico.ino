#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>
#include <ArduinoJson.h>

#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#define WIFI_SSID "MOVISTAR_1720" /* Nombre del Wifi */
#define WIFI_PASSWORD "3HmrBYTaXn4eUFmyXE4z" /* Clave del Wifi */

#define FIREBASE_HOST "riegoautomatizado-iot-default-rtdb.firebaseio.com" /*Dirección de la BBDD */
#define FIREBASE_AUTH "jJB07etotRr5Us4spxsYaJaHBDu3hYyv65HyZDIA" /*Acceso a la BBDD */


#define sensor A0 
#define bomba 5 // Rele donde esta conectada la bomba de agua
#define nivel 4

//0 y 2

#define nivelDos 0
#define elecVal 2

int tiempoMedicion = 5; // tiempo en el que se ira actualizando la bbdd
unsigned long tiempoAntMedicion = 0;// auxiliar para tiempoMedicion

int umbral = 700; // valor desde el cual se considera que la tierra esta seca

bool changed = false;//auxiliar para state bomb

int swValvula= false;
void setup() {
  
  Serial.begin(9600);
    
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD); // Conecta al WiFi
  Serial.print("conectando...");
  while(WiFi.status()!= WL_CONNECTED){
    Serial.print(".");
    delay(500);
    }
  Serial.println();
  Serial.print("conectado: ");
  Serial.println(WiFi.localIP());
  //Ya esta conectado al wifi
  
  //Se conecta a la BBDD
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  // incializaciamos los sensores y modulos
  pinMode(sensor,INPUT);
  pinMode(bomba,OUTPUT);
  pinMode(nivel,INPUT);

  Firebase.stream("valvula")//escucha los cambios en 'valvula'
}

void loop() {
  
  int humedad = getHumedad();
  
  uploadHumedad(humedad);//sube hora y humedad a la bbdd
  uploadStateBomba();//sube el estado de la bomba

  switchValvChange();//detecta los cambios en el switch de la web

  //sensores de nivel de agua horizontal
  int sensorNivel = digitalRead(nivel);
  int sensorNivelDos = digitalRead(nivelDos);

//1:nivel bajo de agua , 0: nivel alto de agua 
  
  //sensor boya inferior y superior en nivel bajo de agua
  if(sensorNivel == 1 && sensorNivelDos==1){ //el tanque se esta quedando vacio
  
      Firebase.setBool("nivel_state",0);//estado del nivel de agua en falso
     
      // procedemos a llenar el tanque
      setValvula(true);//activa electro valvula

      
      while(sensorNivelDos == 1){// mientras el sensor superior siga en nivel bajo de agua 
          //la electro valvula se mantiene encendida y el tanque se ira llenando
          sensorNivelDos = digitalRead(nivelDos);
      }
      setValvula(false)//apaga la electrovalvula
  }

  // el tanque tiene suficiente agua
  if(sensorNivel == 0 && sensorNivelDos == 1){
   
      Firebase.setBool("nivel_state",1);

      //la valvula encendida con el switch en la web 
      if(swValvula==true){
        
        setValvula(true);//activamos electro valvula
        while(sensorNivelDos == 1){// mientras el tanque no este completamente lleno
            //la electro valvula se mantiene encendida y el tanque sigue llenando

            //actualizamos el valor del sensor hasta que tenga nivel alto de agua
            sensorNivelDos = digitalRead(nivelDos);
        }
        setValvula(false)//apaga la electrovalvula

        //el tanque no ha estado completamente vacio pero se lo ha llenado
        //manualmente a traves del switch en la pagina web
      }
      
  }

   if(sensorNivel == 0 && sensorNivelDos == 0){
      //tanque completamente lleno    
   }

  delay(60000);//cada 60 segundos comprobaremos la humedad del suelo 
  //y el nivel del agua en el tanque
}

int getHumedad(){
  //recibe el valor de "humedad" (en realidad, sequedad)
  int humedad = analogRead(sensor);

  //la sequedad pasa el umbral deseado
  if(humedad >= umbral){//humedad insuficiente
      //hay que regar
      setBomba(true); //prende la bomba de agua
      
    }else{//menor al umbral, tierra humeda
      //no hay que regar
      setBomba(false);// mantiene la boma de agua apagada 
    }
  return humedad;
}

String getTime(){
  WiFiClient client;
  HTTPClient http;
  String timeS = "";
  http.begin(client,"http://worldtimeapi.org/api/timezone/America/Lima");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY){
    String payload = http.getString();
    int beginS = payload.indexOf("datetime");
    int endS = payload.indexOf("day_of_week");
    //Serial.println(payload);
    timeS = payload.substring(beginS + 11,endS-3);
    Serial.println(timeS);
    }
    return timeS;
}


void uploadHumedad (int value){//subimos humedad a la bbdd
  if ( millis() - tiempoAntMedicion >= tiempoMedicion * 1000){
    
    tiempoAntMedicion = millis();//milis() tiempo en m desde que el arduino comenzó a correr
    Serial.print("uploadHumedad -> ");
    Serial.println(value);
    String Time = getTime();//Hora
    String path = "mediciones/";
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& data= jsonBuffer.createObject();
    data["time"]= Time;
    data["sensor"]=value;
    Firebase.push(path,data);//Sube Hora y Humedad
    
    }
  }

 void setBomba(bool estado){//activa o no la bomba
   bool estadoAct= digitalRead(bomba);

   if(estadoAct == estado){
      digitalWrite(bomba, !estado);//Esto hace lo que quiero
    
      Firebase.setBool("bomba_state",estado);//subo a Firebase
    
    //uplodeamos humedad pq la bomba se activó 
    int humd = getHumedad();
    tiempoAntMedicion = millis();
    String Time = getTime();
    String path = "mediciones/";
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& data= jsonBuffer.createObject();
    data["time"]= Time;
    data["sensor"]=humd;
    Firebase.push(path,data);


      /* ------*/
      Serial.print("set bomba: ");
      Serial.println(estado);
      changed= true;
    }
  
}

void setValvula(bool estado){
   bool estadoAct= digitalRead(elecVal);

//estadoAct en true es apagado
   if(estadoAct == estado){
      digitalWrite(elecVal, !estado);//Esto hace lo que quiero
    
      //  Firebase.setBool("valv_state",estado);//subo a Firebase
    
      //changedDos= true;
    }
  }



void uploadStateBomba(){//sube el estado de la bomba

    if (changed){
      bool estado = digitalRead(bomba);
      Firebase.setBool("bomba_state",!estado);
      
      if(!Firebase.failed()){
        Serial.println("write success");
        changed = false;
      }else{
          Serial.println("write failed");
      }
      
      delay(1000);
      
    }
}


void switchValvChange(){
  if(Firebase.available()){
    FirebaseObject event = Firebase.readEvent();
    String eventType = event.getString("type");
    if(eventType == "put"){
          Serial.println("!.... Cambios en la BBDD ....!");
          //obtienne el valor booleano del campo valvula en la BBDD
          bool value= Firebase.getBool("valvula");
          
          if(!Firebase.failed()){
          //cambia el valor de la variable segun el switch en la web
          swValvula=value;
          }
          
    }
  }
}
