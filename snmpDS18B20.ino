#include <OneWire.h>
#include <DallasTemperature.h>
#include <UIPEthernet.h>

#define ONE_WIRE_BUS 6

const byte desc[]={0x57,0x68,0x69,0x74,0x65,0x2d,0x44,0x53,0x32,0x53,0x4e,0x4d,0x50};//1.3.6.1.2.1.1.1.0: "White-DS2SNMP"
unsigned long tick;//1.3.6.1.2.1.1.3.0: 42000
byte tickHEX[4] = {0x00,0x00,0x00,0x00};
//const byte desc[]={0x57,0x68,0x69,0x74,0x65,0x2d,0x44,0x53,0x32,0x53,0x4e,0x4d,0x50};//1.3.6.1.2.1.1.5.0: "White-DS2SNMP"
//const byte temp[]= {};//1.3.6.1.4.1.49701.1.1.0: " 24.69"

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

static char celsius_octet[6]="-99.99";
uint32_t prevMillis = millis();
EthernetUDP udp;

struct PDU {
  int pdu_payload_len=0;
  int snmp_version=0;//версия snmp 00-v1 01-v2c 02-v3
  int size_of_community=0;
  byte community[64]={0};
  byte snmp_type_packet=0; //тип пакета a0-GET a1-RESPONSE a1-GET-NEXT
  int size_of_request_id=0;
  byte request_id[64]={0};
  byte err_status=0;
  byte err_index=0;
  int size_of_oid=0;
  byte oid[64]={0};
  byte type_of_result=0;//
  int size_of_result=0;
};


void RequestDallas18b20()
{
unsigned long timing=0;
  while (!sensors.isConversionComplete()) 
  {
    if (millis() - timing > 10000){timing = millis();} // Цикл ничем не занят просто ждем пока изготовятся результаты, целесообразность данного куска кода сомнительна.      
  }
  float celsius=sensors.getTempCByIndex(0);
  dtostrf(celsius,6, 2, celsius_octet);
   // makes it async
  sensors.requestTemperatures();
}



  PDU reqPDU;
