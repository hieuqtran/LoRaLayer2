#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <openssl/sha.h>
#include "base.h"

#define HEADER_LENGTH 16
#define SHA1_LENGTH 40

int retransmitEnabled = 0;
int pollingEnabled = 0;
int beaconModeEnabled = 1;
int hashingEnabled = 1;

int bufferInterval = 5; // check buffer every 10 secs

int beaconInterval = 15;
uint8_t beaconCount = 0;
int beaconModeReached = 0;

uint8_t hashTable[256][SHA1_LENGTH];
uint8_t hashEntry = 0;

struct Buffer {
    uint8_t message[256]; 
    uint8_t length;
};

struct Buffer messageBuffer[8];
int bufferEntry = 0;

uint8_t mac[6];
uint8_t macaddr[13];

struct Packet {
    uint8_t ttl;
    uint8_t totalLength;
    uint8_t source[6];
    uint8_t destination[6];
    uint8_t sequence;
    uint8_t type;
    uint8_t data[239];
};

struct routeTableEntry{
    uint8_t destination[6];
    uint8_t nextHop[6];
    uint8_t distance;
    uint8_t metric;
};

struct routeTableEntry routeTable[255];
int routeEntry = 0;


int isHashNew(uint8_t hash[SHA1_LENGTH]){

    int hashNew = 1;
    Serial.printf("hash is %x\n", hash);
    for( int i = 0 ; i <= hashEntry ; i++){
        if(strcmp(hash, hashTable[i]) == 0){
            hashNew = 0; 
            Serial.printf("Not new!\n");
        }
    }
    if(hashNew){
        // add to hash table
        Serial.printf("New message received");
        Serial.printf("\r\n");
        for( int i = 0 ; i < SHA1_LENGTH ; i++){
            hashTable[hashEntry][i] = hash[i];
        }
        hashEntry++;
    }
    return hashNew;
}


void addToBuffer(uint8_t message[256], int length){

    if(bufferEntry > 7){
        bufferEntry = 0;
    }
    Serial.printf("Adding message to buffer");
    Serial.printf("\r\n");

    messageBuffer[bufferEntry].length = length;
    for( int i = 0 ; i < length ; i++){
        messageBuffer[bufferEntry].message[i] = message[i];    
    }

    bufferEntry++;
}

int checkNeighborTable(struct Packet packet){
    Serial.printf("checking neighbor table\n");

    int neighborNew = 1;
    Serial.printf("source: ");
    for(int i = 0 ; i < 6 ; i++){
        Serial.printf("%x", packet.source[i]);
    }
    Serial.printf("\n");
    for( int i = 0 ; i <= routeEntry ; i++){
        Serial.printf("neighbor %d: ", i);
        for(int j = 0 ; j < 6 ; j++){
            Serial.printf("%x", routeTable[i].destination[j]);
        }
        Serial.printf("\n");
        //had to use memcmp instead of string compare?
        if(memcmp(packet.source, routeTable[i].destination, sizeof(packet.source)) == 0){
            neighborNew = 0; 
        }
    }
    if(neighborNew){
        // add to hash table
        Serial.printf("New neighbor found");
        Serial.printf("\r\n");
        for( int i = 0 ; i < 6; i++){
            routeTable[routeEntry].destination[i] = packet.source[i];
        }
        routeEntry++;
    }else{
        Serial.printf("Not new neighbor!\n");
    }
    return neighborNew;
}
    
int packet_received(char* data, size_t len) {

    //size_t send_len;
    //char foo[256];

    data[len] = '\0';
    //Serial.printf("received %d bytes: %s\n", len, data);

    uint8_t* byteData = ( uint8_t* ) data;
    //Serial.printf("%x\n", byteData);

    struct Packet packet = {
        byteData[0],
        byteData[1], 
        byteData[2], byteData[3], byteData[4], byteData[5], byteData[6], byteData[7],
        byteData[8], byteData[9], byteData[10], byteData[11], byteData[12], byteData[13],
        byteData[14],
        byteData[15],
    };
    memcpy(&packet.data, byteData + HEADER_LENGTH, packet.totalLength-HEADER_LENGTH);

    Serial.printf("ttl: %d\n", packet.ttl);

    Serial.printf("length: %d\n", packet.totalLength);

    Serial.printf("source: ");
    for(int i = 0 ; i < 6 ; i++){
        Serial.printf("%x", packet.source[i]);
    }
    Serial.printf("\n");

    Serial.printf("destination: ");
    for(int i = 0 ; i < 6 ; i++){
        Serial.printf("%x", packet.destination[i]);
    }
    Serial.printf("\n");

    Serial.printf("sequence: %02x\n", packet.sequence);
    Serial.printf("type: %c\n", packet.type);

    Serial.printf("data: ");
    for(int i = 0 ; i < packet.totalLength-HEADER_LENGTH ; i++){
        Serial.printf("%c", packet.data[i]);
    }
    Serial.printf("\n");
    
    switch(packet.type){
        case 'h' :
            Serial.printf("this is a hello packet\n");
            checkNeighborTable(packet);
            break;
        case 'c' :
            Serial.printf("this is a chat message\n");
            break;
        default :
            Serial.printf("message type not found\n");
    }
    addToBuffer(data, len);

    return 0;
}

