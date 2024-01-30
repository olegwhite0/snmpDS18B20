#include <OneWire.h>
#include <DallasTemperature.h>
#include <UIPEthernet.h>

#define ONE_WIRE_BUS 6 //Тут нужно написать ногу на которй висит DS18b20

// отдает оиды
//1.3.6.1.2.1.1.1.0 sysName
//1.3.6.1.2.1.1.3.0 Uptime
//1.3.6.1.2.1.1.5.0 sysDescription
//1.3.6.1.4.1.49701.1.1.0 Temperature in celsius

const byte desc[]={0x57,0x68,0x69,0x74,0x65,0x2d,0x44,0x53,0x32,0x53,0x4e,0x4d,0x50};//1.3.6.1.2.1.1.1.0: "White-DS2SNMP"
// Кодировка оида замороченная, поэтому я просто вписал данные с wireshark
unsigned long tick;//1.3.6.1.2.1.1.3.0: 42000
byte tickHEX[4] = {0x00,0x00,0x00,0x00};//Это значение аптайма в варианте 4 байтов

//--------------------------------------------------------------------------------
//              Обьявление библиотеки работы с датчиком температуры              +
//--------------------------------------------------------------------------------


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

static char celsius_octet[6]="-99.99";
uint32_t prevMillis = millis();
EthernetUDP udp;
//--------------------------------------------------------------------------------
//              Структура для приема пакета                                      +
//--------------------------------------------------------------------------------

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