void setup() {
  Serial.begin(9600);
  uint8_t mac[6] = {0x00,0x01,0x02,0x03,0x04,0x05};
  Ethernet.begin(mac,IPAddress(10,222,128,251));
  RequestDallas18b20();
  int success = udp.begin(161);
    Serial.println(F("initialize: "));
    Serial.println(success ? "success" : "failed");
    tick=256;
    Serial.println(tick);

}

 
void loop() {


   if (millis() - prevMillis > 60000)// Код выполняется раз в минуту
   { 
      prevMillis = millis();
      tick += 6000;
      RequestDallas18b20(); //Опрос датчика температуры Dallas18b20
      // increment up-time counter
   }
  //check for new udp-packet:
  int size = udp.parsePacket();
  if (size > 0) {
    do
      {
        
        int iterator=0;
        byte* msg = (byte*)malloc(size);
        for(int i=0; i<size; i++){msg[i]={0x00} ;};
        int len=udp.read(msg,size);
        
        iterator++;
        reqPDU.pdu_payload_len=(int)msg[iterator];iterator=iterator+3;
            Serial.print(iterator);Serial.print(">");Serial.print("PayloadLEN: ");Serial.println(reqPDU.pdu_payload_len);


        reqPDU.snmp_version=msg[iterator];iterator++;iterator++;
            Serial.print(iterator);Serial.print(">");Serial.print("Version: ");Serial.println(reqPDU.snmp_version,HEX);

        reqPDU.size_of_community=(int)msg[iterator];iterator++;
            Serial.print(iterator);Serial.print(">");Serial.print("SizeOfCommunity: ");Serial.println(reqPDU.size_of_community);
            Serial.print(iterator);Serial.print(">");Serial.print("Community: ");
        for(int i=iterator; i<iterator+reqPDU.size_of_community; i++){reqPDU.community[i-iterator]=msg[i];Serial.print(reqPDU.community[i-iterator],HEX);}
            Serial.println();
        iterator=iterator+reqPDU.size_of_community;

        reqPDU.snmp_type_packet=msg[iterator];iterator=iterator+3;
            Serial.print(iterator);Serial.print(">");Serial.print("TypePacket: ");Serial.println(reqPDU.snmp_type_packet,HEX);
        reqPDU.size_of_request_id=(int)msg[iterator];iterator++;
            Serial.print(iterator);Serial.print(">");Serial.print("SizeRequestId: ");   Serial.println(reqPDU.size_of_request_id,HEX);
        //--------------------
            Serial.print(iterator);Serial.print(">");Serial.print("-------RequestId: ");
        for(int i=iterator; i<iterator+reqPDU.size_of_request_id; i++){
        reqPDU.request_id[i-iterator]=msg[i];
        Serial.print(reqPDU.request_id[i-iterator],HEX);
        }
         Serial.println();
        iterator=iterator+reqPDU.size_of_request_id;
        ----------------------
        iterator=iterator+7+3+2+10;
            Serial.print(iterator);Serial.print(">");Serial.print("SizeOfOID: "); 
        reqPDU.size_of_oid=(int)msg[iterator];iterator++;
        Serial.print(reqPDU.size_of_oid);Serial.println(" "); 
            Serial.print(iterator);Serial.print(">");Serial.print("==>OID==>: ");
        for(int i=iterator; i<iterator+reqPDU.size_of_oid; i++){reqPDU.oid[i-iterator]=msg[i];Serial.print(msg[i],HEX);Serial.print(" ");  }
        Serial.println();
        iterator=iterator+reqPDU.size_of_oid;
        Serial.print(iterator);Serial.print(">");Serial.print("==>end==>: ");
        free(msg);
       // iterator=0;
      }
      while ((size = udp.available())>0);

    
    //finish reading this packet:
    udp.flush();

      Serial.println(F("udp-flush"));

    int success;
    do
      {

        Serial.print(F("remote ip: "));
        Serial.println(udp.remoteIP());
        Serial.print(F("remote port: "));
        success = udp.beginPacket(udp.remoteIP(),udp.remotePort());
        Serial.print(F("beginPacket: "));
        Serial.println(success ? "success" : "failed");
      }

    while (!success);
    int iterator_writer=0;
    int datablock_size=0;
    

    
        tickHEX[0] =  tick;
        tickHEX[1] =  tick >> 8;
        tickHEX[2] =  tick >> 16;
        tickHEX[3] =  tick >> 24;
        
        Serial.print(tick);Serial.print(" ");Serial.print(tickHEX[0],HEX);Serial.print(" ");Serial.print(tickHEX[1],HEX);Serial.print(" ");Serial.print(tickHEX[2],HEX);Serial.print(" ");Serial.println(tickHEX[3],HEX);

      

//----------- OID ---CASE---BLOCK---LOGIC-------
        int hash=0;
        for(int i=0; i<reqPDU.size_of_oid; i++)
        {
          hash=hash+(int)reqPDU.oid[i];
        }

        switch (hash) {
        case 55:
            datablock_size=sizeof(desc);
        break;
        case 57:
                if (tick<=65535)
                    {
                      datablock_size=2;
                    }else{
                      datablock_size=4;
                    }
        break;
        case 59:
            datablock_size=sizeof(desc);
        break;
        case 357:
            datablock_size=6;//sizeof(celsius_octet);
        break;
        default:
            datablock_size=2;
            Serial.print("[");Serial.print("unknown");Serial.println("]");
        break;
}

    int all=7+reqPDU.size_of_community+4+reqPDU.size_of_request_id+12+reqPDU.size_of_oid+2+datablock_size;
    int first_blocksize=all-2;                                                                     // Serial.print(first_blocksize);Serial.print("-");Serial.println(all,HEX);
    int two_blocksize=2+reqPDU.size_of_request_id+12+reqPDU.size_of_oid+2+datablock_size;                        //Serial.print(two_blocksize);Serial.print("-");Serial.println(two_blocksize,HEX);
    int three_blocksize=4+reqPDU.size_of_oid+2+datablock_size;                                                  // Serial.print(three_blocksize);Serial.print("-"); Serial.println(three_blocksize,HEX);
    int four_blocksize=three_blocksize-2;                                                          // Serial.print(four_blocksize);Serial.print("-");Serial.println(four_blocksize,HEX);


    byte* full_block = (byte*)malloc(all);
    for(int i=0; i<all; i++){full_block[i]={0x00} ;};
    //SNMP_SIZE
    full_block[iterator_writer]={0x30};iterator_writer++;
    full_block[iterator_writer]=(byte)first_blocksize;iterator_writer++;
    //SNMP_VERSION
    full_block[iterator_writer]={0x02};iterator_writer++;
    full_block[iterator_writer]={0x01};iterator_writer++;
    full_block[iterator_writer]={0x00};iterator_writer++;
    //SNMP_OCTET_Community
    full_block[iterator_writer]={0x04};iterator_writer++;
    full_block[iterator_writer]=(byte)reqPDU.size_of_community;iterator_writer++;
    for(int i=iterator_writer; i<iterator_writer+reqPDU.size_of_community; i++){full_block[i]=reqPDU.community[i-iterator_writer];}iterator_writer=iterator_writer+(byte)reqPDU.size_of_community;
    //Byte_02_is_write_flag
    full_block[iterator_writer]={0xa2};iterator_writer++;
    full_block[iterator_writer]=(byte)two_blocksize;iterator_writer++;
    full_block[iterator_writer]={0x02};iterator_writer++;
    full_block[iterator_writer]=(byte)reqPDU.size_of_request_id;iterator_writer++;
    for(int i=iterator_writer; i<iterator_writer+reqPDU.size_of_request_id; i++){full_block[i]=reqPDU.request_id[i-iterator_writer];}iterator_writer=iterator_writer+(byte)reqPDU.size_of_request_id;
    // block off error id is - 02 02 00 02 02 00
    full_block[iterator_writer]={0x02};iterator_writer++;
    full_block[iterator_writer]={0x01};iterator_writer++;
    full_block[iterator_writer]={0x00};iterator_writer++;
    full_block[iterator_writer]={0x02};iterator_writer++;
    full_block[iterator_writer]={0x01};iterator_writer++;
    full_block[iterator_writer]={0x00};iterator_writer++;
    // 30 is next block + size
    full_block[iterator_writer]={0x30};iterator_writer++;
    full_block[iterator_writer]=three_blocksize;iterator_writer++;
    // 30 is next block + size
    full_block[iterator_writer]={0x30};iterator_writer++;
    full_block[iterator_writer]=four_blocksize;iterator_writer++;
    // oid block
    full_block[iterator_writer]={0x06};iterator_writer++;
    full_block[iterator_writer]=(byte)reqPDU.size_of_oid;iterator_writer++;
    for(int i=iterator_writer; i<iterator_writer+reqPDU.size_of_oid; i++){full_block[i]=reqPDU.oid[i-iterator_writer];}iterator_writer=iterator_writer+(byte)reqPDU.size_of_oid;
    // data block
    
    //----------- OID ---CASE---BLOCK---LOGIC-------
        hash=0;
        for(int i=0; i<reqPDU.size_of_oid; i++)
        {
          hash=hash+(int)reqPDU.oid[i];
        }

        
        Serial.print("[");Serial.print(hash);Serial.println("]");

        switch (hash) 
        {
          case 55:
                Serial.println("<><><><><>");
                full_block[iterator_writer]={0x04};iterator_writer++;
                full_block[iterator_writer]=(byte)datablock_size;iterator_writer++;
                for(int i=iterator_writer; i<iterator_writer+datablock_size; i++){full_block[i]=desc[i-iterator_writer];}iterator_writer=iterator_writer+datablock_size;
                Serial.println("<><><><><>");for(int i=iterator_writer; i<iterator_writer+datablock_size; i++){Serial.print(full_block[i],HEX);Serial.print(" ");}Serial.println(" ");
            break;
            case 57:
                
                full_block[iterator_writer]={0x43};
                iterator_writer++;
                
                byte buf[4];
                
                buf[0] = tickHEX[3];  buf[1] = tickHEX[2];  buf[2] = tickHEX[1];  buf[3] = tickHEX[0];
                tickHEX[0] = buf[0];  tickHEX[1] = buf[1];  tickHEX[2] = buf[2];  tickHEX[3] = buf[3];

                if (tick<=65535)
                  {
                    datablock_size=2;
                    tickHEX[1]=tickHEX[3];tickHEX[0]=tickHEX[2];
                  }else{
                    datablock_size=4;
                  }
                  
                full_block[iterator_writer]=(byte)datablock_size;iterator_writer++;

                for(int i=iterator_writer; i<iterator_writer+datablock_size; i++)
                  {
                    full_block[i]=tickHEX[i-iterator_writer];
                  }
                iterator_writer=iterator_writer+datablock_size;
                
            break;
            case 59:
                full_block[iterator_writer]={0x04};iterator_writer++;
                full_block[iterator_writer]=(byte)datablock_size;iterator_writer++;
                for(int i=iterator_writer; i<iterator_writer+datablock_size; i++){full_block[i]=desc[i-iterator_writer];}iterator_writer=iterator_writer+datablock_size;
                Serial.println("<><><><><>");for(int i=iterator_writer; i<iterator_writer+datablock_size; i++){Serial.print(full_block[i],HEX);Serial.print(" ");}Serial.println(" ");
            break;
            case 357:
                Serial.println("<><><><><>");
                full_block[iterator_writer]={0x04};iterator_writer++;
                full_block[iterator_writer]=(byte)datablock_size;iterator_writer++;
                for(int i=iterator_writer; i<iterator_writer+datablock_size; i++){full_block[i]=celsius_octet[i-iterator_writer];}
                iterator_writer=iterator_writer+datablock_size;
                Serial.println("<><><><><>");for(int i=iterator_writer; i<iterator_writer+datablock_size; i++){Serial.print(full_block[i],HEX);Serial.print(" ");}Serial.println(" ");

            break;
            default:
                full_block[iterator_writer]={0x05};iterator_writer++;
                full_block[iterator_writer]={0x00};iterator_writer++;
            break;
            hash=0;
      }
        Serial.print("[");Serial.print(hash);Serial.println("]");
    //----------- END ---- OID ---CASE---BLOCK --------
       //END OF PREPEAR RESPONSE STRING
    
    for(int i=0; i<all; i++){Serial.print(full_block[i],HEX);Serial.print(" ");}Serial.println(" ");
      
      success = udp.write(full_block,all);
      
      Serial.print(F("bytes written: "));
      success = udp.endPacket();

      Serial.print(F("endPacket: "));
      Serial.println(success ? "success" : "failed");
      success = udp.endPacket();
      udp.stop();
    //restart with new connection to receive packets from other clients
    //success = udp.begin(161);

      Serial.print(F("restart connection: "));
      Serial.println(success ? "success" : "failed");
      success = udp.begin(161);
      free(full_block);
  }


}