void sendMessage(uint8_t* outgoing, int outgoingLength) {

    //if(!loraInitialized){
    //    return;
    //}

    if(hashingEnabled){
        // do not send message if already transmitted once
        uint8_t hash[SHA1_LENGTH];
        SHA1(outgoing, outgoingLength, hash);
        if(!isHashNew(hash)){
            return;
        }
    }

    Serial.printf("Transmitting packet: ");
    for( int i = 0 ; i < outgoingLength ; i++){
        Serial.printf("%x", outgoing[i]);
    }
    Serial.printf("\r\n");

    //Serial.printf("Sending: ");
    //Serial.printf("%s", outgoing);
    //Serial.printf("\r\n");
   
    /*
    LoRa.beginPacket();
    for( int i = 0 ; i < outgoingLength ; i++){
        LoRa.write(outgoing[i]);
        Serial.printf("%c", outgoing[i]);
    }
    Serial.printf("\r\n");
    LoRa.endPacket();
    LoRa.receive();
    */

    //send_len = snprintf(foo, 256, "Got a packet of %d bytes", (int) len);
    send_packet(outgoing, outgoingLength);
}

long lastCheckTime = 0;
void checkBuffer(){
    if (time(NULL) - lastCheckTime > bufferInterval) {

        if (bufferEntry > 0){

            /* Uncomment if you want race condition to determine a single beacon
            if(!beaconModeReached){
                beaconModeEnabled = 0;
            }
            */

            int transmitLength = messageBuffer[bufferEntry-1].length;
            uint8_t *transmit = malloc(transmitLength);

            Serial.printf("Removing packet from buffer\n");
            for( int i = 0 ; i < transmitLength ; i++){
                transmit[i] = messageBuffer[bufferEntry-1].message[i];
                messageBuffer[bufferEntry-1].message[i] = 0;
            }
            
            if(retransmitEnabled){
                sendMessage(transmit, transmitLength);
            }
            bufferEntry--;
        }else{
            Serial.printf("Buffer is empty");
            Serial.printf("\r\n");
        }
        lastCheckTime = time(NULL);
    }
}

long lastBeaconTime = 0;
void transmitBeacon(){
    if (time(NULL) - lastBeaconTime > beaconInterval) {
        beaconModeReached = 1; 
        int messageLength = 22;
        char buf[256];
        char message[10] = "Hola from\0";
        sprintf(buf, "%s %s", message, macaddr);
        uint8_t* byteMessage = malloc(messageLength);
        byteMessage = ( uint8_t* ) buf;
        uint8_t packetLength = HEADER_LENGTH + messageLength;
        struct Packet testMessage = {
            32,
            packetLength,
            mac[5], mac[4], mac[3], mac[2], mac[1], mac[0],
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            beaconCount,
            'h'
        };

        memcpy(&testMessage.data, byteMessage, testMessage.totalLength);

        uint8_t* sending = malloc(packetLength);
        sending = &testMessage;

        Serial.printf("Sending beacon: ");
        for(int i = 0 ; i < packetLength ; i++){
            Serial.printf("%02x", sending[i]);
        }
        Serial.printf("\r\n");
        send_packet(sending, packetLength);
        beaconCount++;
        lastBeaconTime = time(NULL);
    }
}

void wifiSetup(){

    //WiFi.macAddress(mac);
    // generate random mac address
    srand(time(NULL) + getpid());
    for (int i=0; i<6; i++){
        mac[i] = rand()%256;
    }

    sprintf(macaddr, "%02x%02x%02x%02x%02x%02x\0", mac[5], mac[4], mac[3], mac[2], mac[1], mac [0]);

    Serial.printf("%s\n", macaddr);

    //strcat(ssid, macaddr);
    //WiFi.hostname(hostName);
    //WiFi.mode(WIFI_AP);
    //WiFi.softAPConfig(local_IP, gateway, netmask);
    //WiFi.softAP(ssid);
}


int setup() {

    Serial.printf("initialized\n");

    wifiSetup();

    // random wait at boot
    int wait = rand()%30;
    Serial.printf("waiting %d s\n", wait);
    nsleep(wait, 0);

    return 0;
}



int loop() {

    int packetSize; 

    //if(transmitting() == 1){
        // do stuff while LoRa packet is being sent
        //Serial.print("transmitting a packet...\r\n");
        //return;
    //}else{
        // do stuff when LoRa packet is NOT being sent
        checkBuffer(); 

        if (beaconModeEnabled){
            transmitBeacon();
        }
    //}
    nsleep(1, 0);
}