//--------------------------------------------------------------------------------
//              Функция опроса датчика температуры                               +
//--------------------------------------------------------------------------------

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
    Serial.print(F("initialize: "));
    Serial.println(success ? "success" : "failed");
    tick=256;

}

 
void loop() {
//--------------------------------------------------------------------------------
//              Раз в минуту обновляется аптайм и опрашивается температура       +
//--------------------------------------------------------------------------------


   if (millis() - prevMillis > 60000)// Код выполняется раз в минуту
   { 
      prevMillis = millis();
      if(tick>4294961294)tick=256;//Обнуляем счетчик значением больше 255 чтобы не было сбоя.
      tick += 6000;
      RequestDallas18b20(); //Опрос датчика температуры Dallas18b20
   }

//--------------------------------------------------------------------------------
//                   Блок, в котором мы принимаем пакет и парсим содержимое      +
//--------------------------------------------------------------------------------

  int size = udp.parsePacket();//тут мы получаем размер буфера принятого пакета
  if (size > 0) {
    do
      {
        int iterator=0;//Это счетчик текущей позиции в буфере
        byte* msg = (byte*)malloc(size);//выделяем буфер размером равным принятому пакету
        for(int i=0; i<size; i++){msg[i]={0x00} ;};//записываем нули в буфер на всякий случай
        int len=udp.read(msg,size);//читаем принятый пакет
        iterator++;//первым идет байт 30 указывающий на BER кодировку, его пропускаем
        reqPDU.pdu_payload_len=(int)msg[iterator];iterator=iterator+3;//длина блока данных 1 (включает в себя блоки 2,3,4)
        reqPDU.snmp_version=msg[iterator];iterator++;iterator++;//байт версии протокола если 0 то первая если 1 вторая если 2 то третья
        reqPDU.size_of_community=(int)msg[iterator];iterator++;//размер блока с комьюнити
        for(int i=iterator; i<iterator+reqPDU.size_of_community; i++){reqPDU.community[i-iterator]=msg[i];}//читаем блок коммюнити
        iterator=iterator+reqPDU.size_of_community;//передвигаем указатель на размер блока с коммьюнити
        reqPDU.snmp_type_packet=msg[iterator];iterator=iterator+3;//тип пакет a0 GET a1 GET-NEXT a2 RESPONSE a3 SET 
        reqPDU.size_of_request_id=(int)msg[iterator];iterator++;// читаем размер блока request-id
        for(int i=iterator; i<iterator+reqPDU.size_of_request_id; i++){
          //читаем блок request-id, у ответа на запрос этот блок должен быть одинаковый
          reqPDU.request_id[i-iterator]=msg[i];
        }
        iterator=iterator+reqPDU.size_of_request_id;//передвигаем счетчик на размер блока
        iterator=iterator+11;//Сдвигаем счетчик до поля с оидом. 3 байта ErrorStatus + 3 байта ErrorIndex + 2 байта размер блока3 + 2 байта размер блока4
        reqPDU.size_of_oid=(int)msg[iterator];iterator++;//читаем размер поля с ОИД
        for(int i=iterator; i<iterator+reqPDU.size_of_oid; i++){reqPDU.oid[i-iterator]=msg[i];}//Читаем содержимое поля ОИД
        iterator=iterator+reqPDU.size_of_oid;
        // Освобождаем память буфера чиения
        free(msg);
//--------------------------------------------------------------------------------
//                   отладочный вывод результата                                 +
//--------------------------------------------------------------------------------
      Serial.print("Принят пакет: ");Serial.print("Размер ");Serial.print(reqPDU.pdu_payload_len);Serial.print(" community: ");
      for(int i=0; i<reqPDU.size_of_community; i++){Serial.print(reqPDU.community[i],HEX);Serial.print(" ");}
      Serial.print(" request-id: ");
      for(int i=0; i<reqPDU.size_of_request_id; i++){Serial.print(reqPDU.request_id[i],HEX);Serial.print(" ");}
      Serial.print(" OID: ");
      for(int i=0; i<reqPDU.size_of_oid; i++){Serial.print(reqPDU.oid[i],HEX);Serial.print(" ");}
      Serial.println();
      }
      while ((size = udp.available())>0);

    udp.flush();//эта функция должна ждать пока данные появятся в буфере

    int success;
    do
      {

        Serial.print(F("remote ip: "));
        Serial.print(udp.remoteIP());
        success = udp.beginPacket(udp.remoteIP(),udp.remotePort());
        Serial.print(F(" beginPacket: "));
        Serial.println(success ? "success" : "failed");
      }

    while (!success);
    int iterator_writer=0; // Счетчик указатель позиции в буфере записи
    int datablock_size=0;
 // преобразуем данные из переменной в 4 байта
        tickHEX[3] =  tick;
        tickHEX[2] =  tick >> 8;
        tickHEX[1] =  tick >> 16;
        tickHEX[0] =  tick >> 24;     
            //Serial.print(tick);Serial.print(" ");Serial.print(tickHEX[0],HEX);Serial.print(" ");Serial.print(tickHEX[1],HEX);Serial.print(" ");Serial.print(tickHEX[2],HEX);Serial.print(" ");Serial.println(tickHEX[3],HEX);

// посчитаем сумму оида чтобы разделить одно от другуго
        int hash=0;
        for(int i=0; i<reqPDU.size_of_oid; i++)
        {
          hash=hash+(int)reqPDU.oid[i];
        }
// в зависимости от того какой оид выставим размер данных для расчетов размеров блоков данных, если они будут неверные блок будет ошибочный
        switch (hash) {
        case 55: // оид 1.3.6.1.2.1.1.1.0
            datablock_size=sizeof(desc);
        break;
        case 57: // оид 1.3.6.1.2.1.1.3.0
                if (tick<=65535)
                    {
                      datablock_size=2;
                    }else{
                      datablock_size=4;
                    }
        break;
        case 59: // оид 1.3.6.1.2.1.1.5.0
            datablock_size=sizeof(desc);
        break;
        case 357: // оид 1.3.6.1.4.1.49701.1.1.0
            datablock_size=6;//sizeof(celsius_octet) тут должен быть но результат всегда одного размера 6 байт
        break;
        default:// а тут если любой другой оид, размер допустим два байта потому что выдадим нулевой результат
            datablock_size=2;
            //Serial.print("[");Serial.print("unknown");Serial.println("]");
        break;
}

//--------------------------------------------------------------------------------
//                     Начинаем конструировать ответный пакет                    +
//--------------------------------------------------------------------------------

    int all=7+reqPDU.size_of_community+4+reqPDU.size_of_request_id+12+reqPDU.size_of_oid+2+datablock_size;// Тут считается общий размер пакета
    int first_blocksize=all-2;  // размер  блока данных №1                                                                   
    int two_blocksize=2+reqPDU.size_of_request_id+12+reqPDU.size_of_oid+2+datablock_size; // размер блока данных № 2
    int three_blocksize=4+reqPDU.size_of_oid+2+datablock_size; // размер блока данных № 3
    int four_blocksize=three_blocksize-2;  // размер блока данных № 4         

    byte* full_block = (byte*)malloc(all); // выделяем буфер для ответа размером пакета. 
    for(int i=0; i<all; i++){full_block[i]={0x00} ;}; // заполним буфер нулями на всякий случай
    //SNMP_SIZE
    full_block[iterator_writer]={0x30};iterator_writer++;
    full_block[iterator_writer]=(byte)first_blocksize;iterator_writer++;// размер блока данных № 1
    //SNMP_VERSION
    full_block[iterator_writer]={0x02};iterator_writer++;
    full_block[iterator_writer]={0x01};iterator_writer++;
    full_block[iterator_writer]={0x00};iterator_writer++;// версия протокола 1
    //SNMP_OCTET_Community
    full_block[iterator_writer]={0x04};iterator_writer++;
    full_block[iterator_writer]=(byte)reqPDU.size_of_community;iterator_writer++;
    for(int i=iterator_writer; i<iterator_writer+reqPDU.size_of_community; i++){full_block[i]=reqPDU.community[i-iterator_writer];}iterator_writer=iterator_writer+(byte)reqPDU.size_of_community;
    //Byte_02_is_write_flag
    full_block[iterator_writer]={0xa2};iterator_writer++;// байт типа пакета говорит что это SNMP ANSWER
    full_block[iterator_writer]=(byte)two_blocksize;iterator_writer++; // размер блока данных № 2
    full_block[iterator_writer]={0x02};iterator_writer++;
    full_block[iterator_writer]=(byte)reqPDU.size_of_request_id;iterator_writer++;
    // Request-ID в ответе должен быть такой же как в запросе
    for(int i=iterator_writer; i<iterator_writer+reqPDU.size_of_request_id; i++){full_block[i]=reqPDU.request_id[i-iterator_writer];}iterator_writer=iterator_writer+(byte)reqPDU.size_of_request_id;
    // следующий блок это две переменные типа инт где выводится ошибка, у нас их нет - 02 02 00 02 02 00
    full_block[iterator_writer]={0x02};iterator_writer++;
    full_block[iterator_writer]={0x01};iterator_writer++;
    full_block[iterator_writer]={0x00};iterator_writer++;
    full_block[iterator_writer]={0x02};iterator_writer++;
    full_block[iterator_writer]={0x01};iterator_writer++;
    full_block[iterator_writer]={0x00};iterator_writer++;
    // 30 is next block + size
    full_block[iterator_writer]={0x30};iterator_writer++;
    full_block[iterator_writer]=three_blocksize;iterator_writer++;// размер блока данных № 3
    // 30 is next block + size
    full_block[iterator_writer]={0x30};iterator_writer++; 
    full_block[iterator_writer]=four_blocksize;iterator_writer++; // размер блока данных № 4
    // oid block
    full_block[iterator_writer]={0x06};iterator_writer++;
    full_block[iterator_writer]=(byte)reqPDU.size_of_oid;iterator_writer++;
    for(int i=iterator_writer; i<iterator_writer+reqPDU.size_of_oid; i++){full_block[i]=reqPDU.oid[i-iterator_writer];}iterator_writer=iterator_writer+(byte)reqPDU.size_of_oid;

    //      далее в буфер записываются данные в зависимости от оида
    //      *******************************************************
    
        switch (hash) 
        {
          case 55:// оид 1.3.6.1.2.1.1.1.0
                full_block[iterator_writer]={0x04};iterator_writer++;
                full_block[iterator_writer]=(byte)datablock_size;iterator_writer++;
                for(int i=iterator_writer; i<iterator_writer+datablock_size; i++){full_block[i]=desc[i-iterator_writer];}iterator_writer=iterator_writer+datablock_size;
                //Serial.println("<><><><><>");for(int i=iterator_writer; i<iterator_writer+datablock_size; i++){Serial.print(full_block[i],HEX);Serial.print(" ");}Serial.println(" ");
            break;
            case 57:// оид 1.3.6.1.2.1.1.3.0
                full_block[iterator_writer]={0x43};// байт 43 говорит что тип возвращаемой переменной timetick
                iterator_writer++;
                //byte buf[4];
                //buf[0] = tickHEX[3];  buf[1] = tickHEX[2];  buf[2] = tickHEX[1];  buf[3] = tickHEX[0];
                //tickHEX[0] = buf[0];  tickHEX[1] = buf[1];  tickHEX[2] = buf[2];  tickHEX[3] = buf[3];
                if (tick<=65535)//в выводе блока timetick в зависимости от значения будет либо два байта либо 4, тут анализируется переменная и выбирается размер
                  {
                    datablock_size=2;
                    tickHEX[1]=tickHEX[3];tickHEX[0]=tickHEX[2];// если размер 2 байта, они оказываются в конце и эта строка пишет два байта с конца в первые 2 байта
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
            case 59:// оид 1.3.6.1.2.1.1.5.0
                full_block[iterator_writer]={0x04};iterator_writer++;
                full_block[iterator_writer]=(byte)datablock_size;iterator_writer++;
                for(int i=iterator_writer; i<iterator_writer+datablock_size; i++){full_block[i]=desc[i-iterator_writer];}iterator_writer=iterator_writer+datablock_size;
            break;
            case 357:// оид 11.3.6.1.4.1.49701.1.1.0
                full_block[iterator_writer]={0x04};iterator_writer++;
                full_block[iterator_writer]=(byte)datablock_size;iterator_writer++;
                for(int i=iterator_writer; i<iterator_writer+datablock_size; i++){full_block[i]=celsius_octet[i-iterator_writer];}
                iterator_writer=iterator_writer+datablock_size;

            break;
            default:
                full_block[iterator_writer]={0x05};iterator_writer++;
                full_block[iterator_writer]={0x00};iterator_writer++;
            break;
            hash=0;
      }

       //END OF PREPEAR RESPONSE STRING
    
 //  для отладки выведем буфер если раскоментируем следующее
 //  Serial.print("Full buffer:");for(int i=0; i<all; i++){Serial.print(full_block[i],HEX);Serial.print(" ");}Serial.println(" ");
      
      success = udp.write(full_block,all); //пишем подготовленный буфер в сетевую карту
      
      Serial.print(F("bytes written: "));
      success = udp.endPacket();

      Serial.print(F("endPacket: "));
      Serial.println(success ? "success" : "failed");
      success = udp.endPacket();
      udp.stop();
    //restart with new connection to receive packets from other clients
    success = udp.begin(161);

     // Serial.print(F("restart connection: "));
     // Serial.println(success ? "success" : "failed");
      success = udp.begin(161);
      free(full_block);
      Serial.println();
  }


}

