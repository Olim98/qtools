#include "include.h"

//**********************************************************
//*  Установка формата сектора в конфигурации контроллера
//*
//*  udsize - размер данных в байтах
//*  ss - размер spare в хз каких единицах
//*  eccs - размер ecc в байтах
//**********************************************************
void set_blocksize(unsigned int udsize, unsigned int ss,unsigned int eccs) {
  
unsigned int cfg0=mempeek(nand_cfg0);
cfg0=(cfg0&(~(0x3ff<<9)))|(udsize<<9); //UD_SIZE_BYTES = blocksize
cfg0=cfg0&(~(0xf<<23))|(ss<<23); //SPARE_SIZE_BYTES = ss
cfg0=cfg0&(~(0xf<<19))|(eccs<<19); //ECC_PARITY_SIZE_BYTES = eccs
mempoke(nand_cfg0,cfg0);
}


  
  

//*******************************************
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {
  

			     
unsigned char datacmd[1024]; // секторный буфер
			     
unsigned char srcbuf[8192]; // буфер страницы 
unsigned char membuf[1024]; // буфер верификации
unsigned char databuf[8192], oobuf[8192]; // буферы сектора данных и ООВ
unsigned int fsize;
FILE* in;
int vflag=0;
int cflag=0;
unsigned int flen=0;
#ifndef WIN32
char devname[]="/dev/ttyUSB0";
#else
char devname[20]="";
#endif
unsigned int cfg0bak,cfg1bak,cfgeccbak;
unsigned int i,opt;
unsigned int block,page,sector;
unsigned int startblock=0;
unsigned int chipset9x25=0;
unsigned int bsize;
unsigned int fileoffset=0;
int wmode=0; // режим записи

#define w_standart 0
#define w_linux    1
#define w_yaffs    2
#define w_image    3

while ((opt = getopt(argc, argv, "hp:k:b:f:vc:z:l:o:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для записи сырого образа flash через регистры контроллера\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-k #      - код чипсета (-kl - получить список кодов)\n\
-b #      - начальный номер блока для записи \n\
-f <x>    - выбор формата записи:\n\
        -fs (по умолчанию) - запись только секторов данных\n\
        -fl - запись только секторов данных в линуксовом формате\n\
        -fy - запись yaffs2-образов\n\
        -fi - запись сырого образа данные+OOB, как есть, без пересчета ЕСС\n\
-z #      - размер OOB на одну страницу, в байтах (перекрывает автоопределенный размер)\n\
-l #      - число записываемых блоков, по умолчанию - до конца входного файла\n\
-o #      - смещение в блоках в исходном файле до начала записываемого участка\n\
-v        - проверка записанных данных после записи\n\
-c n      - только стереть n блоков, начиная от начального.\n\
\n");
    return;
    
   case 'k':
    define_chipset(optarg);
    break;

   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 'c':
     sscanf(optarg,"%x",&cflag);
     if (cflag == 0) {
       printf("\n Неправильно указан аргумент ключа -с");
       return;
     }  
     break;
     
   case 'b':
     sscanf(optarg,"%x",&startblock);
     break;
     
   case 'z':
     sscanf(optarg,"%u",&oobsize);
     break;
     
   case 'l':
     sscanf(optarg,"%x",&flen);
     break;
     
   case 'o':
     sscanf(optarg,"%x",&fileoffset);
     break;
     
   case 'v':
     vflag=1;
     break;
     
   case 'f':
     switch(*optarg) {
       case 's':
        wmode=w_standart;
	break;
	
       case 'l':
        wmode=w_linux;
	break;
	
       case 'y':
        wmode=w_yaffs;
	break;
	
       case 'i':
        wmode=w_image;
	break;
	
       default:
	printf("\n Неправильное значение ключа -f\n");
	return;
     }
     break;
     
   case '?':
   case ':':  
     return;
  }
}  


#ifdef WIN32
if (*devname == '\0')
{
   printf("\n - Последовательный порт не задан\n"); 
   return; 
}
#endif

if (!open_port(devname))  {
#ifndef WIN32
   printf("\n - Последовательный порт %s не открывается\n", devname); 
#else
   printf("\n - Последовательный порт COM%s не открывается\n", devname); 
#endif
   return; 
}

if (!cflag) { 
 in=fopen(argv[optind],"rb");
 if (in == 0) {
   printf("\nОшибка открытия входного файла\n");
   return;
 }
 
}
else if (optind < argc) {// в режиме стирания входной файл не нужен
  printf("\n С ключом -с недопустим входной файл\n");
  return;
}

hello(0);


if ((wmode == w_standart)||(wmode == w_linux)) oobsize=0; // для входных файлов без OOB
oobsize/=spp;   // теперь oobsize - это размер OOB на один сектор

// Сброс контроллера nand
nand_reset();

// Сохранение значений реистров контроллера
cfg0bak=mempeek(nand_cfg0);
cfg1bak=mempeek(nand_cfg1);
cfgeccbak=mempeek(nand_ecc_cfg);

// режим стирания
if (cflag) {
  printf("\n");
  for (block=startblock;block<(startblock+cflag);block++) {
    printf("\r Стирание блока %03x",block); 
    block_erase(block);
  }  
  printf("\n");
  return;
}

//ECC on-off
if (wmode != w_image) {
  mempoke(nand_ecc_cfg,mempeek(nand_ecc_cfg)&0xfffffffe); 
  mempoke(nand_cfg1,mempeek(nand_cfg1)&0xfffffffe); 
}
else {
  mempoke(nand_ecc_cfg,mempeek(nand_ecc_cfg)|1); 
  mempoke(nand_cfg1,mempeek(nand_cfg1)|1); 
}
  
// Определяем размер файла
bsize=(pagesize+oobsize*spp)*ppb;  // размер в байтах полного блока флешки, с учетом ООВ
fileoffset*=bsize; // переводим смещение из байтов в блоки
fseek(in,0,SEEK_END);
i=ftell(in);
if (i<=fileoffset) {
  printf("\n Смещение %i выходит за границу файла\n",fileoffset/bsize);
  return;
}
i-=fileoffset; // отрезаем от длины файла размер пропускаемого участка
fseek(in,fileoffset,SEEK_SET); // встаем на начало записываемого участка
fsize=i/bsize; // размер в блоках
if ((i%bsize) != 0) fsize++; // округляем вверх до границы блока

if (flen == 0) flen=fsize;
else if (flen>fsize) {
  printf("\n Указанная длина %u превосходит размер файла %u\n",flen,fsize);
  return;
}  
  
printf("\n Запись из файла %s, стартовый блок %03x, размер %03x\n Режим записи: ",argv[optind],startblock,flen);
switch (wmode) {
  case w_standart:
    printf("только данные, стандартный формат\n");
    break;
    
  case w_linux: 
    printf("только данные, линуксовый формат\n");
    break;
    
  case w_image: 
    printf("сырой образ без расчета ЕСС\n");
	printf(" Формат данных: %u+%u\n",sectorsize,oobsize); 
    break;
    
  case w_yaffs: 
    printf("образ yaffs2\n");
    if (!chipset9x25) set_blocksize(516,1,10); // data - 516, spare - 1 (чего-то), ecc - 0
    else              set_blocksize(516,2,0); // data - 516, spare - 2 (чего-то), ecc - 10
    break;
}   
    
port_timeout(1000);

// цикл по блокам

for(block=startblock;block<(startblock+flen);block++) {
  // стираем блок
  block_erase(block);
  if (chipset9x25) {
    mempoke(nand_ecc_cfg,(mempeek(nand_ecc_cfg))|2); // сброс движка BCH
    mempoke(nand_ecc_cfg,cfgeccbak); // восстановление конфигурации BCH
  } 

  // цикл по страницам
  for(page=0;page<ppb;page++) {
    
    // читаем весь дамп страницы
    if (fread(srcbuf,1,pagesize+(spp*oobsize),in) < (pagesize+(spp*oobsize))) goto endpage;  // образ страницы - page+oob
    // разбираем дамп по буферам
    if (wmode != w_yaffs) 
      // для всех режимов кроме yaffs - формат входного файла 512+obb
     for (i=0;i<spp;i++) {
       memcpy(databuf+sectorsize*i,srcbuf+(sectorsize+oobsize)*i,sectorsize);
       if (oobsize != 0) memcpy(oobuf+oobsize*i,srcbuf+(sectorsize+oobsize)*i+sectorsize,oobsize);
     }  
     else {
      // для режима yaffs - формат входного файла pagesize+obb 
       memcpy(databuf,srcbuf,sectorsize*spp);
       memcpy(oobuf,srcbuf+sectorsize*spp,oobsize*spp);
     }  
    
    // устанавливаем адрес флешки
    printf("\r Блок: %04x   Страница: %02x",block,page); fflush(stdout);
    setaddr(block,page);

    // устанавливаем код команды записи
    switch (wmode) {
      case w_standart:
	mempoke(nand_cmd,0x36); // page program
	break;

      case w_linux:
      case w_yaffs:
        mempoke(nand_cmd,0x39); // запись data+spare
	break;
	 
      case w_image:
        mempoke(nand_cmd,0x37); // запись data+spare+ecc
	break;
    }
    // цикл по секторам
    for(sector=0;sector<spp;sector++) {
      memset(datacmd,0xff,sectorsize+64); // заполняем секторный буфер FF - значения по умолчанию
      
      // заполняем секторный буфер данными
      switch (wmode) {
        case w_linux:
	// линуксовый (китайский извратный) вариант раскладки данных, запись без OOB
          if (sector < (spp-1))  
	 //первые n секторов
             memcpy(datacmd,databuf+sector*(sectorsize+4),sectorsize+4); 
          else 
	 // последний сектор
             memcpy(datacmd,databuf+(spp-1)*(sectorsize+4),sectorsize-4*(spp-1)); // данные последнего сектора - укорачиваем
	  break;
	  
	case w_standart:
	 // стандартный формат - только сектора по 512 байт, без ООВ
          memcpy(datacmd,databuf+sector*sectorsize,sectorsize); 
	  break;
	  
	case w_image:
	 // сырой образ - data+oob, ECC не вычисляется
          memcpy(datacmd,databuf+sector*sectorsize,sectorsize);       // data
          memcpy(datacmd+sectorsize,oobuf+sector*oobsize,oobsize);    // oob
          break;

	case w_yaffs:
	 // образ yaffs - записываем только данные 516-байтными блоками 
	 //  и yaffs-тег в конце последнего блока
	 // входной файл имеет формат page+oob, но при этом тег лежит с позиции 0 OOB 
          if (sector < (spp-1))  
	 //первые n секторов
             memcpy(datacmd,databuf+sector*(sectorsize+4),sectorsize+4); 
          else  {
	 // последний сектор
             memcpy(datacmd,databuf+(spp-1)*(sectorsize+4),sectorsize-4*(spp-1)); // данные последнего сектора - укорачиваем
             memcpy(datacmd+sectorsize-4*(spp-1),oobuf,16 );    // тег yaffs присоединяем к нему
	  }
	  break;
      }
      // пересылаем сектор в секторный буфер
	  if (!memwrite(sector_buf,datacmd,sectorsize+oobsize)) {
		printf("\n Ошибка передачи секторного буфера\n");
		return;
      }	
      // выполняем команду записи и ждем ее завершения
      mempoke(nand_exec,0x1);
      nandwait();
     }  // конец цикла записи по секторам
     if (!vflag) continue;   // верификация не требуется
    // верификация данных
     printf("\r");
     setaddr(block,page);
     mempoke(nand_cmd,0x34); // чтение data+ecc+spare
     
     // цикл верификации по секторам
     for(sector=0;sector<spp;sector++) {
      // читаем очередной сектор 
      mempoke(nand_exec,0x1);
      nandwait();
      
      // читаем секторный буфер
      memread(membuf,sector_buf,sectorsize+oobsize);
      switch (wmode) {
        case w_linux:
 	// верификация в линуксовом формате
	  if (sector != (spp-1)) {
	    // все сектора кроме последнего
	    for (i=0;i<sectorsize+4;i++) 
	      if (membuf[i] != databuf[sector*(sectorsize+4)+i])
                 printf("! block: %04x  page:%02x  sector:%u  byte: %03x  %02x != %02x\n",
			block,page,sector,i,membuf[i],databuf[sector*(sectorsize+4)+i]); 
	  }  
	  else {
	      // последний сектор
	    for (i=0;i<sectorsize-4*(spp-1);i++) 
	      if (membuf[i] != databuf[(spp-1)*(sectorsize+4)+i])
                 printf("! block: %04x  page:%02x  sector:%u  byte: %03x  %02x != %02x\n",
			block,page,sector,i,membuf[i],databuf[(spp-1)*(sectorsize+4)+i]); 
	  }    
	  break; 
	  
	 case w_standart:
	 case w_image:  
         case w_yaffs:  
          // верификация в стандартном формате
	  for (i=0;i<sectorsize;i++) 
	      if (membuf[i] != databuf[sector*sectorsize+i])
                 printf("! block: %04x  page:%02x  sector:%u  byte: %03x  %02x != %02x\n",
			block,page,sector,i,membuf[i],databuf[sector*sectorsize+i]); 
	  break;   
      }  // switch(wmode)
    }  // конец секторного цикла верификации
  }  // конец цикла по страницам 
} // конец цикла по блокам  
endpage:  
mempoke(nand_cfg0,cfg0bak);
mempoke(nand_cfg1,cfg1bak);
mempoke(nand_ecc_cfg,cfgeccbak);
printf("\n");
}

